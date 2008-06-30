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

import errno
import fcntl
import os
import select
import socket
import unittest

from plash.comms.event_loop import poll_fd
import plash.comms.cap_test
import plash.comms.event_loop_test
import plash.comms.simple
import plash.comms.stream as stream


class OutputBufferTest(unittest.TestCase):

    def test_output_buffer(self):
        buf = stream.OutputBuffer()
        self.assertEquals(buf.get_buffer(), "")
        self.assertTrue(buf.is_empty())
        self.assertEquals(buf.get_size(), 0)

        buf.append_bytes("hello ")
        self.assertEquals(buf.get_buffer(), "hello ")
        self.assertFalse(buf.is_empty())
        self.assertEquals(buf.get_size(), 6)

        buf.append_bytes(buffer("world!"))
        self.assertEquals(buf.get_buffer(), "hello world!")
        self.assertFalse(buf.is_empty())
        self.assertEquals(buf.get_size(), 12)

        buf.remove_bytes(4)
        self.assertEquals(buf.get_buffer(), "o world!")
        self.assertFalse(buf.is_empty())
        self.assertEquals(buf.get_size(), 8)

        buf.clear()
        self.assertEquals(buf.get_buffer(), "")
        self.assertTrue(buf.is_empty())
        self.assertEquals(buf.get_size(), 0)


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


def find_pipe_buffer_size():
    # The size of a pipe's buffer is 64k on Linux, but let's not
    # assume that.
    pipe_read, pipe_write = stream.make_pipe()
    fcntl.fcntl(pipe_write.fileno(), fcntl.F_SETFL, os.O_NONBLOCK)
    data = "x" * 4096
    written = 0
    while poll_fd(pipe_write) & select.POLLOUT != 0:
        written += os.write(pipe_write.fileno(), data)
    return written


class ReadabilityAndWritabilityTests(unittest.TestCase):

    def test_sockets(self):
        sock1, sock2 = stream.socketpair()
        for sock in (sock1, sock2):
            self.assertTrue(stream.fd_is_readable(sock))
            self.assertTrue(stream.fd_is_writable(sock))

    def test_pipes(self):
        pipe_read, pipe_write = stream.make_pipe()
        self.assertTrue(stream.fd_is_readable(pipe_read))
        self.assertFalse(stream.fd_is_writable(pipe_read))
        self.assertFalse(stream.fd_is_readable(pipe_write))
        self.assertTrue(stream.fd_is_writable(pipe_write))

    def test_poll_annoying_behaviour(self):
        pipe_read, pipe_write = stream.make_pipe()
        # It would be more useful if poll() returned POLLIN and
        # POLLOUT for unreadable and unwritable FDs respectively,
        # indicating that read() and write() would not block.
        # We do not rely on the behaviour tested here; in fact, we
        # work around it.
        self.assertEquals(poll_fd(pipe_read) & select.POLLOUT, 0)
        self.assertEquals(poll_fd(pipe_write) & select.POLLIN, 0)
        # However, select() behaves more usefully:
        read_fds, write_fds, except_fds = select.select(
            [], [open(os.devnull, "r")], [], 0)
        self.assertEquals(len(write_fds), 1)
        read_fds, write_fds, except_fds = select.select(
            [open(os.devnull, "w")], [], [], 0)
        self.assertEquals(len(read_fds), 1)


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
        self.assertEquals(writer.is_finished_writing(), False)
        loop.once_safely()
        self.assertEquals(writer.is_finished_writing(), True)
        self.assertFalse(loop.is_listening())

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

    def test_writing_to_full_buffer(self):
        # Checks that the writer does not write when the FD's buffer
        # is full, and checks that it does not block.
        loop = self.make_event_loop()
        pipe_read, pipe_write = stream.make_pipe()
        writer = stream.FDBufferedWriter(loop, pipe_write)
        bufferfuls = 5
        size = find_pipe_buffer_size() * bufferfuls
        writer.write("x" * size)
        loop.once_safely()
        # Nothing is reading pipe_read, so writer should still have
        # unwritten data.
        assert writer.buffered_size() > 0
        assert writer.buffered_size() < size
        # But reading should allow the backlog to clear.
        for i in range(bufferfuls):
            self.assertEquals(poll_fd(pipe_read), select.POLLIN)
            os.read(pipe_read.fileno(), size)
            loop.run_awhile()
        self.assertEquals(writer.buffered_size(), 0)

    def test_fd_flags_not_changed(self):
        loop = self.make_event_loop()
        pipe_read, pipe_write = stream.make_pipe()
        def check_flags():
            flags = fcntl.fcntl(pipe_write.fileno(), fcntl.F_GETFL)
            self.assertEquals(flags, os.O_WRONLY)
        check_flags()
        writer = stream.FDBufferedWriter(loop, pipe_write)
        check_flags()
        writer.write("hello")
        loop.once_safely()
        # If FDBufferedWriter sets O_NONBLOCK, it should do so only
        # temporarily, because it could confuse other processes that
        # share the FD.  Changing the flags temporarily is still not
        # ideal though.
        check_flags()

    def test_writing_to_unwritable_fd(self):
        got = []
        loop = self.make_event_loop()
        writer = stream.FDBufferedWriter(
            loop, open("/dev/null", "r"),
            on_unwritable=lambda: got.append("broken"))
        loop.once_safely()
        self.assertTrue(writer._watch.destroyed)
        self.assertEquals(got, ["broken"])
        self.assertFalse(loop.is_listening())

    def test_on_finished_callback(self):
        got = []
        def on_finished():
            got.append("finish")

        loop = self.make_event_loop()
        pipe_read, pipe_write = stream.make_pipe()
        writer = stream.FDBufferedWriter(loop, pipe_write,
                                         on_unwritable=on_finished)
        writer.write("hello")
        loop.once_safely()
        self.assertEquals(writer.buffered_size(), 0)
        # Writer is not finished merely because it has no data buffered.
        self.assertEquals(got, [])
        writer.write("world")
        writer.end_of_stream()
        # Nor is the writer finished because end_of_stream() has been called.
        self.assertEquals(got, [])
        loop.once_safely()
        # Writer is finished once end_of_stream() has been called and
        # either all buffered data has been written or an error was
        # reached.
        self.assertEquals(got, ["finish"])


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
        reader = stream.FDReader(loop, sock1, callback, eof_callback)
        self.assertTrue(loop.will_block())
        # Ordering is significant here.  If we close sock2 first,
        # writing to sock1 will simply raise EPIPE.
        os.write(sock1.fileno(), "this data gets dropped but causes ECONNRESET")
        del sock2
        loop.once_safely()
        self.assertEquals(got, ["EOF"])

    def test_read_buffer_size(self):
        got = []
        def callback(data):
            got.append(data)
        loop = self.make_event_loop()
        sock1, sock2 = stream.socketpair()
        reader = stream.FDReader(loop, sock1, callback, lambda: None,
                                 get_buffer_size=lambda: 2)
        os.write(sock2.fileno(), "hello")
        loop.once_safely()
        self.assertEquals(got, ["he"])
        loop.once_safely()
        self.assertEquals(got, ["he", "ll"])
        loop.once_safely()
        self.assertEquals(got, ["he", "ll", "o"])

    def test_reader_flow_control(self):
        # Checks whether the buffer size of the reader can be changed.
        got = []
        def callback(data):
            got.append(data)
        loop = self.make_event_loop()
        sock1, sock2 = stream.socketpair()
        read_size = 2
        reader = stream.FDReader(loop, sock1, callback, lambda: None,
                                 get_buffer_size=lambda: read_size)
        os.write(sock2.fileno(), "hello world")
        loop.once_safely()
        self.assertEquals(got, ["he"])
        # Setting the read buffer size to zero disables reading.
        read_size = 0
        reader.update_buffer_size()
        self.assertTrue(loop.will_block())
        # But we can set the read buffer back to a non-zero size.
        read_size = 6
        reader.update_buffer_size()
        loop.once_safely()
        self.assertEquals(got, ["he", "llo wo"])

    def test_end_of_stream_multiple_reads(self):
        got = []
        def callback(data):
            got.append(data)
        def eof_callback():
            got.append("EOF")
        loop = self.make_event_loop()
        pipe_read, pipe_write = stream.socketpair()
        reader = stream.FDReader(loop, pipe_read, callback, eof_callback,
                                 get_buffer_size=lambda: 2)
        self.assertTrue(loop.will_block())
        os.write(pipe_write.fileno(), "hello world")
        del pipe_write
        loop.once_safely()
        self.assertEquals(got, ["he", "llo world", "EOF"])
        self.assertTrue(not loop.is_listening())

    def test_aborting_reading(self):
        got = []
        loop = self.make_event_loop()
        pipe_read, pipe_write = stream.socketpair()
        os.write(pipe_write.fileno(), "hello world")
        reader = stream.FDReader(loop, pipe_read,
                                 lambda data: got.append(data),
                                 lambda: got.append("EOF"))
        del pipe_read
        # Calling finish() stops the reader from reading any more.
        reader.finish()
        self.assertFalse(loop.is_listening())
        self.assertEquals(got, ["EOF"])
        # The reader should drop its FD so the pipe should now be broken.
        try:
            os.write(pipe_write.fileno(), "hello world")
        except OSError, exn:
            self.assertEquals(exn.errno, errno.EPIPE)
        else:
            raise AssertionError("Did not raise EPIPE")

    def test_reading_from_unreadable_fd(self):
        got = []
        loop = self.make_event_loop()
        reader = stream.FDReader(loop, open("/dev/null", "w"),
                                 lambda data: got.append(data),
                                 lambda: got.append("EOF"))
        loop.once_safely()
        self.assertTrue(reader._watch.destroyed)
        self.assertEquals(got, ["EOF"])
        self.assertFalse(loop.is_listening())


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

    def _make_forwarded_pipe(self, loop):
        pipe_read2, pipe_write = stream.make_pipe()
        pipe_read, pipe_write2 = stream.make_pipe()
        forwarder = stream.FDForwarder(loop, pipe_read2, pipe_write2)
        return pipe_read, pipe_write, forwarder

    def test_forwarding(self):
        loop = self.make_event_loop()
        pipe_read, pipe_write, forwarder = self._make_forwarded_pipe(loop)
        self.assertTrue(loop.will_block())
        self.assertEquals(poll_fd(pipe_read), 0)
        os.write(pipe_write.fileno(), "hello world")
        loop.once_safely() # read iteration
        loop.once_safely() # write iteration
        self.assertEquals(poll_fd(pipe_read), select.POLLIN)
        self.assertEquals(os.read(pipe_read.fileno(), 1000), "hello world")

    def test_closing_on_read_end(self):
        loop = self.make_event_loop()
        pipe_read, pipe_write, forwarder = self._make_forwarded_pipe(loop)
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
        self.assertFalse(loop.is_listening())

    def test_closing_when_no_reader(self):
        loop = self.make_event_loop()
        pipe_read, pipe_write, forwarder = self._make_forwarded_pipe(loop)
        del pipe_read
        loop.once_safely()
        self.assertFalse(loop.is_listening())

    def test_flow_control(self):
        # Usually with pipes, a writer cannot keep writing while the
        # reader is not reading.  Check that forwarding preserves
        # that.
        big_limit = find_pipe_buffer_size() * 10
        loop = self.make_event_loop()
        pipe_read, pipe_write, forwarder = self._make_forwarded_pipe(loop)
        fcntl.fcntl(pipe_write.fileno(), fcntl.F_SETFL, os.O_NONBLOCK)
        data = "x" * 4096
        written = 0
        while poll_fd(pipe_write) & select.POLLOUT != 0:
            assert written < big_limit
            written += os.write(pipe_write.fileno(), data)
            loop.run_awhile()
        assert written > 0
        # Reading should allow more data to be written.  We have to
        # read at least PIPE_BUF bytes, otherwise poll() will not
        # report the pipe as writable to the FDForwarder.
        os.read(pipe_read.fileno(), 4096)
        self.assertEquals(poll_fd(pipe_write), 0)
        loop.run_awhile()
        self.assertEquals(poll_fd(pipe_write), select.POLLOUT)

    def test_flushing_simple_case(self):
        loop = self.make_event_loop()
        pipe_read, pipe_write, forwarder = self._make_forwarded_pipe(loop)
        os.write(pipe_write.fileno(), "hello world")
        self.assertEquals(forwarder._writer.buffered_size(), 0)
        got = []
        def on_flushed():
            got.append("flushed")
        forwarder.flush(on_flushed)
        # Pending data gets read immediately.
        self.assertEquals(forwarder._writer.buffered_size(), 11)
        # Buts it's not until we go around the event loop that it's written.
        loop.once_safely()
        fcntl.fcntl(pipe_read.fileno(), fcntl.F_SETFL, os.O_NONBLOCK)
        data = os.read(pipe_read.fileno(), 1000)
        self.assertEquals(data, "hello world")
        self.assertEquals(got, ["flushed"])

    def test_flushing_when_unable_to_write(self):
        pipe_size = find_pipe_buffer_size()
        data_written = "h" * pipe_size
        loop = self.make_event_loop()
        pipe_read, pipe_write, forwarder = self._make_forwarded_pipe(loop)
        os.write(pipe_write.fileno(), data_written)
        # Forwarder will forward some but not all the data we just wrote.
        loop.run_awhile()

        # Flushing can read all pending data, but it can't always
        # write all the buffered data.
        got = []
        def on_flushed():
            got.append("flushed")
        forwarder.flush(on_flushed)
        fcntl.fcntl(pipe_read.fileno(), fcntl.F_SETFL, os.O_NONBLOCK)
        data_read = os.read(pipe_read.fileno(), pipe_size)
        remaining = forwarder._writer.buffered_size()
        assert remaining > 0, remaining
        self.assertEquals(len(data_read) + remaining, len(data_written))
        self.assertEquals(got, [])

        # The data should get completely written and we should get
        # notified.
        loop.once_safely()
        data_read += os.read(pipe_read.fileno(), pipe_size)
        self.assertEquals(forwarder._writer.buffered_size(), 0)
        self.assertEquals(len(data_read), len(data_written))
        self.assertEquals(data_read, data_written)
        self.assertEquals(got, ["flushed"])

        # Note that the forwarder still works.  flush() does not revoke it.
        os.write(pipe_write.fileno(), "hello")
        loop.run_awhile()
        self.assertEquals(os.read(pipe_read.fileno(), pipe_size), "hello")

    def test_flushing_with_broken_write_stream(self):
        loop = self.make_event_loop()
        pipe_read, pipe_write, forwarder = self._make_forwarded_pipe(loop)
        os.write(pipe_write.fileno(), "hello world")
        got = []
        def on_flushed():
            got.append("flushed")
        # The flush should count as completed if the forwarder gets an
        # error writing to its write stream, e.g. because the pipe is
        # broken.
        del pipe_read
        forwarder.flush(on_flushed)
        loop.once_safely()
        self.assertEquals(got, ["flushed"])

    def test_forwarding_from_unreadable_fd(self):
        loop = self.make_event_loop()
        pipe_read, pipe_write = stream.make_pipe()
        forwarder = stream.FDForwarder(loop, open("/dev/null", "w"), pipe_write)
        del pipe_write
        loop.once_safely()
        self.assertEquals(os.read(pipe_read.fileno(), 1000), "")

    def test_forwarding_to_unwritable_fd(self):
        loop = self.make_event_loop()
        pipe_read, pipe_write = stream.make_pipe()
        forwarder = stream.FDForwarder(loop, pipe_read, open("/dev/null", "r"))
        del pipe_read
        loop.once_safely()
        self.assertEquals(poll_fd(pipe_write), select.POLLERR | select.POLLOUT)


def get_known_flags():
    return dict(
        (value, name)
        for name, value in select.__dict__.iteritems()
        if name.startswith("POLL"))


def decode_poll_flags(flags):
    known_flags = get_known_flags()
    names = []
    for bit in range(32):
        flag = 1 << bit
        if flags & flag != 0:
            names.append(known_flags.get(flag, str(flag)))
    if len(names) == 0:
        return "0"
    else:
        return "|".join(names)


def get_poll_flag_table():
    output = []
    pipe_size = find_pipe_buffer_size()

    def example(name, fd):
        flags = poll_fd(fd)
        output.append("%-30s %s" % (decode_poll_flags(flags), name))

    class Wrapper(object):
        def __init__(self, fd):
            self._fd = fd
        def fileno(self):
            return self._fd

    def unwrap(fd):
        return os.dup(fd.fileno())

    def unwrap_pair((fd1, fd2)):
        return unwrap(fd1), unwrap(fd2)

    def pair_pending_data(pair, size):
        read_fd, write_fd = pair
        os.write(write_fd.fileno(), "x" * size)
        return pair

    def example_pair(name, maker, i):
        fd_pair = unwrap_pair(maker())
        fd = fd_pair[i]
        example(name, Wrapper(fd))
        os.close(fd_pair[1 - i])
        example(name + " (other closed)", Wrapper(fd))
        os.close(fd_pair[i])

    def shutdown(pair, shut_type):
        # The socket is not necessarily AF_UNIX but the socket module
        # shouldn't actually use those arguments.
        sock = socket.fromfd(pair[1].fileno(),
                             socket.AF_UNIX, socket.SOCK_STREAM)
        sock.shutdown(shut_type)
        return pair

    def example_socket(name, maker):
        example_pair(name, maker, 0)
        example_pair(name + " (SHUT_RD)",
                     lambda: shutdown(maker(), socket.SHUT_RD), 0)
        example_pair(name + " (SHUT_WR)",
                     lambda: shutdown(maker(), socket.SHUT_WR), 0)
        example_pair(name + " (SHUT_RDWR)",
                     lambda: shutdown(maker(), socket.SHUT_RDWR), 0)

    def make_pty():
        fd1, fd2 = os.openpty()
        return stream.WrappedFD(fd1), stream.WrappedFD(fd2)

    def make_pty_eof():
        fd1, fd2 = make_pty()
        # Send Ctrl-D, the console EOF character
        os.write(fd1.fileno(), chr(4))
        return fd1, fd2

    for side, name in [(0, "read"), (1, "write")]:
        example_pair(
            "pipe: %s" % name,
            lambda: stream.make_pipe(), side)
        example_pair(
            "pipe: %s, pending data" % name,
            lambda: pair_pending_data(stream.make_pipe(), pipe_size), side)
    example_socket("socket: Unix", stream.socketpair)
    example_socket("socket: TCP", plash.comms.cap_test.tcp_socketpair)
    example_pair("tty master", make_pty, 0)
    example_pair("tty slave", make_pty, 1)
    example_pair("tty slave (send Ctrl-D)", make_pty_eof, 1)
    example("/dev/null: write", open("/dev/null", "w"))
    example("/dev/null: read", open("/dev/null", "r"))
    example("file: write", open("/tmp/file", "w"))
    example("file: read", open("/tmp/file", "r"))
    return output


class TestPollFlagsTable(unittest.TestCase):

    def test_getting_table(self):
        get_poll_flag_table()


def main():
    for entry in get_poll_flag_table():
        print entry


if __name__ == "__main__":
    main()
