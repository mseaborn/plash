#!/usr/bin/perl -w

# Make a source package from a Subversion working copy.
# This means including added files, leaving out deleted files.
# Allows us to test the source package before checking in.


if(scalar(@ARGV) != 1) { die "Usage: $0 <dest-dir>" }
my $dest = $ARGV[0];

my $files = {};

# It would be nice to use the SVN API instead of parsing the output.
# But can't find documentation yet.

# Actually, we can do all of this using "svn export" on a working copy.
# Replace this...

my @lines = `svn status -v`;
foreach my $line (@lines) {
  chomp($line);
  if($line =~ /^\?/) {}
  elsif($line =~ /^ (.{8}) \s* (\S+) \s* (\S+) \s* (\S+) \s* (\S+) $/x) {
    my ($status, $file) = ($1, $5);
    if($status =~ /^[ AMR]/) {
      $files->{$file} = 1;
    }
    elsif($status =~ /^[DI]/) {
      # Ignore
    }
    else {
      die "Unknown status: $line"
    }
  }
  else { die "Unknown line: $line" }
}


my @hide =
  ('make-source-package.pl',
   glob('web-site/screenshot-*.png'));

foreach my $f (@hide) {
  if($files->{$f}) { delete $files->{$f} }
  else { warn "file $f not present" }
}


mkdir($dest);

foreach my $file (sort(keys(%$files))) {
  if(!-l $file && -d $file) {
    run_cmd('mkdir', '-p', "$dest/$file");
  }
  else {
    run_cmd('cp', '-au', $file, "$dest/$file");
  }
}

run_cmd("cd $dest && autoconf && rm -r autom4te.cache");

run_cmd("cd $dest/docs && ./make-man-pages.pl && rm -r temp");

run_cmd("cd $dest/web-site && ./make.pl");


# Print the added files
foreach my $f (`cd $dest && find`) {
  chomp($f);
  if($f eq '.') {}
  elsif($f =~ /^\.\/(.*)$/) {
    my $f = $1;
    if(!$files->{$f}) {
      print "added $f\n"
    }
  }
  else { die }
}


sub run_cmd {
  my $rc = system(@_);
  if($rc != 0) {
    print join(' ', @_)."\n";
    die "Return code $rc";
  }
}
