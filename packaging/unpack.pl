#!/usr/bin/perl -w

use IO::File;
use Unpack;


if(scalar(@ARGV) != 2) {
  print "Usage: $0 <package-list> <package-name>\n";
  exit(1);
}
my $package_list = $ARGV[0];
my $package = $ARGV[1];


my $server = 'http://localhost:9999/debian'; # approx proxy

my $verbose = 1;
my $do_unpack = 1;
my $dest_dir = "$package/unpacked";

my $do_download = 1;
my $skip_not_found = 1;

# To be removed:
my @base = ('bash',
	    'coreutils', # for /usr/bin/id
	    'sed',
	    # 'xlibs-data',
	    );

my $remove = { #'libc6' => 1, # need to replace to provide /usr/lib/gconv...
	       #'base-files' => 1,
	     };


sub read_packages_list {
  my ($file) = @_;
  
  # List of fields to keep from Packages file
  my @fields = qw(package version
		  filename size md5sum sha1
		  depends pre-depends essential);
  my $fields = { map { ($_ => 1) } @fields };
  
  my $list = [];
  my $f = IO::File->new($file, 'r')
    || die "Can't open \"$file\"";
  while(1) {
    my $block = Unpack::get_block($f);
    if($block eq '') { last }
    
    my $x = $block."\n";
    my $package = {};
    while($x ne '') {
      $x =~ s/^(\S+?):\s*(.*(\n\s+.*)*)(\n|\Z)// || die "Bad data: $block";
      my $key = lc($1);
      my $data = $2;
      if($fields->{$key}) {
	$package->{$key} = $data;
      }
    }
    push(@$list, $package);
  }
  $f->close();
  return $list;
}


print "reading package list...\n";
my $packages = read_packages_list($package_list);

my $name_idx = { map { ($_->{package} => $_) } @$packages };


sub split_dep_list {
  my ($d) = @_;
  map { trim($_) } split(/,/, $d);
}
sub split_disjunction {
  my ($d) = @_;
  map { trim($_) } split(/\|/, $d)
}
sub parse_dep {
  my ($d) = @_;
  $d =~ /^(\S+)(\s*\([^\)]*\))?$/
    || die "Unrecognised dependency format: $d";
  { Name => $1 }
}

sub search_deps {
  my ($deplist, $degree, $name) = @_;
  my $indent = '  ' x $degree;

  # Special case packages
  if($remove->{$name}) {
    if($verbose) { print $indent."$name SKIPPED\n"; }
    return;
  }

  # Check whether package is available
  my $package = $name_idx->{$name};
  if(!defined $package) {
    if($verbose) { print $indent."$name NOT AVAILABLE\n"; }
    push(@{$deplist->{Lacking}}, $name);
    return;
  }

  if($deplist->{Hash}{$name}) {
    # Already added
    if($verbose) { print $indent."($name $package->{version})\n"; }
    if($deplist->{Degree}{$name} > $degree) {
      $deplist->{Degree}{$name} = $degree;
    }
    return;
  }
  if($verbose) { print $indent."$name $package->{version}\n"; }
  
  # Mark the package as visited
  # It gets added to the list later
  $deplist->{Hash}{$name} = 1;
  $deplist->{Degree}{$name} = $degree;

  # Process further dependencies
  my @deps;
  if(defined($package->{depends})) {
    push(@deps, split_dep_list($package->{depends}));
  }
  if(defined($package->{'pre-depends'})) {
    push(@deps, split_dep_list($package->{'pre-depends'}));
  }
  foreach my $dep (@deps) {
    my @or = split_disjunction($dep);
    
    # Only look at first dependency in disjunction
    foreach my $d ($or[0]) {
      my $dep = parse_dep($d);
      search_deps($deplist, $degree + 1, $dep->{Name});
    }
  }

  # Add package to list
  # We do this last so that the list is topologically sorted,
  # with dependencies first
  push(@{$deplist->{List}}, $package);
}


my $deplist =
  { List => [],
    Hash => {},
    Degree => {},
    Lacking => [],
  };

# Add packages marked as Essential, since other packages are not
# required to declare dependencies on them
foreach my $p (@$packages) {
  if(defined $p->{essential} && $p->{essential} eq 'yes') {
    print "essential package: $p->{package}\n";
    search_deps($deplist, 0, $p->{package});
  }
}
foreach my $p (@base) {
  # can be removed now:
  search_deps($deplist, 0, $p);
}
search_deps($deplist, 0, $package);

# Print dependencies, ordered by degree of separation
foreach my $dep (sort { $deplist->{Degree}{$a->{package}} <=>
			$deplist->{Degree}{$b->{package}} }
		 @{$deplist->{List}}) {
  print "$deplist->{Degree}{$dep->{package}} $dep->{package}\n";
}

if(scalar(@{$deplist->{Lacking}}) > 0) {
  print "Missing packages:\n";
  foreach my $p (@{$deplist->{Lacking}}) {
    print "$p\n";
  }
}

# Output topologically-sorted list of packages
run_cmd('mkdir', '-p', $package);
my $f = IO::File->new("$package/package-list", 'w') || die;
foreach my $p (@{$deplist->{List}}) {
  print $f "$p->{package}_$p->{version}\n";
}
$f->close();


sub try_get {
  my ($do_download, $list) = @_;
  
  my $size = 0;
  my $size_to_get = 0;
  foreach my $d (@$list) {
    $size += $d->{size};
    
    $d->{filename} =~ /\/([^\/]+)$/ || die;
    my $leafname = $1;

    $d->{local_file} = "/var/cache/apt/archives/$leafname";
    if(!-e $d->{local_file}) {
      $d->{local_file} = "cache/$leafname";
      if(!-e $d->{local_file}) {
	$size_to_get += $d->{size};
	
	if($do_download) {
	  print "getting $leafname...\n";
	  mkdir('cache');
	  run_cmd('curl', "$server/$d->{filename}",
		  '-o', $d->{local_file});
	}
      }
    }
  }
  printf "to get: %iMb of %iMb\n",
    $size_to_get / (1024 * 1024),
    $size / (1024 * 1024);
}

try_get(0, $deplist->{List});
if($do_download) {
  try_get(1, $deplist->{List});
}

if($do_unpack) {
  print "unpacking...\n";
  mkdir($dest_dir);

  my $p_dest_dir = 'out-packages';
  mkdir($p_dest_dir);
  # unpack in separate dirs
  foreach my $p (@{$deplist->{List}}) {
    if($skip_not_found && !-e $p->{local_file}) { next }

    # Extract main files
    my $dest = "$p_dest_dir/$p->{package}_$p->{version}";
    if(!-e $dest) {
      run_cmd('dpkg-deb', '-x', $p->{local_file}, "$dest.tmp");
      rename("$dest.tmp", $dest) || die "Can't rename $dest into place";
    }

    # Extract control files
    $dest = "out-control/$p->{package}_$p->{version}";
    if(!-e $dest) {
      run_cmd('dpkg-deb', '-e', $p->{local_file}, "$dest.tmp");
      rename("$dest.tmp", $dest) || die "Can't rename $dest into place";
    }
  }
  
  # unpack in single dir
  foreach my $p (@{$deplist->{List}}) {
    my $src = "$p_dest_dir/$p->{package}_$p->{version}";
    
    if($skip_not_found && !-e $src) {
      print "skipping $p->{package}\n";
      next;
    }
    
    foreach my $f (glob("$src/*")) {
      run_cmd('cp', '-lr', $f, $dest_dir);
    }
  }

  run_cmd('mkdir', '-p', "$dest_dir/var/lib/dpkg");
  my $f = IO::File->new("$dest_dir/var/lib/dpkg/status", 'w') || die;
  foreach my $p (@{$deplist->{List}}) {
    print $f
      "Package: $p->{package}\n".
      "Version: $p->{version}\n".
      "Status: install ok installed\n\n";
  }
}


sub trim {
  my ($x) = @_;
  $x =~ /^\s*(.*?)\s*$/ || die;
  $1
}

sub run_cmd {
  print join(' ', @_)."\n";
  my $rc = system(@_);
  if($rc != 0) {
    die "Return code $rc";
  }
}
