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
import unittest

from plash.comms.event_loop import poll_fd
import plash.comms.cap as cap
import plash.comms.event_loop_test
import plash.comms.simple
import plash.comms.stream


class CapProtocolEncodingTest(unittest.TestCase):

    def test_id_encoding(self):
        args = (cap.NAMESPACE_SENDER, 12345)
        self.assertEquals(cap.decode_wire_id(cap.encode_wire_id(*args)), args)

    def test_invoke_encoding(self):
        invoke_args = (123, [4, 5, 6], "body")
        self.assertEquals(
            cap.decode_pocp_message(cap.make_invoke_message(*invoke_args)),
            ("invoke",) + invoke_args)

    def test_drop_encoding(self):
        self.assertEquals(
            cap.decode_pocp_message(cap.make_drop_message(789)),
            ("drop", 789))


def tcp_socketpair():
    # Pick an arbitrary port.  This could interfere with another
    # program running on the machine, but there is not really any way
    # to avoid that.  socketpair() doesn't work for AF_INET.
    address = ("localhost", 47205)
    sock_listener = socket.socket(socket.AF_INET)
    sock_listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock_listener.bind(address)
    sock_listener.listen(1)
    sock1 = socket.socket(socket.AF_INET)
    sock1.connect(address)
    sock2, address2 = sock_listener.accept()
    return (plash.comms.stream.WrappedFD(os.dup(sock1.fileno())),
            plash.comms.stream.WrappedFD(os.dup(sock2.fileno())))


class TestExportTable(unittest.TestCase):

    cls = cap.ExportTableSimple

    def test_exporting_object(self):
        table = self.cls([])
        obj = cap.PlashObject()
        index = table.export_object(obj)
        self.assertEquals(table.get(index), obj)
        table.drop(index)
        self.assertRaises(KeyError, lambda: table.get(index))


class TestExportTablePreservingEQ(TestExportTable):

    cls = cap.ExportTablePreservingEQ

    def test_exporting_object_twice(self):
        table = self.cls([])
        obj = cap.PlashObject()
        index = table.export_object(obj)
        index2 = table.export_object(obj)
        self.assertEquals(index, index2)
        self.assertEquals(table.get(index), obj)
        table.drop(index)
        self.assertEquals(table.get(index), obj)
        table.drop(index)
        self.assertRaises(KeyError, lambda: table.get(index))


class CallLogger(cap.PlashObjectBase):

    def __init__(self):
        self.calls = []

    def cap_invoke(self, *args):
        self.calls.append(("cap_invoke", args))


class SocketPairTestCase(plash.comms.event_loop_test.EventLoopTestCase):

    @classmethod
    def get_method_combinations(cls):
        for methods in super(SocketPairTestCase, cls).get_method_combinations():
            yield ["set_unix_socket"] + methods
            yield ["set_tcp_socket"] + methods

    def set_unix_socket(self):
        self.socketpair = plash.comms.stream.socketpair

    def set_tcp_socket(self):
        self.socketpair = tcp_socketpair


class CapProtocolTestsMixin(object):

    base_class = cap.PlashObjectBase

    def _read_to_list(self, loop, socket_fd):
        messages = []
        plash.comms.stream.FDBufferedReader(loop, socket_fd, messages.append)
        return messages

    def _connection_dropped(self, sock):
        # AF_UNIX sockets set POLLHUP when the other end has been
        # closed, but AF_INET sockets do not set POLLHUP.  Both set
        # POLLIN and return an EOF condition from read().
        if poll_fd(sock) & select.POLLIN:
            return len(os.read(sock.fileno(), 100)) == 0
        return False

    def test_sending(self):
        loop = self.make_event_loop()
        sock1, sock2 = self.socketpair()
        got = self._read_to_list(loop, sock1)
        imported_objects = self.make_connection(loop, sock2, [], 10)
        # Should work for any sequence of valid indexes
        for index in (0, 0, 1, 2):
            imported_objects[index].cap_invoke(("body data", (), ()))
            loop.run_awhile()
            decoded = [cap.decode_pocp_message(data) for data in got]
            self.assertEquals(
                decoded, [("invoke",
                           cap.encode_wire_id(cap.NAMESPACE_RECEIVER, index),
                           [], "body data")])
            got[:] = []

    def test_sending_references(self):
        loop = self.make_event_loop()
        sock1, sock2 = self.socketpair()
        got = self._read_to_list(loop, sock1)
        [imported] = self.make_connection(loop, sock2, [], 1)
        arg_objects = [cap.PlashObject() for i in range(10)]
        imported.cap_invoke(("body", tuple(arg_objects), ()))
        loop.run_awhile()
        decoded = [cap.decode_pocp_message(data) for data in got]
        self.assertEquals(
            decoded, [("invoke",
                       cap.encode_wire_id(cap.NAMESPACE_RECEIVER, 0),
                       [cap.encode_wire_id(cap.NAMESPACE_SENDER, index)
                        for index in range(10)], "body")])
        # Check internal state of the connection
        self.check_exports(
            imported,
            dict((index, arg_objects[index]) for index in range(10)))

    def check_exports(self, imported, exports):
        self.assertEquals(
            imported._connection._exports.get_all_exports(), exports)

    def test_sending_references_preserves_eq(self):
        calls = []
        class Object(cap.PlashObject):
            def cap_invoke(self, args):
                calls.append(args)

        loop = self.make_event_loop()
        sock1, sock2 = self.socketpair()
        got = self._read_to_list(loop, sock1)
        [imported] = self.make_connection(loop, sock2, [], 1)
        writer = plash.comms.stream.FDBufferedWriter(loop, sock1)
        object1 = Object()
        object2 = Object()
        imported.cap_invoke(("body", (object1, object1, object2), ()))
        loop.run_awhile()
        decoded = [cap.decode_pocp_message(data) for data in got]
        self.assertEquals(
            decoded, [("invoke", cap.encode_wire_id(cap.NAMESPACE_RECEIVER, 0),
                       [cap.encode_wire_id(cap.NAMESPACE_SENDER, 0),
                        cap.encode_wire_id(cap.NAMESPACE_SENDER, 0),
                        cap.encode_wire_id(cap.NAMESPACE_SENDER, 1)], "body")])
        # Now check that we can invoke the resulting object and drop
        # it twice
        for unused in range(2):
            writer.write(plash.comms.simple.make_message(
                    cap.make_invoke_message(
                        cap.encode_wire_id(cap.NAMESPACE_RECEIVER, 0),
                        [], "body")))
            writer.write(plash.comms.simple.make_message(
                    cap.make_drop_message(
                        cap.encode_wire_id(cap.NAMESPACE_RECEIVER, 0))))
        loop.run_awhile()

    def test_receiving(self):
        loop = self.make_event_loop()
        sock1, sock2 = self.socketpair()
        exported_objects = [CallLogger() for i in range(10)]
        self.make_connection(loop, sock1, exported_objects)
        writer = plash.comms.stream.FDBufferedWriter(loop, sock2)
        # Should work for any sequence of valid indexes
        for index in (0, 0, 1, 2):
            writer.write(
                plash.comms.simple.make_message(cap.make_invoke_message(
                        cap.encode_wire_id(cap.NAMESPACE_RECEIVER, index),
                        [], "body data")))
            loop.run_awhile()
            self.assertEquals(exported_objects[index].calls,
                              [("cap_invoke", (("body data", (), ()),))])
            exported_objects[index].calls[:] = []

    def test_receiving_references(self):
        calls = []
        class Object(cap.PlashObject):
            def cap_invoke(self, args):
                calls.append(args)

        loop = self.make_event_loop()
        sock1, sock2 = self.socketpair()
        exported = Object()
        self.make_connection(loop, sock1, [exported])
        writer = plash.comms.stream.FDBufferedWriter(loop, sock2)
        # Should work for any sequence of indexes
        indexes = [6, 2, 52, 8, 4]
        writer.write(plash.comms.simple.make_message(cap.make_invoke_message(
                    cap.encode_wire_id(cap.NAMESPACE_RECEIVER, 0),
                    [cap.encode_wire_id(cap.NAMESPACE_SENDER, index)
                     for index in indexes],
                    "body data")))
        loop.run_awhile()
        self.assertEquals(len(calls), 1)
        data, caps, fds = calls[0]
        self.assertEquals(len(caps), len(indexes))
        # Check internal state of received proxy objects
        for index, received_obj in zip(indexes, caps):
            assert isinstance(received_obj, self.base_class), received_obj
            self.check_remote_object(received_obj, object_id=index,
                                     single_use=False)
        # Break "distributed" cycle:  The connection refers to the
        # "exported" object, which refers to imported objects via
        # "calls", which refer to the connection.
        calls[:] = []

    def check_remote_object(self, obj, object_id, single_use):
        self.assertEquals(obj._object_id, object_id)
        self.assertEquals(obj._single_use, single_use)

    def test_receiving_references_preserves_eq(self):
        calls = []
        class Object(cap.PlashObject):
            def cap_invoke(self, args):
                calls.append(args)

        loop = self.make_event_loop()
        sock1, sock2 = self.socketpair()
        self.make_connection(loop, sock1, [Object()])
        got = self._read_to_list(loop, sock2)
        writer = plash.comms.stream.FDBufferedWriter(loop, sock2)
        writer.write(plash.comms.simple.make_message(
                cap.make_invoke_message(
                    cap.encode_wire_id(cap.NAMESPACE_RECEIVER, 0),
                    [cap.encode_wire_id(cap.NAMESPACE_SENDER, 123),
                     cap.encode_wire_id(cap.NAMESPACE_SENDER, 123),
                     cap.encode_wire_id(cap.NAMESPACE_SENDER, 456)], "body")))
        loop.run_awhile()
        self.assertEquals(len(calls), 1)
        data, caps, fds = calls[0]
        self.assertEquals(caps[0], caps[1])
        self.assertNotEquals(caps[1], caps[2])
        # Now check that we get the right number of "drop" messages
        del caps
        calls[:] = []
        loop.run_awhile()
        decoded = [cap.decode_pocp_message(data) for data in got]
        self.assertEquals(
            sorted(decoded),
            [("drop", cap.encode_wire_id(cap.NAMESPACE_RECEIVER, 123)),
             ("drop", cap.encode_wire_id(cap.NAMESPACE_RECEIVER, 123)),
             ("drop", cap.encode_wire_id(cap.NAMESPACE_RECEIVER, 456))])

    def test_dropping_imported_references(self):
        loop = self.make_event_loop()
        sock1, sock2 = self.socketpair()
        got = self._read_to_list(loop, sock1)
        imported_objects = list(self.make_connection(loop, sock2, [], 100))
        imported_objects[42] = None
        loop.run_awhile()
        decoded = [cap.decode_pocp_message(data) for data in got]
        self.assertEquals(
            decoded,
            [("drop", cap.encode_wire_id(cap.NAMESPACE_RECEIVER, 42))])

    def test_dropping_all_imported_references(self):
        loop = self.make_event_loop()
        sock1, sock2 = self.socketpair()
        got = self._read_to_list(loop, sock1)
        imported_objects = self.make_connection(loop, sock2, [], 1)
        del sock2
        del imported_objects
        loop.run_awhile()
        self.assertTrue(self._connection_dropped(sock1))
        decoded = [cap.decode_pocp_message(data) for data in got]
        # The last "drop" message is unnecessary and we should not
        # receive it, because the connection will be dropped instead.
        self.assertEquals(decoded, [])

    def test_dropping_exported_references(self):
        deleted = []
        class Object(cap.PlashObject):
            def __init__(self, index):
                self._index = index
            def __del__(self):
                deleted.append(self._index)

        loop = self.make_event_loop()
        sock1, sock2 = self.socketpair()
        self.make_connection(loop, sock1, [Object(index)
                                           for index in range(100)])
        writer = plash.comms.stream.FDBufferedWriter(loop, sock2)
        # Should work for any sequence of indexes
        indexes = [5, 10, 56, 1, 0]
        for index in indexes:
            writer.write(plash.comms.simple.make_message(cap.make_drop_message(
                        cap.encode_wire_id(cap.NAMESPACE_RECEIVER, index))))
        loop.run_awhile()
        self.assertEquals(deleted, indexes)

    def test_dropping_all_exported_references(self):
        loop = self.make_event_loop()
        sock1, sock2 = self.socketpair()
        self.make_connection(loop, sock1, [cap.PlashObject()
                                           for index in range(100)])
        del sock1
        writer = plash.comms.stream.FDBufferedWriter(loop, sock2)
        for index in range(100):
            writer.write(plash.comms.simple.make_message(cap.make_drop_message(
                        cap.encode_wire_id(cap.NAMESPACE_RECEIVER, index))))
        loop.run_awhile()
        self.assertTrue(self._connection_dropped(sock2))

    def test_handling_dropped_connection(self):
        for import_count in (0, 1, 2):
            deleted = []
            class Object(cap.PlashObject):
                def __init__(self, index):
                    self._index = index
                def __del__(self):
                    deleted.append(self._index)

            loop = self.make_event_loop()
            sock1, sock2 = self.socketpair()
            imported = self.make_connection(
                loop, sock1, [Object(index) for index in range(10)],
                import_count)
            del sock2
            loop.run_awhile()
            self.assertTrue(not loop.is_listening())
            self.assertEquals(sorted(deleted), range(10))
            for imported_object in imported:
                self.check_not_connected(imported_object)
                imported_object.cap_invoke(
                    ("message that will get ignored", (), ()))

    def check_not_connected(self, obj):
        self.assertFalse(obj._connection._connected)

    def test_handling_dropped_connection_econnreset(self):
        import_count = 1
        loop = self.make_event_loop()
        sock1, sock2 = self.socketpair()
        imported = self.make_connection(loop, sock1, [], import_count)
        # Try to trigger an ECONNRESET condition by writing data which
        # is not read before the other end closes the connection.
        imported[0].cap_invoke(("data", (), ()))
        loop.run_awhile()
        del sock2
        loop.once_safely()

    def test_creating_useless_connection(self):
        loop = self.make_event_loop()
        sock1, sock2 = self.socketpair()
        # This connection neither imports nor exports any references,
        # so it starts off moribund.
        self.make_connection(loop, sock1, [])
        del sock1
        self.assertTrue(self._connection_dropped(sock2))

    def test_receiving_and_invoking_single_use_reference(self):
        calls = []
        class Object(cap.PlashObject):
            def cap_invoke(self, args):
                calls.append(args)

        loop = self.make_event_loop()
        sock1, sock2 = self.socketpair()
        exported = Object()
        self.make_connection(loop, sock1, [exported])
        del sock1
        received = self._read_to_list(loop, sock2)
        writer = plash.comms.stream.FDBufferedWriter(loop, sock2)
        writer.write(plash.comms.simple.make_message(cap.make_invoke_message(
                    cap.encode_wire_id(cap.NAMESPACE_RECEIVER, 0),
                    [cap.encode_wire_id(cap.NAMESPACE_SENDER_SINGLE_USE, 1234)],
                    "body data")))
        # Drop the exported object so that the imported single use
        # object is the only one left.
        writer.write(plash.comms.simple.make_message(cap.make_drop_message(
                    cap.encode_wire_id(cap.NAMESPACE_RECEIVER, 0))))
        loop.run_awhile()
        self.assertEquals(len(calls), 1)
        data, caps, fds = calls[0]
        [received_obj] = caps
        assert isinstance(received_obj, self.base_class), received_obj
        self.check_remote_object(received_obj, object_id=1234, single_use=True)
        self.check_object_used_up(received_obj, used_up=False)
        received_obj.cap_invoke(("some return message", (), ()))
        self.check_object_used_up(received_obj, used_up=True)

        # This tests sending a message and immediately disconnecting.
        # The message should be queued, not dropped.  The message
        # should be sent before the connection is dropped.
        loop.run_awhile()
        decoded = [cap.decode_pocp_message(data) for data in received]
        self.assertEquals(
            decoded,
            [("invoke", cap.encode_wire_id(cap.NAMESPACE_RECEIVER, 1234),
              [], "some return message")])
        self.assertTrue(self._connection_dropped(sock2))

    def check_object_used_up(self, obj, used_up):
        self.assertEquals(obj._connection is None, used_up)

    # TODO: check receiving bogus IDs

    # TODO: check receiving bogus messages


class CapProtocolPythonTests(CapProtocolTestsMixin, SocketPairTestCase):

    def make_connection(self, *args, **kwargs):
        return cap.make_connection(*args, **kwargs)


class CapProtocolEndToEndTests(SocketPairTestCase):

    # End-to-end test: makes no assumptions about the protocol's encoding.

    def test_sending_and_receiving(self):
        loop = self.make_event_loop()
        sock1, sock2 = self.socketpair()
        exported_objects = [CallLogger() for i in range(10)]
        cap.make_connection(loop, sock1, exported_objects)
        imported_objects = cap.make_connection(loop, sock2, [], 10)
        # Should work for any sequence of valid indexes
        for index in (0, 1, 1, 2):
            msg = ("some data for the body", (), ())
            imported_objects[index].cap_invoke(msg)
            loop.run_awhile()
            self.assertEquals(exported_objects[index].calls,
                              [("cap_invoke", (msg,))])
            exported_objects[index].calls[:] = []

    def test_call_return(self):
        calls = []
        class Object(cap.LocalObject):
            def cap_call(self, args):
                calls.append(args)
                return ("result body", (), ())

        loop = self.make_event_loop()
        loop.once = loop.once_safely # Should really apply this for all tests
        sock1, sock2 = self.socketpair()
        cap.make_connection(loop, sock1, [Object()])
        [imported] = cap.make_connection(loop, sock2, [], 1)
        result = imported.cap_call(("args body", (), ()))
        self.assertEquals(calls, [("args body", (), ())])
        self.assertEquals(result, ("result body", (), ()))

    def test_call_return_nested(self):
        limit = 4
        calls = []

        class Object(cap.LocalObject):

            def __init__(self, name):
                self._name = name
                self.other = None

            def cap_call(self, args):
                calls.append("call %s" % self._name)
                if len(calls) < limit:
                    self.other.cap_call(("body", (), ()))
                calls.append("return %s" % self._name)
                return ("body", (), ())

        a = Object("a")
        b = Object("b")
        loop = self.make_event_loop()
        loop.once = loop.once_safely # Should really apply this for all tests
        sock1, sock2 = self.socketpair()
        [b_imported] = cap.make_connection(loop, sock1, [a], 1)
        [a_imported] = cap.make_connection(loop, sock2, [b], 1)
        a.other = b_imported
        b.other = a_imported
        result = a_imported.cap_call(("body", (), ()))
        self.assertEquals(calls,
                          ["call a", "call b", "call a", "call b",
                           "return b", "return a", "return b", "return a"])
        # Break "distributed" cycle
        a.other = None
        b.other = None

    def test_return_continuation_dropped(self):
        calls = []
        class Object(cap.PlashObject):
            def cap_invoke(self, (data, caps, fds)):
                calls.append((data, caps[1:], fds))

        loop = self.make_event_loop()
        loop.once = loop.once_safely # Should really apply this for all tests
        sock1, sock2 = self.socketpair()
        cap.make_connection(loop, sock1, [Object()])
        [imported] = cap.make_connection(loop, sock2, [], 1)
        result = imported.cap_call(("args body", (), ()))
        self.assertEquals(calls, [("args body", (), ())])
        self.assertEquals(result, ("Fail", (), ()))


if __name__ == "__main__":
    unittest.main()
