#!/usr/bin/perl -w

use IO::File;
use DirHandle;

my $prefix = '';
my $rtld = 0;
if($ARGV[0] eq '--rtld') {
  $prefix = 'rtld-';
  $rtld = 1;
  shift(@ARGV);
}
# if($ARGV[0] eq '--prefix') {
#   $prefix = $ARGV[1];
#   shift(@ARGV);
#   shift(@ARGV);
# }

my @excl = @ARGV;
my $excl = {};
foreach my $f (@excl) {
  if($f =~ /\/([^\/]+)$/) { $f = $`.'/'.$prefix.$1 }
  $excl->{$f} = 1
}

my @dirs = qw(csu iconv iconvdata locale localedata assert ctype intl catgets math setjmp signal stdlib stdio-common libio dlfcn malloc string wcsmbs timezone time dirent grp pwd posix io termios resource misc socket sysvipc gmon gnulib wctype manual shadow po argp crypt linuxthreads resolv nss rt conform debug linuxthreads_db inet hesiod sunrpc nis nscd streams login elf .);

my @rtld_files = qw(
  csu/rtld-check_fds.os
  csu/rtld-errno.os
  csu/rtld-errno-loc.os
  csu/rtld-divdi3.os
  dirent/rtld-opendir.os
  dirent/rtld-closedir.os
  dirent/rtld-readdir.os
  dirent/rtld-getdents.os
  gmon/rtld-profil.os
  gmon/rtld-prof-freq.os
  io/rtld-xstat64.os
  io/rtld-fxstat64.os
  io/rtld-open.os
  io/rtld-close.os
  io/rtld-read.os
  io/rtld-write.os
  io/rtld-lseek.os
  io/rtld-access.os
  io/rtld-fcntl.os
  io/rtld-getcwd.os
  io/rtld-readlink.os
  io/rtld-xstatconv.os
  io/rtld-lxstat.os
  linuxthreads/rtld-forward.os
  linuxthreads/rtld-libc-cancellation.os
  misc/rtld-getpagesize.os
  misc/rtld-mmap.os
  misc/rtld-munmap.os
  misc/rtld-mprotect.os
  misc/rtld-llseek.os
  posix/rtld-_exit.os
  posix/rtld-getpid.os
  posix/rtld-getuid.os
  posix/rtld-geteuid.os
  posix/rtld-getgid.os
  posix/rtld-getegid.os
  posix/rtld-environ.os
  setjmp/rtld-bsd-_setjmp.os
  setjmp/rtld-__longjmp.os
  signal/rtld-sigaction.os
  stdlib/rtld-exit.os
  stdlib/rtld-cxa_atexit.os
  string/rtld-strchr.os
  string/rtld-strcmp.os
  string/rtld-strlen.os
  string/rtld-strnlen.os
  string/rtld-memchr.os
  string/rtld-memmove.os
  string/rtld-memset.os
  string/rtld-mempcpy.os
  string/rtld-stpcpy.os
  string/rtld-memcpy.os
  string/rtld-strcpy.os
  time/rtld-setitimer.os
);


# Check for directories that might be missing in the list above
my $dirs = {};
foreach my $d (@dirs) { $dirs->{$d} = 1 }
foreach my $d (@{dir_leaves('.')}) {
  if(-d $d && -e "$d/stamp.os" && !$dirs->{$d}) { print STDERR "not included: $d\n" }
}


my @files;
if(!$rtld) {
  my @index_files = map { "$_/stamp.os" } @dirs;

  foreach my $f (@index_files) {
    foreach my $x (split(/\s+/, slurp($f))) {
      push(@files, $x);
    }
  }
}
else { @files = @rtld_files }

foreach my $x (@files) {
  if(!$excl->{$x}) { print "$x\n"; }
  else { $excl->{$x}++ }
}
foreach my $f (keys(%$excl)) {
  if($excl->{$f} <= 1) { print STDERR "Did not exclude $f\n" }
  else {
    print STDERR "Did exclude $f\n"
  }
}


sub slurp {
  my ($file) = @_;
  my $f = IO::File->new($file, 'r');
  if(!defined $f) { die "Can't open `$file' for reading" }
  my $d = join('', <$f>);
  $f->close();
  $d
}

sub dir_leaves {
  my ($dir) = @_;
  my $dh = DirHandle->new($dir);
  if(!defined $dh) { die "Can't open `$dir'" }
  my @got;
  my $x;
  while(defined ($x = $dh->read())) {
    if($x ne '.' && $x ne '..') { push(@got, $x) }
  }
  \@got
}
