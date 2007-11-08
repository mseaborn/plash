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

import protocol_cap


class ProtocolTest(unittest.TestCase):

    def test_id_encoding(self):
        args = (protocol_cap.NAMESPACE_SENDER, 12345)
        self.assertEquals(
            protocol_cap.decode_wire_id(
                protocol_cap.encode_wire_id(*args)),
            args)

    def test_invoke_encoding(self):
        invoke_args = (123, [4, 5, 6], "body")
        self.assertEquals(
            protocol_cap.decode_pocp_message(
                protocol_cap.make_invoke_message(*invoke_args)),
            ("invoke",) + invoke_args)

    def test_drop_encoding(self):
        self.assertEquals(
            protocol_cap.decode_pocp_message(
                protocol_cap.make_drop_message(789)),
            ("drop", 789))


if __name__ == "__main__":
    unittest.main()
