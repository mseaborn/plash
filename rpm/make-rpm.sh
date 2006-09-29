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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.


# This is a hack for building an RPM without:
#  1) copying stuff into /usr/src/rpm/SOURCES
#  2) having to build everything from scratch
# I think the "rpm" tool is really badly designed, but there you go.
# This script also illustrates what you can do with pola-run!

set -e

cd rpm

. ../src/config.sh

mkdir -vp plash-$PLASH_VERSION
(cd plash-$PLASH_VERSION && ln -s ~/projects/plash plash)
tar -cvzf plash-$PLASH_VERSION.tar.gz plash-$PLASH_VERSION
mkdir -vp build
mkdir -vp out
rm -rfv tmp
mkdir -vp tmp

# Needs to access /etc/password to check permissions
../bin/pola-run --prog /usr/bin/rpm -B -f /etc \
  -a=-bb \
  -tal /stuff/plash.spec ../plash.spec \
  -f ~/projects/plash \
  -fl ~/projects/plash/glibc \
  -f ~/projects-source/plash \
  -t  /usr/src/rpm/SOURCES/plash-$PLASH_VERSION.tar.gz plash-$PLASH_VERSION.tar.gz \
  -tw /usr/src/rpm/BUILD build \
  -tw /usr/src/rpm/RPMS/i386 out \
  -tw /var/tmp tmp

rpm2cpio out/plash-$PLASH_VERSION-1.i386.rpm | cpio -t > out/file-list
