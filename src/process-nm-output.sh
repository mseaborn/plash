#!/usr/bin/perl

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


my @lines;
my $line;
while(defined($line = <>)) {
  if($line =~ /^[a-zA-Z0-9]{8}(\s+[A-Za-z]\s+)/) {
    push(@lines, (' 'x8).$1.$');
  }
  else {
    push(@lines, $line);
  }
}

foreach my $x (sort @lines) { print $x; }
print "(".scalar(@lines).")\n";
