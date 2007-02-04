#!/usr/bin/perl -w

use IO::File;
use Unpack;
use PkgConfig;


if(scalar(@ARGV) < 2) {
  print "Usage: $0 <output-dir> <dependency>...\n";
  exit(1);
}
my $package_list = PkgConfig::package_list_combined();
my $output_dir = shift(@ARGV);
my @top_deps = @ARGV;


sub init_deplist {
  { # Input:
    # packages indexes by name:
    Packages => {},
    # Output:
    List => [],
    Hash => {},
    Degree => {},
    Lacking => [],
    Log_lines => [],
  };
}

# Satisfy dependencies depth-first
sub search_deps {
  my ($deplist, $degree, $name) = @_;
  
  my $put_log = sub {
    my ($log_line) = @_;
    my $indent = '  ' x $degree;
    push(@{$deplist->{Log_lines}}, $indent.$log_line);
  };

  # Check whether package is available
  my $package = $deplist->{Packages}{$name};
  if(!defined $package) {
    &$put_log("$name NOT AVAILABLE");
    push(@{$deplist->{Lacking}}, $name);
  }
  else {
    if($deplist->{Hash}{$name}) {
      # Already added
      &$put_log("($name $package->{version})");
      if($deplist->{Degree}{$name} > $degree) {
	$deplist->{Degree}{$name} = $degree;
      }
    }
    else {
      &$put_log("$name $package->{version}");
      
      # Mark the package as visited
      # It gets added to the list later
      $deplist->{Hash}{$name} = 1;
      $deplist->{Degree}{$name} = $degree;
      
      # Process further dependencies
      foreach my $field ('depends', 'pre-depends') {
	if(defined($package->{$field})) {
	  process_depends_field($deplist, $degree + 1, $package->{$field});
	}
      }
      
      # Add package to list
      # We do this last so that the list is topologically sorted,
      # with dependencies first
      push(@{$deplist->{List}}, $package);
    }
  }
}

sub process_depends_field {
  my ($deplist, $degree, $dep_line) = @_;
  
  foreach my $dep (Unpack::split_dep_list($dep_line)) {
    my @or = Unpack::split_disjunction($dep);
    
    # Only look at first dependency in disjunction
    foreach my $d ($or[0]) {
      my $dep = Unpack::parse_dep($d);
      search_deps($deplist, $degree, $dep->{Name});
    }
  }
}

sub add_essential_packages {
  my ($deplist, $packages) = @_;
  
  # Add packages marked as Essential, since other packages are not
  # required to declare dependencies on them
  push(@{$deplist->{Log_lines}}, "essential packages:");
  foreach my $p (@$packages) {
    if(defined $p->{essential} && $p->{essential} eq 'yes') {
      search_deps($deplist, 1, $p->{package});
    }
  }
}

sub warn_missing_dependencies {
  my ($deplist) = @_;
  
  if(scalar(@{$deplist->{Lacking}}) > 0) {
    print "Missing packages:\n";
    foreach my $p (@{$deplist->{Lacking}}) {
      print "$p\n";
    }
  }
}

sub write_result_package_list {
  my ($deplist, $file) = @_;
  
  $f = IO::File->new($file, 'w') || die "Can't open \"$file\"";
  foreach my $p (@{$deplist->{List}}) {
    foreach my $field (qw(package version size md5sum filename base-url)) {
      die "Field $field missing" if !defined $p->{$field};
      printf $f "%s: %s\n", ucfirst($field), $p->{$field};
    }
    print $f "\n";
  }
  $f->close();
}

# Output dependencies, ordered by degree of separation
sub list_by_degree {
  my ($deplist) = @_;
  
  my $out = '';
  foreach my $dep (sort { $deplist->{Degree}{$a->{package}} <=>
			  $deplist->{Degree}{$b->{package}} }
		   @{$deplist->{List}}) {
    $out .= "$deplist->{Degree}{$dep->{package}} $dep->{package}\n";
  }
  return $out;
}


my $deplist = init_deplist();

# List of fields to keep from Packages file
my @fields = qw(package version
		filename size md5sum sha1 base-url
		depends pre-depends essential);
my $packages = Unpack::read_control_file($package_list, \@fields);

# Index by package name
$deplist->{Packages} = { map { ($_->{package} => $_) } @$packages };

foreach my $dep (@top_deps) {
  process_depends_field($deplist, 0, $dep);
}
add_essential_packages($deplist, $packages);

if(!-e $output_dir) {
  mkdir($output_dir) || die "Can't create directory \"$output_dir\": $!";
}

warn_missing_dependencies($deplist);
write_result_package_list($deplist, "$output_dir/package-list");

write_file("$output_dir/choose-log",
	   join('', map { "$_\n" } @{$deplist->{Log_lines}}));
write_file("$output_dir/degrees",
	   list_by_degree($deplist));


# Output topologically-sorted list of packages
#$f = IO::File->new("$output_dir/package-list", 'w') || die;
#foreach my $p (@{$deplist->{List}}) {
#  print $f "$p->{package}_$p->{version}\n";
#}
#$f->close();


sub write_file {
  my ($file, $data) = @_;
  my $f = IO::File->new($file, 'w');
  if(!defined $f) { die "Can't open `$file' for writing" }
  print $f $data;
  $f->close();
}
