#!/usr/bin/perl -w

# Usage: compare.pl file1 file2...
# Outputs a table (in CSV format) showing the test results that differ
# between files.

use IO::File;


my @files = map { [read_file($_)] } @ARGV;

# list of all tests seen
my @tests = dedup(map { $_->[0] } map { @$_ } @files);

my @files_hash = map { { map { ($_->[0] => $_->[1]) } @$_ } } @files;

print join(',', 'test', @ARGV), "\n";
foreach my $test (@tests) {
  my @rs = map { $_->{$test} } @files_hash;
  if(!equal(@rs)) {
    print join(',', $test, @rs), "\n";
  }
}


sub read_file {
  my ($file) = @_;
  my $f = IO::File->new($file, 'r') || die "Can't open '$file'";
  my @lines = <$f>;
  chomp(@lines);
  $f->close();
  map { [split(/:\s*/, $_)] } @lines
}

sub dedup {
  my @x;
  my $seen = {};
  foreach(@_) {
    if(!$seen->{$_}) {
      push(@x, $_);
      $seen->{$_} = 1;
    }
  }
  @x
}

sub equal {
  if(scalar(@_) > 0) {
    my $x = shift;
    foreach(@_) { if($_ ne $x) { return 0 } }
  }
  return 1;
}
