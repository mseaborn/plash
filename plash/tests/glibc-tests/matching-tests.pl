#!/usr/bin/perl -w

# Lists tests that had the given result.

if(scalar(@ARGV) != 1) {
  die "Usage: $0 result-type < results.log";
}
my $result = $ARGV[0];

my $line;
while(defined($line = <STDIN>)) {
  chomp($line);
  if($line =~ /^\*\* (\S+) (\S+)$/) {
    if($2 eq $result) {
      print "$1\n";
    }
  }
}
