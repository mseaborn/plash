#!/usr/bin/perl -w

foreach my $line (`nm $ARGV[0]`) {
  chomp($line);
  $line =~ /^[0-9a-f ]{8} . (\S+)$/i || die "Bad line: $line";
  my $sym = $1;
  if($sym =~ /^export_(.*)$/) {
    print "$sym\n";
  }
}
