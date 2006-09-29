#!/bin/sh

# Copyright (C) 2004 Mark Seaborn
#
# This file is part of Plash.
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


make -C ../glibc-2.3.3/socket objdir=`pwd` -f Makefile -f ../elf/rtld-Rules \
	rtld-all rtld-modules='rtld-recvmsg.os rtld-sendmsg.os rtld-send.os'
make -C ../glibc-2.3.3/stdlib objdir=`pwd` -f Makefile -f ../elf/rtld-Rules \
	rtld-all rtld-modules='rtld-getenv.os'
make -C ../glibc-2.3.3/io objdir=`pwd` -f Makefile -f ../elf/rtld-Rules \
	rtld-all rtld-modules='rtld-fxstat.os rtld-fstat.os'
make -C ../glibc-2.3.3/string objdir=`pwd` -f Makefile -f ../elf/rtld-Rules \
	rtld-all rtld-modules='rtld-strncmp.os'
