#!/bin/bash

# Copyright (C) 2004 Mark Seaborn
#
# This file is part of Plash, the Principle of Least Authority Shell.
#
# Plash is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of
# the License, or (at your option) any later version.
#
# Plash is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with Plash; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.


GLIBC_VERSION=2.3.3
#CC=gcc-3.3

if [ -z $CC ]; then CC=gcc; fi
if [ -z $BUILD_SERVER ]; then BUILD_SERVER=yes; fi
if [ -z $BUILD_LIBC ]; then BUILD_LIBC=no; fi
if [ -z $BUILD_LDSO ]; then BUILD_LDSO=no; fi
if [ "$1" = "-L" ]; then BUILD_LIBC=yes; fi

set -e

DIET=../dietlibc-0.27-build
# _REENTRANT is needed for (potentially) multi-threaded code
# removed: -include $DIET/include/dietref.h -nostdinc -I $DIET/include
OPTS_C="-Wall -nostdlib \
        -g \
	-D_REENTRANT \
	-fPIC"
OPTS_S="-Wall -g"


if [ $BUILD_SERVER = yes ]; then
  # Server: compiled with glibc
  # Doesn't have to be position-independent (though that doesn't hurt)
  # I used to use dietlibc for this, but then I wanted to link with readline
  $CC $OPTS_S '-DHALF_NAME="server"' -c mrs/comms.c -o mrs/comms.o
  $CC $OPTS_S -c mrs/region.c -o mrs/region.o
  $CC $OPTS_S -c mrs/utils.c -o mrs/utils.o
  $CC $OPTS_S -c mrs/driver.c -o mrs/driver.o
  $CC $OPTS_S -c mrs/parse-filename.c -o mrs/parse-filename.o
  $CC $OPTS_S -c mrs/filesysobj.c -o mrs/filesysobj.o
  $CC $OPTS_S -c mrs/filesysobj-fab.c -o mrs/filesysobj-fab.o
  $CC $OPTS_S -c mrs/filesysslot.c -o mrs/filesysslot.o
  $CC $OPTS_S -c mrs/build-fs.c -o mrs/build-fs.o
  $CC $OPTS_S -c mrs/shell.c -o mrs/shell.o
  $CC $OPTS_S -c mrs/shell-parse.c -o mrs/shell-parse.o
  $CC $OPTS_S -c mrs/shell-variants.c -o mrs/shell-variants.o
  $CC $OPTS_S -c mrs/server.c -o mrs/server.o
  # To link server with dietlibc, you need to do:
  # $CC $OPTS $DIET/bin-i386/start.o ...files... \
  #     $DIET/bin-i386/dietlibc.a -lgcc -o mrs/driver
  $CC $OPTS_S  \
	mrs/build-fs.o mrs/shell.o mrs/shell-parse.o mrs/shell-variants.o \
	mrs/server.o mrs/filesysslot.o mrs/filesysobj-fab.o mrs/filesysobj.o \
	mrs/parse-filename.o mrs/comms.o mrs/region.o mrs/utils.o \
	-lreadline -ltermcap -o mrs/shell
fi


if [ $BUILD_LIBC = yes -o $BUILD_LDSO = yes ]; then

  # Client: compiled with dietlibc, position-independent
  $CC $OPTS_C '-DHALF_NAME="client"' -c mrs/comms.c -o mrs/comms.os
  $CC $OPTS_C '-DHALF_NAME="client"' -DIN_RTLD -c mrs/comms.c -o mrs/rtld-comms.os
  $CC $OPTS_C -c mrs/region.c -o mrs/region.os
  $CC $OPTS_C -c mrs/open.c -o mrs/open.os
  $CC $OPTS_C -c mrs/libc-comms.c -o mrs/libc-comms.os
  $CC $OPTS_C -c mrs/libc-fork-exec.c -o mrs/libc-fork-exec.os
  $CC $OPTS_C -c mrs/libc-connect.c -o mrs/libc-connect.os

  echo
  echo Linking combined.os
  # NB. ordering of object files is important
  # --retain-symbols-file my-exports
  # The "-r" option is the same as "--relocateable", but in newer
  # versions this was renamed to "--relocatable".
  # Don't need these, they're left in glibc:
  #   io/close.os io/dup.os io/dup2.os
  #   socket/recvmsg.os socket/sendmsg.os socket/send.os
  # May need io/fstat.oS (part of libc_nonshared.a), but if using
  # correct headers, will get an inline version instead that refers
  # to io/fxstat.os.  io/xstat64.os is needed to provide __have_no_stat64.
  ld -r mrs/open.os mrs/libc-fork-exec.os mrs/libc-connect.os mrs/libc-comms.os mrs/comms.os mrs/region.os \
	posix/fork.os posix/execve.os socket/connect.os \
	io/fstat.oS io/fxstat.os io/xstat64.os io/xstatconv.os \
	-o mrs/combined.os
  #ld -r mrs/combined1.os $DIET/bin-i386/dietlibc.a -o mrs/combined.os
  EXTRA="mrs/combined.os mrs/sysdeps/not-cancel.os"

  ld -r mrs/open.os mrs/libc-comms.os mrs/rtld-comms.os mrs/region.os \
	socket/rtld-recvmsg.os socket/rtld-sendmsg.os socket/rtld-send.os socket/cmsg_nxthdr.os \
	io/rtld-fstat.os io/rtld-fxstat.os io/rtld-xstat64.os io/rtld-xstatconv.os \
	-o mrs/rtld-combined.os
  EXTRA_RTLD="mrs/rtld-combined.os stdlib/rtld-getenv.os string/rtld-strncmp.os"

  # NB. We didn't have to exclude the symbols defined by fxstat{,64}.os.
  # However, unfortunately these files depend on __have_no_stat64, which
  # is defined by xstat64.os.  Rather than mess around with linking to
  # xstat64.os but hiding its other symbols, I have just reimplemented
  # fstat{,64}.
  EXCLUDE="io/open.os io/getcwd.os io/chdir.os io/fchdir.os
	io/xmknod.os
	io/xstat.os io/xstat64.os
	io/lxstat.os io/lxstat64.os
	io/fxstat.os io/fxstat64.os io/xstatconv.os
	io/readlink.os
	io/access.os io/chmod.os io/lchmod.os io/chown.os io/lchown.os
	io/link.os io/symlink.os
	io/mkdir.os io/mkfifo.os
	io/unlink.os io/rmdir.os
	io/statfs.os
	io/utime.os
	misc/utimes.os misc/lutimes.os
	misc/truncate.os
	socket/connect.os
	posix/fork.os posix/vfork.os posix/execve.os
	dirent/opendir.os dirent/closedir.os
	dirent/readdir.os dirent/readdir64.os
	dirent/readdir_r.os dirent/readdir64_r.os
	dirent/getdents.os dirent/getdents64.os
	dirent/rewinddir.os dirent/seekdir.os dirent/telldir.os
	misc/getxattr.os misc/setxattr.os
	misc/lgetxattr.os misc/lsetxattr.os
	misc/listxattr.os misc/removexattr.os
	misc/llistxattr.os misc/lremovexattr.os"
  # privileged things that operate on filenames:
  # misc/mount.os misc/umount.os misc/chroot.os misc/pivot_root.os

  echo
  echo objcopy
  # With -G, all other symbols are converted to be local
  # With -K, all other symbols are removed
  # We need two passes here.  Firstly we need to hide all the symbols that
  # dietlibc defines, such as "fork".  Then we need to rename our symbols,
  # such as "new_fork" to "fork".
  # If you try to do this in one pass with "objcopy", it renames "new_fork"
  # to "fork" and *then* keeps "fork", which leaves us with *two* definitions
  # of "fork", our one and dietlibc's.
  sh mrs/out-link_main.sh
  sh mrs/out-link_rtld.sh
   #objcopy -G open -G __libc_open -G __open \
   #	-G chdir -G __chdir -G fchdir -G __fchdir \
   #	-G getcwd -G __getcwd \
   #	-G __xstat \
   #	-G __lxstat \
   #	-G __fxstat \
   #	-G __xstat64@@GLIBC_2.2 \
   #	-G __lxstat64@@GLIBC_2.2 \
   #	-G __fxstat64@@GLIBC_2.2 \
   #	-G readlink -G __readlink \
   #	-G new_access -G __access \
   #	-G new_chmod -G __chmod \
   #	-G new_chown \
   #	-G link -G __link \
   #	-G new_mkdir -G __mkdir \
   #	-G new_mkfifo \
   #	-G rmdir -G __rmdir \
   #	-G new_statfs -G __statfs \
   #	-G symlink -G __symlink \
   #	-G unlink -G __unlink \
   #	-G new_utime -G __utime \
   #	-G __xmknod -G _xmknod \
   #	-G new_connect -G __connect -G new_libc_connect \
   #	-G opendir -G __opendir -G closedir -G __closedir \
   #	-G readdir -G __readdir -G readdir64@@GLIBC_2.2 -G __readdir64 \
   #	-G rewinddir \
   #	-G telldir -G seekdir \
   #	-G __getdents \
   #	-G new_fork -G new_libc_fork -G __fork -G vfork -G __vfork \
   #	-G new_execve -G __execve \
   #	-G errno --redefine-sym glibc_getenv=getenv \
   #	mrs/combined.os
   #objcopy --redefine-sym new_fork=fork \
   #	--redefine-sym new_libc_fork=__libc_fork \
   #	--redefine-sym new_execve=execve \
   #	--redefine-sym new_access=access \
   #	--redefine-sym new_chmod=chmod \
   #	--redefine-sym new_chown=chown \
   #	--redefine-sym new_mkdir=mkdir \
   #	--redefine-sym new_mkfifo=mkfifo \
   #	--redefine-sym new_statfs=statfs \
   #	--redefine-sym new_utime=utime \
   #	--redefine-sym new_connect=connect \
   #	--redefine-sym new_libc_connect=__libc_connect \
   #	mrs/combined.os

  if false; then
    echo
    echo The following symbols are resolved from outside:
    nm mrs/combined.os | grep " U " | ./mrs/process-nm-output.sh
    echo The following have been added to glibc:
    nm mrs/combined.os | grep " [TW] " | ./mrs/process-nm-output.sh
    echo The following have been removed from glibc:
    nm $EXCLUDE | grep " [TW] " | ./mrs/process-nm-output.sh
    echo
  fi
  echo
  ./mrs/symbol-info.pl $EXCLUDE
  echo
fi

if [ $BUILD_LDSO = yes ]; then
 # Build ld-linux.so (a.k.a. ld.so)
 case $GLIBC_VERSION in
  2.2.5)
    echo Making ld.so
    # The glibc build process creates libc_pic.a.  This is an unnecessary
    # intermediate step for building libc.so, because you can just link
    # the files in the archive directly to produce libc.so.
    # But for building ld.so, we only want to selectively include parts of
    # libc, so using an archive seems to be unavoidable.
    rm -f mrs/libc_pic.a
    ar cruv mrs/libc_pic.a `./mrs/files-to-link.pl $EXCLUDE` mrs/combined-for-ldso.os

    echo Linking ld.so
    $CC -nostdlib -nostartfiles -r -o mrs/librtld.os \
	'-Wl,-(' elf/dl-allobjs.os mrs/libc_pic.a -lgcc '-Wl,-)'
    $CC -nostdlib -nostartfiles -shared -o mrs/ld.so \
	-Wl,-z,combreloc \
	mrs/librtld.os \
	-Wl,--version-script=ld.map -Wl,-soname=ld-linux.so.2

    # Needed when using dietlibc, because it's not built as PIC:
    ./mrs/fixup-ldso.pl mrs/ld.so
    ;;
  2.3.3)
    echo Making ld.so
    rm -f mrs/rtld-libc.a
    ar cruv mrs/rtld-libc.a `./mrs/files-to-link.pl --rtld $EXCLUDE`

    echo Generating linker script
    $CC -nostdlib -nostartfiles -shared \
  	-Wl,-z,combreloc -Wl,-z,defs -Wl,--verbose 2>&1 | \
	  LC_ALL=C \
	  sed -e '/^=========/,/^=========/!d;/^=========/d'	\
	      -e 's/\. = 0 + SIZEOF_HEADERS;/& _begin = . - SIZEOF_HEADERS;/' \
	  > mrs/ld.so.lds

    echo Linking ld.so
    $CC -nostdlib -nostartfiles -shared -o mrs/ld.so \
	-Wl,-z,combreloc -Wl,-z,defs \
	'-Wl,-(' elf/dl-allobjs.os mrs/rtld-libc.a $EXTRA_RTLD -lgcc '-Wl,-)' \
	-Wl,--version-script=ld.map -Wl,-soname=ld-linux.so.2 -T mrs/ld.so.lds
    ;;
  *) echo Unknown version ;;
 esac
fi

if [ $BUILD_LIBC = yes ]; then
# Differences in 2.3.3:
# New arguments used: -static-libgcc -Wl,-z,defs -lgcc_eh
# The name of the file that the linker script was output to
# changed from libc.so.lds to shlib.lds (but we can use any
# name).  Fewer arguments were used when generating the
# linker script -- I suspect that virtually none of the arguments
# are needed, because we're just getting `gcc' to print out the
# script file that it gives to `ld'.
case $GLIBC_VERSION in
  2.2.5)
    # This generates a linker script, mrs/libc.so.lds.
    # The --verbose option gets ld to print various info which includes
    # the internal linker script.  The sed script extracts that.
    echo Making script
    $CC -shared -Wl,-O1 \
	-Wl,-dynamic-linker=/usr/local/lib/ld-linux.so.2 \
	-Bsysdeps/../csu/ \
	-Wl,--version-script=sysdeps/../libc.map \
	-Wl,-soname=libc.so.6 \
	-Wl,-z,combreloc \
	-nostdlib -nostartfiles -e __libc_main -u __register_frame -L. \
	-Lmath -Lelf -Ldlfcn -Lnss -Lnis -Lrt -Lresolv -Lcrypt -Llinuxthreads \
	-Wl,-rpath-link=.:math:elf:dlfcn:nss:nis:rt:resolv:crypt:linuxthreads \
	-o libc.so.new \
	csu/abi-note.o -Wl,--verbose -lgcc 2>&1 | sed -e '/^=========/,/^=========/!d;/^=========/d' -e 's/^.*\.hash[ 	]*:.*$/  .note.ABI-tag : { *(.note.ABI-tag) } &/' > mrs/libc.so.lds
    echo Linking
    # args not used (says gcc): -Bsysdeps/../csu/
    $CC -shared -Wl,-O1 \
	-Wl,-dynamic-linker=/usr/local/lib/ld-linux.so.2 \
	-Wl,--version-script=libc.map \
	-Wl,-soname=libc.so.6 \
	-Wl,-z,combreloc \
	-nostdlib -nostartfiles -e __libc_main -u __register_frame -L. \
	-Lmath -Lelf -Ldlfcn -Lnss -Lnis -Lrt -Lresolv -Lcrypt -Llinuxthreads \
	-Wl,-rpath-link=.:math:elf:dlfcn:nss:nis:rt:resolv:crypt:linuxthreads \
	-o mrs/libc.so \
	-T mrs/libc.so.lds csu/abi-note.o elf/soinit.os $EXTRA `./mrs/files-to-link.pl $EXCLUDE` elf/sofini.os elf/interp.os elf/ld.so -lgcc
    ;;
  2.3.3)
    echo Making script
    $CC -shared -Wl,-O1 -nostdlib -nostartfiles \
	-Wl,-dynamic-linker=/usr/local/lib/ld-linux.so.2 \
	-Wl,-z,combreloc \
	-Wl,--verbose 2>&1 | \
      sed > mrs/libc.so.lds \
        -e '/^=========/,/^=========/!d;/^=========/d' \
        -e 's/^.*\.hash[ 	]*:.*$/  .note.ABI-tag : { *(.note.ABI-tag) } &/' \
        -e 's/^.*\*(\.dynbss).*$/& \
	 PROVIDE(__start___libc_freeres_ptrs = .); \
	 *(__libc_freeres_ptrs) \
	 PROVIDE(__stop___libc_freeres_ptrs = .);/'
    echo Linking
    echo NB. removed -Wl,-O1
    $CC -Wl,--stats,--no-keep-memory \
	-shared -static-libgcc -Wl,-z,defs \
	-Wl,-dynamic-linker=/usr/local/lib/ld-linux.so.2 \
	-Wl,--version-script=libc.map \
	-Wl,-soname=libc.so.6 \
	-Wl,-z,combreloc \
	-nostdlib -nostartfiles -e __libc_main -u __register_frame -L. \
	-Lmath -Lelf -Ldlfcn -Lnss -Lnis -Lrt -Lresolv -Lcrypt -Llinuxthreads \
	-Wl,-rpath-link=.:math:elf:dlfcn:nss:nis:rt:resolv:crypt:linuxthreads \
	-o mrs/libc.so \
	-T mrs/libc.so.lds csu/abi-note.o elf/soinit.os $EXTRA `./mrs/files-to-link.pl $EXCLUDE` elf/sofini.os elf/interp.os elf/ld.so -lgcc -lgcc_eh
  ;;
  *) echo Unknown version ;;
esac

else
  echo
  echo Not building library: use -L for that
fi

if [ ! -e mrs/run-as-nobody ]; then
  echo
  echo You need to compile run-as-nobody and make it setuid root
fi

./mrs/install.sh
