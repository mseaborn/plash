#!/usr/bin/perl -w

# Copyright (C) 2004 Mark Seaborn
#
# This file is part of Plash.
#
# Plash is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of
# the License, or (at your option) any later version.
#
# Plash is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with Plash; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
# USA.


use IO::File;

use lib qw(./src);
use FileUtil (write_file_if_changed);


my $glibc_dir = `. src/config.sh && echo \$GLIBC`;
chomp($glibc_dir);
if($glibc_dir eq '') { die }

# Some non-i386 architectures don't provide all version symbols.
# For example, x86-64 glibc doesn't provide GLIBC_2.0, because that
# predated glibc being ported to x86-64.
# GLIBC_2.0 gets mapped to GLIBC_2.2.5 instead.

# Read glibc's generated abi-versions.h in order to get a mapping of
# version symbols, e.g.
# #define VERSION_libc_GLIBC_2_1_1   GLIBC_2.1.1
my $version_map = {};
{
  my $file = "$glibc_dir/abi-versions.h";
  my $f = IO::File->new($file, 'r') || die "Can't open \"$file\"";
  my $line;
  while(defined($line = <$f>)) {
    if($line =~ /^#define (VERSION_\S+)\s+(\S+)\s*$/) {
      $version_map->{$1} = $2;
    }
  }
  $f->close();
}

sub get_version_symbol {
  my ($sym) = @_;

  # Convert, e.g. "GLIBC_2.2" to "VERSION_libc_GLIBC_2_2"
  $sym =~ s/\./_/g;
  $sym = "VERSION_libc_$sym";
  
  my $x = $version_map->{$sym};
  if(!defined $x) { die "Can't find version symbol \"$sym\"" }
  $x
}


my $aliases = '';
my @keep;
my @rename;


# Needed when linking with dietlibc:
# We want dietlibc and glibc to share errno
push(@keep, 'errno');
# dietlibc doesn't have access to the environment because its
# initialisation code doesn't get called.  Use glibc's getenv instead.
push(@rename, ['glibc_getenv', 'getenv']);

# For fork() in glibc 2.3.3:
push(@keep, '__fork_block'); # exported by glibc's fork.os
push(@keep, '__pthread_fork'); # imported weakly by glibc's fork.os
# Necessary for gcc 3.3
push(@keep, '__i686.get_pc_thunk.bx');
push(@keep, '__i686.get_pc_thunk.cx');

# Defined in posix/getuid.os but used by other id-related files
push(@keep, '__libc_missing_32bit_uids');


my $temp_num = 0;

sub strong_alias {
  my ($old_sym, $new_sym) = @_;
  "__asm__(\".global $new_sym; .set $new_sym, $old_sym\");\n"
}

sub process {
  my ($file, $aliases, $keep, $rename) = @_;
  my $f = IO::File->new($file, 'r');
  if(!defined $f) { die "Can't open file `$file'"; }

  my $line;
  while(defined($line = <$f>)) {
    chomp($line);
    if($line =~ /EXPORT/) {
      #print "$line\n";
      if($line =~ /^\/\*\s+EXPORT:\s+(\S+)(\s+=>\s+(.+?))?\s+\*\/$/) {
	my $sym = $1;
	my $to_syms = $3;
	push(@$keep, $sym);

	if(defined $3) {
	  my $strong_sym = 0;
	  foreach my $s (split(/\s+/, $to_syms)) {
	    if($s =~ /^WEAK:/) {
	      my $s2 = $';
	      $$aliases .= "weak_alias($sym, private_$s2)\n";
	      push(@$keep, "private_$s2");
	      push(@$rename, ["private_$s2", $s2]);
	    }
	    # Versioned symbols
	    # When using glibc's versioned_symbol macro, one would write
	    # versions as eg. "GLIBC_2_2".  This is expanded into
	    # "GLIBC_2.2" using macro trickery and using "abi-versions.h".
	    # But we need to tell the linker which symbols to keep, so
	    # we need to use the expanded versions.
	    # $$aliases .= "versioned_symbol(libc, $sym, $1, $2);\n";
	    # NB. Using these assembler directives is probably somewhat
	    # unnecessary.  We could just introduce versioned symbols using
	    # objcopy's --redefine-sym option (the @$rename list).
	    elsif($s =~ /^VER:(.*),(.*)$/) {
	      my ($name, $ver) = ($1, $2);
	      $ver = get_version_symbol($ver);
	      
	      # my $n = $temp_num++;
	      # $$aliases .= "__asm__(\".set temp$n, $sym; .symver temp$n,$name\@$ver\");\n";
	      # push(@$keep, "$1\@$2");
	      
	      my $n = $temp_num++;
	      $$aliases .= strong_alias($sym, "private_${name}_$n");
	      push(@$keep, "private_${name}_$n");
	      push(@$rename, ["private_${name}_$n", "$name\@$ver"]);
	    }
	    # Default versioned symbol
	    elsif($s =~ /^DEFVER:(.*),(.*)$/) {
	      my ($name, $ver) = ($1, $2);
	      $ver = get_version_symbol($ver);
	      
	      # $$aliases .= "__asm__(\".symver $sym,$1\@\@$2\");\n";
	      # push(@$keep, "$1\@\@$2");

	      my $n = $temp_num++;
	      $$aliases .= strong_alias($sym, "private_${name}_$n");
	      push(@$keep, "private_${name}_$n");
	      push(@$rename, ["private_${name}_$n", "$name\@\@$ver"]);
	    }
	    else {
	      if($strong_sym) {
		$$aliases .= strong_alias($sym, "private_$s");
		push(@$keep, "private_$s");
		push(@$rename, ["private_$s", $s]);
	      }
	      else {
		push(@$rename, [$sym, $s]);
		$strong_sym = 1;
	      }
	    }
	  }
	  if(!$strong_sym) { die "No strong symbol given" }
	}
      }
      else {
	print "Unrecognised line: `$line'\n";
      }
    }
  }
  $f->close();
}

sub obj_copy_cmds {
  my ($obj_file, $keep, $rename) = @_;

  "# This is for use on $obj_file\n".
  'if [ "$#" -ne 1 ]; then echo Expect 1 argument; exit 1; fi'.
  "\n\n".
  "objcopy \\\n".join('', map { "  -G $_ \\\n" } @$keep).
    ' --wildcard -G "export_*"'.
    #'  `for SYM in $(cat shobj/symbol-list); do echo -G $SYM; done` '.
    "  \$1\n".
  "objcopy \\\n".
    join('', map { "  --redefine-sym $_->[0]=$_->[1] \\\n" } @$rename).
    "  \$1\n"
}

my $banner = "Auto-generated by build-link-def.pl";

# Link defs for libc
{
  my $keep1 = [@keep];
  my $rename1 = [@rename];
  foreach my $file ('libc-comms',
		    'libc-misc',
		    'libc-fork-exec',
		    'libc-connect',
		    'libc-getsockopt',
		    'libc-getuid',
		    'libc-utime',
		    'libc-truncate') {
    my $aliases1 = '';
    process("src/$file.c", \$aliases1, $keep1, $rename1);
    output_file("gensrc/out-aliases_$file.h",
		"/* $banner */\n\n".
		$aliases1);
  }
  output_file("gensrc/out-link_main.sh",
	      "# $banner\n\n".
	      obj_copy_cmds('obj/combined.os', $keep1, $rename1));
}

# Link defs for ld-linux/ld.so
{
  my $keep1 = [@keep];
  my $rename1 = [@rename];
  foreach my $file ('libc-comms',
		    'libc-misc',
		    'libc-getuid') {
    my $aliases1 = '';
    process("src/$file.c", \$aliases1, $keep1, $rename1);
    
    # Building ld.so uses same aliases as libc, so this is not needed:
    # output_file("gensrc/out-aliases_$file.h", "/* $banner */\n\n".$aliases1);
  }
  output_file("gensrc/out-link_rtld.sh",
	      "# $banner\n\n".
	      obj_copy_cmds('obj/rtld-combined.os', $keep1, $rename1));
}



sub do_cmd {
  my ($cmd) = @_;
  print "$cmd\n";
  my $r = system($cmd);
  die "return code: $r" if $r;
}


sub output_file {
  my ($file, $data) = @_;
  
  print "Writing $file\n";
  write_file_if_changed($file, $data);
}
