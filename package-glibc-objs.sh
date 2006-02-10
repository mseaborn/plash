#!/bin/bash

# This only works when $GLIBC is set to a glibc build dir,
# not for extracting from Debian's libc6-* packages.

set -e

OUT=shobj
DEST=out-release
PKG=glibc-i386-objs-2.3.5_2

. ./src/config.sh

mkdir -p $DEST/$PKG

LIBS="
  math/libm.so
  crypt/libcrypt.so
  dlfcn/libdl.so
  nis/libnsl.so
  nis/libnss_compat.so
  nis/libnss_nisplus.so
  nis/libnss_nis.so
  nss/libnss_files.so
  hesiod/libnss_hesiod.so
  resolv/libresolv.so
  resolv/libnss_dns.so
  resolv/libanl.so
  rt/librt.so
  login/libutil.so
  locale/libBrokenLocale.so
  malloc/libmemusage.so
  debug/libSegFault.so
  debug/libpcprofile.so"

for F in \
  `cat $OUT/debian/plash-export-list` \
  $LIBS \
  libc_pic.a \
  libc_nonshared.a; \
do
  mkdir -p `dirname $DEST/$PKG/$F`
  echo strip $F
  strip --strip-debug $GLIBC/$F -o $DEST/$PKG/$F
done

for F in `cat $OUT/debian/plash-export-list-nonobj`; do
  mkdir -p `dirname $DEST/$PKG/$F`
  echo copy $F
  cp -p $GLIBC/$F $DEST/$PKG/$F
done

cp -pv copyright-for-glibc-objs $DEST/$PKG/COPYRIGHT

(cd $DEST && tar -cvzf $PKG.tar.gz $PKG)
