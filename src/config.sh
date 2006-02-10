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


PLASH_VERSION=1.14

# Where to find partially-built glibc.
# Used when building.
# GLIBC_PIC_DIR is used to find libc_pic.a
# GLIBC_NONSHARED_DIR is used to find libc_nonshared.a
# GLIBC_SO_DIR is used to find various lib*.so files that are installed unmodified

GLIBC=./glibc-i386-objs-2.3.5_1
GLIBC_PIC_DIR=$GLIBC
GLIBC_NONSHARED_DIR=$GLIBC
GLIBC_SO_DIR=$GLIBC

# For Debian, require libc6-pic, libc6, and libc6-dev
#GLIBC=/usr/lib/libc_pic
#GLIBC_PIC_DIR=/usr/lib
#GLIBC_NONSHARED_DIR=/usr/lib
#GLIBC_SO_DIR=/lib

# Can build from the Debian glibc package's build tree:
# DEB_GLIBC=~mrs/sw/libc/new/glibc-2.3.5.orig/debian
# GLIBC=$DEB_GLIBC/libc6-pic/usr/lib/libc_pic
# GLIBC_PIC_DIR=$DEB_GLIBC/libc6-pic/usr/lib
# GLIBC_NONSHARED_DIR=$DEB_GLIBC/libc6-dev/usr/lib
# GLIBC_SO_DIR=$DEB_GLIBC/libc6/lib


CC=gcc-4.0

PLASH_EXECUTABLES="
	plash
	pola-run
	plash-chroot
	plash-opts
	plash-opts-gtk
	exec-object
	plash-socket-connect
	plash-socket-publish
	plash-run-emacs
	powerbox-req"

# Where to install to.
# These paths are also compiled into the executables.
JAIL_DIR=/var/lib/plash-chroot-jail
JAIL_INSTALL=$JAIL_DIR/special
PLASH_SETUID_BIN_INSTALL=/usr/lib/plash
LIB_INSTALL=/usr/lib/plash/lib

# Where to install executables.
PLASH_BIN_INSTALL=/usr/bin

# Where to install Emacs Lisp file.
PLASH_EMACS_INSTALL=/usr/share/emacs/site-lisp/plash/

# This needs to include any directories that were in
# /etc/ld.so.conf, which is usually used to build /etc/ld.so.cache,
# because the new dynamic linker isn't going to look at the cache.
PLASH_LD_LIBRARY_PATH=$LIB_INSTALL:/lib:/usr/lib:/usr/X11R6/lib:/usr/kerberos/lib:/usr/lib/qt-3.0.3/lib:/usr/lib/sane:/usr/lib/wine

# These are used by run-as-anonymous and gc-uid-locks.
UID_RANGE_START=0x100000
UID_RANGE_END=0x200000
UID_LOCK_DIR=$JAIL_DIR/plash-uid-locks
# Pathname to use from within the chroot jail
UID_LOCK_DIR2=/plash-uid-locks


# JAIL_DIR=/usr/local/chroot-jail
# BIN_INSTALL=/usr/local/plash
# LIB_INSTALL=/usr/local/plash/lib
