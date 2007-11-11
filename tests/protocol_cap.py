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
        self._eof_requested = False
        self._connection_broken = False
        self._watch1 = event_loop.make_watch(fd, self._get_flags, self._handler)
        self._watch2 = event_loop.make_error_watch(fd, self._error_handler)

    def _get_flags(self):
        if len(self._buf) == 0 or self._connection_broken:
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
            self._check_for_disconnect()

    def _error_handler(self, flags):
        # An error occurred, so no more data can be written.
        self._fd = None
        self._buf = ""
        self._connection_broken = True
        self._watch1.remove_watch()
        self._watch2.remove_watch()

    def _check_for_disconnect(self):
        if self._eof_requested and len(self._buf) == 0:
            self._fd = None
            self._watch1.remove_watch()
            self._watch2.remove_watch()

    def write(self, data):
        # Should this raise an exception if called after end_of_stream()?
        if not self._eof_requested and not self._connection_broken:
            self._buf += data

    def end_of_stream(self):
        # Declares that no more data will be written.  Any buffered
        # data should be written before closing the FD.
        self._eof_requested = True
        self._check_for_disconnect()


class FDBufferedReader(object):

    def __init__(self, event_loop, fd, callback):
        self._fd = fd
        self._callback = callback
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

    def _handle(self):
        while True:
            try:
                message = protocol_simple.read_message(self._buffer)
            except protocol_simple.IncompleteMessageException:
                break
            else:
                self._callback(message)


class ConnectionPrivate(object):

    def __init__(self, event_loop, writer, disconnect_callback, export_objects):
        self.event_loop = event_loop
        self._writer = writer
        self._disconnect = disconnect_callback
        self._import_count = 0
        self._export = {}
        for index, obj in enumerate(export_objects):
            self._export[index] = obj

    def _send(self, data):
        self._writer.write(protocol_simple.make_message(data))

    # Encode outgoing objects to IDs
    def _object_to_wire_id(self, obj):
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
            return self._import_object(object_index, single_use=False)
        elif namespace == NAMESPACE_SENDER_SINGLE_USE:
            return self._import_object(object_index, single_use=True)
        else:
            raise Exception("Unknown namespace ID: %i" % namespace)

    def _import_object(self, object_index, single_use):
        self._import_count += 1
        return RemoteObject(self, object_index, single_use)

    def get_initial_imported_objects(self, count):
        return [self._import_object(object_id, single_use=False)
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
            dest_namespace, dest_index = decode_wire_id(object_wire_id)
            assert dest_namespace == NAMESPACE_RECEIVER, dest_namespace
            del self._export[dest_index]
            # This check should not be necessary if the other end
            # behaves sensibly, because it should drop the connection
            # rather than send a final "drop" message across it.
            # TODO: produce a warning if we find the other end's actions
            # have made the connection moribund.
            self._check_for_moribund_connection()
        else:
            raise Exception("Unknown message: %r" % args)

    def send_object_invocation(self, object_id, data, caps, fds, is_single_use):
        self._send(
            make_invoke_message(
                encode_wire_id(NAMESPACE_RECEIVER, object_id),
                [self._object_to_wire_id(object)
                 for object in caps],
                data))
        if is_single_use:
            self._decrement_import_count()

    def drop_imported_reference(self, object_id):
        # _decrement_import_count() might cause the whole connection
        # to be dropped, in which case the second message does not
        # need to be sent and will simply be ignored.
        self._decrement_import_count()
        self._send(make_drop_message(
                encode_wire_id(NAMESPACE_RECEIVER, object_id)))

    def _decrement_import_count(self):
        assert self._import_count > 0, self._import_count
        self._import_count -= 1
        self._check_for_moribund_connection()

    def _check_for_moribund_connection(self):
        # A moribund connection is one where neither side can legally
        # send messages, because no references are imported or
        # exported.
        if self._import_count == 0 and len(self._export) == 0:
            self._disconnect()


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
            self._connection.send_object_invocation(
                self._object_id, data, caps, fds, self._single_use)
            if self._single_use:
                self._connection = None

    def __del__(self):
        if self._connection is not None:
            self._connection.drop_imported_reference(self._object_id)
            # Just in case the object somehow becomes live again
            self._connection = None

    # Implements call-return.  This is not part of the core remote protocol.
    def cap_call(self, (data, caps, fds)):
        # TODO: should really split this into separate resolver/result facets
        return_cont = ReturnContinuation()
        self.cap_invoke((data, (return_cont,) + caps, fds))
        while return_cont._result is None:
            # TODO: this assertion doesn't belong here
            assert self._connection.event_loop.is_listening()
            self._connection.event_loop.once()
        return return_cont._result


class ReturnContinuation(PlashObject):

    def __init__(self):
        self._result = None

    def cap_invoke(self, args):
        if self._result is None:
            self._result = args
        else:
            # TODO: warn about multiple return calls?
            pass


class LocalObject(PlashObject):

    # Half of call-return.  The other half is RemoteObject.cap_invoke.
    def cap_invoke(self, (data, caps, fds)):
        return_cont = caps[0]
        result = self.cap_call((data, caps[1:], fds))
        return_cont.cap_invoke(result)


def make_connection(event_loop, socket_fd, caps_export, import_count=0):
    if len(caps_export) == 0 and import_count == 0:
        # No need to create a connection.  socket_fd will be dropped.
        return []

    def disconnect():
        writer.end_of_stream()
        reader.finish()

    writer = FDBufferedWriter(event_loop, socket_fd)
    connection = ConnectionPrivate(event_loop, writer, disconnect, caps_export)
    reader = FDBufferedReader(event_loop, socket_fd, connection.handle_message)
    return connection.get_initial_imported_objects(import_count)
