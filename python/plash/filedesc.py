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
import os
import select

import plash.mainloop
import plash_core


class FDWatch(object):

    def __init__(self, fd, callback, poll_flags):
        self._fd = fd
        self._callback = callback
        self._watch_id = gobject.io_add_watch(fd, poll_flags, callback)

    def set_watch_flags(self, poll_flags):
        self.destroy()
        # If we register a watch with zero flags, the watch appears to
        # get called.
        if poll_flags != 0:
            self._watch_id = gobject.io_add_watch(self._fd, poll_flags,
                                                  self._callback)

    def destroy(self):
        if self._watch_id is not None:
            gobject.source_remove(self._watch_id)
            self._watch_id = None


class ForwardFD(object):

    def __init__(self, src_fd, dest_fd, buf_size=1024):
        self._src_fd = src_fd
        self._dest_fd = dest_fd
        self._buf_size = buf_size
        self._buf = ""
        self._reader = FDWatch(src_fd, self._read_callback,
                               gobject.IO_IN |
                               gobject.IO_ERR | gobject.IO_HUP)
        self._writer = FDWatch(dest_fd, self._write_callback,
                               gobject.IO_ERR | gobject.IO_HUP)
        self._reason_to_run = plash.mainloop.ReasonToRun()
        plash.mainloop.use_glib_mainloop()
        self._running = True

    def _set_flags(self):
        src_flags = 0
        dest_flags = gobject.IO_ERR | gobject.IO_HUP
        if len(self._buf) == 0:
            src_flags |= gobject.IO_IN | gobject.IO_ERR | gobject.IO_HUP
        else:
            dest_flags |= gobject.IO_OUT
        self._reader.set_watch_flags(src_flags)
        self._writer.set_watch_flags(dest_flags)

    def _read_callback(self, unused_fd, fd_condition):
        # Note that both IO_IN and IO_HUP may be set
        if fd_condition & gobject.IO_IN:
            assert len(self._buf) == 0
            data = os.read(self._src_fd, self._buf_size)
            if len(data) == 0:
                self._tidy_up()
            else:
                self._buf += data
                self._set_flags()
        elif fd_condition & gobject.IO_HUP:
            self._tidy_up()
        else:
            assert False, fd_condition
        return True

    def _write_callback(self, unused_fd, fd_condition):
        if fd_condition & gobject.IO_ERR:
            self._tidy_up()
        elif fd_condition & gobject.IO_OUT:
            written_count = os.write(self._dest_fd, self._buf)
            self._buf = self._buf[written_count:]
            self._set_flags()
        else:
            assert False, fd_condition
        return True

    def _tidy_up(self):
        if self._running:
            self._running = False
            self._reader.destroy()
            self._writer.destroy()
            self._reason_to_run.dispose()
            os.close(self._src_fd)
            os.close(self._dest_fd)

    def flush(self):
        if self._running:
            rlist, wlist, xlist = \
                   select.select([self._src_fd], [self._dest_fd], [], 0)
            if self._src_fd in rlist and self._dest_fd in wlist:
                data = os.read(self._src_fd, self._buf_size)
                self._buf += data
                written = os.write(self._dest_fd, self._buf)
                self._buf = self._buf[written:]
            self._tidy_up()


def proxy_input_fd(fd):
    pipe_read, pipe_write = os.pipe()
    forwarder = ForwardFD(fd, pipe_write)
    return (plash_core.wrap_fd(pipe_read), forwarder)

def proxy_output_fd(fd):
    pipe_read, pipe_write = os.pipe()
    forwarder = ForwardFD(pipe_read, fd)
    return (plash_core.wrap_fd(pipe_write), forwarder)
