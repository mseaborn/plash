
import os


def ensure_dir_exists(dir_path):
    if not os.path.exists(dir_path):
        os.mkdir(dir_path)
    return dir_path

def get_arch():
    return "i386"

def get_package_list_cache_dir():
    return ensure_dir_exists("package-lists")

def get_package_list_combined():
    return os.path.join(get_package_list_cache_dir(),
                        "Packages.combined")

def get_deb_cache_dir():
    """Directory where downloaded .deb files are stored."""
    return ensure_dir_exists("cache")

def get_unpack_cache_dir():
    """Directory where unpacked copies of packages are stored."""
    return ensure_dir_exists("unpack-cache")

def get_desktop_files_dir():
    """Directory where generated .desktop files are written."""
    return ensure_dir_exists("desktop-files")
