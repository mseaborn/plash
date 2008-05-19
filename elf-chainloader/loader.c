/* Copyright 2008 Mark Seaborn
   Based on rtldi.c:
   Copyright 2004 BitWagon Software LLC.  All rights reserved.
   This file may be copied, modified, and distributed under the terms
   of the GNU General Public License (GPL) version 2.
*/

#include <elf.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "stack.h"


/* These are defined in <sys/user.h>, but those definitions call to
   sysconf() which ends up giving the wrong value with dietlibc. */
#define PAGE_SIZE               (1UL << 12)
#define PAGE_MASK               (~(PAGE_SIZE-1))


/* From glibc's elf/link.h */
/* We use this macro to refer to ELF types independent of the native wordsize.
   `ElfW(TYPE)' is used in place of `Elf32_TYPE' or `Elf64_TYPE'.  */
#define ElfW(type)      _ElfW (Elf, __WORDSIZE, type)
#define _ElfW(e,w,t)    _ElfW_1 (e, w, _##t)
#define _ElfW_1(e,w,t)  e##w##t


static void print_error(const char *str)
{
    write(2, str, strlen(str));
}

static void fatal_error(const char *str)
{
    print_error("chainloader: error: ");
    print_error(str);
    print_error("\n");
    exit(127);
}

#define REP8(x) \
    ((x)|((x)<<4)|((x)<<8)|((x)<<12)|((x)<<16)|((x)<<20)|((x)<<24)|((x)<<28))
#define EXP8(y) \
    ((1&(y)) ? 0xf0f0f0f0 : (2&(y)) ? 0xff00ff00 : (4&(y)) ? 0xffff0000 : 0)

// Find convex hull of PT_LOAD (the minimal interval which covers all PT_LOAD),
// and mmap that much, to be sure that a kernel using exec-shield-randomize
// won't place the first piece in a way that leaves no room for the rest.
static unsigned long  // returns relocation constant
xfind_pages(unsigned mflags, ElfW(Phdr) const *phdr, int phnum)
{
    size_t lo= ~0, hi= 0, szlo= 0;
    char *addr;

    mflags += MAP_PRIVATE | MAP_ANONYMOUS;  // '+' can optimize better than '|'
    for (; --phnum >= 0; ++phdr) {
	if (phdr->p_type == PT_LOAD) {
	    if (phdr->p_vaddr < lo) {
		lo = phdr->p_vaddr;
		szlo = phdr->p_filesz;
	    }
	    if (hi < (phdr->p_memsz + phdr->p_vaddr)) {
		hi =  phdr->p_memsz + phdr->p_vaddr;
	    }
	}
    }
    szlo += ~PAGE_MASK & lo;  // page fragment on lo edge
    lo   -= ~PAGE_MASK & lo;  // round down to page boundary 
    hi    =  PAGE_MASK & (hi - lo - PAGE_MASK -1);  // page length
    szlo  =  PAGE_MASK & (szlo    - PAGE_MASK -1);  // page length
    addr = mmap((void *)lo, hi, PROT_READ|PROT_WRITE|PROT_EXEC, mflags, 0, 0);
    if(addr == (void *) -1)
	fatal_error("mmap");
    //munmap(szlo + addr, hi - szlo);  // desirable if PT_LOAD non-contiguous
    return (unsigned long)addr - lo;
}

static void
set_auxv_field(ElfW(auxv_t) *av, int const type, ElfW(Addr) value)
{
    for (;; ++av) {
        if (av->a_type==type) {
            av->a_un.a_val = value;
            return;
        }
        if (av->a_type==AT_NULL) {
            return;
        }
    }
}

static unsigned long
do_xmap(
    int const fdi,
    ElfW(Ehdr) const *const ehdr,
    ElfW(auxv_t) *const av
)
{
    ElfW(Phdr) const *phdr = (ElfW(Phdr) const *)(ehdr->e_phoff +
        (char const *)ehdr);
    unsigned long const reloc = xfind_pages(
        ((ET_DYN!=ehdr->e_type) ? MAP_FIXED : 0), phdr, ehdr->e_phnum);
    int j;
    ElfW(Addr) entry_point = ehdr->e_entry + reloc;

    set_auxv_field(av, AT_ENTRY, entry_point);
    set_auxv_field(av, AT_BASE, reloc);
    set_auxv_field(av, AT_PHNUM, ehdr->e_phnum);
    for (j = 0; j < ehdr->e_phnum; ++phdr, ++j) {
	if (PT_LOAD==phdr->p_type && phdr->p_memsz!=0) {
	    int const prot = 7 & (( (REP8(PROT_EXEC ) & EXP8(PF_X))
				    |(REP8(PROT_READ ) & EXP8(PF_R))
				    |(REP8(PROT_WRITE) & EXP8(PF_W)) )
				  >> ((phdr->p_flags & (PF_R|PF_W|PF_X))<<2) );
	    size_t frag = ~PAGE_MASK & phdr->p_vaddr;  /* at lo end */
	    size_t mlen = frag + phdr->p_filesz;
	    char *addr = (char *)(phdr->p_vaddr - frag + reloc);
	    char *const got = mmap(addr, mlen, prot, MAP_FIXED | MAP_PRIVATE,
				   fdi, phdr->p_offset - frag);
	    if (got != addr)
		fatal_error("mmap");
	    if ((ehdr->e_phoff - phdr->p_offset) < phdr->p_filesz) {
		/* This PT_LOAD contains the real ElfW(Phdr)s. */
		set_auxv_field(av, AT_PHDR,
			       (ElfW(Addr)) ((ehdr->e_phoff - phdr->p_offset)
					     + frag + got));
	    }
	    if (phdr->p_flags & PF_W) {
		memset(addr, 0, frag);  /* fragment at lo end */
	    }
	    addr += mlen;
	    frag = (-mlen) &~ PAGE_MASK;  /* distance to next page boundary */
	    if (phdr->p_flags & PF_W) {
		memset(addr, 0, frag);  /* fragment at hi end */
	    }
	    addr += frag;
	    if ((addr - got) < phdr->p_memsz) { /* need more pages, too */
		mlen = phdr->p_memsz - (addr - got);
		void *mapped_addr =
		    mmap(addr, mlen, prot,
			 MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS,
			 0, 0);
		if (mapped_addr != addr)
		    fatal_error("mmap");
	    }
	}
    }
    if (close(fdi) != 0) {
        fatal_error("close");
    }
    return entry_point;
}

static ElfW(auxv_t) *
find_auxv(struct abi_stack *stack)
{
    char **p = stack->argv + stack->argc + 1;
    /* Skip over envp */
    while(*p)
	p++;
    p++;
    return (void *) p;
}

#if defined(__x86_64__)
#  define LD_SO_PATHNAME "/lib/ld-linux-x86-64.so.2"
#else
#  define LD_SO_PATHNAME "/lib/ld-linux.so.2"
#endif

static ElfW(Addr) __attribute__((unused))
chainloader_fixed_file(struct args *args)
{
    ElfW(auxv_t) *auxv = find_auxv(&args->stack);
    args->new_stack = &args->stack;
    int fd = open(LD_SO_PATHNAME, O_RDONLY);
    if(fd < 0)
	fatal_error("open");
    unsigned char buf[512];
    if (sizeof(buf)!=read(fd, buf, sizeof(buf)))
	fatal_error("read");
    /* do_xmap calls close(fd) */
    ElfW(Addr) entry_point = do_xmap(fd, (ElfW(Ehdr) const *) buf, auxv);
    return entry_point;
}

static ElfW(Addr) __attribute__((unused))
chainloader_given_fd(struct args *args)
{
    ElfW(auxv_t) *auxv = find_auxv(&args->stack);
    long argc = args->stack.argc;
    if(argc < 2) {
	print_error("chainloader: No arguments given\n");
	print_error("Usage: chainloader <FD-number> <args>...\n");
	exit(127);
    }
    int fd = atoi(args->stack.argv[1]);

    /* Remove argument from argv */
    long *stack = (long *) &args->stack;
    stack[2] = stack[1]; /* argv[1] = argv[0]; */
    stack[1] = argc - 1;
    args->new_stack = (struct abi_stack *) (stack + 1);

    unsigned char buf[512];
    if (sizeof(buf)!=read(fd, buf, sizeof(buf)))
	fatal_error("read");
    /* do_xmap calls close(fd) */
    ElfW(Addr) entry_point = do_xmap(fd, (ElfW(Ehdr) const *) buf, auxv);
    return entry_point;
}

ElfW(Addr) chainloader_main(struct args *args)
{
    return LOADER(args);
}
