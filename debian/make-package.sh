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
CHROOT_JAIL=debian/tmp/usr/lib/plash-chroot-jail

set -e

rm -rf debian/tmp
install -d debian/tmp/DEBIAN \
	-d debian/tmp/usr/share/doc/$PACKAGE \
	-d debian/tmp/usr/share/man/man1 \
	-d $CHROOT_JAIL/special \
	-d $CHROOT_JAIL/plash-uid-locks \
	-d $LIBDIR \
	-d debian/tmp/usr/bin

cp -pv debian/copyright debian/tmp/usr/share/doc/$PACKAGE/
cp -pv debian/changelog debian/tmp/usr/share/doc/$PACKAGE/changelog
gzip -9 debian/tmp/usr/share/doc/$PACKAGE/changelog
cp -pv README NOTES NOTES.exec BUGS protocols.txt \
	debian/tmp/usr/share/doc/$PACKAGE/
# Install man pages
cp -pv docs/plash.1 \
	docs/exec-object.1 \
	docs/plash-opts.1 \
	docs/plash-chroot.1 \
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
strip $STRIP_ARGS shobj/ld.so -o $CHROOT_JAIL/special/ld-linux.so.2
./src/install.pl --dest-dir $LIBDIR/

# Install executables
strip $STRIP_ARGS bin/plash          -o debian/tmp/usr/bin/plash
strip $STRIP_ARGS bin/plash-chroot   -o debian/tmp/usr/bin/plash-chroot
strip $STRIP_ARGS bin/plash-opts     -o debian/tmp/usr/bin/plash-opts
strip $STRIP_ARGS bin/plash-opts-gtk -o debian/tmp/usr/bin/plash-opts-gtk
strip $STRIP_ARGS bin/exec-object    -o debian/tmp/usr/bin/exec-object
dpkg-shlibdeps bin/plash bin/plash-chroot bin/plash-opts bin/plash-opts-gtk bin/exec-object

dpkg-gencontrol -isp

chown -R root.root debian/tmp
chmod -R g-ws debian/tmp
chmod go-rwx $CHROOT_JAIL/plash-uid-locks
chmod +x $CHROOT_JAIL/special/ld-linux.so.2
# chmod +s debian/tmp/usr/lib/plash/run-as-nobody
# chmod +s debian/tmp/usr/lib/plash/run-as-nobody+chroot
chmod +s debian/tmp/usr/lib/plash/run-as-anonymous
chmod +s debian/tmp/usr/lib/plash/gc-uid-locks

dpkg --build debian/tmp ..
