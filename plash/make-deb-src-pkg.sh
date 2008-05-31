#!/bin/sh

# Create Debian source package.
# Usage:  make-deb-src-pkg.sh [--reuse-orig] <version> <debian-variant>...
# <debian-variant> is the name of the "debian" directory to use.
#
# eg. To make packages for all variants:
# make-deb-src-pkg.sh 1.16.99 debian-*
#
# By creating packages for all variants at once, we only need to
# create the tarball once, and so we can ensure that we don't get
# slightly different tarballs each time (i.e. with different
# timestamps and file orderings).
# This saves space.
#
# The --reuse-orig option allows reusing an existing .orig tarball,
# which is taken from the current directory.

set -e

CREATE_ORIG=yes
if [ "$1" = "--reuse-orig" ]; then
  CREATE_ORIG=no
  shift
fi

if [ $# -lt 2 ]; then
  echo "Usage: $0 [--reuse-orig] <version> <debian-variant>..."
  exit 1
fi

VERSION=$1
shift


OUT=source-pkg

mkdir $OUT

DEST=$OUT/plash-$VERSION
svn export . $DEST.first-orig
./get-svn-revision.pl > $DEST.first-orig/svn-revision

if [ "$CREATE_ORIG" = yes ]; then
  # Create the .orig tarball without any debian directories in it
  echo "---- creating .orig tarball"
  cp -lr $DEST.first-orig $DEST.orig
  rm -r $DEST.orig/debian-*
  tar -czf $OUT/plash_$VERSION.orig.tar.gz -C $OUT plash-$VERSION.orig
else
  echo "---- reusing .orig tarball"
  cp -l plash_$VERSION.orig.tar.gz $OUT/
  # Unpack it for the benefit of "dpkg-source -ss" below
  tar -xf $OUT/plash_$VERSION.orig.tar.gz -C $OUT
fi

# For each variant of debian, create source package
for VARIANT in $@; do
  echo "---- making source package with \"$VARIANT\""
  mkdir $OUT/$VARIANT
  cp -lr $OUT/plash-$VERSION.orig $OUT/$VARIANT/
  cp -lr $OUT/plash_$VERSION.orig.tar.gz $OUT/$VARIANT/
  
  cp -lr $DEST.first-orig $OUT/$VARIANT/plash-$VERSION
  rm -r $OUT/$VARIANT/plash-$VERSION/debian-*
  cp -lr $DEST.first-orig/$VARIANT $OUT/$VARIANT/plash-$VERSION/debian
  # The -ss option tells dpkg-source that both the directory
  # and the tarfile are available.  It uses the directory for
  # the diff and the tar for the hash.
  (cd $OUT/$VARIANT && dpkg-source -b -ss plash-$VERSION) || false
  
  # Tidy up: unpacked trees not needed
  rm -r $OUT/$VARIANT/plash-$VERSION.orig
  rm -r $OUT/$VARIANT/plash-$VERSION
done

# Tidy up: remove unpacked trees
rm -r $OUT/plash-$VERSION.first-orig
rm -r $OUT/plash-$VERSION.orig
