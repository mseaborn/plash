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


setup_glibc_build () {
  echo > gensrc/abi-versions.h

  LIBC_OBJS="obj/libc-misc_libc.os \
	obj/libc-stat_libc.os \
	obj/libc-fork-exec_libc.os \
	obj/libc-connect_libc.os \
	obj/libc-getuid_libc.os \
	obj/libc-getsockopt_libc.os \
	obj/libc-utime_libc.os \
	obj/libc-truncate_libc.os \
	obj/libc-comms_libc.os \
	obj/libc-at-calls_libc.os \
	obj/libc-inotify_libc.os \
	obj/cap-utils_libc.os \
	obj/cap-call-return_libc.os \
	obj/cap-protocol_libc.os \
	obj/marshal-pack_libc.os \
	obj/filesysobj_libc.os \
	obj/comms_libc.os \
	obj/serialise_libc.os \
	obj/region_libc.os"
}


build_preload_library () {
  echo Linking $OUT/preload-libc.os
  ld -r $LIBC_OBJS \
	obj/libc-preload-import.os \
	-o $OUT/preload-libc.os
}


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


link_preload_library () {
  echo Building $OUT/preload-libc.so
  ./src/get-export-syms.pl $OUT/preload-libc.os >$OUT/symbol-list-libc-preload
  prepare_combined_obj $OUT/preload-libc.os
  objcopy `./src/export-renames.pl --rtld <$OUT/symbol-list-libc-preload` $OUT/preload-libc.os
  gcc -shared -Wl,-z,defs $OUT/preload-libc.os -ldl -o $OUT/preload-libc.so
}


build_small_bits () {
  ./src/make-config-h.sh
  ./src/make-variants.pl
  ./src/make-marshal.pl
  ./src/make-vtables.pl
}


build_shell_etc() {
  LIBC_LINK="-Wl,-z,defs
	-ldl"
  if [ "$USE_GTK" = yes ]; then
    LIBC_LINK="$LIBC_LINK `pkg-config --libs glib-2.0`"
  fi

  echo Linking bin/pola-shell
  $CC $OPTS_S obj/shell.o obj/libplash.a \
	obj/shell-parse.o \
	obj/shell-variants.o \
	obj/shell-globbing.o \
	$LIBC_LINK -lreadline \
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

  echo Linking bin/kernel-exec
  if which diet >/dev/null; then
    diet $CC src/kernel-exec.c -o bin/kernel-exec
  else
    $CC -static src/kernel-exec.c -o bin/kernel-exec
  fi
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
    (cd python
     $PYTHON setup.py build --force
     # Install C module within build tree for use by unit tests
     $PYTHON setup.py install --install-platlib=lib
    ) || false
  fi
}

install_libs_for_testing () {
  mkdir -p lib
  ./src/install-libs.pl --dest-dir lib/
}


all_steps () {
  build_small_bits
  setup_glibc_build
  ./make_objs.py
  build_preload_library
  link_preload_library
  build_shell_etc
  build_gtk_powerbox
  build_python_module
}


if [ "$1" == "--include" ]; then
  # nothing
  true
elif [ $# -gt 0 ]; then
  for ACTION in "$@"; do
    $ACTION
  done
else
  all_steps
fi
