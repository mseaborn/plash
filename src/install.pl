#!/usr/bin/perl -w

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


# This program installs the libc libraries (not including ld.so/ld-linux.so).
# They come from different places and are given different version numbers.

my $libs = [
  ['shobj/libc.so', 'libc.so.6'],
  ['shobj/libpthread.so', 'libpthread.so.0'],
  ['glibc/math/libm.so', 'libm.so.6'],
  ['glibc/crypt/libcrypt.so', 'libcrypt.so.1'],
  ['glibc/dlfcn/libdl.so', 'libdl.so.2'],
  ['glibc/nis/libnsl.so', 'libnsl.so.1'],
  ['glibc/nis/libnss_compat.so', 'libnss_compat.so.2'],
  ['glibc/nis/libnss_nisplus.so', 'libnss_nisplus.so.2'],
  ['glibc/nis/libnss_nis.so', 'libnss_nis.so.2'],
  ['glibc/nss/libnss_files.so', 'libnss_files.so.2'],
  ['glibc/hesiod/libnss_hesiod.so', 'libnss_hesiod.so.2'],
  ['glibc/resolv/libresolv.so', 'libresolv.so.2'],
  ['glibc/resolv/libnss_dns.so', 'libnss_dns.so.2'],
  ['glibc/resolv/libanl.so', 'libanl.so.1'],
  ['glibc/rt/librt.so', 'librt.so.1'],
  ['glibc/login/libutil.so', 'libutil.so.1'],
  ['glibc/locale/libBrokenLocale.so', 'libBrokenLocale.so.1'],
  # These don't have a number in their installed filename:
  ['glibc/malloc/libmemusage.so', 'libmemusage.so'],
  ['glibc/debug/libSegFault.so', 'libSegFault.so'],
  ['glibc/debug/libpcprofile.so', 'libpcprofile.so'],
];

if(scalar(@ARGV) == 2 && $ARGV[0] eq '--dest-dir') {
  my $dest_dir = $ARGV[1];

  foreach my $x (@$libs) {
    do_cmd('strip',
	   '--remove-section=.comment',
	   '--remove-section=.note',
	   $x->[0], '-o', "$dest_dir/$x->[1]");
  }

  # do_cmd('strip', 'mrs/ld.so', '-o', "$dir/ld-linux.so.2");
  # do_cmd('chmod', '+x', "$dir/ld-linux.so.2");
}
elsif(scalar(@ARGV) == 1 && $ARGV[0] eq '--local-cp') {
  my $dest_dir = `. src/config.sh && echo \$LIB_INSTALL`;
  chomp($dest_dir);
  if($dest_dir eq '') { die }
  print "Installing into `$dest_dir'\n";

  # If you just copy to the destination file, it overwrites the
  # destination file which keeps the same inode, and is likely being
  # used by other processes, which will crash.  If you rename a file
  # on top of the destination file, the destination file is unlinked
  # but can still exist; the new file has a new inode.
  foreach my $x (@$libs) {
    do_cmd('cp', $x->[0], "$dest_dir/$x->[1].new");
    rename("$dest_dir/$x->[1].new", "$dest_dir/$x->[1]") || die "Can't rename";
  }
  print "done\n";
}
else {
  print "Usage: install.pl --dest-dir DIR\n";
  print "or:    install.pl --local-cp\n";
  die;
}


sub do_cmd {
  my @args = @_;
  print join(' ', @args)."\n";
  my $rc = system(@args);
  if($rc != 0) { die "Return code $rc" }
}
