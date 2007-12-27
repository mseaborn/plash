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


class ReadOnlyProxyTest(unittest.TestCase):

    def setUp(self):
        self.file_obj = MockFile()
        self.read_only = plash.namespace.make_read_only_proxy(self.file_obj)

    def test_open_for_read_succeeds(self):
        self.read_only.file_open(os.O_RDONLY)
        self.assertEquals(self.file_obj._calls,
                          [("file_open", os.O_RDONLY)])

    def test_open_for_write_fails(self):
        # FIXME: want a different exception
        self.assertRaises(plash.marshal.UnmarshalError,
                          lambda: self.read_only.file_open(os.O_WRONLY))
        self.assertEquals(self.file_obj._calls, [])

    def test_type(self):
        self.assertEquals(self.read_only.fsobj_type(), plash.marshal.OBJT_FILE)
        self.assertEquals(self.file_obj._calls, [("fsobj_type",)])

    def test_stat(self):
        self.assertEquals(self.read_only.fsobj_stat(), mock_stat_info)
        self.assertEquals(self.file_obj._calls, [("fsobj_stat",)])

    def test_chmod(self):
        self.assertRaises(plash.marshal.UnmarshalError,
                          lambda: self.read_only.fsobj_chmod(0777))

    def test_utimes(self):
        self.assertRaises(plash.marshal.UnmarshalError,
                          lambda: self.read_only.fsobj_utimes(1, 2, 3, 4))

    def test_socket_connect(self):
        self.assertRaises(plash.marshal.UnmarshalError,
                          lambda: self.read_only.file_socket_connect(
                                      make_mock_fd()))


if __name__ == "__main__":
    unittest.main()
