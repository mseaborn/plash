#!/usr/bin/perl -w

use IO::File;

$|=1; # switch off output buffering


foreach my $test (split(/\s+/, slurp('test-list'))) {
  my $result;
  print "$test:\t";

  my $input = "$test.input";
  if(!-e $input) { $input = '/dev/null' }
  
  if(!-e $test) { $result = MISSING }
  elsif(join(' ', slurp($input), slurp("$test.env"), slurp("$test.args"))
	=~ /GLIBC_BUILD_DIR/) {
    $result = SKIPPED
  }
  else {
    # LOCPATH=../../glibc/localedata
    # NB. LC_ALL=C is used in glibc's Rules file
    my $rc =
      system("env LC_ALL=C `cat $test.env` $test `cat $test.args` <$input >$test.out 2>&1");
    if($rc == 0) { $result = OK }
    else { $result = FAIL }
  }

  print "$result\n";
}



sub slurp {
  my ($file) = @_;
  my $f = IO::File->new($file, 'r');
  if(!defined $f) { die "Can't open `$file' for reading" }
  my $d = join('', <$f>);
  $f->close();
  $d
}
