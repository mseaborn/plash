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
import socket

import protocol_simple


class WrappedFD(object):

    def __init__(self, fd):
        assert isinstance(fd, int)
        # TODO: check whether we got a valid FD
        self._fd = fd

    def fileno(self):
        return self._fd

    def __del__(self):
        os.close(self._fd)


def make_pipe():
    pipe_read, pipe_write = os.pipe()
    return WrappedFD(pipe_read), WrappedFD(pipe_write)


def socketpair():
    sock1, sock2 = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
    return (WrappedFD(os.dup(sock1.fileno())),
            WrappedFD(os.dup(sock2.fileno())))


class SocketListener(object):

    def __init__(self, event_loop, socket_type, socket_address, callback):
        self._callback = callback
        self._sock = socket.socket(socket_type)
        self._sock.bind(socket_address)
        self._sock.listen(10)
        event_loop.make_watch(self._sock, lambda: select.POLLIN, self._handler)

    def _handler(self, flags):
        sock2 = self._sock.accept()
        self._callback(sock2)


class FDBufferedWriter(object):

    def __init__(self, event_loop, fd):
        self._fd = fd
        self._buf = ""
        self._eof_requested = False
        self._connection_broken = False
        self._watch = event_loop.make_watch_with_error_handler(
            fd, self._get_flags, self._handler, self._error_handler)

    def _get_flags(self):
        if len(self._buf) == 0:
            return 0
        else:
            return select.POLLOUT

    def _handler(self, flags):
        # Note that Python changes the default signal handler for
        # SIGPIPE so that this can return EPIPE instead of killing the
        # process when writing to a pipe with no reader.
        try:
            written = os.write(self._fd.fileno(), self._buf)
        except OSError:
            self._error_handler(0)
        else:
            self._buf = self._buf[written:]
            self._watch.update_flags()
            self._check_for_disconnect()

    def _error_handler(self, flags):
        # An error occurred, so no more data can be written.
        self._fd = None
        self._buf = ""
        self._connection_broken = True
        self._watch.remove_watch()

    def _check_for_disconnect(self):
        if self._eof_requested and len(self._buf) == 0:
            self._fd = None
            self._watch.remove_watch()

    def write(self, data):
        # Should this raise an exception if called after end_of_stream()?
        if not self._eof_requested and not self._connection_broken:
            self._buf += data
            self._watch.update_flags()

    def end_of_stream(self):
        # Declares that no more data will be written.  Any buffered
        # data should be written before closing the FD.
        self._eof_requested = True
        self._check_for_disconnect()

    def buffered_size(self):
        return len(self._buf)


class FDBufferedReader(object):

    def __init__(self, event_loop, fd, callback, eof_callback=lambda: None):
        self._fd = fd
        self._callback = callback
        self._eof_callback = eof_callback
        self._buffer = protocol_simple.InputBuffer()
        self._buffer.connect(self._handle)
        # TODO: extend to do flow control
        self._watch = event_loop.make_watch(fd, lambda: select.POLLIN,
                                            self._read_data)

    def finish(self):
        self._fd = None
        self._callback = None
        self._watch.remove_watch()

    def _read_data(self, flags):
        data = os.read(self._fd.fileno(), 1024)
        if len(data) != 0:
            self._buffer.add(data)
        else:
            self.finish()
            self._eof_callback()

    def _handle(self):
        while True:
            try:
                message = protocol_simple.read_message(self._buffer)
            except protocol_simple.IncompleteMessageException:
                break
            else:
                self._callback(message)
