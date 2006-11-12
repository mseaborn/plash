#!/bin/bash

set -e

if [ "$#" -lt 1 ]; then
  echo "Usage: $0 <glibc-version> [<action> ...]"
  exit 1
fi


GLIBC_VERSION=$1
shift

GLIBC_DIR=glibc-$GLIBC_VERSION
GLIBC_BUILD=glibc-$GLIBC_VERSION-objs
GLIBC_ARCHIVE=glibc-$GLIBC_VERSION.tar.bz2

# Only needed for glibc <2.4
LINUXTHREADS_ARCHIVE=glibc-linuxthreads-$GLIBC_VERSION.tar.bz2


unpack () {
  # Check that files are present
  FAIL=0
  if [ ! -e $GLIBC_ARCHIVE ]; then
    echo "$0: File $GLIBC_ARCHIVE not present"
    echo "$0: Try downloading ftp://ftp.gnu.org/gnu/glibc/$GLIBC_ARCHIVE"
    FAIL=1
  fi
  if [ $GLIBC_VERSION = 2.3.6 ] &&
     [ ! -e $LINUXTHREADS_ARCHIVE ]; then
    echo "$0: File $LINUXTHREADS_ARCHIVE not present"
    echo "$0: Try downloading ftp://ftp.gnu.org/gnu/glibc/$LINUXTHREADS_ARCHIVE"
    FAIL=1
  fi
  if [ $FAIL -ne 0 ]; then
    exit 1
  fi

  # Unpack
  mkdir -p tmp
  bzip2 -cd $GLIBC_ARCHIVE | tar -C tmp -xf -
  if [ $GLIBC_VERSION = 2.3.6 ]; then
    bzip2 -cd $LINUXTHREADS_ARCHIVE | tar -C tmp/$GLIBC_DIR -xf -
    # Apply patch
    patch -p1 -d tmp/$GLIBC_DIR <patches/glibc-2005-10-04.diff
  fi
  # Move into place
  mv -v tmp/$GLIBC_DIR .
}

configure () {
  mkdir -p $GLIBC_BUILD

  # Use --disable-profile to save disc space
  # -g1 saves a lot of space

  if [ $GLIBC_VERSION = 2.3.6 ]; then
    (cd $GLIBC_BUILD && \
     ../$GLIBC_DIR/configure \
	CC=gcc-3.3 \
	--prefix=/usr \
	--enable-add-ons=linuxthreads \
	--without-selinux --enable-kernel=2.2.0 \
	--with-tls --without-__thread \
	--disable-profile CFLAGS="-pipe -fstrict-aliasing -g1 -O3")
  else
    (cd $GLIBC_BUILD && \
     ../$GLIBC_DIR/configure \
	CC=gcc-4.0 \
	--prefix=/usr \
	--without-selinux --enable-kernel=2.2.0 \
	--with-tls \
	--disable-profile CFLAGS="-pipe -fstrict-aliasing -g1 -O3")
  fi
}

build () {
  (cd $GLIBC_BUILD && make)
}

build_extra () {
  ./make-glibc-extra.sh $GLIBC_DIR $GLIBC_BUILD_DIR
}


if [ $# -eq 0 ]; then
  unpack
  configure
  build
  build_extra
else
  for ACTION in $@; do
    case $ACTION in
      unpack|configure|build|build_extra)
        $ACTION
        ;;
      *)
        echo "$0: Unknown action: $ACTION"
	exit 1
	;;
    esac
  done
fi
