#!/usr/bin/perl -w

use IO::File;

my $server = 'http://localhost:9999/debian'; # approx proxy
my $packages = "$ENV{'HOME'}/fetched/ftp.uk.debian.org/debian/dists/testing/main/binary-i386/Packages.gz";

my $package = 'leafpad';

my $verbose = 1;
my $do_unpack = 1;
my $dest_dir = "$package/unpacked";

my $do_download = 0;
my $skip_not_found = 1;

my @base = ('bash',
	    'coreutils', # for /usr/bin/id
	    'sed',
	    # 'xlibs-data',
	    );

my $remove = { 'libc6' => 1, # need to replace to provide /usr/lib/gconv...
	       'base-files' => 1,
	     };


print "reading package list...\n";
my @parts;
foreach my $y (grep { $_ !~ /^\s*$/ }
	       split(/\n\s*\n/,
		     `gzip -cd $packages`)) {
  my $x = $y."\n";
  my $p = {};
  while($x ne '') {
    $x =~ s/^(\S+?):\s*(.*(\n\s+.*)*)(\n|\Z)// || die "Bad data: $y";
    $p->{lc($1)} = $2;
  }
  push(@parts, $p);
}

my $name_idx = { map { ($_->{package} => $_) } @parts };


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
  
  # Add to list
  $deplist->{Hash}{$name} = 1;
  $deplist->{Degree}{$name} = $degree;
  push(@{$deplist->{List}}, $package);

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
}


my $deplist =
  { List => [],
    Hash => {},
    Degree => {},
    Lacking => [],
  };

foreach my $p (@base) {
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
    
    if($skip_not_found && !-e $src) { next }
    
    foreach my $f (glob("$src/*")) {
      run_cmd('cp', '-lr', $f, $dest_dir);
    }
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
