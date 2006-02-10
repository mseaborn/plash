#!/bin/sh

# Copyright (C) 2004 Mark Seaborn
#
# This file is part of Plash, the Principle of Least Authority Shell.
#
# Plash is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of
# the License, or (at your option) any later version.
#
# Plash is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with Plash; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.


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

gcc-3.3 linuxthreads-extras.c -c -O2 -Wall -fPIC -g \
	-o linuxthreads-extras.os
