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

import gobject

import protocol_cap
import protocol_event_loop
import testrunner


def glib_poll_fds(fd_flags):
    # Emulates poll() (when called with a zero timeout) using a
    # one-off call to glib's main loop.
    watch_ids = []
    ready_fds = []
    revoked = False
    for fd, flags_requested in fd_flags.iteritems():
        def handler(fd_unused, flags_got, fd=fd):
            assert not revoked
            ready_fds.append((fd, flags_got))
            # Keep handler: it gets removed soon after
            return True
        # The ERROR_FLAGS are optional with glib even though they are
        # not optional with poll().  That means naive use of glib
        # could result in busy waits if you ignore error conditions on
        # FDs.
        watch_id = gobject.io_add_watch(
            fd, flags_requested | protocol_event_loop.ERROR_FLAGS, handler)
        watch_ids.append(watch_id)
    may_block = False
    gobject.main_context_default().iteration(may_block)
    revoked = True
    for watch_id in watch_ids:
        gobject.source_remove(watch_id)
    return ready_fds


def checked_poll_fds(fd_flags, timeout=None):
    # Check that glib gives the same results as poll().
    ready_fds1 = protocol_event_loop.poll_fds(fd_flags, timeout)
    if timeout == 0:
        ready_fds2 = glib_poll_fds(fd_flags)
        assert ready_fds1 == ready_fds2, (ready_fds1, ready_fds2)
    return ready_fds1


class EventLoopTestCase(testrunner.CombinationTestCase):

    def setup_poll_event_loop(self):
        self.loop = protocol_event_loop.EventLoop(poll_fds=checked_poll_fds)

    def setup_glib_event_loop(self):
        self.loop = protocol_event_loop.GlibEventLoop()
        self.on_teardown(lambda: self.loop.unregister())

    def make_event_loop(self):
        return self.loop


class EventLoopTests(EventLoopTestCase):

    def test_reading(self):
        loop = self.make_event_loop()
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
        loop = self.make_event_loop()
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
        loop = self.make_event_loop()
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
