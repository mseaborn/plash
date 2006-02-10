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


set -e

# These are installed outside of the glibc tree so that we can run a
# process as "nobody" but still let it run ld-linux and load libc.so.

. mrs/config.sh

if [ "$1" = "--root" ]; then
  PREFIX=$2
  MAIN_BIN_INSTALL=/usr/bin
  INST=inst_strip
  install -d $PREFIX$JAIL_INSTALL \
	     $PREFIX$BIN_INSTALL \
	     $PREFIX$LIB_INSTALL \
	     $PREFIX$MAIN_BIN_INSTALL
elif [ "$1" = "--local-link" ]; then
  PREFIX=''
  MAIN_BIN_INSTALL=''
  INST=inst_hard_link
elif [ "$1" = "--local-cp" ]; then
  PREFIX=''
  MAIN_BIN_INSTALL=''
  INST=inst_cp
else
  echo "Usage: install [ --root | --local ]"
  exit 1
fi

inst_cp () {
  echo Installing $PREFIX$2
  cp $1 $PREFIX$2
}

inst_hard_link () {
  echo Installing $PREFIX$2
  ln -f $1 $PREFIX$2
}

inst_strip () {
  echo Installing $1 '->' $PREFIX$2 >&2
  echo $2
  strip $1 -o $PREFIX$2
}

if [ -n "$MAIN_BIN_INSTALL" ]; then
  $INST mrs/shell $MAIN_BIN_INSTALL/plash
fi

$INST mrs/libc.so $LIB_INSTALL/libc.so.6
$INST mrs/libpthread.so $LIB_INSTALL/libpthread.so.0
$INST crypt/libcrypt.so $LIB_INSTALL/libcrypt.so.1
$INST dlfcn/libdl.so $LIB_INSTALL/libdl.so.2
$INST nis/libnsl.so $LIB_INSTALL/libnsl.so.1
$INST nis/libnss_compat.so $LIB_INSTALL/libnss_compat.so.2
$INST nis/libnss_nisplus.so $LIB_INSTALL/libnss_nisplus.so.2
$INST nis/libnss_nis.so $LIB_INSTALL/libnss_nis.so.2
$INST nss/libnss_files.so $LIB_INSTALL/libnss_files.so.2
$INST resolv/libresolv.so $LIB_INSTALL/libresolv.so.2
$INST resolv/libnss_dns.so $LIB_INSTALL/libnss_dns.so.2
$INST rt/librt.so $LIB_INSTALL/librt.so.1
$INST login/libutil.so $LIB_INSTALL/libutil.so.1

# You can use elf/ld.so for BIN_INSTALL; it's much less useful for JAIL_INSTALL
$INST mrs/ld.so $JAIL_INSTALL/ld-linux.so.2
chmod +x $PREFIX$JAIL_INSTALL/ld-linux.so.2
# $INST elf/ld.so $BIN_INSTALL/ld-linux.so.2
$INST mrs/run-as-nobody $BIN_INSTALL/run-as-nobody
$INST mrs/run-as-nobody+chroot $BIN_INSTALL/run-as-nobody+chroot
