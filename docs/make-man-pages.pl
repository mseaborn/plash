#!/usr/bin/perl -w

# Copyright (C) 2005, 2006 Mark Seaborn
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.


# Convert DocBook/XXML to DocBook/XML and then to man pages.
# (Expand some of my custom tags in the first step.)

use IO::File;
use XXMLParse qw(tag tagp get_attr get_attr_opt);
use Transforms;

my $aliases =
  { 'r' => 'replaceable',        # DocBook's <replaceable>; italic
    'ul' => 'itemizedlist',
    'li' => 'listitem',
    'em' => 'emphasis',
    'f' => 'function',
    'pre' => 'programlisting',
  };
my $transform_map =
  { (map {
       my $tag = $_;
       ($tag =>
	sub {
	  my ($x) = @_;
	  tag($aliases->{$tag}, $x->{B})
	})
     } keys(%$aliases)),

    'ps' => sub {
      my ($t) = @_;
      XXMLParse::paras(Transforms::flatten_lists($t->{B}))
    },

    'dl' => sub {
      # DocBook doesn't offer a direct equivalent of HTML's dl tag.
      # It has variablelist, but when an entry has multiple term tags,
      # it will put these on the same line, separated by a comma.
      # This is really misleading for some of my uses.
      # This hack tries to get back the dl tag.
      # When there are multiple "dt"s to one "dd", multiple entries are
      # created in the variablelist.
      my ($t) = @_;
      my @body = grep { /\S/ } Transforms::flatten_lists($t->{B});
      my @out;
      while(scalar(@body)) {
	if(scalar(@body) >= 2 && $body[0]{T} eq 'dt' && $body[1]{T} eq 'dd') {
	  push(@out,
	       tag('varlistentry',
		   tag('term', $body[0]{B}),
		   tag('listitem', $body[1]{B})));
	  shift(@body);
	  shift(@body);
	}
	elsif(scalar(@body) >= 1 && $body[0]{T} eq 'dt') {
	  push(@out,
	       tag('varlistentry',
		   tag('term', $body[0]{B}),
		   tag('listitem', tag('para'))));
	  shift(@body);
	}
	else {
	  print STDERR join(',', map { $_->{T} } @body)."\n";
	  die;
	}
      }
      tag('variablelist', @out)
    },
  };

my @files =
  ('man_pola-run',
   'man_powerbox-req',
   'man_run-as-anonymous',
   'man_gc-uid-locks',
   'man_pola-shell',
   'man_plash-opts',
   'man_exec-object',
   'man_chroot',
   'man_socket-publish',
   'man_socket-connect',
   'man_run-emacs',
   );

system('mkdir -p temp man');

foreach my $name (@files) {
  my $data = XXMLParse::read_file("$name.xxml");
  $data = Transforms::transform($data, $transform_map);
  
  my $out_file = "temp/$name.xml";
  my $f = IO::File->new($out_file, 'w') || die "Can't open \"$out_file\"";
  print $f <<'END';
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE book PUBLIC "-//OASIS//DTD DocBook XML V4.2//EN"
     "http://www.oasis-open.org/docbook/xml/4.2/docbookx.dtd">
END
  XXMLParse::to_html($f, $data);
  $f->close();

  my $cmd = "docbook2x-man ../$out_file";
  my $rc = system("cd man && $cmd");
  if($rc != 0) { die }
}
