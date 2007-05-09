#!/usr/bin/perl -w

my $data = join('', <>);


my $insert = <<END;
    # Insert these symbols into the earliest GLIBC version symbol.
    # These used to go under the version PLASH_GLIBC.  However, I want
    # some executables to link these symbols weakly, so that they don't
    # have to be defined for the executable to work.  Unfortunately,
    # the linker still makes the executable require the PLASH_GLIBC
    # symbol in that case.
    plash_libc_duplicate_connection;
    plash_libc_reset_connection;
    plash_libc_kernel_execve;
END

$data =~ s/^(GLIBC\S+ \s* { \s* global:) /$1\n$insert/x
  || die "Can't find first version in libc.map";


$data .= "\n".<<END;
# The following symbols were in GLIBC_PRIVATE in glibc 2.3.3.  By
# glibc 2.3.5, they have been removed.  I have added them back because
# my libpthread.so needs to import them, in order to re-export them as
# __open etc.  (Ideally libpthread.so should import __open instead of
# __libc_open, but I can't see how to import a symbol and then export
# it with the same name.)
PLASH_GLIBC_PRIVATE {
  __libc_open;
  __libc_open64;
  __libc_connect;
  __libc_accept;
  __libc_close;
  __libc_open_nocancel;
};
END


print $data;
