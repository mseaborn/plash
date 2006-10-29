#!/usr/bin/perl -w

use IO::File;
use Time::HiRes qw(gettimeofday);

$|=1; # switch off output buffering


my $tests = {};
foreach my $test (split(/\s+/, slurp('test-list'))) {
  $tests->{$test} = 1;
}
foreach my $test (split(/\s+/, slurp('test-list-slow'))) {
  delete $tests->{$test};
}


foreach my $test (sort(keys(%$tests))) {
  my $result;
  print "\n--- running $test\n";

  my $input = "$test.input";
  if(!-e $input) { $input = '/dev/null' }
  
  if(!-e $test) {
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
    my $cmd = "env LC_ALL=C `cat tmp-env` $test `cat tmp-args` <$input >$test.out 2>&1";
    
    print "$cmd\n";
    my $time0 = gettimeofday();
    my $rc = system($cmd);
    my $time1 = gettimeofday();
    if($rc == 0) { $result = 'ok' }
    else { $result = 'failed' }
    printf "-- %s took %fs\n", $test, $time1 - $time0;
  }

  print "** $test $result\n";
}



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
