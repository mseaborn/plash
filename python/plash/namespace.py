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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
# USA.

import plash_core
import plash.marshal
import plash.marshal as m


class ReturnUnmarshalError(Exception):
    pass


def call(obj, result, method, *args):
    (m, r) = plash.marshal.unpack(
        obj.cap_call(
        plash.marshal.pack(method, *args)))
    if m == result:
        if len(r) == 1:
            return r[0]
        else:
            return r
    else:
        raise ReturnUnmarshalError("Failed to unmarshal: %r, %r" % (m, r))


m.add_format('resolve_dir', 'cdiS')
m.add_format('resolve_obj', 'cdiiS')

m.add_format('fs_make_node', '')
m.add_format('fs_attach_at_path', 'cSc')
m.add_format('fs_resolve_populate', 'ccdiS')
m.add_format('fs_dir_of_node', 'c')
m.add_format('fs_print_tree', 'c')
m.add_format('make_fs_op', 'c')
m.add_format('make_fs_op_with_log', 'cd')
m.add_format('make_union_dir', 'cc')
m.add_format('make_cow_dir', 'cc')
m.add_format('make_read_only_proxy', 'c')
m.add_format('dirstack_get_path', 'c')
m.add_format('r_dirstack_get_path', 'S')
m.add_format('make_log_from_fd', 'f')


conn_maker = plash_core.make_conn_maker()


def resolve_dir(root, pathname, cwd=None, symlink_limit=100):
    """Resolve pathname relative to root directory and optional cwd.
    Follows symlinks.  Returns a dirstack object.  Fails if the
    pathname does not resolve to a directory."""
    return call(plash_core.resolve_dir, 'r_cap',
                'resolve_dir', root, cwd, symlink_limit, pathname)

def resolve_obj(root, pathname, cwd=None, symlink_limit=100, nofollow=False):
    """Resolve pathname relative to root directory and optional cwd.
    Follows symlinks."""
    if nofollow:
        nofollow = 1
    else:
        nofollow = 0
    return call(plash_core.resolve_obj, 'r_cap',
                'resolve_obj', root, cwd, symlink_limit, nofollow, pathname)


def make_node():
    "Create a namespace node with no objects attached to it."
    return call(plash_core.fs_make_node, 'r_cap', 'fs_make_node')

def attach_at_path(root_node, pathname, obj):
    "Attach obj to the namespace given by root_node at the given pathname."
    return call(plash_core.fs_attach_at_path, 'okay',
                'fs_attach_at_path', root_node, pathname, obj)

def dir_of_node(node):
    "Create a directory object for accessing the given namespace."
    return call(plash_core.fs_dir_of_node, 'r_cap',
                'fs_dir_of_node', node)


class Namespace(object):

    def __init__(self):
        self._root_node = make_node()

    def get_root_dir(self):
        return dir_of_node(self._root_node)

    def attach_at_path(self, pathname, obj):
        attach_at_path(self._root_node, pathname, obj)

    def resolve_populate(self, root_dir, pathname, cwd=None, flags=0):
        resolve_populate(root_dir, self._root_node, pathname, cwd, flags)


FS_READ_ONLY        = 0x0
FS_SLOT_RWC         = 0x1
FS_OBJECT_RW        = 0x2
FS_FOLLOW_SYMLINKS  = 0x4

def resolve_populate(root_dir, root_node, pathname, cwd=None, flags=0):
    return call(plash_core.fs_resolve_populate, 'okay',
                'fs_resolve_populate', root_dir, root_node, cwd,
                flags, pathname)

def make_union_dir(obj1, obj2):
    return call(plash_core.make_union_dir, 'r_cap',
                'make_union_dir', obj1, obj2)

def make_cow_dir(obj1, obj2):
    return call(plash_core.make_cow_dir, 'r_cap',
                'make_cow_dir', obj1, obj2)

def make_read_only_proxy(obj):
    return call(plash_core.make_read_only_proxy, 'r_cap',
                'make_read_only_proxy', obj)

def make_log_from_fd(fd):
    return call(plash_core.make_log_from_fd, 'r_cap',
                'make_log_from_fd', fd)

def dirstack_get_path(obj):
    return call(plash_core.dirstack_get_path, 'r_dirstack_get_path',
                'dirstack_get_path', obj)

def make_fs_op(root_dir, log=None):
    return call(plash_core.make_fs_op, 'r_cap',
                'make_fs_op_with_log', root_dir, log)
