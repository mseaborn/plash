#!/bin/bash

# This only works when $GLIBC is set to a glibc build dir,
# not for extracting from Debian's libc6-* packages.

set -e

OUT=shobj
PKG=glibc-i386-objs-2.3.5_1

. ./src/config.sh

mkdir -p $OUT/$PKG

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
  mkdir -p `dirname $OUT/$PKG/$F`
  echo strip $F
  strip --strip-debug $GLIBC/$F -o $OUT/$PKG/$F
done

for F in `cat $OUT/debian/plash-export-list-nonobj`; do
  mkdir -p `dirname $OUT/$PKG/$F`
  echo copy $F
  cp -p $GLIBC/$F $OUT/$PKG/$F
done

(cd $OUT && tar -cvzf $PKG.tar.gz $PKG)
