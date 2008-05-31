#!/bin/bash

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


function install_html_docs
{
  # Behave nicely if the documentation has not been built.
  for FILE in web-site/out/*; do
    if [ -e $FILE ]; then
      install -d $DEST/usr/share/doc/$PACKAGE/html
      cp -prv $FILE $DEST/usr/share/doc/$PACKAGE/html/
    fi
  done
}

function install_man_pages
{
  for FILE in docs/man/*; do
    if [ -e $FILE ]; then
      install -d $DEST/usr/share/man/man1
      cp -pv $FILE $DEST/usr/share/man/man1/
    fi
  done
  #( cd $DEST/usr/share/man/man1 &&
  #  ln -s plash-opts.1 plash-opts-gtk.1 ) || false
}

function install_examples
{
  mkdir -p $DEST/usr/share/doc/$PACKAGE/examples
  cp -pv packaging/examples/*.pkg $DEST/usr/share/doc/$PACKAGE/examples/
  cp -pv packaging/examples/sources.list $DEST/usr/share/doc/$PACKAGE/examples/
}

function install_python_modules
{
  # Delete build dir in case it contains old *.py modules
  rm -rf python/build

  DEST_FULL=`pwd`/$DEST
  (cd python;
   for PYVERSION in `pyversions -vs`; do
     # Force it to rebuild C module.
     python$PYVERSION setup.py build --force
     # "--no-compile" stops it from byte-compiling code.
     python$PYVERSION setup.py install --no-compile --root=$DEST_FULL
   done
  )
}

function install_python_script
{
  SCRIPT=$1
  cp -av python/scripts/$SCRIPT $DEST/usr/bin/$SCRIPT
}


install_html_docs
install_man_pages
install_examples

./install.sh debian/plash/

if [ "$USE_PYTHON" = yes ]; then
  install_python_modules
  install_python_script pola-run
  install_python_script plash-pkg-update-avail
  install_python_script plash-pkg-choose
  install_python_script plash-pkg-fetch
  install_python_script plash-pkg-unpack
  install_python_script plash-pkg-install
  install_python_script plash-pkg-launch
  install_python_script plash-pkg-deb-inst
  dh_pysupport
fi

mkdir -p $DEST/usr/share/lintian/overrides
cp -av debian/lintian-overrides $DEST/usr/share/lintian/overrides/plash

dh_shlibdeps
dh_installdocs
dh_installchangelogs
dh_compress
dh_strip
dh_gencontrol

chown -R root.root debian/plash
chmod -R g-ws debian/plash

dh_installdeb
dh_md5sums
dh_builddeb
