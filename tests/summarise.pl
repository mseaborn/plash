#!/usr/bin/perl -w

use lib qw(../docs);
use XXMLParse qw(tag tagp);


# Get list of test runs, newest first
my @runs;
foreach my $file (reverse(@ARGV)) {
  push(@runs, read_log($file));
}

# Build a list of what tests exist
my $tests = {};
foreach my $run (@runs) {
  foreach my $test (keys(%{$run->{R}})) {
    $tests->{$test} = 1;
  }
}
my @tests = sort(keys(%$tests));

@runs = remove_dups(@runs);

@tests = sort { compare_tests($a, $b, \@runs) } @tests;

# Create table
sub result_cell {
  my ($run, $test) = @_;
  my $result = $run->{R}{$test};
  if(defined $result) {
    tagp('td', [['class', "result-$result"]], $result)
  }
  else {
    tagp('td', [['class', 'result-none']], '-')
  }
}
sub run_heading {
  my ($run) = @_;
  map { tag('div', $_) } sort { $a <=> $b } keys(%{$run->{Revisions}})
}
my $test_per_row = 1;
my @rows;
if($test_per_row) {
  # Output with a row per test
  push(@rows,
       [tag('th', ''),
	map { tagp('th', [['class', 'col-heading']], run_heading($_)) }
	@runs]);
  push(@rows,
       [tag('th', ''),
	map {
	  tagp('td', [['class', 'time']],
	       map { tag('div', $_) } split(/\s+/, $_->{Time}))
	} @runs]);
  foreach my $test (@tests) {
    push(@rows, [tagp('th', [['class', 'row-heading']], $test),
		 map { my $run = $_; result_cell($run, $test) } @runs]);
  }
}
else {
  # Output with a column per test
  push(@rows, [map { tag('th', $_) } @tests]);
  foreach my $run (@runs) {
    push(@rows, [map { my $test = $_; result_cell($run, $test) } @tests]);
  }
}
XXMLParse::to_html(STDOUT,
		   tag('html',
		       tagp('link', [['rel', 'stylesheet'],
				     ['href', 'summary.css']]),
		       tagp('table', [['border', 1]],
			    map { tag('tr', $_) } @rows)));



# Read a log file to get test results
sub read_log {
  my ($file) = @_;

  my $r = {};

  my $f = IO::File->new($file, 'r') || die "Can't open \"$file\"";
  my $line;
  while(defined($line = <$f>)) {
    # First line is date
    if(!defined $r->{Time}) {
      chomp($line);
      $r->{Time} = $line;
    }
    # Test result line: "** test-name ok"
    if($line =~ /^\*\* (\S+) (ok|failed)/) {
      my ($test, $result) = ($1, $2);
      die unless !defined $r->{R}{$test};
      $r->{R}{$test} = $result;
    }
    if($line =~ /^>> revision (\d+)/) {
      $r->{Revision} = $1;
    }
  }
  $f->close();

  $r
}

# Do two result sets have the same results?
sub results_equal {
  my ($r1, $r2) = @_;

  foreach my $test (@tests) {
    if(maybe_undef($r1->{R}{$test}) ne
       maybe_undef($r2->{R}{$test})) {
      return 0;
    }
  }
  return 1;
}

sub compare_tests {
  my ($test1, $test2, $runs) = @_;
  for(my $i = 0; $i <= $#$runs; $i++) {
    my $r = maybe_undef($runs->[$i]{R}{$test1});
    my $r2 = maybe_undef($runs->[$i]{R}{$test2});
    if($r ne $r2) {
      return $r cmp $r2;
      if($r eq 'failed') {
	return -1;
      }
      if($r2 eq 'failed') {
	return 1;
      }
      return -1; # fallback
    }
  }
  return 0;
}

# Takes a list of result sets.
# Returns the list with duplicates removed.
sub remove_dups {
  my @in = @_;
  
  my @got = ();
  my $last;
  
  while(scalar(@in)) {
    my $x = shift(@in);
    if(!defined $last || !results_equal($last, $x)) {
      $last = $x;
      push(@got, $last);
    }
    # Add to list of revisions
    $last->{Revisions}{$x->{Revision}} = 1;
  }
  @got
}

# Used for suppressing 'not defined' warnings
sub maybe_undef {
  defined $_[0] ? $_[0] : '';
}
