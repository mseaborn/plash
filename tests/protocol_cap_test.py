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
import unittest

from protocol_event_loop import EventLoop
import protocol_cap
import protocol_event_loop
import protocol_simple


class CapProtocolEncodingTest(unittest.TestCase):

    def test_id_encoding(self):
        args = (protocol_cap.NAMESPACE_SENDER, 12345)
        self.assertEquals(
            protocol_cap.decode_wire_id(
                protocol_cap.encode_wire_id(*args)),
            args)

    def test_invoke_encoding(self):
        invoke_args = (123, [4, 5, 6], "body")
        self.assertEquals(
            protocol_cap.decode_pocp_message(
                protocol_cap.make_invoke_message(*invoke_args)),
            ("invoke",) + invoke_args)

    def test_drop_encoding(self):
        self.assertEquals(
            protocol_cap.decode_pocp_message(
                protocol_cap.make_drop_message(789)),
            ("drop", 789))


class FDWrapperTest(unittest.TestCase):

    def test_fds_are_freed(self):
        pipe_read, pipe_write = protocol_cap.make_pipe()
        fd = pipe_read.fileno()
        fcntl.fcntl(fd, fcntl.F_GETFL)
        del pipe_read
        self.assertRaises(IOError,
                          lambda: fcntl.fcntl(fd, fcntl.F_GETFL))


class FDBufferedWriterTest(unittest.TestCase):

    def test_writing(self):
        loop = EventLoop()
        pipe_read, pipe_write = protocol_cap.make_pipe()
        writer = protocol_cap.FDBufferedWriter(loop, pipe_write)
        self.assertEquals(loop._get_fd_flags().values(), [0])
        writer.write("hello")
        self.assertEquals(loop._get_fd_flags().values(), [select.POLLOUT])
        loop.once_safely()
        # Should have written all of buffer to the pipe now
        self.assertEquals(loop._get_fd_flags().values(), [0])

    def test_writing_to_closed_pipe(self):
        loop = EventLoop()
        pipe_read, pipe_write = protocol_cap.make_pipe()
        writer = protocol_cap.FDBufferedWriter(loop, pipe_write)
        writer.write("hello")
        del pipe_read
        self.assertEquals(writer._connection_broken, False)
        loop.once_safely()
        self.assertEquals(writer._connection_broken, True)


class FDBufferedReaderTest(unittest.TestCase):

    def test_end_of_stream(self):
        got = []
        def callback(data):
            got.append(data)
        loop = EventLoop()
        pipe_read, pipe_write = protocol_cap.make_pipe()
        reader = protocol_cap.FDBufferedReader(loop, pipe_read, callback)
        self.assertTrue(loop.will_block())
        os.write(pipe_write.fileno(), protocol_simple.make_message("hello"))
        os.write(pipe_write.fileno(), protocol_simple.make_message("world"))
        self.assertTrue(not loop.will_block())
        loop.once_safely()
        self.assertEquals(got, ["hello", "world"])
        del pipe_write
        loop.once_safely()
        self.assertTrue(not loop.is_listening())


class CallLogger(protocol_cap.PlashObjectBase):

    def __init__(self):
        self.calls = []

    def __getattr__(self, attr):
        def func(*args):
            self.calls.append((attr, args))
        return func


class CapProtocolTests(unittest.TestCase):

    def _read_to_list(self, loop, socket_fd):
        messages = []
        protocol_cap.FDBufferedReader(loop, socket_fd, messages.append)
        return messages

    def _assert_connection_dropped(self, sock):
        self.assertEquals(
            protocol_event_loop.poll_fds({sock.fileno(): 0}, 0),
            [(sock.fileno(), select.POLLHUP)])

    def test_sending(self):
        loop = EventLoop()
        sock1, sock2 = protocol_cap.socketpair()
        got = self._read_to_list(loop, sock1)
        imported_objects = protocol_cap.make_connection(loop, sock2, [], 10)
        # Should work for any sequence of valid indexes
        for index in (0, 0, 1, 2):
            imported_objects[index].cap_invoke(("body data", (), ()))
            loop.run_awhile()
            decoded = [protocol_cap.decode_pocp_message(data) for data in got]
            self.assertEquals(
                decoded, [("invoke",
                           protocol_cap.encode_wire_id(
                               protocol_cap.NAMESPACE_RECEIVER, index),
                           [], "body data")])
            got[:] = []

    def test_sending_references(self):
        loop = EventLoop()
        sock1, sock2 = protocol_cap.socketpair()
        got = self._read_to_list(loop, sock1)
        [imported] = protocol_cap.make_connection(loop, sock2, [], 1)
        arg_objects = [protocol_cap.PlashObject() for i in range(10)]
        imported.cap_invoke(("body", tuple(arg_objects), ()))
        loop.run_awhile()
        decoded = [protocol_cap.decode_pocp_message(data) for data in got]
        self.assertEquals(
            decoded, [("invoke",
                       protocol_cap.encode_wire_id(
                           protocol_cap.NAMESPACE_RECEIVER, 0),
                       [protocol_cap.encode_wire_id(
                            protocol_cap.NAMESPACE_SENDER, index)
                        for index in range(10)], "body")])
        # Check internal state of the connection
        self.assertEquals(
            imported._connection._export,
            dict((index, arg_objects[index]) for index in range(10)))

    def test_receiving(self):
        loop = EventLoop()
        sock1, sock2 = protocol_cap.socketpair()
        exported_objects = [CallLogger() for i in range(10)]
        protocol_cap.make_connection(loop, sock1, exported_objects)
        writer = protocol_cap.FDBufferedWriter(loop, sock2)
        # Should work for any sequence of valid indexes
        for index in (0, 0, 1, 2):
            writer.write(protocol_simple.make_message(
                    protocol_cap.make_invoke_message(
                        protocol_cap.encode_wire_id(
                            protocol_cap.NAMESPACE_RECEIVER, index),
                        [], "body data")))
            loop.run_awhile()
            self.assertEquals(exported_objects[index].calls,
                              [("cap_invoke", (("body data", (), ()),))])
            exported_objects[index].calls[:] = []

    def test_receiving_references(self):
        calls = []
        class Object(protocol_cap.PlashObject):
            def cap_invoke(self, args):
                calls.append(args)

        loop = EventLoop()
        sock1, sock2 = protocol_cap.socketpair()
        exported = Object()
        protocol_cap.make_connection(loop, sock1, [exported])
        writer = protocol_cap.FDBufferedWriter(loop, sock2)
        # Should work for any sequence of indexes
        indexes = [6, 2, 52, 8, 4]
        writer.write(protocol_simple.make_message(
                protocol_cap.make_invoke_message(
                    protocol_cap.encode_wire_id(
                        protocol_cap.NAMESPACE_RECEIVER, 0),
                    [protocol_cap.encode_wire_id(
                            protocol_cap.NAMESPACE_SENDER, index)
                     for index in indexes],
                    "body data")))
        loop.run_awhile()
        self.assertEquals(len(calls), 1)
        data, caps, fds = calls[0]
        self.assertEquals(len(caps), len(indexes))
        # Check internal state of received proxy objects
        for index, received_obj in zip(indexes, caps):
            assert isinstance(received_obj, protocol_cap.RemoteObject)
            self.assertEquals(received_obj._object_id, index)
            self.assertEquals(received_obj._single_use, False)

    def test_dropping_imported_references(self):
        loop = EventLoop()
        sock1, sock2 = protocol_cap.socketpair()
        got = self._read_to_list(loop, sock1)
        imported_objects = protocol_cap.make_connection(loop, sock2, [], 100)
        imported_objects[42] = None
        loop.run_awhile()
        decoded = [protocol_cap.decode_pocp_message(data) for data in got]
        self.assertEquals(
            decoded,
            [("drop", protocol_cap.encode_wire_id(
                        protocol_cap.NAMESPACE_RECEIVER, 42))])

    def test_dropping_all_imported_references(self):
        loop = EventLoop()
        sock1, sock2 = protocol_cap.socketpair()
        got = self._read_to_list(loop, sock1)
        imported_objects = protocol_cap.make_connection(loop, sock2, [], 1)
        del sock2
        del imported_objects
        loop.run_awhile()
        self._assert_connection_dropped(sock1)
        decoded = [protocol_cap.decode_pocp_message(data) for data in got]
        # The last "drop" message is unnecessary and we should not
        # receive it, because the connection will be dropped instead.
        self.assertEquals(decoded, [])

    def test_dropping_exported_references(self):
        deleted = []
        class Object(protocol_cap.PlashObject):
            def __init__(self, index):
                self._index = index
            def __del__(self):
                deleted.append(self._index)

        loop = EventLoop()
        sock1, sock2 = protocol_cap.socketpair()
        protocol_cap.make_connection(loop, sock1, [Object(index)
                                                   for index in range(100)])
        writer = protocol_cap.FDBufferedWriter(loop, sock2)
        # Should work for any sequence of indexes
        indexes = [5, 10, 56, 1, 0]
        for index in indexes:
            writer.write(protocol_simple.make_message(
                    protocol_cap.make_drop_message(
                        protocol_cap.encode_wire_id(
                            protocol_cap.NAMESPACE_RECEIVER, index))))
        loop.run_awhile()
        self.assertEquals(deleted, indexes)

    def test_dropping_all_exported_references(self):
        loop = EventLoop()
        sock1, sock2 = protocol_cap.socketpair()
        protocol_cap.make_connection(loop, sock1, [protocol_cap.PlashObject()
                                                   for index in range(100)])
        del sock1
        writer = protocol_cap.FDBufferedWriter(loop, sock2)
        for index in range(100):
            writer.write(protocol_simple.make_message(
                    protocol_cap.make_drop_message(
                        protocol_cap.encode_wire_id(
                            protocol_cap.NAMESPACE_RECEIVER, index))))
        loop.run_awhile()
        self._assert_connection_dropped(sock2)

    def test_creating_useless_connection(self):
        loop = EventLoop()
        sock1, sock2 = protocol_cap.socketpair()
        # This connection neither imports nor exports any references,
        # so it starts off moribund.
        protocol_cap.make_connection(loop, sock1, [])
        del sock1
        self._assert_connection_dropped(sock2)

    def test_receiving_and_invoking_single_use_reference(self):
        calls = []
        class Object(protocol_cap.PlashObject):
            def cap_invoke(self, args):
                calls.append(args)

        loop = EventLoop()
        sock1, sock2 = protocol_cap.socketpair()
        exported = Object()
        protocol_cap.make_connection(loop, sock1, [exported])
        del sock1
        received = self._read_to_list(loop, sock2)
        writer = protocol_cap.FDBufferedWriter(loop, sock2)
        writer.write(protocol_simple.make_message(
                protocol_cap.make_invoke_message(
                    protocol_cap.encode_wire_id(
                        protocol_cap.NAMESPACE_RECEIVER, 0),
                    [protocol_cap.encode_wire_id(
                            protocol_cap.NAMESPACE_SENDER_SINGLE_USE, 1234)],
                    "body data")))
        # Drop the exported object so that the imported single use
        # object is the only one left.
        writer.write(protocol_simple.make_message(
                protocol_cap.make_drop_message(
                    protocol_cap.encode_wire_id(
                        protocol_cap.NAMESPACE_RECEIVER, 0))))
        loop.run_awhile()
        self.assertEquals(len(calls), 1)
        data, caps, fds = calls[0]
        [received_obj] = caps
        assert isinstance(received_obj, protocol_cap.RemoteObject)
        self.assertEquals(received_obj._object_id, 1234)
        self.assertEquals(received_obj._single_use, True)
        assert received_obj._connection is not None
        received_obj.cap_invoke(("some return message", (), ()))
        assert received_obj._connection is None

        # This tests sending a message and immediately disconnecting.
        # The message should be queued, not dropped.  The message
        # should be sent before the connection is dropped.
        loop.run_awhile()
        decoded = [protocol_cap.decode_pocp_message(data) for data in received]
        self.assertEquals(
            decoded,
            [("invoke", protocol_cap.encode_wire_id(
                           protocol_cap.NAMESPACE_RECEIVER, 1234),
              [], "some return message")])
        self._assert_connection_dropped(sock2)

    # TODO: check receiving bogus IDs

    # TODO: check receiving bogus messages


class CapProtocolEndToEndTests(unittest.TestCase):

    # End-to-end test: makes no assumptions about the protocol's encoding.

    def test_sending_and_receiving(self):
        loop = EventLoop()
        sock1, sock2 = protocol_cap.socketpair()
        exported_objects = [CallLogger() for i in range(10)]
        protocol_cap.make_connection(loop, sock1, exported_objects)
        imported_objects = protocol_cap.make_connection(loop, sock2, [], 10)
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
        class Object(protocol_cap.LocalObject):
            def cap_call(self, args):
                calls.append(args)
                return ("result body", (), ())

        loop = EventLoop()
        loop.once = loop.once_safely # Should really apply this for all tests
        sock1, sock2 = protocol_cap.socketpair()
        protocol_cap.make_connection(loop, sock1, [Object()])
        [imported] = protocol_cap.make_connection(loop, sock2, [], 1)
        result = imported.cap_call(("args body", (), ()))


if __name__ == "__main__":
    unittest.main()
