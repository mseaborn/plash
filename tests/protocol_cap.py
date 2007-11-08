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
import struct

import protocol_simple


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


class FDBufferedReader(object):

    def __init__(self, event_loop, fd, callback):
        self._fd = fd
        self._callback = callback
        self._buffer = protocol_simple.InputBuffer()
        self._buffer.connect(self._handle)
        # TODO: extend to do flow control
        event_loop.make_watch(fd, lambda: select.POLLIN, self._read_data)

    def _read_data(self, flags):
        data = os.read(self._fd.fileno(), 1024)
        self._buffer.add(data)

    def _handle(self):
        while True:
            try:
                message = protocol_simple.read_message(self._buffer)
            except protocol_simple.IncompleteMessageException:
                break
            else:
                self._callback(message)


class ConnectionPrivate(object):

    def __init__(self, writer, export_objects):
        self._writer = writer
        self._export = {}
        for index, obj in enumerate(export_objects):
            self._export[index] = obj

    def send(self, data):
        self._writer.write(protocol_simple.make_message(data))

    # Encode outgoing objects to IDs
    def object_to_wire_id(self, obj):
        assert isinstance(obj, PlashObjectBase)
        # Synergy: need to be able to look inside RemoteObjects
        # Alternative is to maintain a hash table of RemoteObjects
        # we have created for this connection, but that involves extra
        # storage.
        if (isinstance(obj, RemoteObject) and
            obj._connection == self):
            return encode_wire_id(NAMESPACE_RECEIVER, obj._object_id)
        else:
            return encode_wire_id(NAMESPACE_SENDER, self._export_object(obj))
        # TODO: look for single use hint

    def _export_object(self, obj):
        # TODO: check whether already exported using hash table
        # TODO: find free ID more efficiently!
        index = 0
        while index in self._export:
            index += 1
        self._export[index] = obj
        return index

    # Decodes incoming IDs to objects
    def _wire_id_to_object(self, object_wire_id):
        namespace, object_index = decode_wire_id(object_wire_id)
        if namespace == NAMESPACE_RECEIVER:
            return self._export[object_index]
        elif namespace == NAMESPACE_SENDER:
            # TODO: preserve EQ by reusing RemoteObjects
            return RemoteObject(self, object_index, single_use=False)
        elif namespace == NAMESPACE_SENDER_SINGLE_USE:
            return RemoteObject(self, object_index, single_use=True)
        else:
            raise Exception("Unknown namespace ID: %i" % namespace)

    def get_initial_imported_objects(self, count):
        return [RemoteObject(self, object_id, single_use=False)
                for object_id in range(count)]

    def handle_message(self, message_data):
        args = decode_pocp_message(message_data)
        if args[0] == "invoke":
            tag_unused, dest_wire_id, arg_ids, body_data = args
            dest_namespace, dest_index = decode_wire_id(dest_wire_id)
            assert dest_namespace == NAMESPACE_RECEIVER, dest_namespace
            dest_object = self._export[dest_index]
            arg_objects = tuple(self._wire_id_to_object(object_id)
                                for object_id in arg_ids)
            fds = ()
            dest_object.cap_invoke((body_data, arg_objects, fds))
            # TODO: remove single use object from table on invocation
        elif args[0] == "drop":
            tag_unused, object_wire_id = args
            dest_namespace, dest_index = decode_wire_id(dest_wire_id)
            assert dest_namespace == NAMESPACE_RECEIVER, dest_namespace
            self._export.remove(dest_index)
        else:
            raise Exception("Unknown message: %r" % args)


class PlashObjectBase(object):

    # No methods at all, so that __getattr__ works for call logger
    pass


# TODO: move somewhere else
class PlashObject(PlashObjectBase):

    def cap_invoke(self, args):
        raise NotImplementedError()

    def cap_call(self, args):
        raise NotImplementedError()


class RemoteObject(PlashObject):

    def __init__(self, connection, object_id, single_use):
        self._connection = connection
        self._object_id = object_id
        self._single_use = single_use

    def cap_invoke(self, (data, caps, fds)):
        if self._connection is not None:
            self._connection.send(
                make_invoke_message(
                    encode_wire_id(NAMESPACE_RECEIVER, self._object_id),
                    [self._connection.object_to_wire_id(object)
                     for object in caps],
                    data))
            if self._single_use:
                self._connection = None

    def __del__(self):
        self._connection.send(make_drop_message(self._object_id))
        # Just in case the object somehow becomes live again
        self._connection = None


def make_connection(event_loop, socket_fd, caps_export, import_count=0):
    writer = FDBufferedWriter(event_loop, socket_fd)
    connection = ConnectionPrivate(writer, caps_export)
    FDBufferedReader(event_loop, socket_fd, connection.handle_message)
    return connection.get_initial_imported_objects(import_count)
