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

import gobject
import socket
import unittest

import plash_core


class CommsTest(unittest.TestCase):

    def test_freeing_last_reference(self):
        sock_pair = socket.socketpair()
        # Junk data should not be read because the connection is
        # closed down first.
        sock_pair[1].send("junk data")
        sock_pair[1].close()
        plash_core.cap_make_connection.make_conn2(
            plash_core.wrap_fd(sock_pair[0].fileno()), 1, [])
        # "may_block=False" argument to iteration() method not supported.
        # Adding an idle callback is a workaround.
        def idle_callback():
            return False # remove callback
        gobject.idle_add(idle_callback)
        gobject.main_context_default().iteration()


if __name__ == "__main__":
    unittest.main()
