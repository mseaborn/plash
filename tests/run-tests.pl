#!/usr/bin/perl -w

use IO::File;

my $start_dir = `pwd`;
chomp($start_dir);
$start_dir =~ /^\// || die;

$pola_run = "$ENV{'HOME'}/projects/plash/bin/pola-run";
$pola_shell = "$ENV{'HOME'}/projects/plash/bin/pola-shell";
$exec_object = "$ENV{'HOME'}/projects/plash/bin/exec-object";

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
       my $data = run_cmd($pola_shell, '-c', <<END);
def mycmd = capcmd $exec_object /bin/ls /x=(mkfs /lib /bin /usr);
mycmd '/'
END
     });

test('chmod_x',
     sub {
       my $f = IO::File->new('file', O_CREAT | O_EXCL | O_WRONLY) || die;
       $f->close();
       run_cmd($pola_run, qw(--prog /bin/bash -B -fw . -a=-c),
	       '-a', 'chmod +x file');
     });

test('chmod_unreadable',
     sub {
       my $f = IO::File->new('file', O_CREAT | O_EXCL | O_WRONLY) || die;
       $f->close();
       run_cmd($pola_run, qw(--prog /bin/bash -B -fw . -a=-c),
	       '-a', 'chmod a-r file && chmod a+r file');
     });

test('utimes',
     sub {
       my $atime = 42000000;
       my $mtime = 76000000;
       my $f = IO::File->new('file', O_CREAT | O_EXCL | O_WRONLY) || die;
       $f->close();
       run_cmd($pola_run, qw(--prog /usr/bin/perl -B -fw file -a=-e),
	       '-a', "utime($atime, $mtime, 'file') || die \"utime: $!\"");
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
    open(STDERR, ">&PIPE_WRITE") || die;
    exec(@cmd);
    die;
  }
  close(PIPE_WRITE);
  my $data = join('', <PIPE_READ>);
  close(PIPE_READ);
  if(waitpid($pid, 0) != $pid) { die }
  my $rc = $?;
  if($rc != 0) { die "Return code $rc" }
  $data
}