# Copyright (C) 2006, 2007 Mark Seaborn
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

import unittest
import plash.pola_run_args as pola_run_args


class TestFlags(unittest.TestCase):

    def test(self):
        self.assertEqual(pola_run_args.split_flags(""),
                         [])
        self.assertEqual(pola_run_args.split_flags("al,objrw,foo"),
                         ["a", "l", "objrw", "foo"])


if __name__ == '__main__':
    unittest.main()
