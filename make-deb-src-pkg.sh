#!/bin/sh

# Create Debian source package.
# Usage:  make-deb-src-pkg.sh <version> <debian-variant>...
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

set -e

if [ $# -lt 2 ]; then
  echo "Usage: $0 <version> <debian-variant>..."
  exit 1
fi

VERSION=$1
shift


OUT=source-pkg

mkdir $OUT

DEST=$OUT/plash-$VERSION
svn export . $DEST.first-orig

(cd $DEST.first-orig

echo -- autoconf
autoconf
rm -rv autom4te.cache

# Large files for web site not needed
rm -rv web-site/screenshot-*.png
)

# Create the .orig tarball without any debian directories in it
echo "---- creating .orig tarball"
cp -lr $DEST.first-orig $DEST.orig
rm $DEST.orig/debian
rm -r $DEST.orig/debian-*
tar -czf $OUT/plash_$VERSION.orig.tar.gz -C $OUT plash-$VERSION.orig

# For each variant of debian, create source package
for VARIANT in $@; do
  echo "---- making source package with \"$VARIANT\""
  mkdir $OUT/$VARIANT
  cp -lr $OUT/plash-$VERSION.orig $OUT/$VARIANT/
  cp -lr $OUT/plash_$VERSION.orig.tar.gz $OUT/$VARIANT/
  
  cp -lr $DEST.orig $OUT/$VARIANT/plash-$VERSION
  cp -lr $DEST.first-orig/$VARIANT $OUT/$VARIANT/plash-$VERSION/debian
  # The -ss option tells dpkg-source that both the directory
  # and the tarfile are available.  It uses the directory for
  # the diff and the tar for the hash.
  (cd $OUT/$VARIANT && dpkg-source -b -ss plash-$VERSION)
  
  # Tidy up: unpacked trees not needed
  rm -r $OUT/$VARIANT/plash-$VERSION.orig
  rm -r $OUT/$VARIANT/plash-$VERSION
done

# Tidy up: remove unpacked trees
rm -r $OUT/plash-$VERSION.first-orig
rm -r $OUT/plash-$VERSION.orig
