#!/usr/bin/perl -w

use lib qw(../docs);

use XXMLParse qw(tag tagp get_attr get_attr_opt);
use Transforms;
use IO::File;
use File::stat;

  ### \li- Why POLA
  ### \li- FAQs
my $sidebar = XXMLParse::parse(<<'END');
\table class=sidebar border={0} \tr\td\ul{
  \li\a href={download.html}- Download
  \li- Screenshots
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

  my $files = {};
  foreach my $base (qw(index download news examples pola-shell)) {
    my $in_file = "$base.xxml";
    
    my $data = Transforms::expand_paras(XXMLParse::read_file($in_file));
    $files->{$base} = $data;
  }

sub contents_list {
  my ($node) = @_;
  my $entry =
    tagp('a', [['href',
		"$node->{File}".
		(defined $node->{Name} ? "#$node->{Name}" : '')],
	       ['name', $node->{Name}]],
	 $node->{Title});
  
  if(scalar(@{$node->{Children}}) > 0) {
    [$entry,
     tag('ul',
	 map { tag('li', contents_list($_)) } @{$node->{Children}})]
  }
  else {
    $entry
  }
}

my @sections =
  ({ File => 'download', Title => 'Download' },
   { File => 'examples', Title => 'Examples' },
   { File => 'pola-shell', Title => 'pola-shell: A shell for interactive use' },
   { File => 'news', Title => 'News' },
  );

my $contents = {};
push(@{$contents->{Children}},
     { Title => 'Introduction',
       File => "index.html" });
my $section = 1;
foreach my $part (@sections) {
  my $tree = $files->{$part->{File}};

  # Section is:
  #   Level
  #   Title
  #   Children
  my $root = { Level => 0,
	       Title => $part->{Title},
	       File => "$part->{File}.html",
	     };
  push(@{$contents->{Children}}, $root);
  my @stack = ($root);
  foreach my $elt (Transforms::flatten_lists($tree)) {
    if(ref($elt) eq HASH && $elt->{T} =~ /^h(\d+)$/i) {
      my $l = $1;
      die if !($l > 0);
      my $i = $section++;

      my $node = { Level => $l,
		   Title => $elt->{B},
		   File => "$part->{File}.html",
		   Name => get_attr_opt($elt, 'name') || "section$i",
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


sub write_file {
  my ($out_file, $data) = @_;
  
  my $f = IO::File->new($out_file, 'w') || die "Can't open '$out_file'";
  XXMLParse::to_html($f, $data);
  $f->close();
}
