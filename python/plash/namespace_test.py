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
import unittest

import plash_core
import plash.env
import plash.marshal
import plash.namespace


class TestObject(plash.marshal.Pyobj_demarshal):

    pass


class MockDir(plash.marshal.Pyobj_demarshal):

    def __init__(self):
        self._calls = []

    def dir_mkdir(self, mode, leafname):
        self._calls.append(leafname)
        # FIXME: shouldn't have to return tuple
        return ()


class NamespaceTest(unittest.TestCase):

    def test_empty_namespace(self):
        namespace = plash.namespace.Namespace()
        self.assertEquals(namespace.get_root_dir().dir_list(), [])

    def test_listing(self):
        namespace = plash.namespace.Namespace()
        namespace.attach_at_path("/foo", TestObject())
        listing = namespace.get_root_dir().dir_list()
        self.assertEquals([entry["name"] for entry in listing],
                          ["foo"])

    def test_multiple_entries(self):
        namespace = plash.namespace.Namespace()
        namespace.attach_at_path("/foo", TestObject())
        namespace.attach_at_path("/bar", TestObject())
        listing = namespace.get_root_dir().dir_list()
        self.assertEquals(sorted(entry["name"] for entry in listing),
                          ["bar", "foo"])

    # FIXME: this test is skipped because it writes to stdout/stderr.
    def x_test_overwrite_entry(self):
        namespace = plash.namespace.Namespace()
        namespace.attach_at_path("/foo", TestObject())
        namespace.attach_at_path("/foo", TestObject())
        listing = namespace.get_root_dir().dir_list()
        self.assertEquals([entry["name"] for entry in listing],
                          ["foo"])

    def test_call(self):
        namespace = plash.namespace.Namespace()
        mock_dir = MockDir()
        namespace.attach_at_path("/", mock_dir)
        namespace.get_root_dir().dir_mkdir(0777, "blargh")
        self.assertEquals(mock_dir._calls, ["blargh"])


class LogTest(unittest.TestCase):
    
    def test1(self):
        dir_obj = plash.env.get_root_dir()
        plash.namespace.make_fs_op(dir_obj)

    def test2(self):
        dir_obj = plash.env.get_root_dir()
        fd = os.open("/dev/null", os.O_WRONLY | os.O_CREAT | os.O_TRUNC)
        log = plash.namespace.make_log_from_fd(plash_core.wrap_fd(fd))
        plash.namespace.make_fs_op(dir_obj, log)


if __name__ == "__main__":
    unittest.main()
