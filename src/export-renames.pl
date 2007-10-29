#!/usr/bin/perl -w

use IO::File;


my $for_rtld = 0;
if(scalar(@ARGV) == 1 && $ARGV[0] eq '--rtld') {
  $for_rtld = 1;
}


# Read glibc's generated abi-versions.h in order to get a mapping of
# version symbols, e.g.
# #define VERSION_libc_GLIBC_2_1_1   GLIBC_2.1.1
#
# Some non-i386 architectures don't provide all version symbols.
# For example, x86-64 glibc doesn't provide GLIBC_2.0, because that
# predated glibc being ported to x86-64.
# GLIBC_2.0 gets mapped to GLIBC_2.2.5 instead.
#
# When building glibc normally, the preprocessor performs this mapping
# for us.  The concatenation "VERSION_##lib##_##version" gives us (for
# example) "VERSION_libc_GLIBC_2_1_1" (see glibc's shlib-compat.h).
# This would normally be expanded to "GLIBC_2.1.1".  However, in our
# case, it is then concatenated again:
# "exportver_##name##_version_##version".  The preprocessor does not
# expand at every step, so the expansion to "GLIBC_2.1.1" does not
# happen.  Even if the expansion did happen, it could not give a
# single token containing dots.  This all means that we have to do the
# expansion outside of the preprocessor.

my $version_map = {};

if(!$for_rtld) {
  my $glibc_dir = `. src/config.sh && echo \$GLIBC`;
  chomp($glibc_dir);
  if($glibc_dir eq '') { die }

  my $file = "$glibc_dir/abi-versions.h";
  my $f = IO::File->new($file, 'r') || die "Can't open \"$file\"";
  my $line;
  while(defined($line = <$f>)) {
    if($line =~ /^#define (VERSION_\S+)\s+(\S+)\s*$/) {
      $version_map->{$1} = $2;
    }
  }
  $f->close();
}

sub get_version_symbol {
  my ($sym) = @_;

  my $x = $version_map->{$sym};
  if(!defined $x) { die "Can't find version symbol \"$sym\"" }
  $x
}


my $sym;
while(defined($sym = <STDIN>)) {
  chomp($sym);
  if($sym =~ /^export_(.+)$/) {
    print "--redefine-sym $sym=$1\n";
  }
  elsif($sym =~ /^exportver_(.+)__defaultversion__(.+)$/) {
    my $name = $1;
    if(!$for_rtld) {
      my $ver = get_version_symbol($2);
      print "--redefine-sym $sym=$name\@\@$ver\n";
    }
    else {
      # The default symbol version can be linked to ld.so.
      print "--redefine-sym $sym=$name\n";
    }
  }
  elsif($sym =~ /^exportver_(.+)__version__(.+)$/) {
    my $name = $1;
    if(!$for_rtld) {
      my $ver = get_version_symbol($2);
      print "--redefine-sym $sym=$name\@$ver\n";
    }
    else {
      # Old symbol versions are not linked to ld.so.
    }
  }
  else { die }
}
