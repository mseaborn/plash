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

import os
import unittest

import plash.filedesc
import plash.marshal as marshal


def example_fd():
    return plash.filedesc.wrap_fd(os.open("/dev/null", os.O_RDONLY))

def example_cap():
    return marshal.Pyobj_demarshal()


class MarshalTest(unittest.TestCase):

    def test_flat_encoding(self):
        examples = [
            ("i", [123]),
            ("ii", [123, -456]),
            ("s", ["foo"]),
            ("S", ["foo"]),
            ("si", ["hello", 1234]),
            ("f", [example_fd()]),
            ("F", [(example_fd(), example_fd())]),
            ("c", [example_cap()]),
            ("d", [example_cap()]),
            ("d", [None]),
            ("C", [(example_cap(), example_cap())]),
            ("*", [("some data",
                    (example_cap(),),
                    (example_fd(),))]),
            ]
        for format, data_in in examples:
            args_tuple = marshal.format_pack("ABCD", format, *data_in)
            data_out = marshal.format_unpack(format, args_tuple)
            self.assertEquals(data_out, tuple(data_in))

    def test_tree_encoding(self):
        input = ["foo", 1, 10, 100, example_fd(), example_cap()]
        ref, args_tuple = marshal.tree_pack(input)
        output = marshal.tree_unpack(ref, args_tuple)
        self.assertEquals(output, input)


if __name__ == "__main__":
    unittest.main()
