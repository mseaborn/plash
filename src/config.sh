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


PLASH_VERSION=1.9

JAIL_DIR=/var/lib/plash-chroot-jail
JAIL_INSTALL=$JAIL_DIR/special
BIN_INSTALL=/usr/lib/plash
LIB_INSTALL=/usr/lib/plash/lib

UID_RANGE_START=0x100000
UID_RANGE_END=0x200000
UID_LOCK_DIR=$JAIL_DIR/plash-uid-locks

# This needs to include any directories that were in
# /etc/ld.so.conf, which is usually used to build /etc/ld.so.cache,
# because the new dynamic linker isn't going to look at the cache.
PLASH_LD_LIBRARY_PATH=$LIB_INSTALL:/lib:/usr/lib:/usr/X11R6/lib:/usr/kerberos/lib:/usr/lib/qt-3.0.3/lib:/usr/lib/sane:/usr/lib/wine


# JAIL_DIR=/usr/local/chroot-jail
# BIN_INSTALL=/usr/local/plash
# LIB_INSTALL=/usr/local/plash/lib
