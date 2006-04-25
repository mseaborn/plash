#!/usr/bin/perl -w

use IO::File;

my $verbose = 1;

my $start_dir = `pwd`;
chomp($start_dir);
$start_dir =~ /^\// || die;

run_cmd('rm -rf out/');

$pola_run = "$start_dir/../bin/pola-run";
$pola_shell = "$start_dir/../bin/pola-shell";
$exec_object = "$start_dir/../bin/exec-object";

# @pola_run = ($pola_run);
$ENV{'PLASH_DIR'} = "$start_dir/..";
@pola_run = ($pola_run, '--sandbox-prog', "$start_dir/strace-wrapper.sh",
	     '-fl', "$start_dir/../lib");



test('shell_hello',
     sub {
       my $data = cmd_capture($pola_shell, '-c', "echo 'Hello world!'");
       if($data ne "Hello world!\n") { die "Got: \"$data\"" }
     });

test('shell_redirect',
     sub {
       my $data = cmd_capture($pola_shell, '-c', "echo 'Hello world!' 1>&2");
       if($data ne "Hello world!\n") { die "Got: \"$data\"" }
     });

test('shell_execobj_hello',
     sub {
       my $data = cmd_capture($pola_shell, '-c', <<END);
def myecho = capcmd $exec_object /bin/echo / ;
myecho 'Hello there'
END
       if($data ne "Hello there\n") { die "Got: \"$data\"" }
     });

test('shell_execobj_2',
     sub {
       my $data = cmd_capture($pola_shell, '-c', <<END);
def mycmd = capcmd $exec_object /bin/ls /x=(mkfs /lib /bin /usr);
mycmd '/'
END
       if($data ne "bin\nlib\nusr\n") { die "Got: \"$data\"" }
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
			      '-e', '/usr/bin/strace',
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
  my @cmd = @_;
  pipe(PIPE_READ, PIPE_WRITE);
  my $pid = fork();
  if($pid == 0) {
    close(PIPE_READ);
    open(STDOUT, ">&PIPE_WRITE") || die;
    # open(STDERR, ">&PIPE_WRITE") || die;
    exec(@cmd);
    die;
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
