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

from plash.comms.event_loop import poll_fd
import plash.comms.simple


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


def write_nonblocking(fd, data):
    # Set non-blocking mode temporarily.  We restore the old flags in
    # order not to interfere with other processes that might use the
    # FD later (e.g. when the FD is a tty).  Note that if the FD is
    # shared with another process which uses it concurrently, it could
    # unset the flag, leading to a denial of service when this process
    # blocks.  It could also lead to O_NONBLOCK being left set.
    # TODO:
    #  * Use send() with MSG_DONTWAIT on FDs that support it (currently,
    #    on Linux, only sockets).
    #  * Always write less than PIPE_BUF when writing to a pipe.
    #    On Linux that will not block if pipe() reported POLLOUT, even
    #    when O_NONBLOCK is not set.
    original_flags = fcntl.fcntl(fd, fcntl.F_GETFL)
    fcntl.fcntl(fd, fcntl.F_SETFL, original_flags | os.O_NONBLOCK)
    try:
        return os.write(fd, data)
    finally:
        fcntl.fcntl(fd, fcntl.F_SETFL, original_flags)


# O_ACCMODE is not defined in the "os" module, but should be
O_ACCMODE = os.O_RDONLY | os.O_WRONLY | os.O_RDWR

def fd_is_writable(fd):
    access_mode = fcntl.fcntl(fd, fcntl.F_GETFL) & O_ACCMODE
    return access_mode != os.O_RDONLY

def fd_is_readable(fd):
    access_mode = fcntl.fcntl(fd, fcntl.F_GETFL) & O_ACCMODE
    return access_mode != os.O_WRONLY


def coerce_buffer(val):
    # Unfortunately, trying to concatenate a string + a buffer raises
    # an exception.  But we don't want to do str() on the second
    # argument in case it isn't a buffer.
    if isinstance(val, str):
        return val
    elif isinstance(val, buffer):
        return str(val)
    else:
        raise TypeError("Not a buffer or str: %r" % type(val))


class FDBufferedWriter(object):

    def __init__(self, event_loop, fd, on_buffered_size_changed=lambda: None,
                 on_unwritable=lambda: None, on_write=lambda size: None):
        self._fd = fd
        self._on_buffered_size_changed = on_buffered_size_changed
        self._on_unwritable = on_unwritable
        self._on_write = on_write
        self._buf = ""
        self._eof_requested = False
        self._watch = event_loop.make_watch_with_error_handler(
            fd, self._get_flags, self._handler, self._error_handler)
        if not fd_is_writable(fd.fileno()):
            event_loop.call_later(lambda: self._error_handler(0))

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
            written = write_nonblocking(self._fd.fileno(), self._buf)
        except OSError:
            self._error_handler(0)
        else:
            self._buf = self._buf[written:]
            self._watch.update_flags()
            self._check_for_disconnect()
            self._on_buffered_size_changed()
            self._on_write(written)

    def _error_handler(self, flags):
        # An error occurred, so no more data can be written.
        self._fd = None
        self._buf = ""
        self._watch.remove_watch()
        self._on_unwritable()
        self._on_buffered_size_changed()

    def _check_for_disconnect(self):
        if self._eof_requested and len(self._buf) == 0:
            self._fd = None
            self._watch.remove_watch()
            self._on_unwritable()

    def write(self, data):
        # Should this raise an exception if called after end_of_stream()?
        if not self._eof_requested and self._fd is not None:
            # This could attempt to write the data to the FD in
            # non-blocking mode, instead of waiting until the next
            # event loop iteration.
            self._buf += coerce_buffer(data)
            self._watch.update_flags()
            self._on_buffered_size_changed()

    def end_of_stream(self):
        # Declares that no more data will be written.  Any buffered
        # data should be written before closing the FD.
        self._eof_requested = True
        self._check_for_disconnect()

    def buffered_size(self):
        return len(self._buf)

    def is_finished_writing(self):
        # Writing is finished if end_of_stream() was called and all
        # buffered data was written, or if an error occurred.
        return self._fd is None


class FDReader(object):

    def __init__(self, event_loop, fd, callback, eof_callback,
                 get_buffer_size=lambda: 1024):
        self._fd = fd
        self._callback = callback
        self._eof_callback = eof_callback
        self._get_buffer_size = get_buffer_size
        self._watch = event_loop.make_watch_with_error_handler(
            fd, self._get_poll_flags, self._read_data, self._error_handler)
        if not fd_is_readable(fd.fileno()):
            event_loop.call_later(self.finish)

    def _get_poll_flags(self):
        if self._get_buffer_size() > 0:
            return select.POLLIN
        else:
            return 0

    def update_buffer_size(self):
        self._watch.update_flags()

    def finish(self):
        eof_callback = self._eof_callback
        self._fd = None
        self._callback = None
        self._eof_callback = lambda: None
        self._watch.remove_watch()
        eof_callback()

    def _error_handler(self, flags):
        if flags & select.POLLHUP != 0:
            # The writer has closed the connection and can write no
            # more.  We can't use the event loop to read any remaining
            # data because poll() would always return POLLHUP, and we
            # don't need the event loop anyway because there are no
            # more notifications to get.  Reading here is not bounded
            # by get_buffer_size() any more.
            self.read_pending()
        self.finish()

    def _read_data(self, flags):
        self._read_some_data(self._get_buffer_size())

    def _read_some_data(self, buf_size):
        # read() can return ECONNRESET if the other end closed its
        # connection without reading all the data it had been sent.
        try:
            data = os.read(self._fd.fileno(), buf_size)
        except OSError:
            self.finish()
        else:
            if len(data) != 0:
                self._callback(data)
            else:
                self.finish()

    def read_pending(self):
        while (self._fd is not None and
               poll_fd(self._fd) & select.POLLIN != 0):
            self._read_some_data(1000)


class FDForwarder(object):

    def __init__(self, event_loop, input_fd, output_fd, buf_size=1024):
        self._buf_size = buf_size
        self._writer = FDBufferedWriter(
            event_loop, output_fd,
            on_buffered_size_changed=self._on_buffered_size_changed,
            on_unwritable=self._on_unwritable,
            on_write=self._on_write)
        self._reader = FDReader(event_loop, input_fd,
                                self._writer.write,
                                self._writer.end_of_stream,
                                get_buffer_size=self._get_read_size)
        # We track when the writer has written past certain points.
        # The counter could be implemented in FDBufferedWriter, but
        # FDForwarder is currently the only thing that needs this.
        self._bytes_written = 0
        self._sync_handlers = []

    def _on_buffered_size_changed(self):
        self._reader.update_buffer_size()

    def _get_read_size(self):
        return self._buf_size - self._writer.buffered_size()

    def _on_unwritable(self):
        self._reader.finish()
        sync_handlers = self._sync_handlers
        self._sync_handlers = []
        for bytes_read, synced_callback in sync_handlers:
            synced_callback()

    def _on_write(self, byte_count):
        self._bytes_written += byte_count
        self._check_for_completed_flushes()

    def _check_for_completed_flushes(self):
        while len(self._sync_handlers) > 0:
            bytes_read, synced_callback = self._sync_handlers[0]
            if self._bytes_written >= bytes_read:
                self._sync_handlers.pop(0)
                # Might consider calling this using call_later.
                synced_callback()
            else:
                break

    def _call_back_when_writes_synced(self, callback):
        bytes_read = self._bytes_written + self._writer.buffered_size()
        self._sync_handlers.append((bytes_read, callback))
        self._check_for_completed_flushes()

    def flush(self, callback):
        self._reader.read_pending()
        self._call_back_when_writes_synced(callback)


class FDBufferedReader(object):

    def __init__(self, event_loop, fd, callback, eof_callback=lambda: None):
        self._callback = callback
        self._buffer = plash.comms.simple.InputBuffer()
        self._buffer.connect(self._handle)
        self._reader = FDReader(event_loop, fd, self._buffer.add, eof_callback)

    def finish(self):
        self._reader.finish()

    def _handle(self):
        while True:
            try:
                message = plash.comms.simple.read_message(self._buffer)
            except plash.comms.simple.IncompleteMessageException:
                break
            else:
                self._callback(message)
