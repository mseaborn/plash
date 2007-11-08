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
import struct


NAMESPACE_RECEIVER = 0
NAMESPACE_SENDER = 1
NAMESPACE_SENDER_SINGLE_USE = 2


def encode_wire_id(namespace, id):
    return (id << 8) | namespace


def decode_wire_id(id_with_namespace):
    return (id_with_namespace & 0xff, id_with_namespace >> 8)


def make_invoke_message(dest_object_id, argument_object_ids, data):
    return "".join(["Invk", struct.pack("ii", dest_object_id,
                                        len(argument_object_ids))]
                   + [struct.pack("i", obj_id)
                      for obj_id in argument_object_ids]
                   + [data])


def make_drop_message(object_id):
    return "Drop" + struct.pack("i", object_id)


def decode_pocp_message(data):
    tag = data[:4]
    if tag == "Invk":
        dest_id, cap_args_count = struct.unpack("ii", data[4:12])
        offset = 12
        cap_arg_ids = []
        for unused in range(cap_args_count):
            (cap_arg_id,) = struct.unpack("i", data[offset:offset+4])
            cap_arg_ids.append(cap_arg_id)
            offset += 4
        return ("invoke", dest_id, cap_arg_ids, data[offset:])
    elif tag == "Drop":
        (dest_id,) = struct.unpack("i", data[4:])
        return ("drop", dest_id)
    else:
        raise Exception("Unknown message: %s" % tag)


class WrappedFD(object):

    def __init__(self, fd):
        assert isinstance(fd, int)
        self._fd = fd

    def fileno(self):
        return self._fd

    def __del__(self):
        os.close(self._fd)


def make_pipe():
    pipe_read, pipe_write = os.pipe()
    return WrappedFD(pipe_read), WrappedFD(pipe_write)


class FDBufferedWriter(object):

    def __init__(self, event_loop, fd):
        self._fd = fd
        self._buf = ""
        event_loop.make_watch(fd, self._get_flags, self._handler)

    def _get_flags(self):
        if len(self._buf) == 0:
            return 0
        else:
            return select.POLLOUT

    def _handler(self, flags):
        written = os.write(self._fd.fileno(), self._buf)
        self._buf = self._buf[written:]

    def write(self, data):
        self._buf += data
