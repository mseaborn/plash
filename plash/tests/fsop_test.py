# Copyright (C) 2008 Mark Seaborn
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

import struct
import unittest

import plash_core
import plash.env
import plash.namespace


class FsOpTest(unittest.TestCase):

    def test_exec(self):
        fs_op = plash.namespace.make_fs_op(plash.env.get_root_dir())
        input_args = ["arg1", "argument2", "x"]
        filename, argv, exec_fds = \
            fs_op.fsop_exec("/bin/echo", ["argv0"] + input_args)
        # The start part of the result argv depends on ld.so location,
        # so we only check the end.
        self.assertEquals(argv[-len(input_args):], input_args)
        for argv_index, fd in exec_fds:
            assert isinstance(argv_index, int)
            assert isinstance(fd, plash_core.FD)


if __name__ == "__main__":
    unittest.main()
