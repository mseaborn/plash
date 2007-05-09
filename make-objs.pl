#!/usr/bin/perl -w

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

use IO::File;


# Read configure settings
my ($cc, $use_gtk, $check) =
  split(/\n/, `. src/config.sh && echo \$CC; echo \$USE_GTK; echo check`);
if($check ne 'check') { die }
$use_gtk = $use_gtk eq 'yes';


my @c_flags = ('-O1', '-Wall',
	       '-Igensrc', '-Isrc');
my @opts_s = @c_flags;
if($use_gtk) {
  push(@opts_s,
       split(/\s+/, `pkg-config gtk+-2.0 --cflags`),
       '-DPLASH_GLIB');
}
my @opts_c = (@c_flags,
	      '-D_REENTRANT', '-fPIC');


my @library_files;
sub build_lib {
  my ($name, @args) = @_;
  gcc("src/$name.c", "obj/$name.o", @opts_s);
  gcc("src/$name.c", "obj/$name.os", @opts_s, '-D_REENTRANT', '-fPIC');
  push(@library_files, $name);
}

build_lib('region');
build_lib('serialise');
build_lib('serialise-utils');
build_lib('comms');
build_lib('cap-protocol');
build_lib('cap-call-return');
build_lib('cap-utils');
build_lib('cap-utils-libc');
build_lib('marshal-pack');
build_lib('marshal-exec');
build_lib('utils');
build_lib('parse-filename');
build_lib('filesysobj');
build_lib('filesysobj-real');
build_lib('filesysobj-fab');
build_lib('filesysobj-readonly');
build_lib('filesysobj-union');
build_lib('filesysobj-cow');
build_lib('filesysslot');
build_lib('log');
build_lib('log-proxy');
build_lib('reconnectable-obj');
build_lib('build-fs');
build_lib('build-fs-static');
build_lib('build-fs-dynamic');
build_lib('resolve-filename');
build_lib('fs-operations');
build_lib('exec');
build_lib('config-read');
build_lib('shell-fds');
build_lib('shell-wait');

gcc('src/shell.c', 'obj/shell.o', @opts_s);
gcc('src/shell-parse.c', 'obj/shell-parse.o', @opts_s,
    '-Wno-unused');
gcc('src/shell-variants.c', 'obj/shell-variants.o', @opts_s);
gcc('src/shell-globbing.c', 'obj/shell-globbing.o', @opts_s);
gcc('src/shell-options.c', 'obj/shell-options.o', @opts_s);
if($use_gtk) {
  build_lib('powerbox');
}

do_cmd('rm', '-f', 'obj/libplash.a');
do_cmd('rm', '-f', 'obj/libplash_pic.a');
do_cmd('ar', '-cr', 'obj/libplash.a', map { "obj/$_.o" } @library_files);
do_cmd('ar', '-cr', 'obj/libplash_pic.a', map { "obj/$_.os" } @library_files);

# Non-library code
gcc('src/test-caps.c', 'obj/test-caps.o', @opts_s);
gcc('src/shell-options.c', 'obj/shell-options.o', @opts_s);
if($use_gtk) {
  gcc('src/shell-options-gtk.c', 'obj/shell-options-gtk.o', @opts_s);
}
gcc('src/shell-options-cmd.c', 'obj/shell-options-cmd.o', @opts_s);
gcc('src/chroot.c', 'obj/chroot.o', @opts_s);
gcc('src/exec-object.c', 'obj/exec-object.o', @opts_s);
gcc('src/socket-publish.c', 'obj/socket-publish.o', @opts_s);
gcc('src/socket-connect.c', 'obj/socket-connect.o', @opts_s);
gcc('src/run-emacs.c', 'obj/run-emacs.o', @opts_s);
gcc('src/pola-run.c', 'obj/pola-run.o', @opts_s);
gcc('src/powerbox-req.c', 'obj/powerbox-req.o', @opts_s);



# Build object files to be linked into libc.so and ld.so (ld-linux.so)

gcc('src/comms.c', 'obj/rtld-comms.os', @opts_c, '-DIN_RTLD');
gcc('src/region.c', 'obj/region.os', @opts_c);
gcc('src/serialise.c', 'obj/serialise.os', @opts_c);
gcc('src/cap-protocol.c', 'obj/libc-cap-protocol.os', @opts_c,
    '-DIN_LIBC');
gcc('src/cap-protocol.c', 'obj/rtld-cap-protocol.os', @opts_c,
    '-DIN_RTLD');
gcc('src/cap-call-return.c', 'obj/cap-call-return.os', @opts_c);
gcc('src/cap-utils.c', 'obj/cap-utils.os', @opts_c);
gcc('src/marshal-pack.c', 'obj/marshal-pack.os', @opts_c);
gcc('src/dont-free.c', 'obj/dont-free.os', @opts_c);
gcc('src/filesysobj.c', 'obj/filesysobj.os', @opts_c);
gcc('src/libc-preload-import.c', 'obj/libc-preload-import.os', @opts_c);
gcc('src/libc-misc.c', 'obj/libc-misc.os', @opts_c,
    '-DIN_LIBC');
gcc('src/libc-misc.c', 'obj/rtld-libc-misc.os', @opts_c,
    '-DIN_RTLD');
gcc('src/libc-stat.c', 'obj/libc-stat.os', @opts_c, '-DIN_LIBC');
gcc('src/libc-stat.c', 'obj/rtld-libc-stat.os', @opts_c, '-DIN_RTLD');
gcc('src/libc-comms.c', 'obj/libc-comms.os', @opts_c, '-DIN_LIBC');
gcc('src/libc-comms.c', 'obj/rtld-libc-comms.os', @opts_c, '-DIN_RTLD');
gcc('src/libc-fork-exec.c', 'obj/libc-fork-exec.os', @opts_c);
gcc('src/libc-connect.c', 'obj/libc-connect.os', @opts_c);
gcc('src/libc-getsockopt.c', 'obj/libc-getsockopt.os', @opts_c);
gcc('src/libc-getuid.c', 'obj/libc-getuid.os', @opts_c);
gcc('src/libc-utime.c', 'obj/libc-utime.os', @opts_c);
gcc('src/libc-truncate.c', 'obj/libc-truncate.os', @opts_c);
gcc('src/libc-at-calls.c', 'obj/libc-at-calls.os', @opts_c);
gcc('src/libc-inotify.c', 'obj/libc-inotify.os', @opts_c);



# Build powerbox for Gtk

if($use_gtk) {
  my @opts_gtk_pb = ('-Igensrc', '-Isrc',
		     '-Wall',
		     '-fPIC',
		     split(/\s+/, `pkg-config gtk+-2.0 --cflags`));

  gcc('src/gtk-powerbox.c', 'obj/gtk-powerbox.os', @opts_gtk_pb);
  # Not used, but keep building it so that it doesn't bitrot
  gcc('src/gtk-powerbox-noninherit.c', 'obj/gtk-powerbox-noninherit.os', @opts_gtk_pb);
}



sub gcc {
  my ($src, $dest, @opts) = @_;

  # Read the dependencies makefile generated by gcc
  my $deps_file = "$dest.d";
  my @deps;
  if(-e $deps_file) {
    my $d = slurp($deps_file);
    $d =~ s/\\\n//g;
    if($d =~ /\S/) {
      $d =~ /^(\S+):\s*(.*)\s*$/ || die "Don't recognise deps format in $deps_file";
      if($1 ne $dest) { warn "Filename doesn't match" }
      @deps = split(/\s+/, $2);
      # print "$dest deps:\n".join('', map { "  $_\n" } @deps);
    }
  }

  my $b = 0;
  my $time1 = file_mtime($dest);
  if(!defined $time1) { $b = 1 }
  foreach my $dep (@deps) {
    my $t = file_mtime($dep);
    if(defined $t) {
      $b ||= $t > $time1;
    }
    else {
      print "dependency file gone: $dep\n";
      $b = 1;
    }
  }

  if($b) {
    print "Compile $src -> $dest\n";
    do_cmd(split(' ', $cc), @opts,
	   '-c', $src, '-o', $dest,
	   '-MD', '-MF', $deps_file);
  }
  else { print "up-to-date: $dest\n" }
}


sub slurp {
  my ($file) = @_;
  my $f = IO::File->new($file, 'r');
  if(!defined $f) { die "Can't open `$file' for reading" }
  my $d = join('', <$f>);
  $f->close();
  $d
}

sub do_cmd {
  my @args = @_;
  # print join(' ', @args)."\n";
  my $rc = system(@args);
  if($rc != 0) { die "Failed: return code $rc" }
}

sub file_mtime {
  my ($file) = @_;
  my ($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size,
      $atime, $mtime, $ctime, $blksize, $blocks) = stat($file);
  $mtime
}
