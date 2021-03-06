# Copyright (C) 2006, 2007 Mark Seaborn
#
# This file is part of Plash.
#
# Plash is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of
# the License, or (at your option) any later version.
#
# Plash is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with Plash; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
# USA.

import os
import subprocess


def ensure_dir_exists(dir_path):
    if not os.path.exists(dir_path):
        os.mkdir(dir_path)
    return dir_path


def get_arch():
    proc = subprocess.Popen(["dpkg-architecture", "-qDEB_HOST_ARCH"],
                            stdout=subprocess.PIPE)
    stdout, stderr = proc.communicate()
    rc = proc.wait()
    assert rc == 0, rc
    return stdout.strip()


def get_cache_dir():
    if "PLASH_PKG_CACHE_DIR" in os.environ:
        return os.environ["PLASH_PKG_CACHE_DIR"]
    else:
        # Default is ~/.cache/plash-pkg
        home = os.environ["HOME"]
        cache = ensure_dir_exists(os.path.join(home, ".cache"))
        return ensure_dir_exists(os.path.join(cache, "plash-pkg"))

def get_package_list_cache_dir():
    return ensure_dir_exists(os.path.join(get_cache_dir(), "package-lists"))

def get_package_list_combined():
    return os.path.join(get_package_list_cache_dir(),
                        "Packages.combined")

def get_deb_cache_dir():
    """Directory where downloaded .deb files are stored."""
    return ensure_dir_exists(os.path.join(get_cache_dir(), "deb-cache"))

def get_unpack_cache_dir():
    """Directory where unpacked copies of packages are stored."""
    return ensure_dir_exists(os.path.join(get_cache_dir(), "unpack-cache"))

def get_file_cache_dir():
    """Directory where immutable files can be stored, indexed by hash."""
    return ensure_dir_exists(os.path.join(get_cache_dir(), "file-cache"))

def get_desktop_files_dir():
    """Directory where generated .desktop files are written."""
    return ensure_dir_exists(os.path.join(get_cache_dir(), "desktop-files"))
