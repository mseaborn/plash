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

import cStringIO
import StringIO

import testrunner


class ExampleTest(testrunner.TestCase):

    def test_assertequals(self):
        self.assertEquals(1 + 1, 2)

    def test_asserttrue(self):
        self.assertTrue("f" in "foo")


class ExampleCombinationTest(testrunner.CombinationTestCase):

    def setup_stringio(self):
        self.cls = StringIO.StringIO

    def setup_cstringio(self):
        self.cls = cStringIO.StringIO

    def test_writing(self):
        buf = self.cls()
        buf.write("hello")
        buf.write(" world")
        self.assertEquals(buf.getvalue(), "hello world")

    def test_reading(self):
        stream = self.cls("hello world")
        self.assertEquals(stream.read(5), "hello")
        self.assertEquals(stream.read(), " world")
