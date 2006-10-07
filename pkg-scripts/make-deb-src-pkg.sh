#!/bin/bash

# Make a Debian source package for Debian sarge.
# This relies on finding the glibc source in a particular place.

set -e

DEST=plash-1.16.1

rm -rf $DEST
svn export . $DEST

(cd $DEST &&

  autoconf
  # Remove crap that autoconf leaves behind
  rm -rf autom4te.cache

  # Replace debian build scripts
  rm -r debian
  mv debian-sarge debian

  # Remove unnecessary large files/directories
  rm web-site/screenshot-*.png
  rm -r tests/glibc-tests

  # Copy glibc source
  cp -av ~/fetched/ftp.gnu.org/gnu/glibc/glibc{,-linuxthreads}-2.3.5.tar.bz2 .
)

dpkg-source -b $DEST ''
