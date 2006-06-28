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
import struct
import string


methods_by_name = {}
methods_by_code = {}

def method_id(name, code):
    x = { 'code': code, 'name': name }
    assert not (name in methods_by_name)
    assert not (code in methods_by_code)
    methods_by_name[name] = x
    methods_by_code[code] = x

import methods


def add_format(name, format):
    assert not ('format' in methods_by_name[name])
    methods_by_name[name]['format'] = format

add_format('fsop_open', 'iiS')
add_format('fsop_stat', 'iS')
add_format('fsop_readlink', '')
add_format('fsop_chdir', 'S')
add_format('fsop_fchdir', 'c')
add_format('fsop_dir_fstat', 'c')
add_format('fsop_getcwd', '')
add_format('fsop_dirlist', 'S')
add_format('fsop_access', 'iS')
add_format('fsop_mkdir', 'iS')
add_format('fsop_chmod', 'iS')
add_format('fsop_utime', 'iiiiiS')
add_format('fsop_rename', 'sS')
add_format('fsop_link', 'sS')
add_format('fsop_symlink', 'sS')
add_format('fsop_unlink', 'S')
add_format('fsop_rmdir', 'S')
add_format('fsop_connect', 'fS')
add_format('fsop_bind', 'fS')
# 'Exec'

add_format('okay', '')
add_format('fail', 'i')

add_format('r_fsop_open', 'f')
add_format('r_fsop_open_dir', 'fc')
add_format('r_fsop_stat', 'iiiiiiiiiiiii')
#add_format('r_fsop_readlink', 'S')
add_format('r_fsop_getcwd', 'S')

add_format('fsobj_type', '')
add_format('r_fsobj_type', 'i')

add_format('dir_traverse', 'S')
add_format('r_dir_traverse', 'c')
# dir_list
add_format('dir_create_file', 'iiS')
add_format('r_dir_create_file', 'f')
add_format('dir_mkdir', 'iS')
add_format('dir_symlink', 'sS')
add_format('dir_rename', 'scS')
add_format('dir_link', 'scS')
add_format('dir_unlink', 'S')
add_format('dir_rmdir', 'S')
add_format('dir_socket_bind', 'Sf')

add_format('symlink_readlink', '')
add_format('r_symlink_readlink', 'S')

add_format('make_conn', 'iC')
add_format('r_make_conn', 'fC')


int_size = struct.calcsize('i')

def format_pack(method, pattern, *args):
    data = [method]
    caps = []
    fds = []
    assert len(pattern) == len(args)
    
    for i in range(len(pattern)):
        p = pattern[i]
        arg = args[i]
        if p == 'i':
            data.append(struct.pack('i', arg))
        elif p == 's':
            data.append(struct.pack('i', len(s)))
            data.append(arg)
        elif p == 'S':
            data.append(arg)
        elif p == 'c':
            caps.append(arg)
        elif p == 'C':
            caps.extend(arg)
        elif p == 'd':
            if arg == None:
                data.append(struct.pack('i', 0))
            else:
                data.append(struct.pack('i', 1))
                caps.append(arg)
        elif p == 'f':
            fds.append(arg)
        elif p == 'F':
            fds.extend(arg)
        else: raise "Unknown format string char"
    return (string.join(data, ''), tuple(caps), tuple(fds))

def method_unpack(message):
    (data, caps, fds) = message
    return (data[0:int_size],
            (data[int_size:],
             caps,
             fds))

def format_unpack(pattern, msg):
    (data, caps, fds) = msg
    data_pos = 0
    caps_pos = 0
    fds_pos = 0
    args = []

    for f in pattern:
        if f == 'i':
            (v,) = struct.unpack('i', data[data_pos:data_pos+int_size])
            data_pos += int_size
        elif f == 's':
            length = struct.unpack('i', data[data_pos:data_pos+int_size])
            data_pos += int_size
            v = data[data_pos:data_pos+len]
            data_pos += length
        elif f == 'S':
            v = data[data_pos:]
            data_pos = len(data)
        elif f == 'c':
            v = caps[caps_pos]
            caps_pos += 1
        elif f == 'C':
            v = caps[caps_pos:]
            caps_pos = len(caps)
        elif f == 'd':
            (present,) = struct.unpack('i', data[data_pos:data_pos+int_size])
            data_pos += int_size
            if present == 0:
                v = None
            else:
                v = caps[caps_pos]
                caps_pos += 1
        elif f == 'f':
            v = fds[fds_pos]
            fds_pos += 1
        elif f == 'F':
            v = fds[fds_pos:]
            fds_pos = len(fds)
        else: raise "Unknown format string char"
        args.append(v)

    # Ensure that all the data has been matched
    assert data_pos == len(data)
    assert caps_pos == len(caps)
    assert fds_pos == len(fds)
    return tuple(args)


def pack(name, *args):
    method = methods_by_name[name]
    return format_pack(method['code'], method['format'], *args)

def unpack(args):
    (method_code, rest) = method_unpack(args)
    method = methods_by_code[method_code]
    return (method['name'], format_unpack(method['format'], rest))


# See cap_call below
plash.Pyobj.methods = {}

def add_method(name, result):
    assert name in methods_by_name
    assert result in methods_by_name
    
    def outgoing(self, *args):
        (m, r) = unpack(self.cap_call(pack(name, *args)))
        if m == result:
            if len(r) == 1:
                return r[0]
            else:
                return r
        else:
            raise "No match"

    setattr(plash.Wrapper, name, outgoing)

    method_format = methods_by_name[name]['format']
    result_format = methods_by_name[result]['format']
    result_code = methods_by_name[result]['code']
    def incoming(self, args):
        arg_list = format_unpack(method_format, args)
        print "incoming:"
        r = getattr(self, name)(*arg_list)
        if len(result_format) == 1:
            return format_pack(result_code, result_format, r)
        else:
            return format_pack(result_code, result_format, *r)

    plash.Pyobj.methods[methods_by_name[name]['code']] = incoming

add_method('fsobj_type', 'r_fsobj_type')

add_method('dir_traverse', 'r_dir_traverse')
# dir_list
add_method('dir_create_file', 'r_dir_create_file')
add_method('dir_mkdir', 'okay')
add_method('dir_symlink', 'okay')
add_method('dir_rename', 'okay')
add_method('dir_link', 'okay')
add_method('dir_unlink', 'okay')
add_method('dir_rmdir', 'okay')
add_method('dir_socket_bind', 'okay')


def make_conn(self, caps, imports=None):
    if imports == None:
        (m, r) = unpack(self.cap_call(pack('make_conn', 0, caps)))
        if m == 'r_make_conn':
            (fd, caps2) = r
            assert len(caps2) == 0
            return fd
    else:
        (m, r) = unpack(self.cap_call(pack('make_conn', imports, caps)))
        if m == 'r_make_conn':
            return r
    fail()

plash.Wrapper.make_conn = make_conn


def local_cap_invoke(self, args):
    "Converts incoming invocations to calls to cap_call."
    (method, args2) = method_unpack(args)
    if method == 'Call':
        # Invoke the method, and call the return continuation with the result
        (data, caps, fds) = args2
        caps[0].cap_invoke(self.cap_call((data, caps[1:], fds)))
    else:
        # There is nothing we can do, besides warning
        pass

def local_cap_call(self, args):
    "Unmarshals incoming calls to invocations of specific Python methods."
    (method, args2) = method_unpack(args)
    if method in self.methods:
        return self.methods[method](self, args2)
    else:
        print "cap_call unknown:", method
        raise "No match"

plash.Pyobj.cap_invoke = local_cap_invoke
plash.Pyobj.cap_call = local_cap_call
