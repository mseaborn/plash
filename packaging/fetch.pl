#!/usr/bin/perl -w

# Takes a package list and downloads the .debs it contains.

use IO::File;
use Unpack;


if(scalar(@ARGV) != 1) {
  print "Usage: $0 <package-list>\n";
  exit(1);
}
my $package_list = $ARGV[0];


my $list = read_packages_list($package_list);

my $total_size = 0;
my $remaining_size = 0;
my $to_get = 0;

# First pass: what is already available?
foreach my $p (@$list) {
  $total_size += $p->{size};
  
  $p->{filename} =~ /\/([^\/]+)$/ || die;
  my $leafname = $1;
  
  $p->{local_file} = "/var/cache/apt/archives/$leafname";
  if(-e $p->{local_file}) {
    $p->{done} = 1;
  }

  if(!$p->{done}) {
    $p->{local_file} = "cache/$leafname";
    if(-e $p->{local_file}) {
      $p->{done} = 1;
    }
  }

  if(!$p->{done}) {
    $to_get++;
    $remaining_size += $p->{size};
    printf "%10.1fMb  %s %s\n",
      $p->{size} / (1024 * 1024),
      $p->{package}, $p->{version};
    
    mkdir('cache');
    if(!defined $p->{'base-url'}) {
      die "Base-URL field missing: can't make URL for package";
    }
    if(!defined $p->{'filename'}) {
      die "Base-URL field missing: can't make URL for package";
    }
    $p->{url} = Unpack::join_with_slash($p->{'base-url'}, $p->{'filename'});
  }
}
printf "%i of %i packages to get: %.1fMb of %.1fMb\n",
  $to_get, scalar(@$list),
  $remaining_size / (1024 * 1024),
  $total_size / (1024 * 1024);

if($to_get > 0) {
  print "download? [Yn] ";
  my $reply = <STDIN>;
  chomp($reply);
  if($reply =~ /^y?$/i) {
    foreach my $p (@$list) {
      if(!$p->{done}) {
	printf "getting %s %s\n", $p->{package}, $p->{version};
	run_cmd('curl', $p->{url},
		'-o', $p->{local_file});
      }
    }
  }
}


sub read_packages_list {
  my ($file) = @_;
  
  # List of fields to keep from Packages file
  my @fields = qw(package version size md5sum filename base-url);
  my $fields = { map { ($_ => 1) } @fields };
  
  my $list = [];
  my $f = IO::File->new($file, 'r')
    || die "Can't open \"$file\"";
  while(1) {
    my $block = Unpack::get_block($f);
    if($block eq '') { last }
    
    push(@$list, Unpack::block_fields($block, $fields));
  }
  $f->close();
  return $list;
}

sub run_cmd {
  print join(' ', @_)."\n";
  my $rc = system(@_);
  if($rc != 0) {
    die "Return code $rc";
  }
}
