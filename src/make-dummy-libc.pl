#!/usr/bin/perl -w

# Here's the problem:
#
# In glibc 2.3.4, regexec() was given a new symbol version.  This
# means that any executable that was created by linking against glibc
# 2.3.4 or later won't work with earlier versions of glibc.  (Your
# reference to "regexec" gets turned into "regexec@GLIBC_2.3.4".  When
# linking against older versions of glibc, it gets turned into
# "regexec@GLIBC_2.0".)
#
# As it happens, this dependency is unnecessary and gratuitous.  In
# glibc 2.3.4, the only difference between regexec@GLIBC_2.0 and
# regexec@GLIBC_2.3.4 is that the former unsets some flags before
# calling the latter -- but you shouldn't have been using that flags
# anyway.
#
# Unfortunately, the linker "ld" doesn't provide any way to override
# what libc.so says.  We can't say "give me regexec@GLIBC_2.0 instead
# of the default of regexec@GLIBC_2.3.4".  (This is the problem with
# trying to automate something like versioning.  If something goes
# wrong, there should be a manual override, but there isn't.)
#
# *.so files are a mixture of interface and implementation.  When "ld"
# creates a dynamically-linked executable, it looks at the *.so files
# that it is to be linked against to see what interfaces they provide.
#
# The purpose of this script is to create a *.so file that is all
# interface and no implementation.  It copies the interface from a
# real libc.so, changes it a little (changes "regexec"'s symbol
# versions), and creates a dummy libc.so.
#
# Even without the interface change, using the dummy libc.so causes
# the executable to be slightly different.  This may cause problems in
# the future...


# Oh dear...
# With a symbol, the following info is stored:
#  * Whether it's a function (F) or a variable (O).
#  * Its size.  This makes sense for a variable: it's an important part of
#    the ABI, and the dynamic linker will check whether both sides of a
#    reference match and print a warning if they don't.
#    For a function, it's the size of the code -- an implementation detail
#    that gets copied into the executable!
#    This makes an executable built with dummy libc differ gratuitously
#    from one built against a real libc.so.

use IO::File;


open(INPUT, "objdump -T /lib/libc.so.6 |") || die;

my $out_file = 'gensrc/dummy-libc.c';
my $f = IO::File->new($out_file, 'w') || die "Can't open '$out_file'";

print $f "static void function() {}\n";
# print $f "static const int variable[0];\n";

my $n = 0;
my $line;
while(defined($line = <INPUT>)) {
  chomp($line);
  if($line =~ /^([0-9a-fA-Z]{8})
               \ ([gl ])
                 ([w ])
               \ \ \ ([dD ])
               ([OF ])
               \ (\S+)
               \s+([0-9a-fA-F]{8})
               \s+((\S+)?\s+)
               (\S+)\s*$/x) {
    my ($addr, $flag1, $flag2, $flag, $section, $size, $ver, $sym) =
      ($1, $2, $3, $5, $6, $7, $9, $10);
      # map { substr($line, $-[$_], $+[$_] - $-[$_]) } (1, 2, 3, 5, 6, 7, 9, 10);
    if($section ne '*UND*' && # undefined symbols
       $flag1 ne 'l' &&       # 'l' means an entry for a section, not a symbol
       $section ne '*ABS*' && # only used for symbols standing for a version
       $sym ne 'regexec'      # Symbol to modify
      ) {
      if(defined $ver) {
	my $temp = 'temp'.($n++);
	if($flag eq 'F') {
	  print $f strong_alias('function', $temp);
	  # print $f "void $temp() {}\n";
	}
	elsif($flag eq 'O') {
	  # print $f strong_alias('variable', $temp);
	  # The symbol's size should match.
	  print $f "char $temp\[0x$size];\n";
	}
	else {
	  print STDERR ":: $line\n";
	  die
	}

	# This affects ld's --cref output.
	# It doesn't appear to affect the executables that get built.
	if($flag2 eq 'w') {
	  print $f "__asm__(\".weak $temp\");\n";
	}
	
	if($ver =~ /^\((.*)\)$/) {
	  print $f "__asm__(\".symver $temp,$sym\@$1\");\n";
	}
	else {
	  print $f "__asm__(\".symver $temp,$sym\@\@$ver\");\n";
	}
      }
      else {
	print STDERR ":: $line\n";
      }
    }
  }
}

sub ver_fun {
  my ($sym, $ver) = @_;
  my $temp = 'temp'.($n++);
  print $f strong_alias('function', $temp);
  print $f "__asm__(\".symver $temp,$sym\@\@$ver\");\n";
}

ver_fun('regexec', 'GLIBC_2.0');

close(INPUT);


sub strong_alias {
  my ($old_sym, $new_sym) = @_;
  "__asm__(\".global $new_sym; .set $new_sym, $old_sym\");\n"
}
