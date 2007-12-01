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

from plash.comms.cap_test import poll_fd
import plash.comms.event_loop_test
import plash.comms.simple
import plash.comms.stream as stream


class FDWrapperTest(unittest.TestCase):

    def test_fds_are_freed(self):
        pipe_read, pipe_write = stream.make_pipe()
        fd = pipe_read.fileno()
        fcntl.fcntl(fd, fcntl.F_GETFL)
        del pipe_read
        self.assertRaises(IOError,
                          lambda: fcntl.fcntl(fd, fcntl.F_GETFL))


class SocketListenerTest(plash.comms.event_loop_test.EventLoopTestCase):

    def test_listener(self):
        loop = self.make_event_loop()
        socket_path = os.path.join(self.make_temp_dir(), "socket")
        got = []
        def callback(sock):
            got.append(sock)
        stream.SocketListener(loop, socket.AF_UNIX, socket_path, callback)
        self.assertTrue(loop.will_block())
        sock = socket.socket(socket.AF_UNIX)
        sock.connect(socket_path)
        loop.once_safely()
        self.assertEquals(len(got), 1)


class FDBufferedWriterTest(plash.comms.event_loop_test.EventLoopTestCase):

    def test_writing(self):
        loop = self.make_event_loop()
        pipe_read, pipe_write = stream.make_pipe()
        writer = stream.FDBufferedWriter(loop, pipe_write)
        self.assertEquals(writer.buffered_size(), 0)
        writer.write("hello")
        self.assertEquals(writer.buffered_size(), 5)
        loop.once_safely()
        # Should have written all of buffer to the pipe now
        self.assertEquals(writer.buffered_size(), 0)

    def test_writing_to_closed_pipe(self):
        loop = self.make_event_loop()
        pipe_read, pipe_write = stream.make_pipe()
        writer = stream.FDBufferedWriter(loop, pipe_write)
        writer.write("hello")
        del pipe_read
        self.assertEquals(writer._connection_broken, False)
        loop.once_safely()
        self.assertEquals(writer._connection_broken, True)

    def test_writing_end_of_stream_with_data_buffered(self):
        loop = self.make_event_loop()
        pipe_read, pipe_write = stream.make_pipe()
        writer = stream.FDBufferedWriter(loop, pipe_write)
        del pipe_write
        writer.write("hello")
        writer.end_of_stream()
        loop.once_safely()
        self.assertEquals(poll_fd(pipe_read), select.POLLHUP | select.POLLIN)
        self.assertEquals(os.read(pipe_read.fileno(), 100), "hello")
        self.assertEquals(poll_fd(pipe_read), select.POLLHUP)
        self.assertEquals(os.read(pipe_read.fileno(), 100), "")


class FDReaderTest(plash.comms.event_loop_test.EventLoopTestCase):

    def test_end_of_stream(self):
        got = []
        def callback(data):
            got.append(data)
        def eof_callback():
            got.append("EOF")
        loop = self.make_event_loop()
        # Unix domain sockets report EOF from read() but pipes will
        # return an error from poll() instead.
        pipe_read, pipe_write = stream.socketpair()
        reader = stream.FDReader(loop, pipe_read, callback, eof_callback)
        self.assertTrue(loop.will_block())
        os.write(pipe_write.fileno(), "hello world")
        self.assertTrue(not loop.will_block())
        loop.once_safely()
        self.assertEquals(got, ["hello world"])
        del pipe_write
        loop.once_safely()
        self.assertEquals(got, ["hello world", "EOF"])
        self.assertTrue(not loop.is_listening())

    def test_reading_with_econnreset(self):
        # If A has written data when B has not read and B has closed
        # its socket, when A tries to read it will get ECONNRESET.
        got = []
        def callback(data):
            got.append(data)
        def eof_callback():
            got.append("EOF")
        loop = self.make_event_loop()
        sock1, sock2 = stream.socketpair()
        reader = stream.FDBufferedReader(loop, sock1, callback, eof_callback)
        self.assertTrue(loop.will_block())
        # Ordering is significant here.  If we close sock2 first,
        # writing to sock1 will simply raise EPIPE.
        os.write(sock1.fileno(), "this data gets dropped but causes ECONNRESET")
        del sock2
        loop.once_safely()
        self.assertEquals(got, ["EOF"])


class FDBufferedReaderTest(plash.comms.event_loop_test.EventLoopTestCase):

    def test_end_of_stream(self):
        got = []
        def callback(data):
            got.append(data)
        def eof_callback():
            got.append("EOF")
        loop = self.make_event_loop()
        # Unix domain sockets report EOF from read() but pipes will
        # return an error from poll() instead.
        pipe_read, pipe_write = stream.socketpair()
        reader = stream.FDBufferedReader(loop, pipe_read, callback,
                                         eof_callback)
        self.assertTrue(loop.will_block())
        os.write(pipe_write.fileno(), plash.comms.simple.make_message("hello"))
        os.write(pipe_write.fileno(), plash.comms.simple.make_message("world"))
        self.assertTrue(not loop.will_block())
        loop.once_safely()
        self.assertEquals(got, ["hello", "world"])
        del pipe_write
        loop.once_safely()
        self.assertEquals(got, ["hello", "world", "EOF"])
        self.assertTrue(not loop.is_listening())


class FDForwardTest(plash.comms.event_loop_test.EventLoopTestCase):

    def test_forwarding(self):
        loop = self.make_event_loop()
        pipe_read2, pipe_write = stream.make_pipe()
        pipe_read, pipe_write2 = stream.make_pipe()
        stream.FDForwader(loop, pipe_read2, pipe_write2)
        self.assertTrue(loop.will_block())
        self.assertEquals(poll_fd(pipe_read), 0)
        os.write(pipe_write.fileno(), "hello world")
        loop.once_safely() # read iteration
        loop.once_safely() # write iteration
        self.assertEquals(poll_fd(pipe_read), select.POLLIN)
        self.assertEquals(os.read(pipe_read.fileno(), 1000), "hello world")

    def test_closing_on_read_end(self):
        loop = self.make_event_loop()
        pipe_read2, pipe_write = stream.make_pipe()
        pipe_read, pipe_write2 = stream.make_pipe()
        stream.FDForwader(loop, pipe_read2, pipe_write2)
        del pipe_read2
        del pipe_write2
        self.assertTrue(loop.will_block())
        self.assertEquals(poll_fd(pipe_read), 0)
        os.write(pipe_write.fileno(), "hello world")
        del pipe_write
        loop.once_safely() # read iteration
        loop.once_safely() # write iteration
        self.assertEquals(poll_fd(pipe_read), select.POLLHUP | select.POLLIN)
        self.assertEquals(os.read(pipe_read.fileno(), 1000), "hello world")
        self.assertEquals(poll_fd(pipe_read), select.POLLHUP)
        self.assertEquals(os.read(pipe_read.fileno(), 1000), "")
