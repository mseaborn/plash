#!/bin/sh

# Copyright (C) 2004, 2005 Mark Seaborn
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


# This install program just covers the basics.
# It doesn't install man pages or other documentation.

usage () {
  echo "Usage: $0 [--nocheckroot] <dest-dir>"
  echo "To install normally, do: $0 /"
  echo "$0 must be run as root in order to install setuid root programs"
  exit 1
}

if [ "$1" = "--nocheckroot" ]; then
  shift
elif [ "`id -u`" != 0 ]; then
  usage
fi

if [ "$#" -ne 1 -o "$1" = "" ]; then
  usage
fi
DEST=$1


set -e

. src/config.sh

PLASH_EXECUTABLES="
	pola-run
	pola-shell
	plash-chroot
	plash-opts
	exec-object
	plash-socket-connect
	plash-socket-publish
	plash-run-emacs
	powerbox-req"
if "$USE_GTK" = yes; then
  PLASH_EXECUTABLES="$PLASH_EXECUTABLES plash-opts-gtk"
fi


# Rename the file into place so that it uses a new inode
# and doesn't interfere with running programs.
strip_install () {
  STRIP_ARGS="--remove-section=.comment --remove-section=.note"
  strip $STRIP_ARGS "$1" -o "$2.new"
  mv "$2.new" "$2"
}


# Install executables into /usr/bin
install -d $DEST/$PLASH_BIN_INSTALL
for F in $PLASH_EXECUTABLES; do
  strip_install bin/$F $DEST/$PLASH_BIN_INSTALL/$F
done


# Create chroot jail:  including "special" and "plash-uid-locks" dirs
install -d $DEST/$JAIL_DIR
install -d $DEST/$JAIL_INSTALL
install -d $DEST/$UID_LOCK_DIR
# Create empty lock file
> $DEST/$UID_LOCK_DIR/flock-file

# Set permissions
chmod go-rwx $DEST/$UID_LOCK_DIR


# Install executables into /usr/lib/plash
install -d $DEST/$PLASH_SETUID_BIN_INSTALL
strip_install setuid/run-as-anonymous \
	$DEST/$PLASH_SETUID_BIN_INSTALL/run-as-anonymous
strip_install setuid/gc-uid-locks \
	$DEST/$PLASH_SETUID_BIN_INSTALL/gc-uid-locks
strip_install setuid/run-as-anonymous_static \
	$DEST/$JAIL_DIR/run-as-anonymous

# Set permissions:  make them setuid
chmod +s $DEST/$PLASH_SETUID_BIN_INSTALL/run-as-anonymous
chmod +s $DEST/$PLASH_SETUID_BIN_INSTALL/gc-uid-locks
chmod +s $DEST/$JAIL_DIR/run-as-anonymous


# Install dynamic linker (ld.so) inside chroot jail
strip_install shobj/ld.so $DEST/$JAIL_INSTALL/ld-linux.so.2

# Set permissions:  make it executable
chmod +x $DEST/$JAIL_INSTALL/ld-linux.so.2


# Install glibc's lib*.so files into /usr/lib/plash/lib
install -d $DEST/$LIB_INSTALL
./src/install-libs.pl --dest-dir $DEST/$LIB_INSTALL

if [ "$USE_GTK" = yes ]; then
  strip_install shobj/powerbox-for-gtk.so $DEST/$LIB_INSTALL/powerbox-for-gtk.so
fi

# Install Emacs lisp file
install -d $DEST/$PLASH_EMACS_INSTALL
cp -v src/plash-gnuserv.el src/powerbox.el $DEST/$PLASH_EMACS_INSTALL/
