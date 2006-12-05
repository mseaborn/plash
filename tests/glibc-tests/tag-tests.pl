#!/usr/bin/perl -w

# Usage:
# tag-tests.pl <result> <tag-file>
#
# eg.
# tag-tests.pl add ignore $(matching-tests.pl failed <results.log)
#   -- Adds tests with a result of "failed" to the "ignore" list
# tag-tests.pl remove ignore-plash $(matching-tests.pl ok <results.log)
#   -- Remove tests with a result of "ok" from the "ignore-plash" list

use IO::File;


if(scalar(@ARGV) < 2) {
  die "Usage: $0 <add|remove> <tag-file> <test-name>...";
}
my $act = shift;
my $file = shift;
my @names = @ARGV;

my $f = IO::File->new($file, 'r') || die "Can't open $file";
my @lines = <$f>;
chomp(@lines);
my $lines_map = { map { ($_ => 1) } @lines };

if($act eq 'add') {
  foreach my $name (@names) {
    if(!$lines_map->{$name}) {
      push(@lines, $name);
      print "added $name\n";
    }
  }
}
elsif($act eq 'remove') {
  foreach my $name (@names) {
    if($lines_map->{$name}) {
      @lines = grep { $_ ne $name } @lines;
      print "removed $name\n";
    }
  }
}
else {
  die "Unknown action: $act";
}

write_file($file, join('', map { "$_\n" } @lines));


sub write_file {
  my ($file, $data) = @_;
  my $f = IO::File->new($file, 'w');
  if(!defined $f) { die "Can't open `$file' for writing" }
  print $f $data;
  $f->close();
}
