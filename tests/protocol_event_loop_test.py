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

    def test_reading(self):
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

    def test_removing_watch_on_error(self):
        loop = protocol_event_loop.EventLoop()
        pipe_read, pipe_write = protocol_cap.make_pipe()

        got_callbacks = []
        def callback(flags):
            got_callbacks.append(flags)

        watch_read = loop.make_watch(pipe_read, lambda: 0, callback)
        self.assertTrue(loop.will_block())
        del pipe_write
        self.assertTrue(not loop.will_block())
        loop.once_safely()
        self.assertTrue(watch_read.destroyed)
        self.assertTrue(not loop.is_listening())
        self.assertEquals(got_callbacks, [])

    def test_error_callback(self):
        loop = protocol_event_loop.EventLoop()
        pipe_read, pipe_write = protocol_cap.make_pipe()
        del pipe_write

        got_callbacks = []
        def error_callback(flags):
            got_callbacks.append(flags)

        loop.make_error_watch(pipe_read, error_callback)
        loop.once_safely()
        self.assertEquals(got_callbacks, [select.POLLHUP])


if __name__ == "__main__":
    unittest.main()
