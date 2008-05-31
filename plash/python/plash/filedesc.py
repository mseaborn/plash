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

import gobject

import plash.comms.event_loop
import plash.comms.stream
import plash.mainloop
import plash_core


wrap_fd = plash_core.wrap_fd


def dup_and_wrap_fd(fd):
    return wrap_fd(os.dup(fd))


def ForwardFD(source_fd, dest_fd, buf_size=1024):
    # TODO: take event_loop as an argument
    event_loop = plash.mainloop.event_loop
    return plash.comms.stream.FDForwarder(event_loop, wrap_fd(source_fd),
                                          wrap_fd(dest_fd), buf_size)


def proxy_input_fd(fd):
    pipe_read, pipe_write = os.pipe()
    forwarder = ForwardFD(fd, pipe_write)
    return (wrap_fd(pipe_read), forwarder)

def proxy_output_fd(fd):
    pipe_read, pipe_write = os.pipe()
    forwarder = ForwardFD(pipe_read, fd)
    return (wrap_fd(pipe_write), forwarder)
