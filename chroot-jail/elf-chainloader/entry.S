
#if defined(__i386__)

	.globl _start
_start:
	# TODO: preserve atexit function (in %edx)
	subl $4, %esp # Placeholder slot in stack
	pushl %esp # Push argument
	call chainloader_main
	addl $4, %esp # Discard argument pushed earlier
	popl %esp # Change the stack pointer to that set by the C function
	jmp *%eax

#elif defined(__x86_64__)

	.globl _start
_start:
	# TODO:	preserve atexit function (in %rdx)
	subq $8, %rsp # Placeholder slot in stack
	movq %rsp, %rdi # %rdi is first argument
	call chainloader_main
	popq %rsp # Change the stack pointer to that set by the C function
	jmp *%rax

#else

#error Unknown architecture

#endif


#if defined(__x86_64__)

/* This replaces dietlibc's __unified_syscall routine.  The difference
   is that it doesn't fill out errno, which saves the chainloader
   from needing a data segment, which would get left behind after the
   chainloader has finished. */

#include <asm/unistd.h>

.weak exit
exit:
.global _exit
_exit:
	mov	$__NR_exit,%al

.global __unified_syscall
__unified_syscall:
	movzbl	%al, %eax
	mov	%rcx, %r10
	syscall
	cmpq	$-128, %rax
	jbe	.Lnoerror
	orq	$-1, %rax
.Lnoerror:
	ret

#endif
