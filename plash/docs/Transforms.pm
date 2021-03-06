# Copyright (C) 2006 Mark Seaborn
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

package Transforms;

use XXMLParse qw(tag tagp);


sub flatten_lists {
  my ($x) = @_;
  if(ref($x) eq ARRAY) { map { flatten_lists($_) } @$x }
  else { $x }
}

# Returns a list in which each element is either an untagged paragraph
# or some tag.
sub split_para_list {
  my ($data) = @_;
  my $paras = [];
  my $last = [];
  foreach my $elt (flatten_lists($data)) {
    if(!ref($elt) && $elt =~ /^\n([ \t]*\n)+$/) {
      push(@$paras, $last);
      $last = [];
    }
    else { push(@$last, $elt) }
  }
  push(@$paras, $last);
  [map {
     my $x = [grep { ref($_) || /\S/ } @$_];
     if(scalar(@$x) == 1) { $x->[0] } else { $_ }
   }
   grep { scalar(@$_) > 0 } @$paras]
}

# Takes a list and wraps untagged elements in a <p> or <para> tag (as
# specified by $tag).
sub introduce_paras {
  my ($tag, $list) = @_;
  [map {
     my $x = $_;
     if(ref($x) eq HASH) { $x, "\n\n" } else { tag($tag, $x), "\n\n" }
   } @$list]
}

sub transform {
  my ($t, $map) = @_;

  if(!ref($t)) { $t }
  elsif(ref($t) eq ARRAY) { [map { transform($_, $map) } @$t] }
  elsif(ref($t) eq HASH) {
#    print "$t->{T}\n";
    my $fun = $map->{$t->{T}};
    if(defined $fun) {
      transform(&$fun($t), $map);
    }
    else {
      { T => $t->{T},
	A => $t->{A},
        B => transform($t->{B}, $map) }
    }
  }
  else { die }
}

# Expands the <paras> tag
sub expand_paras {
  my ($data) = @_;
  transform($data,
	    { 'paras' => sub {
		my ($d) = @_;
		introduce_paras('p', split_para_list($d->{B}))
	      }
	    });
}

sub group_sections {
  my @list = flatten_lists(\@_);
  my $f;
  $f = sub {
    my ($level, $out) = @_;
    while(@list) {
      my $elt = $list[0];
      if(ref($elt) eq HASH && $elt->{T} =~ /^h(\d+)$/i) {
	my $l = $1;
	if($l > $level) {
	  shift(@list);
	  my $out2 = [];
	  push(@$out, tag('section', tag('title', $elt->{B}), $out2));
	  &$f($l, $out2);
	}
	else { last }
      }
      else {
	shift(@list);
	push(@$out, $elt);
      }
    }
  };
  my $out = [];
  &$f(0, $out);
  if(scalar(@list) > 0) { die }
  $out
}

1;
