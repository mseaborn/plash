#!/usr/bin/perl -w

foreach my $sym (<>) {
  chomp($sym);
  if($sym =~ /^export_(\S+)$/) {
    print "--redefine-sym export_$1=$1\n";
  }
  else { die }
}
