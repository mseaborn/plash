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

import unittest

import protocol_simple


class SimpleProtocolEncodingTest(unittest.TestCase):

    def test_padding(self):
        for i in range(20):
            self.assertEquals((i + protocol_simple.pad_size(i)) % 4, 0)
            self.assertEquals(i + protocol_simple.pad_size(i),
                              protocol_simple.round_up_to_word(i))

    def test_message_encoding(self):
        buf = protocol_simple.InputBuffer()
        got = []

        def callback():
            try:
                got.append(protocol_simple.read_message(buf))
            except protocol_simple.IncompleteMessageException:
                pass

        buf.connect(callback)
        buf.add(protocol_simple.make_message("hello"))
        self.assertEquals(got, ["hello"])
        # Test handling incomplete messages
        for char in protocol_simple.make_message("character at a time"):
            buf.add(char)
        self.assertEquals(got, ["hello", "character at a time"])


if __name__ == "__main__":
    unittest.main()
