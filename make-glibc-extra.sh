#!/bin/sh

GLIBC_SOURCE_DIR=...
GLIBC_BUILD_DIR=...

echo "Edit me"
exit 1

# This is a list of files needed for building Plash's version of ld.so.
# They are not built in the normal course of glibc's build process
# because they are not normally linked into ld.so.
PLASH_RTLD_EXTRA_OBJS = \
	socket/rtld-recvmsg.os \
        socket/rtld-sendmsg.os \
        socket/rtld-send.os \
        socket/rtld-cmsg_nxthdr.os \
        io/rtld-fstat.os \
	io/rtld-fxstat.os \
        io/rtld-xstat64.os \
	io/rtld-xstatconv.os \
        io/rtld-dup.os \
        stdlib/rtld-getenv.os \
        string/rtld-strncmp.os \
	string/rtld-strcpy.os
	
for FILE in $PLASH_RTLD_EXTRA_OBJS; do
  make -C $GLIBC_SOURCE_DIR/`dirname $FILE` \
    objdir=$GLIBC_BUILD_DIR \
    -f Makefile -f ../elf/rtld-Rules rtld-all \
    rtld-modules=`basename $FILE`
done
