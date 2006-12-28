
package Unpack;

use IO::File;


# Read APT's sources.list file.
# Returns a source list, which contains hashes with:
#   S_base => base URL, e.g. "http://ftp.uk.debian.org/debian"
#   S_comp => component, e.g. "dists/stable/main"
sub read_sources_list {
  my ($file) = @_;

  my $sources = [];

  my $f = IO::File->new($file, 'r') || die "Can't open \"$file\"";
  my $line;
  while(defined($line = <$f>)) {
    chomp($line);
    
    # Ignore comments and empty lines
    if($line =~ /^\s*$/ ||
       $line =~ /^\s*#/) {
    }
    # Read a "deb" line
    elsif($line =~ /^\s* deb \s+ (.+)$/x) {
      my @args = split(/\s+/, $1);
      
      if(scalar(@args) == 0) {
	die "Malformed deb line in $file: URL missing: $line";
      }
      my $url = shift(@args);
      
      if(scalar(@args) == 0) {
	die "Malformed deb line in $file: distribution name missing: $line";
      }
      my $dist = shift(@args);
      
      if($dist =~ /\/$/ && scalar(@args) == 0) {
	push(@$sources, { S_base => $url,
			  S_component => $dist });
      }
      else {
	if(scalar(@args) == 0) {
	  die "Malformed deb line in $file: no component names: $line";
	}
	foreach my $component (@args) {
	  push(@$sources, { S_base => $url,
			    S_component => "dists/$dist/$component" });
	}
      }
    }
    # Ignore "deb-src" lines
    elsif($line =~ /^\s* deb-src \s+/x) {
    }
    else {
      die "Unrecognised line in $file: $line";
    }
  }
  return $sources;
}


sub join_with_slash {
  my ($a, $b) = @_;
  $a =~ s#/+$##; # remove trailing slashes
  $b =~ s#^/+##; # remove leading slashes
  return "$a/$b";
}


# Fetches Packages.gz file and returns filename.
sub fetch_packages_file {
  my ($src, $dest_dir, $arch) = @_;

  my $url =
    join_with_slash($src->{S_base},
		    join_with_slash($src->{S_component},
				    "binary-$arch/Packages.gz"));
  
  my $filename = $url;
  $filename =~ s#http://## || die "Only HTTP is supported: $url";
  $filename =~ s#/#_#g;
  $filename = "$dest_dir/$filename";

  print "Get $url\n";
  if(!-e $filename) {
    run_cmd('curl', $url, '-o', $filename);
  }

  return $filename;
}


sub get_block {
  my ($f) = @_;

  my $data = '';
  while(defined($line = <$f>)) {
    if($line =~ /^\s*$/) {
      # Blank line terminates block
      last;
    }
    $data .= $line;
  }
  return $data;
}


sub run_cmd {
  print join(' ', @_)."\n";
  my $rc = system(@_);
  if($rc != 0) {
    die "Return code $rc";
  }
}

1;
