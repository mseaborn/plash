#!/usr/bin/perl -w

# Outputs info about how to get source from the SVN repository.
# Run "svn update" before running this, otherwise the revision
# number may be incorrect.


my $info = `svn info`;

# Rewrite URL to remove user name
my $url = get_field($info, 'URL');
if($url =~ /^(svn|http):/) {}
elsif($url =~ s#^svn\+ssh://([A-Za-z_]*)@#svn://#) {}
else {
  die "Unrecognised URL format: $url";
}

my $rev = get_field($info, 'Revision');

print "\nSource is available from Subversion repository:\n";
print "URL: $url\n";
foreach my $field ('Revision', 'Last Changed Date', 'Repository UUID') {
  printf "%s: %s\n", $field, get_field($info, $field);
}

print "\nAcquire with:\n";
print "svn co $url -r$rev\n";


sub get_field {
  my ($info, $field) = @_;
  $info =~ /^$field: (.*)$/m || die "Field \"$field\" not present";
  return $1;
}
