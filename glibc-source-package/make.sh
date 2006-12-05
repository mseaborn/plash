#!/bin/sh

set -e

BASE=glibc-source-2.3.6-2.3.6

mkdir $BASE
mkdir $BASE.orig

cp -al glibc-2.3.6.tar.bz2 glibc-linuxthreads-2.3.6.tar.bz2 $BASE.orig

# A trick for copying a directory while excluding ".svn":
tar -cf - debian --exclude .svn | tar -xf - -C $BASE

dpkg-source -b -i".*tar.bz2" -I.svn $BASE $BASE.orig

rm -rf $BASE
# dpkg-source deletes $BASE.orig itself


# Now unpack the source package and build it
dpkg-source -x glibc-source-2.3.6_2.3.6-1.dsc
(cd $BASE && dpkg-buildpackage -rfakeroot -D)
# Again:
rm -rf $BASE
