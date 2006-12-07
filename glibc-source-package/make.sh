#!/bin/sh

set -e

if [ $# -ne 1 ]; then
  echo "Usage: $0 <glibc-version>"
  exit 1
fi

VERSION=$1
BASE=glibc-source-$VERSION-$VERSION

mkdir $BASE
mkdir $BASE.orig

case $VERSION in
  2.3.6)
    cp -al glibc-$VERSION.tar.bz2 glibc-linuxthreads-$VERSION.tar.bz2 $BASE.orig
    ;;
  2.4)
    cp -al glibc-$VERSION.tar.bz2 $BASE.orig
    ;;
esac

mkdir $BASE/debian
# A trick for copying a directory while excluding ".svn":
tar -cf - -C debian-$VERSION . --exclude .svn | tar -xf - -C $BASE/debian

dpkg-source -b -i".*tar.bz2" -I.svn $BASE $BASE.orig

rm -rf $BASE
# dpkg-source deletes $BASE.orig itself


# Now unpack the source package and build it
dpkg-source -x glibc-source-${VERSION}_$VERSION-1.dsc
(cd $BASE && dpkg-buildpackage -rfakeroot -D)
# Again:
rm -rf $BASE
