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

gcc -Wall run-as-nobody.c -o run-as-nobody
gcc -Wall run-as-nobody+chroot.c -o run-as-nobody+chroot
gcc -Wall run-as-anonymous.c -o run-as-anonymous
gcc -Wall gc-uid-locks.c -o gc-uid-locks

setuid_root () {
  chown root:root $1
  chmod +s $1
}

setuid_root run-as-nobody
setuid_root run-as-nobody+chroot
setuid_root run-as-anonymous
setuid_root gc-uid-locks

# mkdir /usr/local/chroot-jail
# mkdir /usr/local/chroot-jail/special
# chown mrs: /usr/local/chroot-jail/special
# mkdir /usr/local/mrs
# chown mrs: /usr/local/mrs
