#!/usr/bin/perl -w

# Copyright (C) 2004 Mark Seaborn
#
# This file is part of Plash.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
# USA.


# Show what symbols have been removed from and added to glibc.
# Show what symbols the code I've added uses.

my $added = {};
my $removed = {};

print "The following symbols are resolved from outside:\n";
foreach my $x (@{nm('obj/combined.os')}) {
  if($x->{Type} eq 'U') { print "  $x->{Name}\n"; }
  if($x->{Type} =~ /^[TW]$/) {
    $added->{$x->{Name}} = 1;
  }
}

foreach my $x (map { @{nm("glibc/$_")} } @ARGV) {
  if($x->{Type} =~ /^[TW]$/) {
    $removed->{$x->{Name}} = 1;
  }
}

print "The following symbols have been removed from glibc but not added:\n";
foreach my $n (sort(keys(%$removed))) {
  if(!$added->{$n}) {
    print "  $n\n";
  }
}

print "The following symbols have been added to glibc but were not in the original:\n";
foreach my $n (sort(keys(%$added))) {
  if(!$removed->{$n}) {
    print "  $n\n";
  }
}


sub nm {
  my ($file) = @_;
  my @lines = split(/\n/, `nm $file`);
  my @got;
  foreach my $line (@lines) {
    next if($line =~ /^\s+$/);
    if($line =~ /^([a-zA-Z0-9 ]{8})\s+([A-Za-z?])\s+/) {
      push(@got, { Addr => $1, Type => $2, Name => $' });
    }
    else {
      print "Unrecognised line: `$line'\n";
    }
  }
  \@got
}
