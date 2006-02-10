#!/bin/bash

# Copyright (C) 2004, 2005 Mark Seaborn
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

set -e
set -x

OPTS_C="-Wall -nostdlib \
        -g \
	-D_REENTRANT \
	-fPIC"



# Build the shell
build_server () {
  OPTS_S="-O1 -Wall -g `pkg-config gtk+-2.0 --cflags`"

  # Server: compiled with glibc
  # Doesn't have to be position-independent (though that doesn't hurt)
  # I used to use dietlibc for this, but then I wanted to link with readline
  $CC $OPTS_S -c src/region.c -o obj/region.o
  $CC $OPTS_S -c src/serialise.c -o obj/serialise.o
  $CC $OPTS_S '-DHALF_NAME="server"' -c src/comms.c -o obj/comms.o
  $CC $OPTS_S -c src/cap-protocol.c -o obj/cap-protocol.o
  $CC $OPTS_S -c src/cap-call-return.c -o obj/cap-call-return.o
  $CC $OPTS_S -c src/cap-utils.c -o obj/cap-utils.o
  $CC $OPTS_S -c src/cap-utils-libc.c -o obj/cap-utils-libc.o
  $CC $OPTS_S -c src/utils.c -o obj/utils.o
  $CC $OPTS_S -c src/parse-filename.c -o obj/parse-filename.o
  $CC $OPTS_S -c src/filesysobj.c -o obj/filesysobj.o
  $CC $OPTS_S -c src/filesysobj-real.c -o obj/filesysobj-real.o
  $CC $OPTS_S -c src/filesysobj-fab.c -o obj/filesysobj-fab.o
  $CC $OPTS_S -c src/filesysobj-readonly.c -o obj/filesysobj-readonly.o
  $CC $OPTS_S -c src/filesysobj-union.c -o obj/filesysobj-union.o
  $CC $OPTS_S -c src/filesysslot.c -o obj/filesysslot.o
  $CC $OPTS_S -c src/log-proxy.c -o obj/log-proxy.o
  $CC $OPTS_S -c src/reconnectable-obj.c -o obj/reconnectable-obj.o
  $CC $OPTS_S -c src/build-fs.c -o obj/build-fs.o
  $CC $OPTS_S -c src/build-fs-static.c -o obj/build-fs-static.o
  $CC $OPTS_S -c src/build-fs-dynamic.c -o obj/build-fs-dynamic.o
  $CC $OPTS_S -c src/shell.c -o obj/shell.o
  $CC $OPTS_S -Wno-unused -c src/shell-parse.c -o obj/shell-parse.o
  $CC $OPTS_S -c src/shell-variants.c -o obj/shell-variants.o
  $CC $OPTS_S -c src/shell-globbing.c -o obj/shell-globbing.o
  $CC $OPTS_S -c src/shell-fds.c -o obj/shell-fds.o
  $CC $OPTS_S -c src/shell-wait.c -o obj/shell-wait.o
  $CC $OPTS_S -c src/shell-options.c -o obj/shell-options.o
  $CC $OPTS_S -c src/resolve-filename.c -o obj/resolve-filename.o
  $CC $OPTS_S -c src/fs-operations.c -o obj/fs-operations.o
  # To link server with dietlibc, you need to do:
  # $CC $OPTS $DIET/bin-i386/start.o ...files... \
  #     $DIET/bin-i386/dietlibc.a -lgcc -o mrs/driver
  rm -f obj/libplash.a
  ar cru obj/libplash.a \
	obj/shell-parse.o \
	obj/shell-variants.o \
	obj/shell-globbing.o \
	obj/shell-fds.o \
	obj/shell-wait.o \
	obj/build-fs.o obj/build-fs-static.o obj/build-fs-dynamic.o \
	obj/fs-operations.o obj/resolve-filename.o \
	obj/cap-utils.o obj/cap-utils-libc.o obj/cap-call-return.o obj/cap-protocol.o \
	obj/filesysslot.o \
	obj/filesysobj-fab.o \
	obj/filesysobj-union.o \
	obj/filesysobj-readonly.o \
	obj/filesysobj-real.o \
	obj/filesysobj.o \
	obj/log-proxy.o \
	obj/reconnectable-obj.o \
	obj/parse-filename.o obj/comms.o \
	obj/serialise.o obj/region.o obj/utils.o
  # `pkg-config gtk+-2.0 --libs`
  $CC $OPTS_S obj/shell.o obj/libplash.a \
	-lreadline -ltermcap \
	-o bin/plash

  $CC $OPTS_S src/test-caps.c obj/libplash.a -o bin/test-caps

  LIBC_LINK="shobj/libc.so shobj/ld.so"
  # libc.so contains a reference to __libc_stack_end, which ld.so defines.
  # With a newer `ld', I have to link ld.so here too.  With an older one,
  # I could leave ld.so out.
  $CC $OPTS_S \
	src/chroot.c $LIBC_LINK \
	obj/libplash.a -o bin/plash-chroot
  $CC $OPTS_S \
	src/exec-object.c $LIBC_LINK \
	obj/libplash.a -o bin/exec-object
  $CC $OPTS_S \
	src/run-emacs.c $LIBC_LINK \
	obj/libplash.a -o bin/run-emacs

  $CC $OPTS_S -c src/shell-options.c -o obj/shell-options.o
  $CC $OPTS_S \
	src/shell-options-gtk.c obj/shell-options.o $LIBC_LINK \
	obj/libplash.a `pkg-config gtk+-2.0 --libs` -o bin/plash-opts-gtk
  $CC $OPTS_S \
	src/shell-options-cmd.c obj/shell-options.o $LIBC_LINK \
	obj/libplash.a -o bin/plash-opts
}



# Build objects that are used by both libc.so and ld-linux.so (ld.so)
build_client () {
  # This needs to come first because it generates header files.
  ./src/make-link-def.pl

  # Client: position-independent
  # Used to be compiled with dietlibc
  $CC $OPTS_C '-DHALF_NAME="client"' -c src/comms.c -o obj/comms.os
  $CC $OPTS_C '-DHALF_NAME="client"' -DIN_RTLD -c src/comms.c -o obj/rtld-comms.os
  $CC $OPTS_C -c src/region.c -o obj/region.os
  $CC $OPTS_C -c src/serialise.c -o obj/serialise.os
  $CC $OPTS_C -DIN_LIBC -c src/cap-protocol.c -o obj/cap-protocol.os
  $CC $OPTS_C -DIN_RTLD -c src/cap-protocol.c -o obj/rtld-cap-protocol.os
  $CC $OPTS_C -c src/cap-call-return.c -o obj/cap-call-return.os
  $CC $OPTS_C -c src/cap-utils.c -o obj/cap-utils.os
  $CC $OPTS_C -c src/dont-free.c -o obj/dont-free.os
  $CC $OPTS_C -c src/filesysobj.c -o obj/filesysobj.os
  $CC $OPTS_C -c src/libc-misc.c -o obj/libc-misc.os
  $CC $OPTS_C -c src/libc-comms.c -o obj/libc-comms.os
  $CC $OPTS_C -c src/libc-fork-exec.c -o obj/libc-fork-exec.os
  $CC $OPTS_C -c src/libc-connect.c -o obj/libc-connect.os
  $CC $OPTS_C -c src/libc-getuid.c -o obj/libc-getuid.os
  $CC $OPTS_C -c src/libc-utime.c -o obj/libc-utime.os
  $CC $OPTS_C -c src/libc-truncate.c -o obj/libc-truncate.os

  echo
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
  echo Linking obj/combined.os
  ld -r obj/libc-misc.os \
	obj/libc-fork-exec.os \
	obj/libc-connect.os \
	obj/libc-getuid.os \
	obj/libc-utime.os \
	obj/libc-truncate.os \
	obj/libc-comms.os \
	obj/cap-utils.os \
	obj/cap-call-return.os \
	obj/cap-protocol.os \
	obj/filesysobj.os \
	obj/comms.os \
	obj/serialise.os \
	obj/region.os \
	glibc/posix/getuid.os \
	glibc/posix/getgid.os \
	glibc/posix/fork.os \
	glibc/posix/execve.os \
	glibc/socket/connect.os \
	glibc/socket/bind.os \
	glibc/io/fstat.oS glibc/io/fxstat.os \
	glibc/io/xstat64.os glibc/io/xstatconv.os \
	-o obj/combined.os
  EXTRA="obj/combined.os"
  # mrs/sysdeps/not-cancel.os

  echo Linking obj/rtld-combined.os
  OBJS_FOR_RTLD="
	socket/rtld-recvmsg.os \
	socket/rtld-sendmsg.os \
	socket/rtld-send.os \
	socket/cmsg_nxthdr.os \
	io/rtld-fstat.os io/rtld-fxstat.os \
	io/rtld-xstat64.os io/rtld-xstatconv.os"
  ld -r obj/libc-misc.os obj/libc-getuid.os \
	obj/libc-comms.os \
	obj/cap-utils.os \
	obj/cap-call-return.os \
	obj/rtld-cap-protocol.os \
	obj/filesysobj.os \
	obj/rtld-comms.os \
	obj/region.os \
	obj/dont-free.os \
	`for F in $OBJS_FOR_RTLD; do echo glibc/$F; done` \
	-o obj/rtld-combined.os
  EXTRA_RTLD="obj/rtld-combined.os
	glibc/stdlib/rtld-getenv.os
	glibc/string/rtld-strncmp.os"

  # NB. We didn't have to exclude the symbols defined by fxstat{,64}.os.
  # However, unfortunately these files depend on __have_no_stat64, which
  # is defined by xstat64.os.  Rather than mess around with linking to
  # xstat64.os but hiding its other symbols, I have just reimplemented
  # fstat{,64}.
  # open64.os used to be okay to leave in for glibc 2.2.5, because it
  # was defined in terms of open().  But in 2.3.3 it includes a syscall.
  EXCLUDE="io/open.os io/open64.os io/creat.os io/creat64.os
	io/getcwd.os io/chdir.os io/fchdir.os
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
	stdio-common/rename.os
	misc/utimes.os misc/lutimes.os
	misc/truncate.os
	socket/connect.os socket/bind.os
	posix/fork.os posix/vfork.os posix/execve.os
	dirent/opendir.os dirent/closedir.os
	dirent/readdir.os dirent/readdir64.os
	dirent/readdir_r.os dirent/readdir64_r.os
	dirent/getdents.os dirent/getdents64.os
	dirent/rewinddir.os dirent/seekdir.os dirent/telldir.os
	dirent/dirfd.os
	misc/getxattr.os misc/setxattr.os
	misc/lgetxattr.os misc/lsetxattr.os
	misc/listxattr.os misc/removexattr.os
	misc/llistxattr.os misc/lremovexattr.os
	posix/getuid.os posix/getgid.os
	posix/geteuid.os posix/getegid.os
	posix/setuid.os posix/setgid.os
	posix/setresuid.os posix/setresgid.os
	misc/seteuid.os misc/setegid.os
	misc/setreuid.os misc/setregid.os
	plash/not-cancel-open.os"
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
  sh src/out-link_main.sh
  sh src/out-link_rtld.sh

  if false; then
    echo
    echo The following symbols are resolved from outside:
    nm obj/combined.os | grep " U " | ./src/process-nm-output.sh
    echo The following have been added to glibc:
    nm obj/combined.os | grep " [TW] " | ./src/process-nm-output.sh
    echo The following have been removed from glibc:
    nm $EXCLUDE | grep " [TW] " | ./src/process-nm-output.sh
    echo
  fi
  echo
  ./src/symbol-info.pl $EXCLUDE
  echo
}



# Build ld-linux.so (also known as ld.so)
build_ldso () {
 case $GLIBC_VERSION in
  2.3.3)
    echo Making ld.so
    rm -f obj/rtld-libc.a
    ar cruv obj/rtld-libc.a `./src/files-to-link.pl --rtld $EXCLUDE`

    echo Generating linker script
    $CC -nostdlib -nostartfiles -shared \
  	-Wl,-z,combreloc -Wl,-z,defs -Wl,--verbose 2>&1 | \
	  LC_ALL=C \
	  sed -e '/^=========/,/^=========/!d;/^=========/d'	\
	      -e 's/\. = 0 + SIZEOF_HEADERS;/& _begin = . - SIZEOF_HEADERS;/' \
	  > src/ld.so.lds

    echo Linking ld.so
    $CC -nostdlib -nostartfiles -shared -o shobj/ld.so \
	-Wl,-z,combreloc -Wl,-z,defs \
	'-Wl,-(' glibc/elf/dl-allobjs.os obj/rtld-libc.a $EXTRA_RTLD -lgcc '-Wl,-)' \
	-Wl,--version-script=glibc/ld.map -Wl,-soname=ld-linux.so.2 -T src/ld.so.lds
    ;;
  *) echo Unknown version ;;
 esac
}



# Build libpthread.so
build_libpthread () {
  echo Linking libpthread.so
  # For some reason, linuxthreads builds its own linuxthreads/libc.so
  # and then links with it.  Why?

  PTHREAD_OBJS="attr.os cancel.os condvar.os join.os manager.os mutex.os ptfork.os ptlongjmp.os pthread.os pt-sigsuspend.os signals.os specific.os errno.os lockfile.os semaphore.os spinlock.os rwlock.os pt-machine.os oldsemaphore.os events.os getcpuclockid.os pspinlock.os barrier.os ptclock_gettime.os ptclock_settime.os sighandler.os pthandles.os libc-tls-loc.os pt-allocrtsig.os
	ptw-write.os ptw-read.os ptw-close.os ptw-fcntl.os ptw-recv.os ptw-recvfrom.os ptw-recvmsg.os ptw-send.os ptw-sendmsg.os ptw-sendto.os ptw-fsync.os ptw-lseek.os ptw-lseek64.os ptw-llseek.os ptw-msync.os ptw-nanosleep.os ptw-pause.os ptw-pread.os ptw-pread64.os ptw-pwrite.os ptw-pwrite64.os ptw-tcdrain.os ptw-wait.os ptw-waitpid.os
	pt-system.os old_pthread_atfork.os pthread_atfork.os"
  # removed:
  # ptw-accept.os ptw-connect.os ptw-open.os ptw-open64.os

  (cd src/sysdeps; ./make.sh)
  # linuxthreads-extras.os ensures that libpthread.so contains __open
  # and other symbols, which is needs to export.
  # Actually, I can't reproduce any problems when this isn't included
  # (eg. mp3blaster links fine with it gone, even with LD_BIND_NOW=1),
  # so I can't remember exactly why it's there.
  # However, it *does* make sure that the `nm -D' output is the same as
  # it is for the normal build (er, except for weak/non-weak differences).
  PTHREAD_OBJS2="src/sysdeps/linuxthreads-extras.os
	`for F in $PTHREAD_OBJS; do echo glibc/linuxthreads/$F; done`"

  $CC -shared -static-libgcc -Wl,-O1 -Wl,-z,defs \
	-Wl,-dynamic-linker=/lib/ld-linux.so.2 \
	-Blinuxthreads -Bcsu \
	-Wl,--version-script=glibc/libpthread.map -Wl,-soname=libpthread.so.0 \
	-Wl,-z,combreloc \
	-Wl,--enable-new-dtags,-z,nodelete \
	-Wl,--enable-new-dtags,-z,initfirst \
	-o shobj/libpthread.so \
	-T glibc/shlib.lds \
	glibc/csu/abi-note.o $PTHREAD_OBJS2 \
	glibc/elf/interp.os glibc/linuxthreads/libc.so glibc/libc_nonshared.a glibc/elf/ld.so
}



# Build libc.so
build_libc () {
# Differences in 2.3.3:
# New arguments used: -static-libgcc -Wl,-z,defs -lgcc_eh
# The name of the file that the linker script was output to
# changed from libc.so.lds to shlib.lds (but we can use any
# name).  Fewer arguments were used when generating the
# linker script -- I suspect that virtually none of the arguments
# are needed, because we're just getting `gcc' to print out the
# script file that it gives to `ld'.
case $GLIBC_VERSION in
  2.3.3)
    echo Making script
    $CC -shared -Wl,-O1 -nostdlib -nostartfiles \
	-Wl,-dynamic-linker=/lib/ld-linux.so.2 \
	-Wl,-z,combreloc \
	-Wl,--verbose 2>&1 | \
      sed > src/libc.so.lds \
        -e '/^=========/,/^=========/!d;/^=========/d' \
        -e 's/^.*\.hash[ 	]*:.*$/  .note.ABI-tag : { *(.note.ABI-tag) } &/' \
        -e 's/^.*\*(\.dynbss).*$/& \
	 PROVIDE(__start___libc_freeres_ptrs = .); \
	 *(__libc_freeres_ptrs) \
	 PROVIDE(__stop___libc_freeres_ptrs = .);/'
    echo Linking libc.so
    echo NB. could remove -Wl,-O1 for faster linking
    $CC -Wl,--stats,--no-keep-memory -Wl,-O1 \
	-shared -static-libgcc -Wl,-z,defs \
	-Wl,-dynamic-linker=/lib/ld-linux.so.2 \
	-Wl,--version-script=src/libc.map \
	-Wl,-soname=libc.so.6 \
	-Wl,-z,combreloc \
	-nostdlib -nostartfiles -e __libc_main -u __register_frame -L. \
	-Lmath -Lelf -Ldlfcn -Lnss -Lnis -Lrt -Lresolv -Lcrypt -Llinuxthreads \
	-Wl,-rpath-link=.:math:elf:dlfcn:nss:nis:rt:resolv:crypt:linuxthreads \
	-o shobj/libc.so \
	-T src/libc.so.lds \
	glibc/csu/abi-note.o glibc/elf/soinit.os $EXTRA \
	`./src/files-to-link.pl $EXCLUDE` \
	glibc/elf/sofini.os glibc/elf/interp.os glibc/elf/ld.so \
	-lgcc -lgcc_eh
  ;;
  *) echo Unknown version ;;
esac
}



build_small_bits () {
  ./src/make-config-h.sh
  ./src/make-variants.pl
  ./src/make-marshal.pl
  ./src/make-vtables.pl
}



if [ -z $CC ]; then CC=gcc; fi

# if [ -z $BUILD_SERVER ]; then build_server; fi
# if [ -z $BUILD_LIBC ]; then BUILD_LIBC=no; fi
# if [ -z $BUILD_LIBPTHREAD ]; then BUILD_LIBPTHREAD=no; fi
# if [ -z $BUILD_LDSO ]; then BUILD_LDSO=no; fi
# if [ "$1" = "-L" ]; then BUILD_LIBC=yes; fi
# 
# if [ $BUILD_LIBC = yes -o $BUILD_LDSO = yes ]; then build_client; fi
# 
# if [ $BUILD_LDSO = yes ]; then build_ldso; fi
# if [ $BUILD_LIBPTHREAD = yes ]; then build_libpthread; fi
# if [ $BUILD_LIBC = yes ]; then build_libc; fi

case "$1" in
  libc)
    build_small_bits
    build_client
    build_libc
    build_ldso
    build_libpthread
    ;;
  shell)
    build_small_bits
    build_server
    ;;
  *)
    echo "Usage: [CC=gcc-XX] ./make.sh libc|shell"
    ;;
esac
