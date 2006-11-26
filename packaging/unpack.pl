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
	     };


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

sub get_deps {
  my ($p) = @_;
  my @deps;
  if(defined $p->{depends}) {
    foreach my $d (map { trim($_) } split(/,/, $p->{depends})) {
      my @or = map { trim($_) } split(/\|/, $d);
      foreach my $d ($or[0]) {
	if($d =~ /^(\S+)(\s*\([^\)]*\))?$/) {
	  push(@deps, $1);
	}
      }
    }
  }
  @deps
}

sub all_deps {
  my ($p) = @_;
  my $seen = {};
  my @q = ($p);
  while(@q) {
    my $p = pop(@q);
    foreach my $d (get_deps($p)) {
      if(!$seen->{$d}) {
	$seen->{$d} = 1;
	my $x = $name_idx->{$d};
	if(defined $x) {
	  push(@q, $x);
	}
      }
    }
  }
  keys(%$seen)
}

sub add_deps {
  my ($p) = @_;
  #print "$p->{package}\n";
  
  if(defined $p->{deps}) { return }
  $p->{deps} = {};
  
  if(defined $p->{depends}) {
    foreach my $d (map { trim($_) } split(/,/, $p->{depends})) {
      my @or = map { trim($_) } split(/\|/, $d);
      
      #if(scalar(@or) > 1) { print "$d\n"; }
      
      foreach my $d ($or[0]) {
	if($d =~ /^(\S+)(\s*\([^\)]*\))?$/) {
	  my $name = $1;
	  my $p2 = $name_idx->{$1};
	  if(!defined $p2) { print "$name\n" }
	  add_deps($p2);
	}
	else { die "Bad dep: $d" }
      }
    }
  }
}

if(0) {
  foreach my $p (@parts) {
    $p->{deps} = [all_deps($p)];
  }
  foreach my $p (sort { scalar(@{$a->{deps}}) <=>
			  scalar(@{$b->{deps}}) } @parts) {
    printf "%s\n%i\n%s\n\n",
      $p->{package},
	scalar(@{$p->{deps}}),
	  join(', ', @{$p->{deps}});
  }
}

my @deps;

foreach my $name ($package, @base, all_deps($name_idx->{$package})) {
  # skip special case
  if($remove->{$name}) { next }
  
  my $d = $name_idx->{$name};
  if(defined $d) {
    push(@deps, $d);
  }
  else {
    print "missing: $name\n";
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

try_get(0, \@deps);
if($do_download) {
  try_get(1, \@deps);
}

if($do_unpack) {
  print "unpacking...\n";
  mkdir($dest_dir);
  if(1) {
    # unpack in single dir
    foreach my $p (@deps) {
      run_cmd('dpkg-deb', '-x', $p->{local_file},
	      $dest_dir);
    }
  }
  else {
    # unpack in separate dirs
    foreach my $p (@deps) {
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
