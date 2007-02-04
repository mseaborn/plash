
package PkgConfig;


sub ensure_dir_exists {
  my ($dir) = @_;
  if(!-e $dir) {
    mkdir($dir) || die "Can't create directory \"$dir\": $!";
  }
  return $dir;
}


sub get_arch {
  "i386"
}

sub package_list_cache_dir {
  ensure_dir_exists("package-lists")
}

sub package_list_combined {
  package_list_cache_dir().'/Packages.combined';
}

# Cache dir for unpacked package trees
sub unpack_cache_dir {
  ensure_dir_exists("unpack-cache");
}

1;
