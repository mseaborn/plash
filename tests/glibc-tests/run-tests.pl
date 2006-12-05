#!/usr/bin/perl -w

use IO::File;
use Time::HiRes qw(gettimeofday);

$|=1; # switch off output buffering


sub read_test_list {
  my ($file) = @_;
  my @list = grep { !/^#/ && /\S/ } split(/\n/, slurp($file));
  return { map { ($_ => 1) } @list }
}

my $tests = read_test_list('test-list');

# Don't run slow tests
foreach my $test (keys(%{read_test_list('test-list-slow')})) {
  delete $tests->{$test};
}

my $ignore_tests = read_test_list('ignore');


# Print progress on a single line if using a terminal
my $is_tty = -t STDOUT;
my $last_msg = '';

sub log_msg {
  my ($x) = @_;
  if($is_tty) {
    print (("\b \b" x length($last_msg))."\r".$x);
    $last_msg = $x;
  }
}


my $out_file = "results.log";
my $out = IO::File->new($out_file, 'w')
  || die "Can't open \"$out_file\"";

my $i = 0;
my $no_tests = scalar(keys(%$tests));
my $time_start = gettimeofday();

foreach my $test (sort(keys(%$tests))) {
  my $result;
  print $out "\n--- running $test\n";

  # Print progress info
  my $taken = gettimeofday() - $time_start;
  log_msg("done $i/$no_tests tests; ".
	  sprintf("taken %.1fs; ", $taken).
	  "running $test");
  $i++;

  my $input = "$test.input";
  if(!-e $input) { $input = '/dev/null' }
  
  if(!-e "bin/$test") {
    $result = 'missing';
  }
  else {
    my $env = slurp("$test.env");
    my $args = slurp("$test.args");
    
    my $dir = "../../glibc-2.3.6-objs";
    $env =~ s/GLIBC_BUILD_DIR/$dir/g;
    $args =~ s/GLIBC_BUILD_DIR/$dir/g;
    write_file('tmp-env', $env);
    write_file('tmp-args', $args);
    
    # LOCPATH=../../glibc/localedata
    # NB. LC_ALL=C is used in glibc's Rules file
    if(system("mkdir -p `dirname out/$test`") != 0) { die }
    my $cmd = "env LC_ALL=C `cat tmp-env` bin/$test `cat tmp-args` <$input >out/$test.out 2>&1";
    
    print $out "$cmd\n";
    my $time0 = gettimeofday();
    my $rc = system($cmd);
    my $time1 = gettimeofday();
    if($rc == 0) { $result = 'ok' }
    else { $result = 'failed' }
    printf $out "-- %s took %fs\n", $test, $time1 - $time0;
  }

  # If we're ignoring the test, tag the result
  if($ignore_tests->{$test}) {
    $result .= '[ignore]';
  }
  
  if($result eq 'failed') {
    log_msg('');
    print "failed: $test\n";
  }

  print $out "** $test $result\n";
}

log_msg('');
my $took = gettimeofday() - $time_start;
printf "ran %i tests, took %.2fs; %.2fs per test\n",
  $no_tests, $took,
  ($no_tests == 0 ? 0 : $took / $no_tests);

$out->close();



sub slurp {
  my ($file) = @_;
  my $f = IO::File->new($file, 'r');
  if(!defined $f) { die "Can't open `$file' for reading" }
  my $d = join('', <$f>);
  $f->close();
  $d
}

sub write_file {
  my ($file, $data) = @_;
  my $f = IO::File->new($file, 'w');
  if(!defined $f) { die "Can't open `$file' for writing" }
  print $f $data;
  $f->close();
}
