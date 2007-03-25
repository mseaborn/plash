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

import plash_pkg.sources
import plash_pkg.control


class SourcesListTest(unittest.TestCase):

    def test1(self):
        line = "deb http://ftp.debian.org/debian etchy main universe"
        self.assertEquals(
            list(plash_pkg.sources.read_sources_list_line(line)),
            [{"S_base": "http://ftp.debian.org/debian",
              "S_component": "dists/etchy/main"},
             {"S_base": "http://ftp.debian.org/debian",
              "S_component": "dists/etchy/universe"},
             ])

    def test2(self):
        line = "deb http://ftp.debian.org/debian dists/etchy/main"
        self.assertEquals(
            list(plash_pkg.sources.read_sources_list_line(line)),
            [{"S_base": "http://ftp.debian.org/debian",
              "S_component": "dists/etchy/main"}])


class BlockTest(unittest.TestCase):

    def test1(self):
        data = ["Field: content",
                "Another-field: more contents"]
        self.assertEquals(plash_pkg.control.block_fields(data),
                          {"field": "content",
                           "another-field": "more contents"})

    def test2(self):
        data = ["Field1: multi-line",
                " contents",
                "Field2: line1",
                " line2"]
        self.assertEquals(plash_pkg.control.block_fields(data),
                          {"field1": "multi-line\ncontents",
                           "field2": "line1\nline2"})

    def test3(self):
        data = ["not a proper field"]
        self.assertRaises(plash_pkg.control.BadControlLineError,
                          plash_pkg.control.block_fields,
                          data)


if __name__ == "__main__":
    unittest.main()
