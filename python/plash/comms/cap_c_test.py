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

import plash_core
import plash.comms.cap as cap
import plash.comms.cap_test as cap_test
import plash.comms.event_loop
import plash.comms.stream
import plash.filedesc
import testrunner


class CapProtocolCTests(cap_test.CapProtocolTestsMixin, testrunner.TestCase):

    # TODO: remove the need for this, and have wrapped C objects
    # inherit from a common base class
    base_class = (cap.PlashObjectBase, plash_core.Wrapper)

    def tearDown(self):
        super(CapProtocolCTests, self).tearDown()
        plash_core.cap_close_all_connections()
        self.assertFalse(plash_core.cap_server_exporting())

    def make_event_loop(self):
        loop = plash.comms.event_loop.GlibEventLoop()
        self.on_teardown(loop.destroy)
        return loop

    def socketpair(self):
        return plash.comms.stream.socketpair()

    def make_connection(self, loop, sock_fd, export_caps, import_count=0):
        sock_fd = plash.filedesc.dup_and_wrap_fd(sock_fd.fileno())
        return plash_core.cap_make_connection.make_conn2(sock_fd, import_count,
                                                         export_caps)

    def test_sending_references_preserves_eq(self):
        # C implementation does not yet have this property.
        pass

    def test_receiving_references_preserves_eq(self):
        # C implementation does not yet have this property.
        pass

    # These methods test based on looking at the implementation's
    # internal state, which we can't do with the C implementation.
    # Could improve these tests to work behaviourally instead.

    def check_exports(self, imported, exports):
        pass

    def check_remote_object(self, obj, object_id, single_use):
        pass

    def check_not_connected(self, obj):
        pass

    def check_object_used_up(self, obj, used_up):
        pass
