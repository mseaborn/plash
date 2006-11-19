#!/usr/bin/perl -w

use IO::File;


my $for_rtld = 0;
if(scalar(@ARGV) == 1 && $ARGV[0] eq '--rtld') {
  $for_rtld = 1;
}


my $glibc_dir = `. src/config.sh && echo \$GLIBC`;
chomp($glibc_dir);
if($glibc_dir eq '') { die }

my $version_map = {};

my $file = "$glibc_dir/abi-versions.h";
my $f = IO::File->new($file, 'r') || die "Can't open \"$file\"";
my $line;
while(defined($line = <$f>)) {
  if($line =~ /^#define (VERSION_\S+)\s+(\S+)\s*$/) {
    $version_map->{$1} = $2;
  }
}
$f->close();

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
    my $ver = get_version_symbol($2);
    if(!$for_rtld) {
      print "--redefine-sym $sym=$name\@\@$ver\n";
    }
    else {
      # The default symbol version can be linked to ld.so.
      print "--redefine-sym $sym=$name\n";
    }
  }
  elsif($sym =~ /^exportver_(.+)__version__(.+)$/) {
    my $name = $1;
    my $ver = get_version_symbol($2);
    if(!$for_rtld) {
      print "--redefine-sym $sym=$name\@$ver\n";
    }
    else {
      # Old symbol versions are not linked to ld.so.
    }
  }
  else { die }
}
