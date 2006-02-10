#!/bin/sh

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


PACKAGE=plash
LIBDIR=debian/tmp/usr/lib/plash/lib
CHROOT_JAIL=debian/tmp/var/lib/plash-chroot-jail

set -e

rm -rf debian/tmp
install -d debian/tmp/DEBIAN \
	-d debian/tmp/usr/share/doc/$PACKAGE/html \
	-d debian/tmp/usr/share/man/man1 \
	-d $CHROOT_JAIL/special \
	-d $CHROOT_JAIL/plash-uid-locks \
	-d $LIBDIR \
	-d debian/tmp/usr/bin \
	-d debian/tmp/usr/share/emacs/site-lisp/plash/

> $CHROOT_JAIL/plash-uid-locks/flock-file

cp -pv debian/copyright debian/tmp/usr/share/doc/$PACKAGE/
cp -pv debian/changelog debian/tmp/usr/share/doc/$PACKAGE/changelog
gzip -9 debian/tmp/usr/share/doc/$PACKAGE/changelog
cp -pv README NOTES NOTES.exec BUGS protocols.txt \
	debian/tmp/usr/share/doc/$PACKAGE/
cp -pv docs/out-html/* debian/tmp/usr/share/doc/$PACKAGE/html/
# Install man pages
cp -pv  docs/out-man/plash.1 \
	docs/out-man/exec-object.1 \
	docs/out-man/plash-opts.1 \
	docs/out-man/plash-chroot.1 \
	docs/out-man/plash-run-emacs.1 \
	docs/out-man/pola-run.1 \
	docs/out-man/plash-socket-connect.1 \
	docs/out-man/plash-socket-publish.1 \
	debian/tmp/usr/share/man/man1/
gzip -9 debian/tmp/usr/share/man/man1/*.1
( cd debian/tmp/usr/share/man/man1 &&
  ln -s plash-opts.1.gz plash-opts-gtk.1.gz )
# Could use "cp -a A B" instead of "strip A -o B"

STRIP_ARGS="--remove-section=.comment --remove-section=.note"

# strip $STRIP_ARGS setuid/run-as-nobody -o debian/tmp/usr/lib/plash/run-as-nobody
# strip $STRIP_ARGS setuid/run-as-nobody+chroot -o debian/tmp/usr/lib/plash/run-as-nobody+chroot
strip $STRIP_ARGS setuid/run-as-anonymous -o debian/tmp/usr/lib/plash/run-as-anonymous
strip $STRIP_ARGS setuid/gc-uid-locks -o debian/tmp/usr/lib/plash/gc-uid-locks
strip $STRIP_ARGS setuid/run-as-anonymous_static -o $CHROOT_JAIL/run-as-anonymous
strip $STRIP_ARGS shobj/ld.so -o $CHROOT_JAIL/special/ld-linux.so.2
./src/install.pl --dest-dir $LIBDIR/

# Install executables
./pkg-install.sh debian/tmp
dpkg-shlibdeps debian/tmp/usr/bin/*

# Install Emacs Lisp file
cp -pv src/plash-gnuserv.el debian/tmp/usr/share/emacs/site-lisp/plash/

dpkg-gencontrol -isp

chown -R root.root debian/tmp
chmod -R g-ws debian/tmp
chmod go-rwx $CHROOT_JAIL/plash-uid-locks
chmod +x $CHROOT_JAIL/special/ld-linux.so.2
# chmod +s debian/tmp/usr/lib/plash/run-as-nobody
# chmod +s debian/tmp/usr/lib/plash/run-as-nobody+chroot
chmod +s debian/tmp/usr/lib/plash/run-as-anonymous
chmod +s debian/tmp/usr/lib/plash/gc-uid-locks
chmod +s $CHROOT_JAIL/run-as-anonymous

dpkg --build debian/tmp ..
