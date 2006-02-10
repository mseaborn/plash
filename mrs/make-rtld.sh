#!/bin/sh

make -C ../glibc-2.3.3/socket objdir=`pwd` -f Makefile -f ../elf/rtld-Rules \
	rtld-all rtld-modules='rtld-recvmsg.os rtld-sendmsg.os rtld-send.os'
make -C ../glibc-2.3.3/stdlib objdir=`pwd` -f Makefile -f ../elf/rtld-Rules \
	rtld-all rtld-modules='rtld-getenv.os'
make -C ../glibc-2.3.3/io objdir=`pwd` -f Makefile -f ../elf/rtld-Rules \
	rtld-all rtld-modules='rtld-fxstat.os rtld-fstat.os'
make -C ../glibc-2.3.3/string objdir=`pwd` -f Makefile -f ../elf/rtld-Rules \
	rtld-all rtld-modules='rtld-strncmp.os'
