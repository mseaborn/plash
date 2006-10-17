#!/usr/bin/perl -w

use IO::File;
use Cwd;
use File::stat;

my $verbose = 1;

my $start_dir = `pwd`;
chomp($start_dir);
$start_dir =~ /^\// || die;

run_cmd('rm -rf out/');

$pola_run = "pola-run";
$pola_shell = "pola-shell";
$exec_object = "exec-object";

@pola_run = ($pola_run);

my @failed;


test('shell_hello',
     sub {
       my $data = cmd_capture($pola_shell, '-c', "echo 'Hello world!'");
       assert_equal($data, "Hello world!\n", 'output');
     });

test('shell_attach',
     sub {
       my $x = "test contents";
       write_file('my_file', $x);
       my $data = cmd_capture($pola_shell, '-c', "cat some_file_name=(F my_file)");
       assert_equal($data, $x, 'output');
     });
test('shell_attach_name',
     sub {
       my $data = cmd_capture($pola_shell, '-c', "echo some_file_name=(F .)");
       assert_equal($data, "some_file_name\n", 'output');
     });

test('shell_redirect',
     sub {
       my $data = cmd_capture($pola_shell, '-c', "echo 'Hello world!' 1>temp_file");
       assert_equal($data, '', 'output');
       assert_equal(read_file('temp_file'), "Hello world!\n", 'temp_file');
     });

test('shell_bash_exec',
     sub {
       # NB. Add "+ ." just to suppress bash's warning
       my $data = cmd_capture($pola_shell, '-c',
			      "sh -c '/bin/echo \"Successful call\"' + .");
       assert_equal($data, "Successful call\n", 'output');
     });

test('shell_execobj_hello',
     sub {
       my $data = cmd_capture($pola_shell, '-c', <<END);
def myecho = capcmd $exec_object /bin/echo / ;
myecho 'Hello there'
END
       if($data !~ /^Hello there$/m) { die "Got: \"$data\"" }
     });

test('shell_execobj_2',
     sub {
       my $extra =
	 defined $ENV{'PLASH_LIBRARY_DIR'}
	   ? "/usr/plash-testlib=(F $ENV{'PLASH_LIBRARY_DIR'})"
	   : '';
       my $data = cmd_capture({ After_fork => sub {
				  $ENV{'LD_LIBRARY_PATH'} =
				    "/usr/plash-testlib:$ENV{'LD_LIBRARY_PATH'}"
				}
			      },
			      $pola_shell, '-c', <<END);
def mycmd = capcmd $exec_object /bin/ls /x=(mkfs /lib /bin /usr $extra);
mycmd '/'
END
       if($data !~ /^bin\nlib\nusr$/m) { die "Got: \"$data\"" }
     });

# The cwd should not be set in the callee if the caller's cwd is not
# visible in the callee's namespace.
test('shell_cwd_unset',
     sub {
       # Assumes we start off in a temporary directory that is not
       # visible by default.
       my $data = cmd_capture
	 ($pola_shell, '-c',
	  q(perl -e 'use Cwd; my $cwd = getcwd();
                     print (defined $cwd ? $cwd : "unset")'));
       assert_equal($data, 'unset', 'output');
     });

# Check that cwd is defined when we explicitly add the argument "+ .".
test('shell_cwd_set',
     sub {
       my $caller_cwd = getcwd();
       my $data = cmd_capture
	 ($pola_shell, '-c',
	  q(perl -e 'use Cwd; my $cwd = getcwd();
                     print (defined $cwd ? $cwd : "unset")'
	    + .));
       assert_equal($data, $caller_cwd, 'output');
     });

test('shell_execobj_cwd_unset',
     sub {
       my $data = cmd_capture($pola_shell, '-c', <<END);
def mycmd = capcmd $exec_object /bin/pwd /x=(mkfs /lib /bin /usr $ENV{'PLASH_LIBRARY_DIR'});
mycmd 2>&1
END
       # Ensure that pwd printed an error.
       # Would be better to invoke perl via exec-object, but exec-object
       # doesn't allow passing extra arguments.
       if($data !~ m#^/bin/pwd:#m) { die "Got: \"$data\"" }
     });

test('shell_execobj_cwd_set',
     sub {
       my $caller_cwd = getcwd();
       my $data = cmd_capture($pola_shell, '-c', <<END);
def mycmd = capcmd $exec_object /bin/pwd /x=(mkfs /lib /bin /usr $ENV{'PLASH_LIBRARY_DIR'});
mycmd + .
END
       # Check that output contains string.
       # Would be better to check for equality, but there's output noise.
       if($data !~ /^$caller_cwd$/m) { die "Got: \"$data\"" }
     });



test('hello',
     sub {
       my $data = cmd_capture(@pola_run, qw(-B --prog /bin/echo),
			      '-a', 'Hello world');
       assert_equal($data, "Hello world\n", 'output');
     });

# Tests execve but not fork, because bash does a tail call
test('bash_exec',
     sub {
       # do "--cwd /" to stop bash complaining about unset cwd
       my $data = cmd_capture(@pola_run, qw(-B --cwd /),
			      '-e', '/bin/bash', '-c', '/bin/echo yeah');
       assert_equal($data, "yeah\n", 'output');
     });

test('bash_fork',
     sub {
       my $data = cmd_capture(@pola_run, qw(-B --cwd /),
			      '-e', '/bin/bash', '-c',
			      '/bin/echo yeah; /bin/true');
       assert_equal($data, "yeah\n", 'output');
     });

test('strace',
     sub {
       my $data = cmd_capture(@pola_run, '-B',
			      '-e', '/usr/bin/strace', '-c',
			      '/bin/echo', 'Hello world');
       if($data !~ /Hello world/) { die "Got: \"$data\"" }
     });

test('chmod_x',
     sub {
       my $f = IO::File->new('file', O_CREAT | O_EXCL | O_WRONLY) || die;
       $f->close();
       run_cmd(@pola_run, qw(-B -fw .),
	       '-e', '/bin/bash', '-c', 'chmod +x file');
       if(!-x 'file') { die 'file not executable' }
     });

test('chmod_unreadable',
     sub {
       my $f = IO::File->new('file', O_CREAT | O_EXCL | O_WRONLY) || die;
       $f->close();
       run_cmd(@pola_run, qw(-B -fw .),
	       '-e', '/bin/bash', '-c',
	       'chmod a-r file && chmod a+r file');
     });

# We should not be able to set the setuid or setgid bits on files.
test('chmod_setuid',
     sub {
       write_file('file', '');
       my $rc = system(@pola_run, qw(-B -fw .),
		       '-e', '/bin/chmod', '+s', 'file');
       if($rc == 0) {
	 die "chmod succeeded but shouldn't have done so";
       }
       else {
	 print "chmod failed as expected\n";
       }
       if((stat('file')->mode() & 01000) != 0) {
	 printf("mode: %o\n", stat('file')->mode());
	 die "setuid/setgid/sticky bit set";
       }
     });

test('utimes',
     sub {
       my $atime = 42000000;
       my $mtime = 76000000;
       my $f = IO::File->new('file', O_CREAT | O_EXCL | O_WRONLY) || die;
       $f->close();
       run_cmd(@pola_run, qw(-B -fw file),
	       '-e', '/usr/bin/perl', '-e',
	       qq{ utime($atime, $mtime, 'file') || die "utime: \$!" });
       my $st = stat('file');
       if($st->atime() != $atime) { die "atime mismatch" }
       if($st->mtime() != $mtime) { die "atime mismatch" }
     });


test('python_hellow',
     sub {
       my $data = cmd_capture('python', "$start_dir/hellow.py");
       assert_equal($data, "Hello world!\nexited with status: 0\n", 'output');
     });

test('python_tests',
     sub {
       run_cmd('python', "$start_dir/test-dirs.py");
     });


test('clobber_comm_fd',
     sub {
       run_cmd('gcc', '-Wall', "$start_dir/clobber-comm-fd.c",
	       '-o', "$start_dir/clobber-comm-fd");
       my $x = cmd_capture({ Get_stderr => 1 },
			   @pola_run, '-B',
			   '-f', "$start_dir/clobber-comm-fd",
			   '-e', "$start_dir/clobber-comm-fd");
       assert_equal($x, "close refused as expected\n", 'output');
     });

# Same test but with executable linked to libpthread.so
test('clobber_comm_fd_pthread',
     sub {
       run_cmd('gcc', '-Wall', "$start_dir/clobber-comm-fd.c",
	       '-o', "$start_dir/clobber-comm-fd-pthread", '-lpthread');
       my $x = cmd_capture({ Get_stderr => 1 },
			   @pola_run, '-B',
			   '-f', "$start_dir/clobber-comm-fd-pthread",
			   '-e', "$start_dir/clobber-comm-fd-pthread");
       assert_equal($x, "close refused as expected\n", 'output');
     });

# Does a socket created in the sandbox return expected SO_PEERCRED
# information?
test('getsockopt-uid',
     sub {
       run_cmd('gcc', '-Wall', "$start_dir/test-getsockopt.c",
	       '-o', "$start_dir/test-getsockopt");
       # Try running the test program outside of Plash first.
       my $data1 = cmd_capture("$start_dir/test-getsockopt");
       assert_equal($data1, "IDs are the same, as expected\n", 'output1');
       my $data2 = cmd_capture(@pola_run, '-B',
			       '-f', "$start_dir/test-getsockopt",
			       '-e', "$start_dir/test-getsockopt");
       assert_equal($data2, "IDs are the same, as expected\n", 'output2');
     });

# "install" uses chown(filename, -1, -1): ensure that that works
test('install_chown',
     sub {
       my $f = IO::File->new('file', O_CREAT | O_EXCL | O_WRONLY) || die;
       $f->close();
       run_cmd(@pola_run, '-B', '-fw', '.',
	       '-e', '/usr/bin/install', '-T', 'file', 'dest');
     });


print "\n";
if(scalar(@failed)) {
  printf "%i tests failed: %s\n", scalar(@failed), join(', ', @failed);
}
else {
  print "Success!\n";
}


sub test {
  my ($name, $f) = @_;
  eval {
    my $dir = "$start_dir/out/$name";
    run_cmd('mkdir', '-p', $dir);
    chdir($dir) || die;
    print "\n--- running $name\n" if $verbose;
    &$f();
  };
  if($@) {
    print "** $name failed: $@\n";
    push(@failed, $name);
  }
  else {
    print "** $name ok\n";
  }
}

# Assert that $got is equal to $expect.
sub assert_equal {
  my ($got, $expect, $desc) = @_;
  if($got ne $expect) {
    print "MISMATCH for \"$desc\":\n";
    print "GOT:\n$got\n";
    print "EXPECTED:\n$expect\n";
    die "Failed";
  }
  else {
    print "- assert ok for \"$desc\"\n";
  }
}

sub run_cmd {
  my $rc = system(@_);
  if($rc != 0) { die "Return code $rc" }
}

# Run command, capture output.
# Like backtick operator, but takes a list and doesn't use shell.
sub cmd_capture {
  my $opts = {};
  if(ref($_[0]) eq HASH) { $opts = shift; }
  my @cmd = @_;

  pipe(PIPE_READ, PIPE_WRITE);
  my $pid = fork();
  if($pid == 0) {
    eval {
      close(PIPE_READ);
      if(defined $opts->{After_fork}) { &{$opts->{After_fork}}() }
      open(STDOUT, ">&PIPE_WRITE") || die;
      if($opts->{Get_stderr}) {
	open(STDERR, ">&PIPE_WRITE") || die;
      }
      exec(@cmd);
      die;
    };
    exit 1;
  }
  close(PIPE_WRITE);
  my @lines = <PIPE_READ>;
  my $data = join('', @lines);
  close(PIPE_READ);
  if(waitpid($pid, 0) != $pid) { die }
  my $rc = $?;
  if($rc != 0) {
    print join('', map { ">>$_" } @lines);
    die "Return code $rc"
  }
  $data
}

sub read_file {
  my ($file) = @_;
  my $f = IO::File->new($file, 'r');
  if(!defined $f) { die "Can't open `$file' for reading" }
  my $data = join('', <$f>);
  $f->close();
  $data;
}

sub write_file {
  my ($file, $data) = @_;
  my $f = IO::File->new($file, 'w');
  if(!defined $f) { die "Can't open `$file' for writing" }
  print $f $data;
  $f->close();
}
