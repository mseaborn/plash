#!/usr/bin/perl -w

use Unpack;
use PkgConfig;
use POSIX;
use Cwd qw(abs_path);


sub usage {
  print STDERR "Usage:\n";
  print STDERR "$0 <package-dir> --init <package-spec-file>\n";
  print STDERR "$0 <package-dir> --get\n";
  print STDERR "$0 <package-dir> --inst\n";
  exit(1);
}

if(scalar(@ARGV) < 2) {
  usage();
}
my $output_dir = shift(@ARGV);

if($ARGV[0] eq '--init' && scalar(@ARGV) == 2) {
  PkgConfig::ensure_dir_exists($output_dir);
  write_file("$output_dir/config", read_file($ARGV[1]));
}
elsif($ARGV[0] eq '--get' && scalar(@ARGV) == 1) {
  my $config = read_config_file("$output_dir/config");
  choose_and_unpack($output_dir, $config);
  init_write_layer($output_dir);
}
elsif($ARGV[0] eq '--inst' && scalar(@ARGV) == 1) {
  my $config = read_config_file("$output_dir/config");
  PkgConfig::ensure_dir_exists("desktop-files");
  
  # Add a prefix to the name of the .desktop file so that it doesn't
  # clash with existing .desktop files and override them.
  write_file(sprintf("desktop-files/plash-%s.desktop",
		     get_field($config, 'pet-id')),
	     desktop_file($config));
}
else {
  usage();
}


sub read_config_file {
  my ($file) = @_;
  my $list = Unpack::read_control_file($file,
				       [qw(nick-name pet-name pet-id icon
					   depends mimetype)]);
  die if scalar(@$list) != 1;
  return $list->[0];
}

sub get_field {
  my ($spec, $field) = @_;
  if(!defined $spec->{$field}) {
    die "Field \"$field\" missing";
  }
  return $spec->{$field};
}


sub choose_and_unpack {
  my ($output_dir, $config) = @_;
  
  run_cmd("./choose.pl", $output_dir, get_field($config, 'depends'));
  run_cmd("./fetch.pl", "$output_dir/package-list");
  
  my $unpack_dir = "$output_dir/unpacked";
  # Delete any existing unpacked tree
  if(-d $unpack_dir) {
    run_cmd("rm", "-rf", $unpack_dir);
  }
  run_cmd("./unpack.pl", "$output_dir/package-list", $unpack_dir);
}

sub init_write_layer {
  my ($output_dir) = @_;

  my $dest = "$output_dir/write_layer";
  if(!-e $dest) {
    PkgConfig::ensure_dir_exists($dest);

    # Many programs rely on being able to get the user name from their
    # UID, so create a simple /etc/passwd file.
    my $uid = getuid();
    my @pw_ent = getpwuid($uid);
    if(scalar(@pw_ent) > 0) {
      my $user_name = $pw_ent[0];
      my $home_dir = $pw_ent[7];
      PkgConfig::ensure_dir_exists("$dest/etc");
      write_file("$dest/etc/passwd",
		 sprintf("%s:x:%i:%i::%s:/bin/false\n",
			 $user_name, $uid, $uid, $home_dir));
      
      run_cmd('mkdir', '-p', "$dest/$home_dir");
    }
    else {
      warn "No passwd entry found for $uid";
    }
  }
}

sub desktop_file {
  my ($spec) = @_;
  
  my @fields;
  push(@fields, ['Encoding', 'UTF-8']);
  push(@fields, ['Type', 'Application']);
  push(@fields, ['Terminal', 'false']);
  
  push(@fields, ['Name', $spec->{'pet-name'}]);
  if(defined $spec->{mimetype}) {
    push(@fields, ['MimeType', $spec->{mimetype}]);
  }
  if(defined $spec->{icon}) {
    push(@fields, ['Icon', $spec->{icon}]);
  }

  my $launcher = abs_path("launch-pet-app.py");
  my $abs_output_dir = abs_path($output_dir);  
  # FIXME: should quote unusual chars in substituted strings
  push(@fields, ['Exec', "$launcher --app-dir $abs_output_dir --open-files %f"]);
  push(@fields, ['Categories', 'SandboxedApps;']);

  my $out = '';
  $out .= "[Desktop Entry]\n";
  foreach my $field (@fields) {
    if(!defined $field->[1]) { die "Field $field->[0] not defined" }
    if($field->[1] =~ /\n/) { die "Field $field->[0] contains newline" }
    $out .= sprintf "%s=%s\n", $field->[0], $field->[1];
  }
  return $out;
}


sub run_cmd {
  my $rc = system(@_);
  if($rc != 0) {
    print join(' ', @_)."\n";
    die "Return code $rc";
  }
}

sub read_file {
  my ($file) = @_;
  my $f = IO::File->new($file, 'r');
  if(!defined $f) { die "Can't open `$file' for reading" }
  my $d = join('', <$f>);
  $f->close();
  $d
}

sub write_file {
  my ($file, $data) = @_;
  my $f = IO::File->new($file, 'w');
  if(!defined $f) { die "Can't open `$file' for writing" }
  print $f $data;
  $f->close();
}
