# Copyright (C) 2005 Mark Seaborn
#
# This file is part of Plash, the Principle of Least Authority Shell.
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

package XXMLParse;

# \tag;
# \tag: greedy
# \tag- single line
# \tag~ paragraph
# \tag+upto-whitespace
# \tag attr1=text1 attr2={text};
# \tag1\tag2: ...
# \Elt; becomes &lt;
# {...}  these don't affect structure of data on their own (they interpolate)

# Perhaps add a special indicator when the tag uses para separators?
# or when it is whitespace sensitive?
# eg. \*ps
# or \*pre: ...

# Swallow whitespace after tag's [:-~+]

# When you are combining two tags, you're usually going to want to give
# them the same suffix character.  ie. You want them both to consume the
# same amount.
# \listitem~\para~ Text...
# As a shortcut, you can omit the first suffix char:
# \listitem\para~ Text...


use IO::File;

sub read_file {
  my ($file) = @_;
  my $f = IO::File->new($file, 'r');
  if(!defined $f) { die "Can't open file `$file'" }
  my $data = join('', <$f>);
  if($data =~ /\t/) { warn "File `$file' contains tabs" }
  my $x = parse($data, Filename => $file);
  $f->close();
  $x;
}

sub parse {
  my ($data, @args) = @_;
  my $got = [];
  parse_greedy({ Data => $data, @args }, $got);
  $got
}


# Format:
# a tree node is:
#  * text
#  * an array ref
#  * a hash ref of:
#    T => tag name
#    B => body tree
#    A => attributes list of [$name, $val] pairs

sub get_attr {
  my ($t, $n) = @_;
  foreach my $p (@{$t->{A}}) {
    if($p->[0] eq $n) { return flatten_attr($p->[1]) }
  }
  die "Can't get attribute `$n'"
}
sub flatten_attr {
  my ($t) = @_;
  if(!ref($t)) { $t }
  elsif(ref($t) eq ARRAY) { join('', map { flatten_attr($_) } @$t) }
  else { die "Tags not allowed in attributes" }
}

sub to_html {
  my ($out, $t, $in_attr) = @_;
  
  if(!ref($t)) {
    $t =~ s/&/&amp;/g;
    if($in_attr) { $t =~ s/"/&quot;/g; }
    $t =~ s/</&lt;/g;
    $t =~ s/>/&gt;/g;
    print $out $t;
  }
  elsif(ref($t) eq ARRAY) {
    foreach(@$t) { to_html($out, $_, $in_attr); }
  }
  elsif($t->{T} =~ /^E(.*)$/) { print $out "&$1;" }
  elsif($t->{T} eq 'ps') { to_html($out, paras(@{$t->{B}})) }
  else {
    if($in_attr) { warn "Tag inside attribute"; }
    if(scalar(@{$t->{B}}) > 0) {
      print $out "<$t->{T}";
      attrs_to_html($out, $t->{A});
      print $out ">";
      to_html($out, $t->{B});
      print $out "</$t->{T}>";
    }
    else {
      print $out "<$t->{T}";
      attrs_to_html($out, $t->{A});
      print $out " />";
    }
  }
}
sub attrs_to_html {
  my ($out, $attrs) = @_;
  foreach my $a (@$attrs) {
    print $out " $a->[0]";
    if(defined $a->[1]) {
      print $out '="';
      to_html($out, $a->[1], 1);
      print $out '"';
    }
  }
}

# Used for the 'ps' tag:  empty lines are converted into paragraph breaks.
sub paras {
  my $tag_name = 'para'; # for DocBook
  my $paras = [];
  my $last = [];
  foreach my $elt (@_) {
    if(!ref($elt) && $elt =~ /^\n([ \t]*\n)+$/) {
      if(scalar(@$last)) { push(@$paras, { T=>$tag_name, B=>$last }, "\n\n") }
      $last = [];
    }
    else { push(@$last, $elt) }
  }
  if(@$last) { push(@$paras, { T=>$tag_name, B=>$last }) }
  $paras
}

# This is broken because a tag like \pre: swallows whitespace following it,
# and so the indentation that this function sees is not what was in the
# original text.
sub preformatted {
  #printf STDERR "Input: %s\n",
  #  join('', map { if(ref($_)) { '' } else { "<$_>" } } @_);
  my @all =
    map {
      if(!ref($_)) { split(/(\n)/, $_) }
      else { $_ }
    } @_;
  # Split into lines
  my $line = [];
  my $lines = [];
  foreach my $elt (grep { $_ ne '' } @all) {
    if($elt eq "\n") {
      push(@$lines, $line);
      $line = [];
    }
    else {
      push(@$line, $elt);
    }
  }
  push(@$lines, $line);
  # Calculate the amount of indentation that all lines share
  # Empty lines are ignored
  my $indent = 100;
  foreach my $line (@$lines) {
    if(!ref($line->[0])) {
      if(defined $line->[0] && $line->[0] =~ /^( *)\S/) {
	my $n = length($1);
	# print STDERR "got $n\n";
	if($indent > $n) { $indent = $n }
      }
    }
    else { $indent = 0 }
  }
  # Remove first line if empty
  if(scalar(@{$lines->[0]}) == 0) {
    shift(@$lines)
  }
  # Remove last line if empty
  if(scalar(@{$lines->[-1]}) == 0) {
    pop(@$lines)
  }
  # Remove the indentation
  # print STDERR "Indent: $indent\n";
  foreach my $line (@$lines) {
    if(!ref($line->[0])) {
      $line->[0] = substr($line->[0], $indent);
      # print STDERR ">>$line->[0]\n";
    }
  }
  my @out = map { @$_, "\n" } @$lines;
  #printf STDERR "Result: %s\n",
  #  join('', map { if(ref($_)) { '' } else { "<$_>" } } @out);
  @out
}


sub parse_tag {
  my ($a, $got1) = @_;
  if($a->{Data} =~ /\G([\\{}])/gc) { push(@$got1, $1) }
  elsif($a->{Data} =~ /\G([a-zA-Z_][a-zA-Z_0-9]*)/gc) {
    my $tag_name = $1;
    my $body = [];
    my $attrs = [];
    # print "$tag_name\n";

    # For debugging files:
    if($tag_name eq 'STOP') {
      printf STDERR "STOP found, index %i\n", pos($a->{Data});
      pos($a->{Data}) = length($a->{Data});
      return;
    }
    
    while(1) {
      if($a->{Data} =~ /\G\s+/gc) {}
      elsif($a->{Data} =~ /\G;/gc)       { last }
      elsif($a->{Data} =~ /\G:\s*/gc)    { parse_greedy($a, $body); last }
      elsif($a->{Data} =~ /\G-[ \t]*/gc) { parse_line($a, $body); last }
      elsif($a->{Data} =~ /\G~[ \t]*/gc) { parse_para($a, $body); last }
      elsif($a->{Data} =~ /\G\+/gc)      { parse_to_whitespace($a, $body); last }
      elsif($a->{Data} =~ /\G\\/gc)      { parse_tag($a, $body); last }
      elsif($a->{Data} =~ /\G([a-zA-Z_][a-zA-Z_0-9]*)(=?)/gc) {
	my $attr_name = $1;
	my $got = [];
	if($2 eq '=') {
	  my $got = [];
	  parse_attr($a, $got);
	  push(@$attrs, [$attr_name, $got]);
	}
	else { push(@$attrs, [$attr_name]) }
      }
      else {
	parse_error($a);
	die "Bad tag contents"
      }
    }
    push(@$got1, { T => $tag_name, A => $attrs, B => $body });
  }
  else { parse_error($a); die "Bad tag" }
}

sub parse_upto_brace {
  my ($a, $got) = @_;
  parse_greedy($a, $got);
  if($a->{Data} =~ /\G\}/gc) {}
  else { parse_error($a); die 'Missing }' }
}

sub parse_greedy {
  my ($a, $got) = @_;
  while(1) {
    my $i = pos($a->{Data});
    if($a->{Data} =~ /\G\}/gc) { pos($a->{Data}) = $i; last }
    elsif($a->{Data} =~ /\G\{/gc) { parse_upto_brace($a, $got) }
    elsif($a->{Data} =~ /\G\\/gc) { parse_tag($a, $got) }
    elsif($a->{Data} =~ /\G([^\n{}\\]*|\n([ \t]*\n)*)/gc) { push(@$got, $1) }
    elsif($i == length($a->{Data})) { last }
    else { parse_error($a); die "Parse error" }
  }
}

sub parse_line {
  my ($a, $got) = @_;
  while(1) {
    my $i = pos($a->{Data});
    if($a->{Data} =~ /\G[}\n]/gc) { pos($a->{Data}) = $i; last }
    elsif($a->{Data} =~ /\G\{/gc) { parse_upto_brace($a, $got) }
    elsif($a->{Data} =~ /\G\\/gc) { parse_tag($a, $got) }
    elsif($a->{Data} =~ /\G([^\n{}\\]*)/gc) { push(@$got, $1) }
    elsif($i == length($a->{Data})) { last }
    else { parse_error($a); die "Parse error" }
  }
}

sub parse_para {
  my ($a, $got) = @_;
  while(1) {
    my $i = pos($a->{Data});
    if($a->{Data} =~ /\G(\}|\n[ \t]*\n)/gc) { pos($a->{Data}) = $i; last }
    elsif($a->{Data} =~ /\G\{/gc) { parse_upto_brace($a, $got) }
    elsif($a->{Data} =~ /\G\\/gc) { parse_tag($a, $got) }
    elsif($a->{Data} =~ /\G([^\n{}\\]*|\n([ \t]*\n)*)/gc) { push(@$got, $1) }
    elsif($i == length($a->{Data})) { last }
    else { parse_error($a); die "Parse error" }
  }
}

sub parse_to_whitespace {
  my ($a, $got) = @_;
  while(1) {
    my $i = pos($a->{Data});
    if($a->{Data} =~ /\G[} \t\n]/gc) { pos($a->{Data}) = $i; last }
    elsif($a->{Data} =~ /\G\{/gc) { parse_upto_brace($a, $got) }
    elsif($a->{Data} =~ /\G\\/gc) { parse_tag($a, $got) }
    elsif($a->{Data} =~ /\G([^\n{}\\ \t]*)/gc) { push(@$got, $1) }
    elsif($i == length($a->{Data})) { last }
    else { parse_error($a); die "Parse error" }
  }
}

sub parse_attr {
  my ($a, $got) = @_;
  while(1) {
    my $i = pos($a->{Data});
    if($a->{Data} =~ /\G\{/gc) { parse_upto_brace($a, $got) }
    elsif($a->{Data} =~ /\G\\/gc) { parse_tag($a, $got) }
    elsif($a->{Data} =~ /\G([a-zA-Z0-9_]+)/gc) { push(@$got, $1) }
    elsif($i == length($a->{Data})) { last }
    else { last }
  }
}

sub parse_error {
  my ($a) = @_;
  if(defined $a->{Filename}) {
    print STDERR "Error in file \"$a->{Filename}\":\n";
  }
  print STDERR "Error at: ".substr($a->{Data}, pos($a->{Data}))."\n";
}

1;
