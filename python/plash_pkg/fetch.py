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


def deb_filename(pkg):
    return "%(package)s_%(version)s_%(architecture)s_%(sha1)s.deb" % pkg


class PackageToDownload(object):

    def __init__(self, pkg):
        self.pkg = pkg
        self.done = False
        self.url = None

        self.local_file = os.path.join(plash_pkg.config.get_deb_cache_dir(),
                                       deb_filename(pkg))
        if os.path.exists(self.local_file):
            self.done = True
        else:
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
           % (to_get, len(packages),
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
