#!/usr/bin/perl -w

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

use IO::File;


@opts_s = ('-O1', '-Wall',
	   '-Igensrc', '-Isrc',
	   # '-g',
	   # '-DGC_DEBUG',
	   split(/\s+/, `pkg-config gtk+-2.0 --cflags`));

gcc('src/region.c', 'obj/region.o', @opts_s);
gcc('src/serialise.c', 'obj/serialise.o', @opts_s);
gcc('src/serialise-utils.c', 'obj/serialise-utils.o', @opts_s);
gcc('src/comms.c', 'obj/comms.o', @opts_s,
    '-DHALF_NAME="server"');
gcc('src/cap-protocol.c', 'obj/cap-protocol.o', '-DPLASH_GLIB', @opts_s);
gcc('src/cap-call-return.c', 'obj/cap-call-return.o', @opts_s);
gcc('src/cap-utils.c', 'obj/cap-utils.o', @opts_s);
gcc('src/cap-utils-libc.c', 'obj/cap-utils-libc.o', @opts_s);
gcc('src/marshal-exec.c', 'obj/marshal-exec.o', @opts_s);
gcc('src/utils.c', 'obj/utils.o', @opts_s);
gcc('src/parse-filename.c', 'obj/parse-filename.o', @opts_s);
gcc('src/filesysobj.c', 'obj/filesysobj.o', @opts_s);
gcc('src/filesysobj-real.c', 'obj/filesysobj-real.o', @opts_s);
gcc('src/filesysobj-fab.c', 'obj/filesysobj-fab.o', @opts_s);
gcc('src/filesysobj-readonly.c', 'obj/filesysobj-readonly.o', @opts_s);
gcc('src/filesysobj-union.c', 'obj/filesysobj-union.o', @opts_s);
gcc('src/filesysslot.c', 'obj/filesysslot.o', @opts_s);
gcc('src/log-proxy.c', 'obj/log-proxy.o', @opts_s);
gcc('src/reconnectable-obj.c', 'obj/reconnectable-obj.o', @opts_s);
gcc('src/build-fs.c', 'obj/build-fs.o', @opts_s);
gcc('src/build-fs-static.c', 'obj/build-fs-static.o', @opts_s);
gcc('src/build-fs-dynamic.c', 'obj/build-fs-dynamic.o', @opts_s);
gcc('src/shell.c', 'obj/shell.o', @opts_s);
gcc('src/shell-parse.c', 'obj/shell-parse.o', @opts_s,
    '-Wno-unused');
gcc('src/shell-variants.c', 'obj/shell-variants.o', @opts_s);
gcc('src/shell-globbing.c', 'obj/shell-globbing.o', @opts_s);
gcc('src/shell-fds.c', 'obj/shell-fds.o', @opts_s);
gcc('src/shell-wait.c', 'obj/shell-wait.o', @opts_s);
gcc('src/shell-options.c', 'obj/shell-options.o', @opts_s);
gcc('src/resolve-filename.c', 'obj/resolve-filename.o', @opts_s);
gcc('src/fs-operations.c', 'obj/fs-operations.o', @opts_s);
gcc('src/powerbox.c', 'obj/powerbox.o', @opts_s);

# Non-library code
gcc('src/test-caps.c', 'obj/test-caps.o', @opts_s);
gcc('src/shell-options.c', 'obj/shell-options.o', @opts_s);
gcc('src/shell-options-gtk.c', 'obj/shell-options-gtk.o', @opts_s);
gcc('src/shell-options-cmd.c', 'obj/shell-options-cmd.o', @opts_s);
gcc('src/chroot.c', 'obj/chroot.o', @opts_s);
gcc('src/exec-object.c', 'obj/exec-object.o', @opts_s);
gcc('src/socket-publish.c', 'obj/socket-publish.o', @opts_s);
gcc('src/socket-connect.c', 'obj/socket-connect.o', @opts_s);
gcc('src/run-emacs.c', 'obj/run-emacs.o', @opts_s);
gcc('src/pola-run.c', 'obj/pola-run.o', @opts_s);
gcc('src/powerbox-req.c', 'obj/powerbox-req.o', @opts_s);



# Build object files to be linked into libc.so and ld.so (ld-linux.so)

my @opts_c = ('-Wall', '-nostdlib',
	      '-Igensrc', '-Isrc',
	      # '-g',
	      '-D_REENTRANT', '-fPIC');

gcc('src/comms.c', 'obj/comms.os', @opts_c,
    '-DHALF_NAME="client"');
gcc('src/comms.c', 'obj/rtld-comms.os', @opts_c,
    '-DHALF_NAME="client"', '-DIN_RTLD');
gcc('src/region.c', 'obj/region.os', @opts_c);
gcc('src/serialise.c', 'obj/serialise.os', @opts_c);
gcc('src/cap-protocol.c', 'obj/cap-protocol.os', @opts_c,
    '-DIN_LIBC');
gcc('src/cap-protocol.c', 'obj/rtld-cap-protocol.os', @opts_c,
    '-DIN_RTLD');
gcc('src/cap-call-return.c', 'obj/cap-call-return.os', @opts_c);
gcc('src/cap-utils.c', 'obj/cap-utils.os', @opts_c);
gcc('src/dont-free.c', 'obj/dont-free.os', @opts_c);
gcc('src/filesysobj.c', 'obj/filesysobj.os', @opts_c);
gcc('src/libc-misc.c', 'obj/libc-misc.os', @opts_c,
    '-DIN_LIBC');
gcc('src/libc-misc.c', 'obj/rtld-libc-misc.os', @opts_c,
    '-DIN_RTLD');
gcc('src/libc-comms.c', 'obj/libc-comms.os', @opts_c);
gcc('src/libc-fork-exec.c', 'obj/libc-fork-exec.os', @opts_c);
gcc('src/libc-connect.c', 'obj/libc-connect.os', @opts_c);
gcc('src/libc-getuid.c', 'obj/libc-getuid.os', @opts_c);
gcc('src/libc-utime.c', 'obj/libc-utime.os', @opts_c);
gcc('src/libc-truncate.c', 'obj/libc-truncate.os', @opts_c);



# Build powerbox for Gtk

my @opts_gtk_pb = ('-Igensrc', '-Isrc',
		   '-Wall',
		   '-fPIC',
		   split(/\s+/, `pkg-config gtk+-2.0 --cflags`));

gcc('src/gtk-powerbox.c', 'obj/gtk-powerbox.os', @opts_gtk_pb);



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
    do_cmd('gcc-3.3', @opts,
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
