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


unpack () {
  if [ ! -e $GLIBC_ARCHIVE ]; then
    echo "$0: File $GLIBC_ARCHIVE not present"
    echo "$0: Try downloading ftp://ftp.gnu.org/gnu/glibc/$GLIBC_ARCHIVE"
    exit 1
  fi
  mkdir -p tmp
  bzip2 -cd $GLIBC_ARCHIVE | tar -C tmp -xf -
  mv -v tmp/$GLIBC_DIR .
}

configure () {
  mkdir -p $GLIBC_BUILD

  # The following have been removed for glibc 2.4:
  # --without-__thread
  # --enable-add-ons=linuxthreads
  # CC=gcc-3.3

  # Use --disable-profile to save disc space
  # -g1 saves a lot of space
  cd $GLIBC_BUILD && \
  ../$GLIBC_DIR/configure \
	CC=gcc-4.0 \
	--prefix=/usr \
	--without-selinux --enable-kernel=2.2.0 \
	--with-tls \
	--disable-profile CFLAGS="-pipe -fstrict-aliasing -g1 -O3"
}

build () {
  cd $GLIBC_BUILD && make
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
