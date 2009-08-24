
CFLAGS = -O2 -Wall -Werror -fno-stack-protector \
	-isystem /usr/lib/diet/include
# dietlibc does not mark itself as not needing an executable stack, so
# need "-z noexecstack" at link time.  Ideally we would only need to
# apply this to entry.S.
BASELINKFLAGS = -static -nostartfiles -nostdlib -Wl,-z,noexecstack
LINKFLAGS = $(BASELINKFLAGS) /usr/lib/diet/lib/libc.a -Wl,-T,link.lds

EXECUTABLES = chainloader chainloader-fixed test-entry test-argv test-sbrk

all: $(EXECUTABLES)

# This is based on the way glibc adds a line to the linker script
# to build ld.so.
# We statically link to a fixed address.  We aim to use the same address
# as the one the kernel loads ld.so at when ld.so is invoked directly,
# but that is not always possible because it varies between 32-bit and
# 64-bit kernels.
# Keeping it the same as normal should reduce potential problems.
# It is significant because the kernel sets up the heap to follow the
# executable's data segment.
# This should work until someone else decides to have a fixed-position
# library or executable with an overlapping address.  Then we'll have
# to fix this.  Making the chainloader relocatable is possible but
# more work because it would have to relocate itself the way ld.so
# does.
# The address must fit in 32 bits otherwise we get linker errors about
# relocation overflows.
link.lds:
	$(CC) $(BASELINKFLAGS) -Wl,--verbose 2>/dev/null | \
	  LC_ALL=C \
	  sed -e '/^=========/,/^=========/!d;/^=========/d'	\
            -e 's/0x08048000/0x80000000/g # for i386' \
	    -e 's/0x400000/0x80000000/g # for x86-64' \
	    > $@

chainloader: entry.o loader.c link.lds
	$(CC) $(CFLAGS) -DLOADER=chainloader_given_fd entry.o loader.c \
		$(LINKFLAGS) -o $@

chainloader-fixed: entry.o loader.c link.lds
	$(CC) $(CFLAGS) -DLOADER=chainloader_fixed_file entry.o loader.c \
		$(LINKFLAGS) -o $@

test-entry: entry.o test-entry.o link.lds
	$(CC) entry.o test-entry.o $(LINKFLAGS) -o $@

test-argv: entry.o test-argv.o link.lds
	$(CC) entry.o test-argv.o $(LINKFLAGS) -o $@

test-sbrk: test-sbrk.o
	$(CC) $^ -o $@

clean:
	rm -f link.lds
	rm -f entry.o loader.o test-entry.o test-argv.o test-sbrk.o
	rm -f $(EXECUTABLES)
