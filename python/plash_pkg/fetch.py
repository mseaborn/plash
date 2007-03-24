
"""
Usage: plash-pkg-fetch <package-list-file>

Takes a package list and ensures that the .debs it lists are locally
available by downloading them.
"""

import os
import re
import subprocess
import sys

import plash_pkg.config
import plash_pkg.control
import plash_pkg.utils


class PackageToDownload(object):

    def __init__(self, pkg):
        self.pkg = pkg
        self.done = False
        self.url = None
        
        match = re.search("/([^/]+)$", pkg["filename"])
        assert match is not None
        leafname = match.group(1)
        self.local_file = os.path.join(plash_pkg.config.get_deb_cache_dir(),
                                       leafname)
        if os.path.exists(self.local_file):
            self.done = True
        
        if not self.done:
            apt_cache_file = os.path.join("/var/cache/apt/archives",
                                          leafname)
            if os.path.exists(apt_cache_file):
                rc = subprocess.call(["cp", apt_cache_file, self.local_file])
                assert rc == 0
                self.done = True

        if not self.done:
            if "base-url" not in pkg:
                raise Exception("Can't make URL for package: "
                                "Base-URL field missing")
            if "filename" not in pkg:
                raise Exception("Can't make URL for package: "
                                "Filename field missing")
            self.url = plash_pkg.utils.join_with_slash(pkg["base-url"],
                                                       pkg["filename"])

    def get_size(self):
        return int(self.pkg["size"])

    def download(self):
        print "getting %s %s" % (self.pkg["package"],
                                 self.pkg["version"])
        rc = subprocess.call(["curl", self.url, "-o", self.local_file])
        assert rc == 0


def main(args):
    if len(args) != 1:
        print __doc__
        return 1
    package_list = args[0]
    packages = []
    fh = open(package_list, "r")
    try:
        for block in plash_pkg.control.file_blocks(fh):
            fields = plash_pkg.control.block_fields(block)
            if "filelist-ref" not in fields:
                packages.append(PackageToDownload(fields))
    finally:
        fh.close()

    total_size = 0
    remaining_size = 0
    to_get = 0
    for pkg in packages:
        total_size += pkg.get_size()
        if not pkg.done:
            remaining_size += pkg.get_size()
            to_get += 1
            print ("%10.1fMb  %s %s"
                   % (pkg.get_size() / (1024 * 1024),
                      pkg.pkg["package"],
                      pkg.pkg["version"]))
    print ("%i of %i packages to get: %.1fMb of %.1fMb"
           % (to_get, len(package_list),
              remaining_size / (1024 * 1024),
              total_size / (1024 * 1024)))
    if to_get > 0:
        print "download? [Yn] ",
        reply = sys.stdin.readline().rstrip()
        if not re.match("y?$", reply, re.I):
            sys.exit(1)
        for pkg in packages:
            if not pkg.done:
                pkg.download()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
