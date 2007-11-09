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
import select
import unittest

import protocol_cap
import protocol_event_loop


class EventLoopTest(unittest.TestCase):

    def test(self):
        loop = protocol_event_loop.EventLoop()
        self.assertTrue(loop.will_block())
        pipe_read, pipe_write = protocol_cap.make_pipe()

        got = []
        def callback(flags):
            got.append(os.read(pipe_read.fileno(), 100))

        watch_read = loop.make_watch(pipe_read, lambda: select.POLLIN,
                                     callback)
        self.assertTrue(loop.will_block())
        os.write(pipe_write.fileno(), "hello")
        self.assertTrue(not loop.will_block())
        loop.once_safely()
        self.assertEquals(got, ["hello"])
        self.assertTrue(loop.will_block())


if __name__ == "__main__":
    unittest.main()
