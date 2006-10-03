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


PACKAGE=plash
LIBDIR=debian/plash/usr/lib/plash/lib
CHROOT_JAIL=debian/plash/var/lib/plash-chroot-jail

DEST=debian/plash

set -e

. ./src/config.sh

rm -rf debian/plash
rm -f debian/substvars
# Need to delete these otherwise they accumulate auto-generated
# code from python-support each time this is run.
rm -f debian/plash.postinst.debhelper
rm -f debian/plash.prerm.debhelper

install -d $DEST/DEBIAN


# Install docs

install -d $DEST/usr/share/doc/$PACKAGE/html
install -d $DEST/usr/share/man/man1

cp -pv debian/copyright $DEST/usr/share/doc/$PACKAGE/
cp -pv debian/changelog $DEST/usr/share/doc/$PACKAGE/changelog

# Behave nicely if the documentation has not been built.
for FILE in web-site/out/*; do if [ -e $FILE ]; then
  cp -prv $FILE $DEST/usr/share/doc/$PACKAGE/html/
fi; done

# Install man pages
for FILE in docs/man/*; do if [ -e $FILE ]; then
  cp -pv $FILE $DEST/usr/share/man/man1/
fi; done
( cd $DEST/usr/share/man/man1 &&
  ln -s plash-opts.1 plash-opts-gtk.1 ) || false


./install.sh debian/plash/

if "$USE_PYTHON" = yes; then
  DEST_FULL=`pwd`/$DEST
  (cd python;
   for PYVERSION in `pyversions -vs`; do
     # "--no-compile" stops it from byte-compiling code.
     python$PYVERSION setup.py install --no-compile --root=$DEST_FULL
   done
  )
  dh_pysupport
fi

mkdir -p $DEST/usr/share/lintian/overrides
cp -av debian/lintian-overrides $DEST/usr/share/lintian/overrides/plash

dh_shlibdeps
dh_compress
dh_gencontrol

chown -R root.root debian/plash
chmod -R g-ws debian/plash
chmod go-rwx $CHROOT_JAIL/plash-uid-locks
chmod +x $CHROOT_JAIL/special/ld-linux.so.2
chmod +s debian/plash/usr/lib/plash/run-as-anonymous
chmod +s debian/plash/usr/lib/plash/gc-uid-locks
chmod +s $CHROOT_JAIL/run-as-anonymous

dh_installdeb
dh_md5sums
dh_builddeb
