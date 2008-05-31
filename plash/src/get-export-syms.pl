#!/usr/bin/perl -w

foreach my $line (`nm --format=posix --extern-only $ARGV[0]`) {
  chomp($line);
  $line =~ /^(\S+)/i || die "Bad line: $line";
  my $sym = $1;
  if($sym =~ /^export(ver)?_(.*)$/) {
    print "$sym\n";
  }
}
