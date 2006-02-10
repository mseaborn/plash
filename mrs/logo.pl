#!/usr/bin/perl -w

use Math::Trig;


my $pix1 = [
"XXXX  X      XXX   XXX  X   X                 ",
"X   X X     X   X X   X X   X                 ",
"X   X X     X   X X     X   X                 ",
"XXXX  X     XXXXX  XXX  XXXXX                 ",
"X     X     X   X     X X   X                 ",
"X     X     X   X X   X X   X                 ",
"X     XXXXX X   X  XXX  X   X                 ",
];

my $pix2 =
  [
   "XXXXX    X          XXX      XXX    X     X           ",
   "X    X   X         X   X    X   X   X     X              ",
   "X     X  X        X     X  X     X  X     X              ",
   "X     X  X        X     X  X        X     X              ",
   "X    X   X        X     X   X       X     X               ",
   "XXXXX    X        XXXXXXX    XXX    XXXXXXX               ",
   "X        X        X     X       X   X     X            ",
   "X        X        X     X        X  X     X                     ",
   "X        X        X     X  X     X  X     X              ",
   "X        X        X     X   X   X   X     X            ",
   "X        XXXXXXX  X     X    XXX    X     X            ",
  ];

sub bitmap {
  my ($data, $style, $scale1) = @_;
  my $scale = $scale1 / scalar(@$data);
  my @out;
  my $y = 0;
  foreach my $line (reverse @$data) {
    my $x = 0;
    for($x = 0; $x < length($line); $x++) {
      if(substr($line, $x, 1) ne ' ') {
	my $path =
	  ['newpath',
	   $x*$scale, $y*$scale, 'moveto',
	   ($x+1)*$scale, $y*$scale, 'lineto',
	   ($x+1)*$scale, ($y+1)*$scale, 'lineto',
	   $x*$scale, ($y+1)*$scale, 'lineto',
	   'closepath'];
	if($style eq Fill) { push(@out, [$path, 'fill']) }
	elsif($style eq Outline) { push(@out, [$path, 'stroke']) }
	elsif($style eq Inv) {
	  push(@out, [$path, 'fill',
		      'gsave', [1,1,1], 'setrgbcolor', $path, 'stroke', 'grestore'])
	}
	else { die }
      }
    }
    $y++;
  }
  [@out]
}


sub posn {
  my ($p) = @_;
  [$p->{X}, $p->{Y}]
}

sub put_at {
  my ($x, $pos) = @_;
  ['gsave',
   $pos->{X}, $pos->{Y}, 'translate', $x,
   'grestore']
}

sub put_at1 {
  my ($x, $h, $pos) = @_;
  ['gsave',
   $pos->{X} - $x->[1]{$h}{X}, $pos->{Y} - $x->[1]{$h}{Y}, 'translate',
   $x->[0],
   'grestore']
}

sub rotate {
  my ($x, $a) = @_;
  ['gsave',
   $a, 'rotate', $x,
   'grestore']
}

sub scale {
  my ($x, $f) = @_;
  ['gsave',
   $f, $f, 'scale', $x,
   'grestore']
}


sub box {
  my ($opts) = @_;
  my $c1 = { X => -$opts->{Size}{X}/2, Y => -$opts->{Size}{Y}/2 };
  my $c2 = { X => -$opts->{Size}{X}/2, Y => $opts->{Size}{Y}/2 };
  my $c3 = { X => $opts->{Size}{X}/2, Y => $opts->{Size}{Y}/2 };
  my $c4 = { X => $opts->{Size}{X}/2, Y => -$opts->{Size}{Y}/2 };

  [
   ['%box', 'newpath',
    posn($c1), 'moveto',
    posn($c2), 'lineto',
    posn($c3), 'lineto',
    posn($c4), 'lineto',
    'closepath',
    'stroke'],
   { T => { X => 0, Y => $opts->{Size}{Y}/2 },
     B => { X => 0, Y => -$opts->{Size}{Y}/2 },
     L => { X => -$opts->{Size}{Y}/2, Y => 0 },
     R => { X => $opts->{Size}{Y}/2, Y => 0 },
   }
  ]
}

sub box3d {
  my ($opts) = @_;
  my $c1 = { X => -$opts->{Size}{X}/2, Y => -$opts->{Size}{Y}/2 };
  my $c2 = { X => -$opts->{Size}{X}/2, Y => $opts->{Size}{Y}/2 };
  my $c3 = { X => $opts->{Size}{X}/2, Y => $opts->{Size}{Y}/2 };
  my $c4 = { X => $opts->{Size}{X}/2, Y => -$opts->{Size}{Y}/2 };

  my $sq =
   ['%box', 'newpath',
    posn($c1), 'moveto',
    posn($c2), 'lineto',
    posn($c3), 'lineto',
    posn($c4), 'lineto',
    'closepath',
    'stroke'];
  my $line =
    ['newpath',
     -$opts->{Offset}{X}/2, -$opts->{Offset}{Y}/2, 'moveto',
     $opts->{Offset}{X}/2, $opts->{Offset}{Y}/2, 'lineto',
     'stroke'];

  [[put_at($sq, { X => -$opts->{Offset}{X}/2, Y => -$opts->{Offset}{Y}/2 }),
    put_at($sq, { X => $opts->{Offset}{X}/2, Y => $opts->{Offset}{Y}/2 }),
    put_at($line, $c1),
    put_at($line, $c2),
    put_at($line, $c3),
    put_at($line, $c4),
   ],
   { T => { X => 0, Y => $opts->{Size}{Y}/2 },
     B => { X => 0, Y => -$opts->{Size}{Y}/2 },
     L => { X => -$opts->{Size}{Y}/2, Y => 0 },
     R => { X => $opts->{Size}{Y}/2, Y => 0 },
   }
  ]
}

sub refarrow {
  my ($opts) = @_;
  my $s = $opts->{Size};
  my $a = $opts->{Angle};
  ['%refarrow', 'newpath',
   -$s * sin($a), $s * cos($a), 'moveto',
   0, 0, 'lineto',
   $s * sin($a), $s * cos($a), 'lineto',
   'stroke']
}

sub msgarrow {
  my ($opts) = @_;
  my $s = $opts->{Size};
  my $a = $opts->{Angle};
  my $path =
    ['%refarrow', 'newpath',
     -$s * sin($a), $s * $opts->{Fract} * cos($a), 'moveto',
     0, -$s * (1-$opts->{Fract}) * cos($a), 'lineto',
     $s * sin($a), $s * $opts->{Fract} * cos($a), 'lineto',
     'closepath'];
  ['gsave', ($opts->{Open} ? [1,1,1] : [0,0,0]), 'setrgbcolor', $path, 'fill', 'grestore',
   $path, 'stroke']
}

sub refline {
  my ($opts, $a, $b, $angle) = @_;
  ['%refline', 'newpath',
   posn($a), 'moveto',
   posn($b), 'lineto',
   'stroke',
   put_at(rotate(refarrow($opts->{Arrow}), $angle), $b)]
}


my @logos;

{
my $arrow = { Size => 10, Angle => deg2rad(30), Fract => 0, Open => 0 };
my $refline = { Arrow => $arrow };
my $box = { Size => { X => 50, Y => 50 },
	    Offset => { X => 10, Y => 10 } };

my $b = box($box);
my $len = 100;
my $p1 = { X=>0, Y=>$len };
my $p2 = { X=>0, Y=>-$len };
my $pm = { X=>0, Y=>0 };
my $pr = { X=>$len, Y=>0 };
my $top =
  [put_at1($b, 'B', $p1),
   refline($refline, $p1, $p2, 0),
   put_at1($b, 'T', $p2),
   refline($refline, $pm, $pr, 90),
   put_at1($b, 'L', $pr),
   msgarrow($arrow),
   put_at(bitmap($pix1, Fill, 60), { X=>$len + 90, Y=>-30 }),
  ];
push(@logos, $top);
}

{
my $arrow = { Size => 10, Angle => deg2rad(30), Fract => 0.5, Open => 1 };
my $refline = { Arrow => $arrow };
my $box = { Size => { X => 50, Y => 50 },
	    Offset => { X => 10, Y => 10 } };

my $b = box3d($box);
my $len = 100;
my $p1 = { X=>0, Y=>$len };
my $p2 = { X=>0, Y=>-$len };
my $pm = { X=>0, Y=>0 };
my $pr = { X=>$len, Y=>0 };
my $top =
  [put_at1($b, 'B', $p1),
   refline($refline, $p1, $p2, 0),
   put_at1($b, 'T', $p2),
   refline($refline, $pm, $pr, 90),
   put_at1($b, 'L', $pr),
   msgarrow($arrow),
   put_at(bitmap($pix2, Outline, 60), { X=>$len + 90, Y=>-30 }),
  ];
push(@logos, $top);
}

{
my $arrow = { Size => 10, Angle => deg2rad(30), Fract => 1/4, Open => 0 };
my $refline = { Arrow => $arrow };

my $b1 = box3d({ Size => { X => 50, Y => 50 },
		 Offset => { X => 10, Y => 10 } });
my $b2 = box3d({ Size => { X => 60, Y => 60 },
		 Offset => { X => 10, Y => 10 } });
my $len = 50;
my $p1 = { X=>0, Y=>$len };
my $p2 = { X=>0, Y=>-$len };
my $pm = { X=>0, Y=>0 };
my $pr = { X=>$len, Y=>0 };
my $top =
  [put_at1($b1, 'B', $p1),
   refline($refline, $p1, $p2, 0),
   put_at1($b1, 'T', $p2),
   refline($refline, $pm, $pr, 90),
   put_at1($b2, 'L', $pr),
   msgarrow($arrow),
   put_at(bitmap($pix1, Inv, 60), { X=>$len + 90, Y=>-30 }),
  ];
push(@logos, $top);
}

{
my $arrow = { Size => 10, Angle => deg2rad(30), Fract => 1/3, Open => 0 };
my $refline = { Arrow => $arrow };

my $b1 = box3d({ Size => { X => 30, Y => 30 },
		 Offset => { X => 7, Y => 7 } });
my $b2 = box3d({ Size => { X => 60, Y => 60 },
		 Offset => { X => 10, Y => 10 } });
my $len = 50;
my $p1 = { X=>0, Y=>$len };
my $p2 = { X=>0, Y=>-$len };
my $pm = { X=>0, Y=>0 };
my $pr = { X=>$len, Y=>0 };
my $top =
  [put_at1($b1, 'B', $p1),
   refline($refline, $p1, $p2, 0),
   put_at1($b1, 'T', $p2),
   refline($refline, $pm, $pr, 90),
   put_at1($b2, 'L', $pr),
   msgarrow($arrow),
   put_at(bitmap($pix2, Fill, 60), { X=>$len + 90, Y=>-30 }),
  ];
push(@logos, $top);
}

my $pos = 100;
foreach my $logo (@logos) {
  output([put_at(scale($logo, 0.5), { X => 200, Y => $pos })]);
  $pos += 200;
}


sub output {
  my ($x) = @_;
  if(ref $x eq 'ARRAY') {
    foreach (@$x) { output($_) }
  }
  else {
    #if($x eq '') { die }
    print "$x\n";
  }
}
