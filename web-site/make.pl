#!/usr/bin/perl -w

use lib qw(../docs);

use XXMLParse qw(tag tagp get_attr get_attr_opt);
use Transforms;
use IO::File;
use File::stat;


# code
# samp (output)
# kbd (input)
# var
my $aliases =
  { 'envar' => 'code',
    'r' => 'var',        # DocBook's <replaceable>; italic
    'option' => 'code',  # DocBook's <option>; usually bold, but not here
    'f' => 'code',       # filename or function?
    'function' => 'code',
    'filename' => 'code',
    'userinput' => 'kbd',
    'command' => 'code',
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

    # 'option' => sub { $_[0]{B} },

    'xxmlexample' =>
    sub {
      my ($t) = @_;
      my @body = grep { /\S/ } @{$t->{B}};
      die unless scalar(@body) == 2;
      die unless $body[0]{T} eq 'xxml';
      die unless $body[1]{T} eq 'xml';
      tagp('table', [['border', 1]],
	   tag('tr',
	       tag('td', 'XXML'),
	       tag('td', tag('pre', $body[0]{B}))),
	   tag('tr',
	       tag('td', 'XML'),
	       tag('td', tag('pre', $body[1]{B}))));
    },
  };


# Generate smaller versions of images
foreach my $base (qw(screenshot-gnumeric
		     screenshot-inkscape
		     screenshot-xemacs)) {
  my @cmd = ('convert', '-scale', '30%x30%',
	     "$base.png", "out/${base}-small.png");
  my $rc = system(@cmd);
  if($rc != 0) { die "Exited with code $rc: ".join(' ', @cmd) }
}


  ### \li- Why POLA
  ### \li- FAQs
my $sidebar = XXMLParse::parse(<<'END');
\table class=sidebar border={0} \tr\td\ul{
  \li\a href={download.html}- Download
  \li\a href={screenshots.html}- Screenshots
  \li\a href={contents.html}- Documentation contents
  \li\a href={examples.html}- Examples
  \li\a href={news.html}- News
  \li\a href={#mailing-list}- Mailing list
  \li\a href={http://svn.gna.org/viewcvs/plash/trunk/}- Browse source code
  \li\a href={#related}- Related systems
  \li\a href={#roadmap}- Roadmap
}
END

sub mail {
  my ($m) = @_;
  tagp('a', [['href', "mailto:$m"]], $m)
}
my $footer =
  [tagp('hr'),
   tagp('div', [['class', 'footer']],
	tagp('table', [['width', '100%']],
	     tagp('tr', [['valign', 'top']],
		  tag('td',
		      'Mark Seaborn ', tag('br'),
		      mail('mseaborn@onetel.com')),
		  tagp('td', [['align', 'right']],
		       tagp('a', [['href', 'contents.html']],
			    'Up: Contents')))))];


# A section is:
#   Level => number
#   Title => text
#   Children => list
my $section = 1;
sub get_contents {
  my ($root, $tree) = @_;
  my @stack = ($root);
  foreach my $elt (Transforms::flatten_lists($tree)) {
    if(ref($elt) eq HASH && $elt->{T} =~ /^h(\d+)$/i) {
      my $l = $1;
      die if !($l > 0);
      my $i = $section++;

      my $node = { Level => $l,
		   Title => $elt->{B},
		   File => $root->{File},
		   Name => get_attr_opt($elt, 'name') || "section$i",
		   Children => [],
		 };
      $elt->{B} =
	tagp('a', [['name', $node->{Name}],
		   ['href', "contents.html#$node->{Name}"]],
	     $node->{Title});
      
      while($stack[0]{Level} >= $l) { shift(@stack); }
      push(@{$stack[0]{Children}}, $node);
      unshift(@stack, $node);
    }
  }
}

sub contents_list {
  my ($node) = @_;
  my @got;

  if(defined $node->{Title}) {
    push(@got,
	 tagp('a',
	      (defined $node->{Name}
	       ? [['href', "$node->{File}#$node->{Name}"],
		  ['name', $node->{Name}]]
	       : [['href', $node->{File}]]),
	      $node->{Title}));
  }
  if(scalar(@{$node->{Children}}) > 0) {
    push(@got,
	 tag('ul',
	     map { tag('li', contents_list($_)) } @{$node->{Children}}));
  }
  \@got
}

my @sections =
  ({ File => 'download', Title => 'Download' },
   { File => 'examples', Title => 'Examples' },
   { File => 'screenshots', Title => 'Screenshots' },
   { File => 'powerbox', Title => 'The powerbox: a GUI for granting authority' },
   { File => 'pola-run', Title => 'pola-run: A command line tool for launching sandboxed programs' },
   { File => 'environment', Title => 'Plash\'s sandbox environment' },
   { File => 'pola-shell', Title => 'pola-shell: A shell for interactive use' },
   { File => 'exec-objs', Title => 'Executable objects: a replacement for setuid executables' },
   { File => 'protocols', Title => 'Communication protocols' },
   { File => 'methods', Title => 'RPC methods' },
   { File => 'news', Title => 'News' },
   { File => 'internals', Title => 'Internals' },
   { File => 'issues', Title => 'Issues' },
   { File => 'copyright', Title => 'Copyright' },
  );

my $files = {};
foreach my $part (@sections,
		  { File => 'index' }) {
  $files->{$part->{File}} =
    Transforms::expand_paras(XXMLParse::read_file("$part->{File}.xxml"));
}

my $contents = {};

{
  my $intro_contents =
    { Level => 0,
      Title => 'Introduction',
      File => "index.html",
      Children => [] };
  get_contents($intro_contents, $files->{'index'});
  push(@{$contents->{Children}}, $intro_contents);
}
     
foreach my $part (@sections) {
  my $tree = $files->{$part->{File}};
  my $sect_contents =
    { Level => 0,
      Title => $part->{Title},
      File => "$part->{File}.html",
      Children => [],
    };
  get_contents($sect_contents, $tree);
  push(@{$contents->{Children}}, $sect_contents);

  write_file("out/$part->{File}.html",
	     tag('html',
		 tag('head',
		     tag('title', 'Plash: ', $part->{Title}),
		     tagp('link', [['rel', 'stylesheet'],
				   ['href', 'styles.css']])),
		 tag('body',
		     # Top bar
		     tagp('table', [['class', 'topbar'],
				    ['width', '100%']],
			  tag('tr',
			      tagp('td', [['width', '22%']],
				  tagp('div', [['class', 'logo']],
				       tagp('img', [['src', 'logo-abc-trans.png']]))),
			      tag('td',
				  tagp('h1', [['class', 'h0']],
				       'Plash: tools for practical least privilege'),
				  tag('h1', $part->{Title})))),
		     tagp('div', [['class', 'body']],
			  tagp('div', [['class', 'indent']],
			       $tree)),
		     $footer)));
}

write_file('out/contents.html',
	   tag('html',
		 tag('head',
		     tag('title', 'Plash: tools for least privilege'),
		     tagp('link', [['rel', 'stylesheet'],
				   ['href', 'styles.css']])),
		 tag('body',
		     # Top bar
		     tagp('table', [['class', 'topbar'],
				    ['width', '100%']],
			  tag('tr',
			      tagp('td', [['width', '22%']],
				  tagp('div', [['class', 'logo']],
				       tagp('img', [['src', 'logo-abc-trans.png']]))),
			      tag('td',
				  tagp('h1', [['class', 'h0']], 'Plash: tools for practical least privilege'),
				  tag('h1', 'Table of contents')))),
		     tagp('div', [['class', 'body']],
			  contents_list($contents)),
		     $footer,
		     )));

write_file('out/index.html',
	   tag('html',
	       tag('head',
		   tag('title', 'Plash: tools for least privilege'),
		   tagp('link', [['rel', 'stylesheet'],
				 ['href', 'styles.css']])),
	       tag('body',
		   # Top bar
		   tagp('table', [['class', 'topbar'],
				  ['width', '100%']],
			tag('tr',
			    tagp('td', [['width', '22%']],
				 tagp('div', [['class', 'logo']],
				      tagp('img', [['src', 'logo-abc-trans.png']]))),
			    tag('td',
				tag('h1', 'Plash: tools for practical least privilege')))),
		   tagp('div', [['class', 'body']],
			tagp('table', [['width', '100%']],
			     tagp('tr', [['valign', 'top']],
				  tag('td',
				      tagp('div', [['class', 'indent']],
					   $files->{'index'})),
				  tag('td', $sidebar)))),
		   
		   #tagp('div', [['align', 'right']],
		   #  $sidebar),
		   #tagp('div', [['class', 'indent']],
		   #  $files->{'index'})
		   $footer,
		  )));


sub get_tags {
  my ($x, $tags) = @_;
  if(!ref($x)) {}
  elsif(ref($x) eq ARRAY) { foreach my $x (@$x) { get_tags($x, $tags) } }
  elsif(ref($x) eq HASH) {
    $tags->{$x->{T}} = 1;
    get_tags($x->{B}, $tags);
  }
  else { die }
}

sub write_file {
  my ($out_file, $data) = @_;

  my @html_tags =
    qw(html
       head title link
       body
       h1 h2 h3
       p div pre
       hr br
       img
       table tr th td
       ul ol li
       dl dt dd
       a strong em cite tt code var samp kbd
       );
  my $html_tags = { map { ($_ => 1) } @html_tags };
  
  print "Writing $out_file\n";

  $data = Transforms::transform($data, $transform_map);

  # Warn about non-HTML tags
  my $tags = {};
  get_tags($data, $tags);
  foreach my $tag (keys(%$tags)) {
    if(!$html_tags->{$tag}) {
      print "  Unknown tag: $tag\n";
    }
  }

  my $f = IO::File->new($out_file, 'w') || die "Can't open '$out_file'";
  XXMLParse::to_html($f, $data);
  $f->close();
}
