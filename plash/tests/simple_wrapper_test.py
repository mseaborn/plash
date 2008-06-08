# Copyright (C) 2008 Mark Seaborn
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

import subprocess
import unittest


class SimpleWrapperTest(unittest.TestCase):

    def test_exit_code(self):
        rc = subprocess.call(["simple_wrapper.py", "/bin/sh", "-c", "exit 42"])
        self.assertEquals(rc, 42)

    def test_signal_exit_code(self):
        rc = subprocess.call(["simple_wrapper.py", "/bin/sh", "-c", "kill $$"])
        self.assertEquals(rc, 127)


if __name__ == "__main__":
    unittest.main()
