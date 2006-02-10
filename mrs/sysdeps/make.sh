#!/bin/sh

srcdir=../../../glibc-2.3.3/
objdir=../../

gcc-3.3 not-cancel.c -c -std=gnu99 -O2 \
	-Wall -Winline -Wstrict-prototypes -Wwrite-strings \
	-g -mpreferred-stack-boundary=4 -fPIC \
	-I$srcdir/include -I$objdir -I$srcdir \
	-I$srcdir/mrs/sysdeps -I$srcdir/sysdeps/i386/elf -I$srcdir/linuxthreads/sysdeps/unix/sysv/linux/i386 -I$srcdir/linuxthreads/sysdeps/unix/sysv/linux -I$srcdir/linuxthreads/sysdeps/pthread -I$srcdir/sysdeps/pthread -I$srcdir/linuxthreads/sysdeps/unix/sysv -I$srcdir/linuxthreads/sysdeps/unix -I$srcdir/linuxthreads/sysdeps/i386/i686 -I$srcdir/linuxthreads/sysdeps/i386 -I$srcdir/sysdeps/unix/sysv/linux/i386 -I$srcdir/sysdeps/unix/sysv/linux -I$srcdir/sysdeps/gnu -I$srcdir/sysdeps/unix/common -I$srcdir/sysdeps/unix/mman -I$srcdir/sysdeps/unix/inet -I$srcdir/sysdeps/unix/sysv/i386 -I$srcdir/sysdeps/unix/sysv -I$srcdir/sysdeps/unix/i386 -I$srcdir/sysdeps/unix -I$srcdir/sysdeps/posix -I$srcdir/sysdeps/i386/i686/fpu -I$srcdir/sysdeps/i386/i686 -I$srcdir/sysdeps/i386/i486 -I$srcdir/sysdeps/i386/fpu -I$srcdir/sysdeps/i386 -I$srcdir/sysdeps/wordsize-32 -I$srcdir/sysdeps/ieee754/ldbl-96 -I$srcdir/sysdeps/ieee754/dbl-64 -I$srcdir/sysdeps/ieee754/flt-32 -I$srcdir/sysdeps/ieee754 -I$srcdir/sysdeps/generic/elf -I$srcdir/sysdeps/generic \
	-D_LIBC_REENTRANT -include $srcdir/include/libc-symbols.h -DPIC -DSHARED \
	-DHAVE_INITFINI -o not-cancel.os \
	-MD -MP -MF not-cancel.os.dt
