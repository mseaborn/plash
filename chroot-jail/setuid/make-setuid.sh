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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
# USA.


if [ -z $CC ]; then CC=gcc; fi

set -e

# $CC -Wall run-as-nobody.c -o run-as-nobody
# $CC -Wall run-as-nobody+chroot.c -o run-as-nobody+chroot
# These need to #include "config.h"
$CC -Wall -I../gensrc run-as-anonymous.c -o run-as-anonymous
$CC -Wall -I../gensrc gc-uid-locks.c -o gc-uid-locks

if which diet >/dev/null; then
  # The -Wl,-z,norelro switch is a workaround for dietlibc/toolchain bugs
  # in Ubuntu intrepid.
  # See https://bugs.launchpad.net/ubuntu/+source/binutils/+bug/254790
  STATIC_CC="diet $CC -fno-stack-protector -Wl,-z,norelro"
else
  STATIC_CC="$CC"
  echo "dietlibc not found: statically linking with glibc instead"
  echo "This makes run-as-anonymous_static a lot bigger than it needs to be (400k instead of 18k)"
fi

$STATIC_CC -DIN_CHROOT_JAIL \
	-Wall -static -I../gensrc run-as-anonymous.c -o run-as-anonymous_static
