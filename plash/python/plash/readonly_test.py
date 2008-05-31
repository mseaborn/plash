# Copyright (C) 2007 Mark Seaborn
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

import plash.filedesc
import plash.marshal
import plash.namespace


def call_logger(method_func):
    def wrapper(self, *args):
        self._calls.append((method_func.__name__,) + args)
        return method_func(self, *args)

    return wrapper


mock_stat_info = {"st_dev": 0, "st_ino": 0,
                  "st_mode": 0, "st_nlink": 0,
                  "st_uid": 0, "st_gid": 0,
                  "st_rdev": 0,
                  "st_size": 0, "st_blksize": 0, "st_blocks": 0,
                  "st_atime": 0, "st_mtime": 0, "st_ctime": 0}


def make_mock_fd():
    return plash.filedesc.wrap_fd(os.open("/dev/null", os.O_RDONLY, 0))


class ReadOnlyProxyTestBase(unittest.TestCase):

    def assert_rejected(self, func):
        # FIXME: want a different exception
        self.assertRaises(plash.marshal.UnmarshalError, func)
        self.assertEquals(self.orig_obj._calls, [])


class MockFile(plash.marshal.Pyobj_demarshal):

    def __init__(self):
        self._calls = []

    @call_logger
    def fsobj_type(self):
        return plash.marshal.OBJT_FILE

    @call_logger
    def fsobj_stat(self):
        return mock_stat_info

    @call_logger
    def file_open(self, flags):
        return make_mock_fd()


class ReadOnlyFileProxyTest(ReadOnlyProxyTestBase):

    def setUp(self):
        self.orig_obj = MockFile()
        self.read_only = plash.namespace.make_read_only_proxy(self.orig_obj)

    def test_open_for_read_succeeds(self):
        self.read_only.file_open(os.O_RDONLY)
        self.assertEquals(self.orig_obj._calls,
                          [("file_open", os.O_RDONLY)])

    def test_open_for_write_fails(self):
        self.assert_rejected(lambda: self.read_only.file_open(os.O_WRONLY))

    def test_type(self):
        self.assertEquals(self.read_only.fsobj_type(), plash.marshal.OBJT_FILE)
        self.assertEquals(self.orig_obj._calls, [("fsobj_type",)])

    def test_stat(self):
        self.assertEquals(self.read_only.fsobj_stat(), mock_stat_info)
        self.assertEquals(self.orig_obj._calls, [("fsobj_stat",)])

    def test_chmod(self):
        self.assert_rejected(lambda: self.read_only.fsobj_chmod(0777))

    def test_utimes(self):
        self.assert_rejected(lambda: self.read_only.fsobj_utimes(1, 2, 3, 4))

    def test_socket_connect(self):
        self.assert_rejected(
            lambda: self.read_only.file_socket_connect(make_mock_fd()))


class MockDir(plash.marshal.Pyobj_demarshal):

    def __init__(self):
        self._calls = []

    @call_logger
    def fsobj_type(self):
        return plash.marshal.OBJT_DIR

    @call_logger
    def fsobj_stat(self):
        return mock_stat_info

    @call_logger
    def dir_list(self):
        return [{"inode": 1, "type": 0, "name": "foo"}]


class ReadOnlyDirProxyTest(ReadOnlyProxyTestBase):

    def setUp(self):
        self.orig_obj = MockDir()
        self.read_only = plash.namespace.make_read_only_proxy(self.orig_obj)

    def test_type(self):
        self.assertEquals(self.read_only.fsobj_type(), plash.marshal.OBJT_DIR)
        self.assertEquals(self.orig_obj._calls, [("fsobj_type",)])

    def test_stat(self):
        self.assertEquals(self.read_only.fsobj_stat(), mock_stat_info)
        self.assertEquals(self.orig_obj._calls, [("fsobj_stat",)])

    def test_dir_list(self):
        self.assertEquals(self.read_only.dir_list(),
                          [{"inode": 1, "type": 0, "name": "foo"}])
        self.assertEquals(self.orig_obj._calls, [("dir_list",)])

    def test_create_file(self):
        self.assert_rejected(
            lambda: self.read_only.dir_create_file(os.O_WRONLY, 0666,
                                                   "leafname"))

    def test_mkdir(self):
        self.assert_rejected(
            lambda: self.read_only.dir_mkdir(0666, "leafname"))

    def test_symlink(self):
        self.assert_rejected(
            lambda: self.read_only.dir_symlink("dest_path", "leafname"))

    def test_rename(self):
        self.assert_rejected(
            lambda: self.read_only.dir_rename("from_leafname",
                                              self.read_only,
                                              "to_leafname"))

    def test_link(self):
        self.assert_rejected(
            lambda: self.read_only.dir_link("from_leafname",
                                            self.read_only,
                                            "to_leafname"))

    def test_unlink(self):
        self.assert_rejected(lambda: self.read_only.dir_unlink("leafname"))

    def test_rmdir(self):
        self.assert_rejected(lambda: self.read_only.dir_rmdir("leafname"))

    def test_socket_bind(self):
        self.assert_rejected(
            lambda: self.read_only.dir_socket_bind("leafname", make_mock_fd()))


class MockSymlink(plash.marshal.Pyobj_demarshal):

    def __init__(self):
        self._calls = []

    @call_logger
    def fsobj_type(self):
        return plash.marshal.OBJT_SYMLINK

    @call_logger
    def symlink_readlink(self):
        return "link_dest"


class ReadOnlySymlinkProxyTest(ReadOnlyProxyTestBase):

    def setUp(self):
        self.orig_obj = MockSymlink()
        self.read_only = plash.namespace.make_read_only_proxy(self.orig_obj)

    def test_type(self):
        self.assertEquals(self.read_only.fsobj_type(),
                          plash.marshal.OBJT_SYMLINK)
        self.assertEquals(self.orig_obj._calls, [("fsobj_type",)])

    def test_readlink(self):
        self.assertEquals(self.read_only.symlink_readlink(), "link_dest")


if __name__ == "__main__":
    unittest.main()
