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

    def test(self):
        loop = EventLoop()
        pipe_read, pipe_write = protocol_cap.make_pipe()
        writer = protocol_cap.FDBufferedWriter(loop, pipe_write)
        self.assertTrue(not loop.is_listening())
        writer.write("hello")
        self.assertTrue(loop.is_listening())
        loop.once_safely()
        # Should have written all of buffer to the pipe now
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

    # TODO: check receiving bogus IDs

    # TODO: check receiving bogus messages

    # TODO: check dropping references


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
