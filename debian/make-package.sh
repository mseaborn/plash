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

DEST=debian/tmp

set -e

rm -rf debian/tmp

install -d $DEST/DEBIAN


# Install docs

install -d $DEST/usr/share/doc/$PACKAGE/html
install -d $DEST/usr/share/man/man1

cp -pv debian/copyright $DEST/usr/share/doc/$PACKAGE/
cp -pv debian/changelog $DEST/usr/share/doc/$PACKAGE/changelog
gzip -9 $DEST/usr/share/doc/$PACKAGE/changelog
cp -prv web-site/out/* $DEST/usr/share/doc/$PACKAGE/html/

# Install man pages
cp -pv docs/man/* $DEST/usr/share/man/man1/
gzip -9 $DEST/usr/share/man/man1/*.1
( cd $DEST/usr/share/man/man1 &&
  ln -s plash-opts.1.gz plash-opts-gtk.1.gz ) || false


./install.sh debian/tmp/

# Normally we'd put this in debian/control:
#   Depends: ${shlibs:Depends}
# and do:
#   dpkg-shlibdeps debian/tmp/usr/bin/*
# but this generates an unnecessary dep on libc6 2.3.5.
# Perhaps generate a "Recommends" field instead?

mkdir -p $DEST/usr/share/lintian/overrides
cp -av debian/lintian-overrides $DEST/usr/share/lintian/overrides/plash

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
