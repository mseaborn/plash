#!/usr/bin/perl -w

# Show what symbols have been removed from and added to glibc.
# Show what symbols the code I've added uses.

my $added = {};
my $removed = {};

print "The following symbols are resolved from outside:\n";
foreach my $x (@{nm('mrs/combined.os')}) {
  if($x->{Type} eq 'U') { print "  $x->{Name}\n"; }
  if($x->{Type} =~ /^[TW]$/) {
    $added->{$x->{Name}} = 1;
  }
}

foreach my $x (map { @{nm($_)} } @ARGV) {
  if($x->{Type} =~ /^[TW]$/) {
    $removed->{$x->{Name}} = 1;
  }
}

print "The following symbols have been removed from glibc but not added:\n";
foreach my $n (sort(keys(%$removed))) {
  if(!$added->{$n}) {
    print "  $n\n";
  }
}

print "The following symbols have been added to glibc but were not in the original:\n";
foreach my $n (sort(keys(%$added))) {
  if(!$removed->{$n}) {
    print "  $n\n";
  }
}


sub nm {
  my ($file) = @_;
  my @lines = split(/\n/, `nm $file`);
  my @got;
  foreach my $line (@lines) {
    next if($line =~ /^\s+$/);
    if($line =~ /^([a-zA-Z0-9 ]{8})\s+([A-Za-z?])\s+/) {
      push(@got, { Addr => $1, Type => $2, Name => $' });
    }
    else {
      print "Unrecognised line: `$line'\n";
    }
  }
  \@got
}
