#!/usr/bin/perl -w

# Get the Packages.gz files for the distributions listed in sources.list,
# and combine them into a single Packages file.  Adds a Base-URL field
# to each package entry in the output.

use Unpack;
use PkgConfig;


my $output_dir = PkgConfig::package_list_cache_dir();
my $output_file = PkgConfig::package_list_combined();


my $list = Unpack::read_sources_list('sources.list');

foreach my $src (@$list) {
  $src->{S_file} =
    Unpack::fetch_packages_file($src, $output_dir, PkgConfig::get_arch());
}


my $output = IO::File->new($output_file, 'w') || die "Can't open $output_file";
foreach my $src (@$list) {
  my $file = $src->{S_file};
  my $f = IO::File->new("zcat $file |") || die "Can't open $file";
  while(1) {
    my $block = Unpack::get_block($f);
    if($block eq '') {
      last;
    }
    
    # Remove the Description field to save space.
    $block =~ s/^Description:.*\n(\s+.*)*(\n|\Z)//im;
    
    print $output "Base-URL: $src->{S_base}\n";
    print $output "$block\n";
  }
  $f->close();
}
$output->close();
