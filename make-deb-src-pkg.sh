#!/bin/sh

# Create Debian source package.
# Usage:  make-source-pkg.sh <debian-variant>
# <debian-variant> is the name of the "debian" directory to use.

set -e

if [ $# -ne 1 ]; then
  echo "Usage: $0 <debian-variant>"
  exit 1
fi

DEBIAN=$1

VERSION=1.16.1


mkdir source-pkg

DEST=source-pkg/plash-$VERSION
svn export . $DEST

(cd $DEST

echo -- autoconf
autoconf
rm -rv autom4te.cache

# Large files for web site not needed
rm -rv web-site/screenshot-*.png

# Remove the "debian" symlink
rm debian
mv $DEBIAN debian

# Remove the other debian branches
rm -r debian-*
)


# Now prepare for dpkg-source
echo -- prepare for dpkg-source
# Copy source package to .orig dir
cp -lr $DEST $DEST.orig
# The "debian" dir will be in the patch, not the "orig" tarball
rm -r $DEST.orig/debian
(cd source-pkg && dpkg-source -b plash-$VERSION plash-$VERSION.orig)
