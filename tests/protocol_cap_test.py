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

import fcntl
import os
import select
import unittest

import protocol_cap


class CapProtocolEncodingTest(unittest.TestCase):

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


class FDWatch(object):

    def __init__(self, fd, get_flags, callback):
        self.fd = fd
        self.get_flags = get_flags
        self.callback = callback


def poll_fds(fd_flags, timeout=None):
    poller = select.poll()
    for fd, flags in fd_flags.iteritems():
        poller.register(fd, flags)
    return poller.poll(timeout)


class EventLoop(object):

    # The design of select.poll prevents us from reusing the poll
    # instance.

    def __init__(self):
        self._watches = []

    def make_watch(self, fd, flags, callback):
        watch = FDWatch(fd, flags, callback)
        self._watches.append(watch)
        return watch

    def _get_fd_flags(self):
        fd_flags = {}
        for watch in self._watches:
            add_flags = watch.get_flags()
            if add_flags != 0:
                fd = watch.fd.fileno()
                fd_flags[fd] = fd_flags.get(fd, 0) | add_flags
        return fd_flags

    def is_listening(self):
        # Whether there are any events we're currently interested in.
        # If not, the loop would block forever.
        return len(self._get_fd_flags()) > 0

    def will_block(self):
        ready = poll_fds(self._get_fd_flags(), 0)
        return len(ready) == 0

    def once(self):
        ready = dict(poll_fds(self._get_fd_flags()))
        self._process_ready(ready)

    def once_safely(self):
        ready = dict(poll_fds(self._get_fd_flags(), 0))
        assert len(ready) != 0, self._get_fd_flags()
        self._process_ready(ready)

    def _process_ready(self, ready):
        for watch in self._watches:
            fd = watch.fd.fileno()
            if fd in ready:
                # Assumes the watch has not been changed since make_poller()
                relevant_flags = ready[fd] & watch.get_flags()
                if relevant_flags != 0:
                    self._assert_still_relevant(fd, relevant_flags)
                    watch.callback(relevant_flags)

    def _assert_still_relevant(self, fd, expected_flags):
        # Be paranoid, and check flags again.  A handler could have a
        # side effect of causing the earlier poll results to no longer
        # be relevant.
        poller = select.poll()
        poller.register(fd, expected_flags)
        ready = poller.poll(0)
        assert len(ready) == 1, ready
        fd2, current_flags = ready[0]
        assert fd2 == fd
        assert current_flags & expected_flags == expected_flags


class FDWrapperTest(unittest.TestCase):

    def test_fds_are_freed(self):
        pipe_read, pipe_write = protocol_cap.make_pipe()
        fd = pipe_read.fileno()
        fcntl.fcntl(fd, fcntl.F_GETFL)
        del pipe_read
        self.assertRaises(IOError,
                          lambda: fcntl.fcntl(fd, fcntl.F_GETFL))


class EventLoopTest(unittest.TestCase):

    def test(self):
        loop = EventLoop()
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


class FDBufferedWriterTest(unittest.TestCase):

    def test(self):
        loop = EventLoop()
        pipe_read, pipe_write = protocol_cap.make_pipe()
        writer = protocol_cap.FDBufferedWriter(loop, pipe_write)
        self.assertTrue(not loop.is_listening())
        writer.write("hello")
        self.assertTrue(loop.is_listening())
        loop.once_safely()
        # Should have written all of buffer to the pipe now
        self.assertTrue(not loop.is_listening())


if __name__ == "__main__":
    unittest.main()
