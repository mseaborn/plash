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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.


die if scalar(@ARGV) != 1;

# Modifies the permissions flag for one of the sections.
# Usually it's "rx".  This sets it to "rwx".

open(FILE, "+<$ARGV[0]") || die "Can't open: $!";

my $x;
seek(FILE, 0x4c, 0);
read(FILE, $x, 4);
if($x eq pack('i', 7)) { die "Has already been modified" }
if($x ne pack('i', 5)) { die "Unexpected contents" }

seek(FILE, 0x4c, 0);
print FILE pack('i', 7);

close(FILE);
