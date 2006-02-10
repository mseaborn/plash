#!/usr/bin/perl

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
