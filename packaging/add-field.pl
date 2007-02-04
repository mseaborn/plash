#!/usr/bin/perl -w

# This is a tool for adding a field to all blocks in a Packages file.
#
# Usage:
# zcat Packages.gz |
#   add-field.pl "Base-URL: http://localhost:9999/debian/" > Packages-new

use Unpack;

if(scalar(@ARGV) != 1) {
  die "Usage: $0 field <input >output";
}

while(1) {
  my $block = Unpack::get_block(STDIN);
  if($block eq '') {
    last;
  }

  # Remove the Description field to save space.
  $block =~ s/^Description:.*\n(\s+.*)*(\n|\Z)//im;
  
  print "$ARGV[0]\n$block\n";
}
