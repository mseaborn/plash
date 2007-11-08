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

import struct


class IncompleteMessageException(Exception):

    pass


def pad_size(size):
    return (4 - (size & 3)) & 3


def round_up_to_word(size):
    return (size + 3) & ~3


def make_message(data):
    # TODO: handle file descriptors
    fds_count = 0
    padding = "\0" * pad_size(len(data))
    return "MSG!" + struct.pack("ii", len(data), fds_count) + data + padding


class InputBuffer(object):

    def __init__(self):
        self._buf = ""
        self._callback = lambda: None

    def connect(self, callback):
        self._callback = callback

    def add(self, data):
        self._buf += data
        self._callback()

    def get_buffer(self):
        return self._buf

    def remove_bytes(self, size):
        self._buf = self._buf[size:]


def decode_message(data):
    if len(data) >= 4:
        assert data[:4] == "MSG!"
    if len(data) < 12:
        raise IncompleteMessageException()
    check_tag, size, fds_count = struct.unpack("iii", data[:12])
    message_size = 12 + round_up_to_word(size)
    if len(data) < message_size:
        raise IncompleteMessageException()
    body_data = data[12:12+size]
    return (body_data, message_size)


def read_message(buf):
    body_data, message_size = decode_message(buf.get_buffer())
    buf.remove_bytes(message_size)
    return body_data
