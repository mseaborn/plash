#!/usr/bin/perl -w

use IO::File;
use Cwd;

my $verbose = 1;

my $start_dir = `pwd`;
chomp($start_dir);
$start_dir =~ /^\// || die;

run_cmd('rm -rf out/');

$pola_run = "pola-run";
$pola_shell = "pola-shell";
$exec_object = "exec-object";

@pola_run = ($pola_run);


test('shell_hello',
     sub {
       my $data = cmd_capture($pola_shell, '-c', "echo 'Hello world!'");
       if($data ne "Hello world!\n") { die "Got: \"$data\"" }
     });

test('shell_attach',
     sub {
       my $x = "test contents";
       write_file('my_file', $x);
       my $data = cmd_capture($pola_shell, '-c', "cat some_file_name=(F my_file)");
       if($data ne $x) { die "Got: \"$data\"" }
     });
test('shell_attach_name',
     sub {
       my $data = cmd_capture($pola_shell, '-c', "echo some_file_name=(F .)");
       if($data ne "some_file_name\n") { die "Got: \"$data\"" }
     });

test('shell_redirect',
     sub {
       my $data = cmd_capture($pola_shell, '-c', "echo 'Hello world!' 1>temp_file");
       my $data2 = read_file('temp_file');
       if($data ne '' ||
	  $data2 ne "Hello world!\n") { die "Got: \"$data2\"" }
     });

test('shell_bash_exec',
     sub {
       # NB. Add "+ ." just to suppress bash's warning
       my $data = cmd_capture($pola_shell, '-c',
			      "sh -c '/bin/echo \"Successful call\"' + .");
       if($data ne "Successful call\n") { die "Got: \"$data\"" }
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
       if($data ne 'unset') { die "Got: \"$data\"" }
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
       if($data ne $caller_cwd) { die "Got: \"$data\"" }
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
       if($data ne "Hello world\n") { die "Got: \"$data\"" }
     });

# Tests execve but not fork, because bash does a tail call
test('bash_exec',
     sub {
       # do "--cwd /" to stop bash complaining about unset cwd
       my $data = cmd_capture(@pola_run, qw(-B --cwd /),
			      '-e', '/bin/bash', '-c', '/bin/echo yeah');
       if($data ne "yeah\n") { die "Got: \"$data\"" }
     });

test('bash_fork',
     sub {
       my $data = cmd_capture(@pola_run, qw(-B --cwd /),
			      '-e', '/bin/bash', '-c',
			      '/bin/echo yeah; /bin/true');
       if($data ne "yeah\n") { die "Got: \"$data\"" }
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

test('utimes',
     sub {
       my $atime = 42000000;
       my $mtime = 76000000;
       my $f = IO::File->new('file', O_CREAT | O_EXCL | O_WRONLY) || die;
       $f->close();
       run_cmd(@pola_run, qw(-B -fw file),
	       '-e', '/usr/bin/perl', '-e',
	       qq{ utime($atime, $mtime, 'file') || die "utime: \$!" });
       my @st = stat('file');
       if($st[8] != $atime) { die "atime mismatch" }
       if($st[9] != $mtime) { die "atime mismatch" }
     });


test('python_hellow',
     sub {
       my $data = cmd_capture('python', "$start_dir/hellow.py");
       if($data ne "Hello world!\nexited with status: 0\n") { die "Got: \"$data\"" }
     });


test('clobber_comm_fd',
     sub {
       run_cmd('gcc', '-Wall', "$start_dir/clobber-comm-fd.c",
	       '-o', "$start_dir/clobber-comm-fd");
       my $x = cmd_capture({ Get_stderr => 1 },
			   @pola_run, '-B',
			   '-f', "$start_dir/clobber-comm-fd",
			   '-e', "$start_dir/clobber-comm-fd");
       die "Got: \"$x\"" if $x ne "close refused as expected\n";
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
       die "Got: \"$x\"" if $x ne "close refused as expected\n";
     });

# "install" uses chown(filename, -1, -1): ensure that that works
test('install_chown',
     sub {
       my $f = IO::File->new('file', O_CREAT | O_EXCL | O_WRONLY) || die;
       $f->close();
       run_cmd(@pola_run, '-B', '-fw', '.',
	       '-e', '/usr/bin/install', '-T', 'file', 'dest');
     });


sub test {
  my ($name, $f) = @_;
  eval {
    my $dir = "$start_dir/out/$name";
    run_cmd('mkdir', '-p', $dir);
    chdir($dir) || die;
    print "\n--- running $name\n" if $verbose;
    &$f();
  };
  if($@) { print "** $name failed: $@\n" }
  else { print "** $name ok\n" }
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
