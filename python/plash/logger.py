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

import plash.marshal
import traceback

indent = ['>']

# Logging proxy object
class logger(plash.marshal.Pyobj_marshal):
    def __init__(self, x1): self.x = x1
    def cap_call(self, args):
        print indent[0], "call",
        try:
            print plash.marshal.unpack(args)
        except:
            print args, "exception:",
            traceback.print_exc()
        try:
            i = indent[0]
            indent[0] = i + '  '
            try:
                r = self.x.cap_call(args)
            finally:
                indent[0] = i
        except:
            print indent[0], "Exception in cap_call:",
            traceback.print_exc()
            return
        print indent[0], "->",
        try:
            print plash.marshal.unpack(r)
        except:
            print r
        return r
