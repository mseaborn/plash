#!/bin/bash

set -e

function link_source
{
  plash_dir=$(pwd)
  (cd glibc-source &&
   ./link-source.sh $plash_dir
  ) || false
}

function configure_glibc
{
  mkdir -p glibc-build
  (cd glibc-source && autoconf) || false
  (cd glibc-build &&
   ../glibc-source/configure \
    --host=$(dpkg-architecture -qDEB_HOST_GNU_TYPE) \
    --build=$(dpkg-architecture -qDEB_HOST_GNU_TYPE) \
    CC="gcc-4.1 -fno-stack-protector" \
    --prefix=/usr \
    --without-selinux \
    --enable-kernel=2.2.0 \
    --with-tls \
    --disable-profile \
    CFLAGS="-pipe -fstrict-aliasing -g1 -O3"
  ) || false
}

function make_glibc
{
  PYTHONPATH=$(pwd)/python/lib \
  BUILT_WRAPPER=$(pwd)/python/scripts/simple_wrapper.py \
  PLASH_LDSO_PATH=/usr/bin/env \
  make -C glibc-build

  copy_ldso
  ./make.sh install_libs_for_testing
}

function copy_ldso
{
  cp -av glibc-build/elf/ld.so shobj/ld.so
}


for action in "$@"; do
  case $action in
    link_source|configure_glibc|make_glibc|copy_ldso)
      $action
      ;;
    *)
      echo Unknown action: $action
      exit 1
      ;;
  esac
done
