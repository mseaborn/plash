#!/bin/sh

# Copyright (C) 2004, 2005, 2006 Mark Seaborn
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


set -e

if test "x$GLIBC_SOURCE_DIR" = x || test "x$GLIBC_BUILD_DIR" = x; then
  echo "$0: GLIBC_SOURCE_DIR and GLIBC_BUILD_DIR must be defined"
  exit 1
fi

# This is a list of files needed for building Plash's version of ld.so.
# They are not built in the normal course of glibc's build process
# because they are not normally linked into ld.so.
PLASH_RTLD_EXTRA_OBJS="
        socket/rtld-recvmsg.os
        socket/rtld-sendmsg.os
        socket/rtld-send.os
        socket/rtld-cmsg_nxthdr.os
        io/rtld-fstat.os
        io/rtld-fxstat.os
        io/rtld-xstat64.os
        io/rtld-xstatconv.os
        io/rtld-dup.os
        stdlib/rtld-getenv.os
        string/rtld-strncmp.os
        string/rtld-strcpy.os"

for FILE in $PLASH_RTLD_EXTRA_OBJS; do
  make -C $GLIBC_SOURCE_DIR/`dirname $FILE` \
    objdir=$GLIBC_BUILD_DIR \
    -f Makefile -f ../elf/rtld-Rules rtld-all \
    rtld-modules=`basename $FILE`
done
