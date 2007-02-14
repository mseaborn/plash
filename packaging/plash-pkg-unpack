#!/usr/bin/perl -w

# Takes a package list and unpacks all the packages into the given
# destination directory.  It actually unpacks the packages into a
# cache directory and then hard links them to the destination.

use IO::File;
use Unpack;
use PkgConfig;


if(scalar(@ARGV) != 2) {
  print "Usage: $0 <package-list> <dest-dir>\n";
  exit(1);
}
my $package_list = $ARGV[0];
my $dest_dir = $ARGV[1];
my $unpack_cache = PkgConfig::unpack_cache_dir();

my @fields = qw(package version filename);
my $list = Unpack::read_control_file($package_list, \@fields);


# unpack in separate dirs
foreach my $p (@$list) {
  $p->{filename} =~ /\/([^\/]+)$/ || die;
  my $leafname = $1;
  my $deb_file = "cache/$leafname";
  if(!-e $deb_file) {
    die "File not present: $deb_file";
  }

  die if !defined $p->{package};
  die if !defined $p->{version};

  my $out_dir = "$unpack_cache/$p->{package}_$p->{version}";
  PkgConfig::ensure_dir_exists($out_dir);

  my $dest;

  # Extract main files -- data.tar.gz from the .deb
  $dest = "$out_dir/data";
  if(!-e $dest) {
    if(-e "$dest.tmp") { die "Remove $dest.tmp and try again" }
    run_cmd('dpkg-deb', '-x', $deb_file, "$dest.tmp");
    rename("$dest.tmp", $dest) || die "Can't rename $dest into place";
  }

  # Extract control files -- control.tar.gz from the .deb
  $dest = "$out_dir/control";
  if(!-e $dest) {
    if(-e "$dest.tmp") { die "Remove $dest.tmp and try again" }
    run_cmd('dpkg-deb', '-e', $deb_file, "$dest.tmp");
    rename("$dest.tmp", $dest) || die "Can't rename $dest into place";
  }
}

# unpack in single dir
mkdir($dest_dir) || die "Can't create directory \"$dest_dir\": $!";
foreach my $p (@$list) {
  my $src = "$unpack_cache/$p->{package}_$p->{version}/data";
  
  foreach my $f (glob("$src/*")) {
    run_cmd('cp', '-lr', $f, $dest_dir);
  }
}

# TO BE REMOVED/MOVED:
# Populate dpkg status dir
run_cmd('mkdir', '-p', "$dest_dir/var/lib/dpkg");
my $f = IO::File->new("$dest_dir/var/lib/dpkg/status", 'w') || die;
foreach my $p (@$list) {
  print $f
    "Package: $p->{package}\n".
    "Version: $p->{version}\n".
    "Status: install ok installed\n\n";
}
$f->close();


sub run_cmd {
  my $rc = system(@_);
  if($rc != 0) {
    print join(' ', @_)."\n";
    die "Return code $rc";
  }
}
