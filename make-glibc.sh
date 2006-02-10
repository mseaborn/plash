#!/bin/bash

# Copyright (C) 2004, 2005 Mark Seaborn
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

GLIBC_DIR=glibc-2.3.3
GLIBC_ARCHIVE=glibc-2.3.3.tar.bz2
LINUXTHREADS_ARCHIVE=glibc-linuxthreads-2.3.3.tar.gz


# unpack
# patch
# link in or copy `plash'

if [ -e glibc-source ]; then
  echo "glibc-source exists: assuming this is unpacked and patched glibc"
  echo
else
  echo "glibc-source doesn't exist: need to unpack and patch glibc"
  if [ ! -e $GLIBC_ARCHIVE ]; then
    echo "Error: $GLIBC_ARCHIVE is not present"
    EXIT=1
  fi
  if [ ! -e $LINUXTHREADS_ARCHIVE ]; then
    echo "Error: $LINUXTHREADS_ARCHIVE is not present"
    EXIT=1
  fi
  if [ -n "$EXIT" ]; then exit 1; fi

  echo Unpack:
  mkdir -p tmp
  echo Unpacking $GLIBC_ARCHIVE
  bzip2 -cd $GLIBC_ARCHIVE | tar -C tmp -xf -
  echo Unpacking $LINUXTHREADS_ARCHIVE
  gzip -cd $LINUXTHREADS_ARCHIVE | tar -C tmp/$GLIBC_DIR -xf -

  echo Patch:
  echo Apply diff
  patch -p1 -d tmp/$GLIBC_DIR <src/glibc-2004-12-06.diff
  echo Copy extra files
  # Using symlinks would make it easier to edit these files, but they
  # wouldn't be relative symlinks.
  cp -r glibc-extra/plash --target-directory=tmp/$GLIBC_DIR

  echo Done unpacking/patching: move into place
  mv -v tmp/$GLIBC_DIR glibc-source

  echo
fi

# configure

echo Configure glibc
mkdir -p glibc
(cd glibc &&
 ../glibc-source/configure --enable-add-ons=plash,linuxthreads \
	--prefix=/usr \
	--without-tls --without-__thread \
	CC=gcc-3.3 CPP=cpp-3.3)

# build

echo
echo Build glibc
(cd glibc && make)

# build some versions of files that aren't usually used by glibc

echo
echo Build some more parts of glibc

for FILE in socket/rtld-recvmsg.os \
	socket/rtld-sendmsg.os \
	socket/rtld-send.os \
	socket/cmsg_nxthdr.os \
	io/rtld-fstat.os io/rtld-fxstat.os \
	io/rtld-xstat64.os io/rtld-xstatconv.os \
	io/rtld-dup.os \
	stdlib/rtld-getenv.os \
	string/rtld-strncmp.os
do
  make -C glibc-source/`dirname $FILE` objdir=`pwd`/glibc \
	-f Makefile -f ../elf/rtld-Rules \
	rtld-all rtld-modules=`basename $FILE`
done
