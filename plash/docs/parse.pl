#!/usr/bin/perl -w

use IO::File;
use XXMLParse qw(tag);


my $subst =
  { map { $_ => XXMLParse::read_file("$_.xxml") }
    ('shell',
     'environment',
     'exec-objs',
     'protocols',
     'methods',
     'internals',
     'bugs',
     'copyright',
     'man_pola-shell',
     'man_plash-opts',
     'man_exec-object',
     'man_chroot',
     'man_socket-publish',
     'man_socket-connect',
     'man_run-emacs',
     'man_run-as-anonymous',
     'man_gc-uid-locks',
     'man_pola-run',
    ) };

# use Data::Dumper;
# print Dumper($got);

my $aliases =
  { 'r' => 'replaceable',
    'ul' => 'itemizedlist',
    'li' => 'listitem',
    'em' => 'emphasis',
    'f' => 'function',
    'pre' => 'programlisting',
  };

sub transform {
  my ($t, $map) = @_;

  if(!ref($t)) { $t }
  elsif(ref($t) eq ARRAY) { [map { transform($_, $map) } @$t] }
  elsif(ref($t) eq HASH) {
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

sub traverse {
  my ($t) = @_;

  if(!ref($t)) { $t }
  elsif(ref($t) eq ARRAY) {
    [map { traverse($_) } @$t]
  }
  elsif($t->{T} eq 'c') { '' } # remove comments
  elsif($t->{T} eq 'ps') { XXMLParse::paras(@{traverse($t->{B})}) }
  elsif($t->{T} eq 'splice') {
    my $n = XXMLParse::get_attr($t, 'name');
    traverse($subst->{$n}
	     || die "$n not defined")
  }
  elsif($t->{T} eq 'programlisting') {
    tag('programlisting', XXMLParse::preformatted(@{traverse($t->{B})}))
  }
  elsif($t->{T} eq 'xxmlexample') {
    my @body = remove_ws(map { traverse($_) } @{$t->{B}});
    die unless scalar(@body) == 2;
    die unless $body[0]{T} eq 'xxml';
    die unless $body[1]{T} eq 'xml';
    traverse(tag('variablelist',
		 tag('varlistentry',
		     tag('term', 'XXML'),
		     tag('listitem', tag('programlisting', $body[0]{B}))),
		 tag('varlistentry',
		     tag('term', 'XML'),
		     tag('listitem', tag('programlisting', $body[1]{B})))))
  }
  # disabled
  elsif($t->{T} eq 'xxmlexample' && 0) {
    my @body = remove_ws(map { traverse($_) } @{$t->{B}});
    die unless scalar(@body) == 2;
    die unless $body[0]{T} eq 'xxml';
    die unless $body[1]{T} eq 'xml';
    tagp('informaltable', [['pgwide', 1]],
	 tagp('tgroup', [['cols', 2], ['align', 'left']],
	      tagp('colspec', [['colwidth', '1*']]),
	      tagp('colspec', [['colwidth', '1*']]),
	      tag('thead', tag('row',
			       tag('entry', 'XXML'),
			       tag('entry', 'XML'))),
	      tag('tbody', tag('row',
			       tag('entry', tag('programlisting', $body[0]{B})),
			       tag('entry', tag('programlisting', $body[1]{B}))))))
  }
  elsif($t->{T} eq 'dl') {
    # DocBook doesn't offer a direct equivalent of HTML's dl tag.
    # It has variablelist, but when an entry has multiple term tags,
    # it will put these on the same line, separated by a comma.
    # This is really misleading for some of my uses.
    # This hack tries to get back the dl tag.
    # When there are multiple "dt"s to one "dd", multiple entries are
    # created in the variablelist.
    my @body = remove_ws(@{$t->{B}});
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
    traverse(tag('variablelist', @out));
  }
  else {
    my $a = $aliases->{$t->{T}};
    if(defined $a) { tag($a, traverse($t->{B})) }
    else {
      { T => $t->{T},
	A => $t->{A},
	B => traverse($t->{B}) }
    }
  }
}

# Remove whitespace elements from a list
sub remove_ws {
  grep { /\S/ } @_
}


sub show_tags {
  my ($data) = @_;
  my $tags = {};
  my $f;
  $f = sub {
    my ($t) = @_;
    if(ref($t) eq ARRAY) { foreach(@$t) { &$f($_) } }
    elsif(ref($t) eq HASH) {
      push(@{$tags->{$t->{T}}}, join(' ', map { $_->[0] } @{$t->{A}}));
      &$f($t->{B});
    }
  };
  &$f($data);
  foreach my $tag (sort(keys(%$tags))) { print "$tag\n"; }
}

my $expand =
  { 'splice' =>
    sub {
      my ($t) = @_;
      my $n = XXMLParse::get_attr($t, 'name');
      $subst->{$n} || die "$n not defined"
    },
  };


my $in_file = 'index.xxml';
my $got = XXMLParse::read_file($in_file);
$got = transform($got, $expand);

show_tags($got);

my $out_file = 'out-test.xml';
my $out = IO::File->new($out_file, 'w') || die "Can't open \"$out_file\"";

print $out <<'END';
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE book PUBLIC "-//OASIS//DTD DocBook XML V4.2//EN"
     "http://www.oasis-open.org/docbook/xml/4.2/docbookx.dtd">
END
XXMLParse::to_html($out, traverse($got));
$out->close();
