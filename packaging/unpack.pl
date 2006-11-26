#!/usr/bin/perl -w

use IO::File;

my $packages = "$ENV{'HOME'}/fetched/ftp.uk.debian.org/debian/dists/testing/main/binary-i386/Packages.gz";

my $package = 'gnumeric';

my $do_unpack = 0;
my $dest_dir = 'unpacked-gnumeric';

my $do_download = 0;

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
    print $indent."$name NOT AVAILABLE\n";
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
  if(defined($package->{depends})) {
    foreach my $dep (split_dep_list($package->{depends})) {
      my @or = split_disjunction($dep);

      # Only look at first dependency in disjunction
      foreach my $d ($or[0]) {
	my $dep = parse_dep($d);
	search_deps($deplist, $degree + 1, $dep->{Name});
      }
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
    my $f = $1;

    $d->{local_file} = "/var/cache/apt/archives/$f";
    if(!-e $d->{local_file}) {
      $d->{local_file} = "ftp.uk.debian.org/debian/$d->{filename}";
      if(!-e $d->{local_file}) {
	$size_to_get += $d->{size};
	if($do_download) {
	  run_cmd('wget', "http://ftp.uk.debian.org/debian/$d->{filename}");
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
  if(1) {
    # unpack in single dir
    foreach my $p (@{$deplist->{List}}) {
      run_cmd('dpkg-deb', '-x', $p->{local_file},
	      $dest_dir);
    }
  }
  else {
    # unpack in separate dirs
    foreach my $p (@{$deplist->{List}}) {
      run_cmd('dpkg-deb', '-x', $p->{local_file},
	      "$dest_dir/$p->{package}_$p->{version}");
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
