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

import unittest

import plash_pkg.choose


class ChooseTest(unittest.TestCase):

    def test(self):
        chooser = plash_pkg.choose.DepChooser()

        def make_pkg(d):
            d["version"] = "1.0-1"
            return d
        editor = make_pkg({"package": "editor",
                           "depends": "libgtk"})
        libgtk = make_pkg({"package": "libgtk",
                           "depends": "libcircular"})
        libcircular = make_pkg({"package": "libcircular",
                                "depends": "libgtk"})
        libuseless = make_pkg({"package": "libuseless"})
        bash = make_pkg({"package": "bash",
                         "essential": "yes"})
        for pkg in [editor, libgtk, libcircular, libuseless, bash]:
            chooser.add_avail_package(pkg)

        chooser.search_deps("editor")
        chooser.add_essential_packages()
        self.assertEquals(chooser.get_output(),
                          [libcircular, libgtk, editor, bash])


if __name__ == "__main__":
    unittest.main()
