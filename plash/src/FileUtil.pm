
package FileUtil;

require Exporter;
@ISA = qw(Exporter);
@EXPORT_OK = qw(read_file write_file write_file_if_changed);

use IO::File;


sub read_file {
  my ($file) = @_;
  my $f = IO::File->new($file, 'r');
  if(!defined $f) { die "Can't open `$file' for reading" }
  my $data = join('', <$f>);
  $f->close();
  $data;
}

sub write_file {
  my ($file, $data) = @_;
  my $f = IO::File->new($file, 'w');
  if(!defined $f) { die "Can't open `$file' for writing" }
  print $f $data;
  $f->close();
}


# Don't write out the file if there has been no change.
# Saves having to recompile files that depend on this output.
sub write_file_if_changed {
  my ($file, $data) = @_;

  if(!-e $file || $data ne read_file($file)) {
    write_file($file, $data);
  }
}
