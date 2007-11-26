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
import weakref

import protocol_simple
import protocol_stream


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


class ExportTableSimple(object):

    def __init__(self, initial_exported_objects):
        self._by_index = {}
        for index, obj in enumerate(initial_exported_objects):
            self._by_index[index] = obj

    # For use in unit tests
    def get_all_exports(self):
        return self._by_index

    def export_object(self, obj):
        # TODO: find free ID more efficiently!
        index = 0
        while index in self._by_index:
            index += 1
        self._by_index[index] = obj
        return index

    def get(self, index):
        return self._by_index[index]

    def drop(self, index):
        del self._by_index[index]

    def is_empty(self):
        return len(self._by_index) == 0


class ExportTablePreservingEQ(object):

    def __init__(self, initial_exported_objects):
        self._by_index = {}
        self._by_object = {}
        self._export_count = {}
        for index, obj in enumerate(initial_exported_objects):
            self._by_index[index] = obj
            self._by_object[obj] = index
            self._export_count[index] = 1

    # For use in unit tests
    def get_all_exports(self):
        return self._by_index

    def export_object(self, obj):
        index = self._by_object.get(obj, None)
        if index is not None:
            self._export_count[index] += 1
            return index
        # TODO: find free ID more efficiently!
        index = 0
        while index in self._by_index:
            index += 1
        self._by_index[index] = obj
        self._by_object[obj] = index
        self._export_count[index] = 1
        return index

    def get(self, index):
        return self._by_index[index]

    def drop(self, index):
        self._export_count[index] -= 1
        if self._export_count[index] == 0:
            del self._by_object[self._by_index[index]]
            del self._by_index[index]
            del self._export_count[index]

    def is_empty(self):
        return len(self._by_index) == 0


class ConnectionPrivate(object):

    def __init__(self, event_loop, writer, disconnect_callback, exports):
        self.event_loop = event_loop
        self._connected = True
        self._writer = writer
        self._disconnect = disconnect_callback
        self._import_count = 0
        self._imported = weakref.WeakValueDictionary()
        self._exports = exports

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
            return encode_wire_id(NAMESPACE_SENDER,
                                  self._exports.export_object(obj))
        # TODO: look for single use hint

    # Decodes incoming IDs to objects
    def _wire_id_to_object(self, object_wire_id):
        namespace, object_index = decode_wire_id(object_wire_id)
        if namespace == NAMESPACE_RECEIVER:
            return self._exports.get(object_index)
        elif namespace == NAMESPACE_SENDER:
            # TODO: preserve EQ by reusing RemoteObjects
            return self._import_object(object_index, single_use=False)
        elif namespace == NAMESPACE_SENDER_SINGLE_USE:
            return self._import_object(object_index, single_use=True)
        else:
            raise Exception("Unknown namespace ID: %i" % namespace)

    def _import_object(self, object_index, single_use):
        imported = self._imported.get(object_index, None)
        if imported is not None:
            imported._ref_count += 1
        else:
            self._import_count += 1
            imported = RemoteObject(self, object_index, single_use)
            self._imported[object_index] = imported
        return imported

    def get_initial_imported_objects(self, count):
        return [self._import_object(object_id, single_use=False)
                for object_id in range(count)]

    def handle_message(self, message_data):
        assert self._connected
        args = decode_pocp_message(message_data)
        if args[0] == "invoke":
            tag_unused, dest_wire_id, arg_ids, body_data = args
            dest_namespace, dest_index = decode_wire_id(dest_wire_id)
            assert dest_namespace == NAMESPACE_RECEIVER, dest_namespace
            dest_object = self._exports.get(dest_index)
            arg_objects = tuple(self._wire_id_to_object(object_id)
                                for object_id in arg_ids)
            fds = ()
            dest_object.cap_invoke((body_data, arg_objects, fds))
            # TODO: remove single use object from table on invocation
        elif args[0] == "drop":
            tag_unused, object_wire_id = args
            dest_namespace, dest_index = decode_wire_id(object_wire_id)
            assert dest_namespace == NAMESPACE_RECEIVER, dest_namespace
            self._exports.drop(dest_index)
            # This check should not be necessary if the other end
            # behaves sensibly, because it should drop the connection
            # rather than send a final "drop" message across it.
            # TODO: produce a warning if we find the other end's actions
            # have made the connection moribund.
            self._check_for_moribund_connection()
        else:
            raise Exception("Unknown message: %r" % args)

    def handle_disconnection(self):
        # If the other end drops the connection, this
        # ConnectionPrivate can still remain live because the
        # RemoteObject wrappers for the imported objects (now broken)
        # can remain live.  We must ensure that we don't hold
        # references to exported objects.
        self._connected = False
        self._exports = None

    def send_object_invocation(self, object_id, data, caps, fds, is_single_use):
        if self._connected:
            self._send(
                make_invoke_message(
                    encode_wire_id(NAMESPACE_RECEIVER, object_id),
                    [self._object_to_wire_id(object)
                     for object in caps],
                    data))
            if is_single_use:
                self._decrement_import_count()

    def drop_imported_reference(self, object_id, ref_count):
        # _decrement_import_count() might cause the whole connection
        # to be dropped, in which case the second message does not
        # need to be sent and will simply be ignored.
        self._decrement_import_count()
        # TODO: add a more general "drop" message that takes a count,
        # instead of having to send multiple messages.
        for unused in range(ref_count):
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
        if (self._connected and
            self._import_count == 0 and self._exports.is_empty()):
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
        self._ref_count = 1
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
            self._connection.drop_imported_reference(self._object_id,
                                                     self._ref_count)
            # Just in case the object somehow becomes live again
            self._connection = None

    # Implements call-return.  This is not part of the core remote protocol.
    def cap_call(self, (data, caps, fds)):
        result_slot = ResultSlot()
        return_cont = ReturnContinuation(result_slot)
        self.cap_invoke((data, (return_cont,) + caps, fds))
        del return_cont
        while not result_slot.is_resolved():
            # TODO: this assertion doesn't belong here
            assert self._connection.event_loop.is_listening()
            self._connection.event_loop.once()
        return result_slot.get()


class ResultSlot(object):

    def __init__(self):
        self._result = None

    def is_resolved(self):
        return self._result is not None

    def resolve(self, val):
        self._result = val

    def get(self):
        return self._result


class ReturnContinuation(PlashObject):

    def __init__(self, slot):
        self._result_slot = slot

    def cap_invoke(self, args):
        if not self._result_slot.is_resolved():
            self._result_slot.resolve(args)
        else:
            # TODO: warn about multiple return calls?
            pass

    def __del__(self):
        if not self._result_slot.is_resolved():
            self._result_slot.resolve(("Fail", (), ()))


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
        error_watch.remove_watch()

    def on_fd_error(flags):
        connection.handle_disconnection()

    def on_eof():
        connection.handle_disconnection()
        writer.end_of_stream()
        error_watch.remove_watch()

    export_table = ExportTablePreservingEQ(caps_export)
    writer = protocol_stream.FDBufferedWriter(event_loop, socket_fd)
    connection = ConnectionPrivate(event_loop, writer, disconnect, export_table)
    reader = protocol_stream.FDBufferedReader(
        event_loop, socket_fd, connection.handle_message, on_eof)
    error_watch = event_loop.make_error_watch(socket_fd, on_fd_error)
    return connection.get_initial_imported_objects(import_count)
