#!/bin/bash

# Copyright (C) 2004, 2005 Mark Seaborn
#
# This file is part of Plash.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
# USA.


set -e
# set -x

. ./src/config.sh

# Directory for writing glibc-dependent output files.
OUT=shobj
mkdir -p $OUT bin obj gensrc


# Unset to ensure that debugging info is included.  This slows down the build.
STRIP_EARLY=1


# These are object files from glibc that the Plash-libc code links to.
# They are not to be visible outside of the Plash-libc code.
# These are linked into the combined-*.os files below.
# Their symbols are hidden by use of "objcopy".
OBJS_FOR_LIBC="
	posix/getuid.os
	posix/getgid.os
	posix/geteuid.os
	posix/getegid.os
	posix/fork.os
	posix/execve.os
	socket/connect.os
	socket/bind.os
	socket/getsockname.os
	socket/getsockopt.os
	io/close.os
	io/dup2.os
	io/fstat.oS io/fxstat.os
	io/xstat64.os io/xstatconv.os"
OBJS_FOR_RTLD="
	socket/rtld-recvmsg.os
	socket/rtld-sendmsg.os
	socket/rtld-send.os
	socket/rtld-cmsg_nxthdr.os
	posix/getuid.os
	posix/getgid.os
	posix/geteuid.os
	posix/getegid.os
	io/rtld-close.os
	io/rtld-fstat.os io/rtld-fxstat.os
	io/rtld-xstat64.os io/rtld-xstatconv.os"

# These are object files that are removed from the normal glibc (from
# libc_pic.a) before creating libc.so.  They are system calls related
# to the filesystem namespace.
#  * NB. We didn't have to exclude the symbols defined by
#    fxstat{,64}.os.  However, unfortunately these files depend on
#    __have_no_stat64, which is defined by xstat64.os.  Rather than
#    mess around with linking to xstat64.os but hiding its other
#    symbols, I have just reimplemented fstat{,64}.
#  * open64.os used to be okay to leave in for glibc 2.2.5, because it
#    was defined in terms of open().  But in 2.3.3 it includes a
#    syscall.
# The following operate on filenames and could be excluded, but are not
# because they are privileged and so not usable from Plash anyway:
#   misc/mount.os misc/umount.os misc/chroot.os misc/pivot_root.os
EXCLUDE="io/open.os io/open64.os io/creat.os io/creat64.os
	io/not-cancel-open.os
	io/close.os
	io/not-cancel-close.os
	io/not-cancel-close-ns.os
	io/dup2.os
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
	socket/getsockname.os
	socket/getsockopt.os
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
	misc/setreuid.os misc/setregid.os"
if [ $GLIBC_VERSION -ge 240 ]; then
  EXCLUDE="$EXCLUDE
	dirent/fdopendir.os
	io/fxstatat.os
	io/fxstatat64.os"
fi

if [ ! -d "$GLIBC" ]; then
  echo "Warning: glibc object files directory \"$GLIBC\" does not exist"
fi

# Used by the SHLIB_COMPAT macro
cp -av $GLIBC/abi-versions.h gensrc/



# This links the plash-libc code that is to be linked into libc.so and
# ld.so.  This involves some linker trickery: we want to link to
# glibc's fork(), but export our own, different fork() that uses the
# former.
build_libc_ldso_extras () {
  # NB. ordering of object files is important
  # --retain-symbols-file my-exports
  # Don't need these, they're left in glibc:
  #   io/close.os io/dup.os io/dup2.os
  #   socket/recvmsg.os socket/sendmsg.os socket/send.os
  # May need io/fstat.oS (part of libc_nonshared.a), but if using
  # correct headers, will get an inline version instead that refers
  # to io/fxstat.os.  io/xstat64.os is needed to provide __have_no_stat64.

  # We could extract these from libc_pic.a.
  # But fstat.oS is an exception -- it can be extracted from libc_nonshared.a
  # (in libc6-dev).
  # It seems simpler to put the files individually into libc6-pic.
  #   mkdir -p $OUT/tmp
  #   GLIBC_ABS=`cd $GLIBC && pwd`
  #   (cd $OUT/tmp &&
  #    ar -xv $GLIBC_ABS/libc_pic.a \
  #      `for F in $OBJS_FOR_LIBC; do basename $F; done`)
  #   OBJS_FOR_LIBC2=`for F in $OBJS_FOR_LIBC; do echo $OUT/tmp/$(basename $F); done`

  # The "-r" option for "ld" is the same as "--relocateable", but in
  # newer versions this was renamed to "--relocatable".

  OBJS_FOR_LIBC2=`for F in $OBJS_FOR_LIBC; do echo $GLIBC/$F; done`
  echo Linking $OUT/combined-libc.os
  ld -r obj/libc-misc.os \
	obj/libc-stat.os \
	obj/libc-fork-exec.os \
	obj/libc-connect.os \
	obj/libc-getuid.os \
	obj/libc-getsockopt.os \
	obj/libc-utime.os \
	obj/libc-truncate.os \
	obj/libc-comms.os \
	obj/cap-utils.os \
	obj/cap-call-return.os \
	obj/libc-cap-protocol.os \
	obj/marshal-pack.os \
	obj/filesysobj.os \
	obj/comms.os \
	obj/serialise.os \
	obj/region.os \
	$OBJS_FOR_LIBC2 \
	-o $OUT/combined-libc.os

  echo Linking $OUT/combined-rtld.os
  ld -r obj/rtld-libc-misc.os \
	obj/rtld-libc-stat.os \
	obj/libc-getuid.os \
	obj/rtld-libc-comms.os \
	obj/cap-utils.os \
	obj/cap-call-return.os \
	obj/rtld-cap-protocol.os \
	obj/marshal-pack.os \
	obj/filesysobj.os \
	obj/rtld-comms.os \
	obj/region.os \
	obj/dont-free.os \
	`for F in $OBJS_FOR_RTLD; do echo $GLIBC/$F; done` \
	-o $OUT/combined-rtld.os

  # With -G, all other symbols are converted to be local
  # With -K, all other symbols are removed
  # We need two passes here.  Firstly we need to hide all the symbols that
  # dietlibc defines, such as "fork".  Then we need to rename our symbols,
  # such as "new_fork" to "fork".
  # If you try to do this in one pass with "objcopy", it renames "new_fork"
  # to "fork" and *then* keeps "fork", which leaves us with *two* definitions
  # of "fork", our one and dietlibc's.

  prepare_combined_obj () {
    # Hide all symbols except those to be exported to the rest of glibc.
    
    # errno:  this can be removed?
    # __fork_block:  exported by glibc's fork.os.
    # __pthread_fork:  imported weakly by glibc's fork.os.
    # __i686.get_pc_thunk.*:  some linker magic (necessary for gcc >=3.3).
    # __libc_missing_32bit_uids:  defined in posix/getuid.os but used by
    #     other UID/GID-related files.
    
    objcopy --wildcard \
      -G "export_*" \
      -G "exportver_*" \
      -G errno \
      -G __fork_block \
      -G __pthread_fork \
      -G __i686.get_pc_thunk.bx \
      -G __i686.get_pc_thunk.cx \
      -G __libc_missing_32bit_uids \
      $1
  }

  # TO BE REMOVED:
  REDEFINES="--redefine-sym glibc_getenv=getenv"
  
  echo Hiding and renaming symbols in $OUT/combined-libc.os
  ./src/get-export-syms.pl $OUT/combined-libc.os >$OUT/symbol-list-libc
  prepare_combined_obj $OUT/combined-libc.os
  objcopy $REDEFINES `./src/export-renames.pl <$OUT/symbol-list-libc` $OUT/combined-libc.os
  
  echo Hiding and renaming symbols in $OUT/combined-rtld.os
  ./src/get-export-syms.pl $OUT/combined-rtld.os >$OUT/symbol-list-rtld
  prepare_combined_obj $OUT/combined-rtld.os
  objcopy $REDEFINES `./src/export-renames.pl --rtld <$OUT/symbol-list-rtld` $OUT/combined-rtld.os
}


# Build the dynamic linker, ld-linux.so (also known as ld.so)
build_ldso () {
  # Normally glibc creates elf/rtld-libc.a
  echo 'Making ld.so (the dynamic linker)'

  RTLD_EXTRA_OBJS="
       stdlib/rtld-getenv.os
       string/rtld-strncmp.os
       posix/rtld-uname.os
       io/rtld-dup.os"

  cp -av $GLIBC/elf/rtld-libc.a $OUT/rtld-libc.a
  # Remove some object files
  # Add the "rtld-" prefix to object filenames
  ar -d $OUT/rtld-libc.a `for F in $EXCLUDE; do echo rtld-$(basename $F); done`
  # Add these extra object files
  ar -r $OUT/rtld-libc.a `for F in $RTLD_EXTRA_OBJS; do echo $GLIBC/$F; done`

  echo "  Generating linker script: $OUT/ld.so.lds"
  $CC -nostdlib -nostartfiles -shared \
	-Wl,-z,combreloc -Wl,-z,defs -Wl,--verbose 2>&1 | \
	LC_ALL=C \
    sed -e '/^=========/,/^=========/!d;/^=========/d'	\
	    -e 's/\. = 0 + SIZEOF_HEADERS;/& _begin = . - SIZEOF_HEADERS;/' \
	> $OUT/ld.so.lds

  echo "  Linking $OUT/ld.so"
  $CC -nostdlib -nostartfiles -shared -o $OUT/ld.so \
	-Wl,-z,combreloc \
	-Wl,-z,defs \
	-Wl,-z,relro \
	'-Wl,-(' $GLIBC/elf/dl-allobjs.os $OUT/rtld-libc.a $OUT/combined-rtld.os -lgcc '-Wl,-)' \
	-Wl,--version-script=$GLIBC/ld.map \
	-Wl,-soname=ld-linux.so.2 \
	-T $OUT/ld.so.lds
}


build_libc () {
  echo Making libc.so
  echo "  Generating linker script: $OUT/libc.so.lds"
  $CC -shared -Wl,-O1 -nostdlib -nostartfiles \
	-Wl,-dynamic-linker=/lib/ld-linux.so.2 \
	-Wl,-z,combreloc \
	-Wl,--verbose 2>&1 | \
    sed > $OUT/libc.so.lds \
        -e '/^=========/,/^=========/!d;/^=========/d' \
        -e 's/^.*\.hash[ 	]*:.*$/  .note.ABI-tag : { *(.note.ABI-tag) } &/' \
        -e 's/^.*\*(\.dynbss).*$/& \
	 PROVIDE(__start___libc_freeres_ptrs = .); \
	 *(__libc_freeres_ptrs) \
	 PROVIDE(__stop___libc_freeres_ptrs = .);/'

  echo "  Generating libc symbol version map: $OUT/libc.map"
  ./src/make-libc-map.pl <$GLIBC/libc.map >$OUT/libc.map

  if false; then
    # Without using libc_pic.a:
    OBJ_FILES=`cat $OUT/obj-file-list-libc`
    OBJ_FILES=`for F in $OBJ_FILES; do echo $GLIBC/$F; done`
  else
    # Using libc_pic.a:
    echo "  Making $OUT/libc_rem.a"
    if [ $STRIP_EARLY ]; then
      # Stripping libc_pic.a now will make later steps go faster.
      strip --strip-debug $GLIBC_PIC_DIR/libc_pic.a -o $OUT/libc_rem.a
    else
      cp -av $GLIBC_PIC_DIR/libc_pic.a $OUT/libc_rem.a
    fi
    ar -d $OUT/libc_rem.a `for F in $EXCLUDE; do basename $F; done`

    OBJ_FILES="-Wl,--whole-archive $OUT/libc_rem.a -Wl,--no-whole-archive"
  fi
  
  echo "  Linking $OUT/libc.so"
  # Arguments that have been removed:
  #   -L. -Lmath -Lelf -Ldlfcn -Lnss -Lnis -Lrt -Lresolv -Lcrypt -Llinuxthreads
  #   -Wl,-rpath-link=.:math:elf:dlfcn:nss:nis:rt:resolv:crypt:linuxthreads
  # echo NB. could remove -Wl,-O1 for faster linking
  $CC -Wl,--stats,--no-keep-memory -Wl,-O1 \
	-shared -static-libgcc \
	-Wl,-z,defs \
	-Wl,-dynamic-linker=/lib/ld-linux.so.2 \
	-Wl,--version-script=$OUT/libc.map \
	-Wl,-soname=libc.so.6 \
	-Wl,-z,combreloc \
	-nostdlib -nostartfiles -e __libc_main -u __register_frame \
	-o $OUT/libc.so \
	-T $OUT/libc.so.lds \
	$GLIBC/csu/abi-note.o $GLIBC/elf/soinit.os $OUT/combined-libc.os \
	$OBJ_FILES \
	$GLIBC/elf/sofini.os $GLIBC/elf/interp.os $OUT/ld.so \
	-lgcc -lgcc_eh
  # Normally we'd use $GLIBC/elf/ld.so there, not $OUT/ld.so.
  # Could link against the installed /lib/ld-linux.so.2 instead.
}


build_libpthread () {
  echo 'Making libpthread.so'

  if [ $GLIBC_VERSION -ge 240 ]; then
    LIBPTHREAD_DIR=nptl
  else
    LIBPTHREAD_DIR=linuxthreads
  fi

  if [ $GLIBC_VERSION -lt 240 ]; then
    # linuxthreads builds its own linuxthreads/libc.so and then links
    # against it.  This is a hack for changing which symbols
    # libpthread.so imports from libc.so.  The code in
    # linuxthreads/libc.so is never used; only the interface is
    # significant.
    # (See the comment in linuxthreads/Makefile.)

    echo '  Build dummy libc.so solely for linking libpthread.so against'
    if [ $STRIP_EARLY ]; then
      # Stripping libc_pic.a now will make later steps go faster.
      strip --strip-debug $GLIBC_PIC_DIR/libc_pic.a -o $OUT/libc_pic_lite.a
    else
      cp -av $GLIBC_PIC_DIR/libc_pic.a $OUT/libc_pic_lite.a
    fi
    ar -d $OUT/libc_pic_lite.a errno.os herrno.os res_libc.os

    #  * Note that "-z defs" must be removed: it causes unresolved symbols
    #    to be an error.  Removing those files above causes unresolved symbols.
    #  * glibc uses -d as an argument to "ld" here when building the
    #    intermediate *.os file.  What does it do?
    #  * glibc 2.3.5 seems to add -Wl,-z,relro -- important?
    $CC -Wl,--stats,--no-keep-memory  \
	-shared -static-libgcc \
	-Wl,-dynamic-linker=/lib/ld-linux.so.2 \
	-Wl,--version-script=$OUT/libc.map \
	-Wl,-soname=libc.so.6 \
	-Wl,-z,combreloc \
	-Wl,-z,relro \
	-nostdlib -nostartfiles -e __libc_main -u __register_frame \
	-Wl,-rpath-link=.:math:elf:dlfcn:nss:nis:rt:resolv:crypt:linuxthreads \
	-o $OUT/dummy-libc.so \
	-T $GLIBC/shlib.lds \
	$GLIBC/csu/abi-note.o $GLIBC/elf/soinit.os \
	-Wl,--whole-archive \
	       $OUT/libc_pic_lite.a $GLIBC/linuxthreads/libc-tsd.os \
	-Wl,--no-whole-archive \
	$GLIBC/elf/sofini.os $GLIBC/elf/interp.os $OUT/ld.so \
	-lgcc -lgcc_eh
  fi

  echo "  Making $OUT/libpthread_rem.a"
  cp -av $GLIBC/$LIBPTHREAD_DIR/libpthread_pic.a $OUT/libpthread_rem.a
  ar -dv $OUT/libpthread_rem.a \
	ptw-accept.os ptw-connect.os ptw-open.os ptw-open64.os \
	ptw-close.os
  
  # linuxthreads-extras.os ensures that libpthread.so contains __open
  # and other symbols, which it needs to export.
  # Actually, I can't reproduce any problems when this isn't included
  # (eg. mp3blaster links fine with it gone, even with LD_BIND_NOW=1),
  # so I can't remember exactly why it's there.
  # However, it *does* make sure that the `nm -D' output is the same as
  # it is for the normal build (er, except for weak/non-weak differences).
  $CC -c src/libpthread-extras.c -o $OUT/libpthread-extras.os \
	-O2 -Wall -fPIC -g

  echo "  Linking $OUT/libpthread.so"
  #  * Note that "-Blinuxthreads -Bcsu" are necessary: they cause
  #    linuxthreads/crti.o and linuxthreads/crtn.o to get linked.
  #    Without this, some important initialisation doesn't get done,
  #    and libpthread segfaults in __pthread_initialize_manager().
  mkdir -p $OUT/libpthread
  cp -av $GLIBC/$LIBPTHREAD_DIR/crt{i,n}.o $OUT/libpthread
  if [ $GLIBC_VERSION -lt 240 ]; then
    LIBPTHREAD_LIBC=$OUT/dummy-libc.so
    LINK_LIBPTHREAD=
  else
    LIBPTHREAD_LIBC=$OUT/libc.so
    LINK_LIBPTHREAD="-e __nptl_main"
  fi
  $CC -shared -static-libgcc -Wl,-O1 -Wl,-z,defs \
	-Wl,-dynamic-linker=/lib/ld-linux.so.2 \
	-B$OUT/libpthread -Bcsu \
	-Wl,--version-script=$GLIBC/libpthread.map -Wl,-soname=libpthread.so.0 \
	-Wl,-z,combreloc \
	-Wl,-z,relro \
	-Wl,--enable-new-dtags,-z,nodelete \
	-Wl,--enable-new-dtags,-z,initfirst \
	$LINK_LIBPTHREAD \
	-o $OUT/libpthread.so \
	-T $GLIBC/shlib.lds \
	$GLIBC/csu/abi-note.o \
	-Wl,--whole-archive $OUT/libpthread_rem.a $OUT/libpthread-extras.os -Wl,--no-whole-archive \
	$GLIBC/elf/interp.os $LIBPTHREAD_LIBC $GLIBC_NONSHARED_DIR/libc_nonshared.a $OUT/ld.so
  #  * Normally we'd use $GLIBC/elf/ld.so there, not $OUT/ld.so.
  #    Could link against the installed /lib/ld-linux.so.2 instead.
}


build_small_bits () {
  ./src/make-config-h.sh
  ./src/make-variants.pl
  ./src/make-marshal.pl
  ./src/make-vtables.pl
}


build_shell_etc() {
  # Populate $OUT/lib with shared objects to link against
  rm -rf $OUT/lib
  mkdir -p $OUT/lib
  GLIBC_DIR=`cd $GLIBC && pwd`
  (cd $OUT/lib
  ln -sf ../libc.so libc.so.6
  ln -sf $GLIBC_DIR/libc_nonshared.a libc_nonshared.a
  rm -f libc.so
  echo "GROUP ( libc.so.6 libc_nonshared.a )" >libc.so
  ln -sf ../ld.so ld-linux.so.2
  ln -sf ../libpthread.so libpthread.so.0
  ln -sf $GLIBC_DIR/math/libm.so libm.so
  ln -sf $GLIBC_DIR/dlfcn/libdl.so libdl.so
  ln -sf $GLIBC_DIR/rt/librt.so librt.so
  ln -sf libm.so libm.so.6
  ln -sf librt.so librt.so.1
  ln -sf libdl.so libdl.so.2
  )

  # Even though we use weak references, we need to link with our libc.so,
  # otherwise the weak references are omitted entirely from the dynamic
  # linking tables.  They need to be associated with a particular shared
  # object.
  # We need to link with libdl.so and libm.so in case the installed ones
  # aren't compatible with our libc.so and ld.so (they might be non-2.3.5).
  # $GLIBC/math/libm.so $GLIBC/dlfcn/libdl.so shobj/libc.so shobj/ld.so
  # FIXME: remove glib-2.0 from this
  LIBC_LINK="-Wl,-z,defs
	-L$OUT/lib -Wl,-rpath-link=$OUT/lib"
  if [ "$USE_GTK" = yes ]; then
    LIBC_LINK="$LIBC_LINK `pkg-config --libs glib-2.0`"
  fi

  # `pkg-config gtk+-2.0 --libs`
  # If I want this to work on my old RedHat system (using 2.2.5), it's
  # now necessary to omit $LIBC_LINK, which makes the executable depend
  # on 2.3.5 (which it will depend on anyway if you build on a system
  # where that is installed).  2.3.3 didn't have this problem.
  # Other points:
  #  * Used to link "-ltermcap".  This worked for building on RedHat 7.3,
  #    but produced an executable that didn't work on recent Debians.
  #    (However, building on Debian effectively ignored "-ltermcap" and
  #    pulled in libncurses instead.)
  #  * Link with "-lncurses".  This builds on RedHat 7.3 and Debian.
  #    ncurses replaces termcap.
  echo Linking bin/pola-shell
  $CC $OPTS_S obj/shell.o obj/libplash.a \
	obj/shell-parse.o \
	obj/shell-variants.o \
	obj/shell-globbing.o \
	$LIBC_LINK -lreadline -ltermcap \
	-o bin/pola-shell

  echo Linking bin/run-emacs
  $CC $OPTS_S obj/run-emacs.o $LIBC_LINK obj/libplash.a -o bin/plash-run-emacs

  if [ "$USE_GTK" = yes ]; then
    LINK_GTK=`pkg-config gtk+-2.0 --libs`
  else
    LINK_GTK=""
  fi
  echo Linking bin/pola-run
  $CC $OPTS_S obj/pola-run.o $LIBC_LINK obj/libplash.a $LINK_GTK -o bin/pola-run

  echo Linking bin/powerbox-req
  $CC $OPTS_S obj/powerbox-req.o $LIBC_LINK obj/libplash.a -o bin/powerbox-req

  if [ "$USE_GTK" = yes ]; then
    echo Linking bin/plash-opts-gtk
    $CC $OPTS_S \
	obj/shell-options-gtk.o obj/shell-options.o $LIBC_LINK \
	obj/libplash.a `pkg-config gtk+-2.0 --libs` -o bin/plash-opts-gtk
  fi
  echo Linking bin/plash-opts
  $CC $OPTS_S \
	obj/shell-options-cmd.o obj/shell-options.o $LIBC_LINK \
	obj/libplash.a -o bin/plash-opts

  echo Linking bin/chroot
  $CC $OPTS_S obj/chroot.o $LIBC_LINK obj/libplash.a -o bin/plash-chroot
  echo Linking bin/exec-object
  $CC $OPTS_S obj/exec-object.o $LIBC_LINK obj/libplash.a -o bin/exec-object

  echo Linking bin/socket-publish
  $CC $OPTS_S obj/socket-publish.o $LIBC_LINK obj/libplash.a -o bin/plash-socket-publish
  echo Linking bin/socket-connect
  $CC $OPTS_S obj/socket-connect.o $LIBC_LINK obj/libplash.a -o bin/plash-socket-connect

  $CC $OPTS_S obj/test-caps.o $LIBC_LINK obj/libplash.a -o bin/test-caps
}

build_gtk_powerbox () {
  if [ "$USE_GTK" = yes ]; then
    echo Linking shobj/gtk-powerbox.so
    $CC -shared -Wl,-z,defs \
	obj/gtk-powerbox.os \
	obj/libplash_pic.a \
	`pkg-config --libs gtk+-2.0` -ldl \
	-o shobj/powerbox-for-gtk.so
  fi
}

build_python_module () {
  if [ "$USE_PYTHON" = yes ]; then
    echo Building Python module
    (cd python && $PYTHON setup.py build --force)
  fi
}


if [ "$1" != "--include" ]; then

build_small_bits
(export CC; cd setuid && ./make-setuid.sh)
./make-objs.pl
build_libc_ldso_extras
# Building ld.so first is useful because libc.so and libpthread.so link against it.
build_ldso
build_libc
build_libpthread
build_shell_etc
build_gtk_powerbox
build_python_module

fi
