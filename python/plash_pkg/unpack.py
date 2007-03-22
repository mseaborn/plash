
"""
Usage: plash-pkg-unpack <package-list> <dest-dir>

Unpacks the packages listed in <package-list> into <dest-dir>.
Re-uses/shares file inodes by hard linking them from a cache
directory.
"""

import os
import re
import sha
import shutil
import subprocess
import sys

import plash_pkg.config
import plash_pkg.control


verbose = True

def read_control_file(filename):
    fh = open(filename, "r")
    try:
        return [plash_pkg.control.block_fields(b)
                for b in plash_pkg.control.file_blocks(fh)]
    finally:
        fh.close()

def sha_of_file(filename):
    digest = sha.new()
    fh = open(filename, "r")
    try:
        while True:
            data = fh.read(4096)
            if len(data) == 0:
                break
            digest.update(data)
        return digest
    finally:
        fh.close()


class DebCache(object):
    """Represents cache of .deb package files."""

    def __init__(self):
        self._cache_dir = plash_pkg.config.get_deb_cache_dir()

    def get_deb(self, pkg):
        match = re.search("/([^/]+)$", pkg["filename"])
        assert match is not None
        leafname = match.group(1)
        deb_file = os.path.join(self._cache_dir, leafname)
        if not os.path.exists(deb_file):
            raise Exception("File not present: %s" % deb_file)

        if "sha1" not in pkg:
            raise Exception("Package %(package)s %(version)s lacks a SHA1 field"
                            % pkg)

        # Check hash.  The contents of the file will be in the cache for
        # when we unpack the .deb afterwards.
        hash_expect = pkg["sha1"]
        hash_got = sha_of_file(deb_file).hexdigest()
        if hash_expect != hash_got:
            raise Exception("Hashes do not match on file: %s" % deb_file)

        return deb_file


class UnpackCache(object):
    """Cache of unpacked package trees."""

    def __init__(self, deb_cache):
        self._deb_cache = deb_cache
        self._cache_dir = plash_pkg.config.get_unpack_cache_dir()

    def _unpack_deb(self, pkg, dest_dir):
        """Unpack the data.tar.gz and control.tar.gz parts of a .deb."""
        deb_file = self._deb_cache.get_deb(pkg)
        dest_data_dir = os.path.join(dest_dir, "data")
        dest_control_dir = os.path.join(dest_dir, "control")
        rc = subprocess.call(["dpkg-deb", "-x", deb_file, dest_data_dir])
        assert rc == 0
        rc = subprocess.call(["dpkg-deb", "-e", deb_file, dest_control_dir])
        assert rc == 0

    def get_unpacked(self, pkg):
        out_dir = os.path.join(self._cache_dir,
                               "%(package)s_%(version)s_%(sha1)s" % pkg)
        if not os.path.exists(out_dir):
            print "unpack %(package)s %(version)s" % pkg
            temp_dir = "%s.tmp" % out_dir
            if os.path.exists(temp_dir):
                raise Exception("Remove %s and try again" % temp_dir)
            os.mkdir(temp_dir)
            try:
                self._unpack_deb(pkg, temp_dir)
            except:
                shutil.rmtree(temp_dir, ignore_errors=True)
                raise
            os.rename(temp_dir, out_dir)
        return out_dir


class FileCache(object):
    """Cache of individual files, named by hash."""

    def __init__(self):
        self._cache_dir = plash_pkg.config.get_file_cache_dir()

    def get_file_by_hash(self, file_hash):
        return os.path.join(self._cache_dir, file_hash)


def unpack_file_list(file_cache, dest_dir, ref):
    fh = open(file_cache.get_file_by_hash(ref), "r")
    for line in fh:
        line = line.rstrip("\n")
        if line != "":
            file_hash, dest_path = line.split("\t", 1)
            # don't unpack into root!  remove this later...
            dest_path = dest_path.lstrip("/")
            mkdir_p(os.path.join(dest_dir,
                                 os.path.dirname(dest_path)))
            os.link(file_cache.get_file_by_hash(file_hash),
                    os.path.join(dest_dir, dest_path))

def unpack_to_single_dir(unpack_cache, file_cache, dest_dir, pkg):
    if "filelist-ref" in pkg:
        unpack_file_list(file_cache, dest_dir, pkg["filelist-ref"])
    else:
        src = unpack_cache.get_unpacked(pkg)
        src_data = os.path.join(src, "data")
        for leaf in os.listdir(src_data):
            rc = subprocess.call(["cp", "-lr", os.path.join(src_data, leaf),
                                  dest_dir])
            assert rc == 0

def mkdir_p(dir_path):
    if not os.path.exists(dir_path):
        os.makedirs(dir_path)

def fill_out_dpkg_status(dest_dir, packages):
    dpkg_dir = os.path.join(dest_dir, "var", "lib", "dpkg")
    mkdir_p(dpkg_dir)
    fh = open(os.path.join(dpkg_dir, "status"), "w")
    try:
        for pkg in packages:
            fh.write("Package: %(package)s\n"
                     "Version: %(version)s\n"
                     "Status: install ok installed\n\n"
                     % pkg)
    finally:
        fh.close()

def main(args):
    if len(args) != 2:
        print __doc__
        return 1
    package_list = args[0]
    dest_dir = args[1]
    deb_cache = DebCache()
    file_cache = FileCache()
    unpack_cache = UnpackCache(deb_cache)
    packages = read_control_file(package_list)
    os.mkdir(dest_dir)
    for pkg in packages:
        unpack_to_single_dir(unpack_cache, file_cache, dest_dir, pkg)
    fill_out_dpkg_status(dest_dir, packages)
