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
import socket
import unittest

from protocol_cap_test import poll_fd
import protocol_event_loop_test
import protocol_simple
import protocol_stream


class FDWrapperTest(unittest.TestCase):

    def test_fds_are_freed(self):
        pipe_read, pipe_write = protocol_stream.make_pipe()
        fd = pipe_read.fileno()
        fcntl.fcntl(fd, fcntl.F_GETFL)
        del pipe_read
        self.assertRaises(IOError,
                          lambda: fcntl.fcntl(fd, fcntl.F_GETFL))


class SocketListenerTest(protocol_event_loop_test.EventLoopTestCase):

    def test_listener(self):
        loop = self.make_event_loop()
        socket_path = os.path.join(self.make_temp_dir(), "socket")
        got = []
        def callback(sock):
            got.append(sock)
        protocol_stream.SocketListener(loop, socket.AF_UNIX, socket_path,
                                       callback)
        self.assertTrue(loop.will_block())
        sock = socket.socket(socket.AF_UNIX)
        sock.connect(socket_path)
        loop.once_safely()
        self.assertEquals(len(got), 1)


class FDBufferedWriterTest(protocol_event_loop_test.EventLoopTestCase):

    def test_writing(self):
        loop = self.make_event_loop()
        pipe_read, pipe_write = protocol_stream.make_pipe()
        writer = protocol_stream.FDBufferedWriter(loop, pipe_write)
        self.assertEquals(writer.buffered_size(), 0)
        writer.write("hello")
        self.assertEquals(writer.buffered_size(), 5)
        loop.once_safely()
        # Should have written all of buffer to the pipe now
        self.assertEquals(writer.buffered_size(), 0)

    def test_writing_to_closed_pipe(self):
        loop = self.make_event_loop()
        pipe_read, pipe_write = protocol_stream.make_pipe()
        writer = protocol_stream.FDBufferedWriter(loop, pipe_write)
        writer.write("hello")
        del pipe_read
        self.assertEquals(writer._connection_broken, False)
        loop.once_safely()
        self.assertEquals(writer._connection_broken, True)

    def test_writing_end_of_stream_with_data_buffered(self):
        loop = self.make_event_loop()
        pipe_read, pipe_write = protocol_stream.make_pipe()
        writer = protocol_stream.FDBufferedWriter(loop, pipe_write)
        del pipe_write
        writer.write("hello")
        writer.end_of_stream()
        loop.once_safely()
        self.assertEquals(poll_fd(pipe_read), select.POLLHUP | select.POLLIN)
        self.assertEquals(os.read(pipe_read.fileno(), 100), "hello")
        self.assertEquals(poll_fd(pipe_read), select.POLLHUP)
        self.assertEquals(os.read(pipe_read.fileno(), 100), "")


class FDBufferedReaderTest(protocol_event_loop_test.EventLoopTestCase):

    def test_end_of_stream(self):
        got = []
        def callback(data):
            got.append(data)
        loop = self.make_event_loop()
        pipe_read, pipe_write = protocol_stream.make_pipe()
        reader = protocol_stream.FDBufferedReader(loop, pipe_read, callback)
        self.assertTrue(loop.will_block())
        os.write(pipe_write.fileno(), protocol_simple.make_message("hello"))
        os.write(pipe_write.fileno(), protocol_simple.make_message("world"))
        self.assertTrue(not loop.will_block())
        loop.once_safely()
        self.assertEquals(got, ["hello", "world"])
        del pipe_write
        loop.once_safely()
        self.assertTrue(not loop.is_listening())
