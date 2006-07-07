# Copyright (C) 2006 Mark Seaborn
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.

import plash
import plash_marshal
import plash_marshal as m


def call(obj, result, method, *args):
    (m, r) = plash_marshal.unpack(
        obj.cap_call(
        plash_marshal.pack(method, *args)))
    if m == result:
        if len(r) == 1:
            return r[0]
        else:
            return r
    else:
        # FIXME
        fail()


m.add_format('resolve_dir', 'cdiS')

m.add_format('fs_make_node', '')
m.add_format('fs_attach_at_path', 'cSc')
m.add_format('fs_resolve_populate', 'ccdiS')
m.add_format('fs_dir_of_node', 'c')
m.add_format('fs_print_tree', 'c')


conn_maker = plash.make_conn_maker()


def resolve_dir(root, pathname, cwd=None, symlink_limit=100):
    return call(plash.resolve_dir, 'r_cap',
                'resolve_dir', root, cwd, symlink_limit, pathname)


def make_node():
    "Create a namespace node with no objects attached to it."
    return call(plash.fs_make_node, 'r_cap', 'fs_make_node')

def attach_at_path(root_node, pathname, obj):
    "Attach obj to the namespace given by root_node at the given pathname."
    return call(plash.fs_attach_at_path, 'okay',
                'fs_attach_at_path', root_node, pathname, obj)

def dir_of_node(node):
    "Create a directory object for accessing the given namespace."
    return call(plash.fs_dir_of_node, 'r_cap',
                'fs_dir_of_node', node)

FS_READ_ONLY        = 0x0
FS_SLOW_RWC         = 0x1
FS_OBJECT_RW        = 0x2
FS_FOLLOW_SYMLINKS  = 0x4

def resolve_populate(root_dir, root_node, pathname, cwd=None, flags=0):
    return call(plash.fs_resolve_populate, 'okay',
                'fs_resolve_populate', root_dir, root_node, cwd,
                flags, pathname)
