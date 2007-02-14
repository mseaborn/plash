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
import struct
import string


class FormatStringError:
    pass

class UnpackError:
    pass

class UnmarshalError:
    pass


methods_by_name = {}
methods_by_code = {}

def method_id(name, code):
    x = { 'code': code, 'name': name }
    assert not (name in methods_by_name), name
    assert not (code in methods_by_code), code
    methods_by_name[name] = x
    methods_by_code[code] = x

import plash.methods


class Format_str:
    def __init__(self, code, fmt):
        self.code = code
        self.fmt = fmt
    def pack_a(self, *x): return format_pack(self.code, self.fmt, *x)
    def pack_r(self, x): return format_pack(self.code, self.fmt, *x)
    def unpack_a(self, args): return format_unpack(self.fmt, args)
    def unpack_r(self, args): return format_unpack(self.fmt, args)

class Format_str1:
    def __init__(self, code, fmt):
        assert len(fmt) == 1, fmt
        self.code = code
        self.fmt = fmt
    def pack_a(self, x): return format_pack(self.code, self.fmt, x)
    def pack_r(self, x): return format_pack(self.code, self.fmt, x)
    def unpack_a(self, args): return format_unpack(self.fmt, args)
    def unpack_r(self, args): return format_unpack(self.fmt, args)[0]

class Args_write:
    def __init__(self):
        self.data = []
        self.caps = []
        self.fds = []
    def put_data(self, x):
        self.data.append(x)
    def put_int(self, x):
        self.data.append(struct.pack('i', x))
    def put_strsize(self, x):
        self.put_int(len(x))
        self.data.append(x)
    def pack(self):
        return (string.join(self.data, ''),
                tuple(self.caps),
                tuple(self.fds))

class Args_read:
    def __init__(self, args):
        (self.data, self.caps, self.fds) = args
        self.data_pos = int_size
        self.caps_pos = 0
        self.fds_pos = 0
    def get_int(self):
        (v,) = struct.unpack('i', self.data[self.data_pos : self.data_pos + int_size])
        self.data_pos += int_size
        return v
    def get_data(self, len):
        v = self.data[self.data_pos : self.data_pos + len]
        self.data_pos += len
        return v
    def get_strsize(self):
        len = self.get_int()
        return self.get_data(len)
    def data_endp(self):
        return self.data_pos == len(self.data)
    def check_end(self):
        assert self.data_pos == len(self.data)
        assert self.caps_pos == len(self.caps)
        assert self.fds_pos == len(self.fds)

def pack_dirlist(s, entries):
    for entry in entries:
        s.put_int(entry['inode'])
        s.put_int(entry['type'])
        s.put_strsize(entry['name'])
def unpack_dirlist(s):
    entries = []
    while not s.data_endp():
        inode = s.get_int()
        type = s.get_int()
        name = s.get_strsize()
        entries.append({ 'inode': inode, 'type': type, 'name': name })
    return entries

# These methods are slightly different.  One includes the number of
# entries.  This should ideally be changed elsewhere.
class M_r_dir_list:
    def pack_r(self, entries):
        s = Args_write()
        s.put_data(methods_by_name['r_dir_list']['code'])
        s.put_int(len(entries))
        pack_dirlist(s, entries)
        return s.pack()
    def unpack_r(self, arg):
        s = Args_read(arg)
        # msg = s.get_int() # Ignored
        size = s.get_int()
        entries = unpack_dirlist(s)
        assert size == len(entries)
        s.check_end()
        return entries
    def pack_a(self, a): return self.pack_r(a)
    def unpack_a(self, a): return (self.unpack_r(a),)
class M_r_fsop_dirlist:
    def pack_r(self, entries):
        s = Args_write()
        s.put_int(methods_by_name['r_fsop_dirlist']['code'])
        pack_dirlist(s, entries)
        return s.pack()
    def unpack_r(self, arg):
        s = Args_read(arg)
        # msg = s.get_int() # Ignored
        # size = s.get_int()
        entries = unpack_dirlist(s)
        s.check_end()
        return entries
    def pack_a(self, a): return self.pack_r(a)
    def unpack_a(self, a): return (self.unpack_r(a),)

class M_r_stat:
    def __init__(self, code): self.code = code
    def pack_r(self, st):
        return format_pack(self.code,
                           'iiiiiiiiiiiii',
                           st['st_dev'],
                           st['st_ino'],
                           st['st_mode'],
                           st['st_nlink'],
                           st['st_uid'],
                           st['st_gid'],
                           st['st_rdev'],
                           st['st_size'],
                           st['st_blksize'],
                           st['st_blocks'],
                           st['st_atime'],
                           st['st_mtime'],
                           st['st_ctime'])
    def unpack_r(self, args):
        x = format_unpack('iiiiiiiiiiiii', args)
        return { 'st_dev': x[0],
                 'st_ino': x[1],
                 'st_mode': x[2],
                 'st_nlink': x[3],
                 'st_uid': x[4],
                 'st_gid': x[5],
                 'st_rdev': x[6],
                 'st_size': x[7],
                 'st_blksize': x[8],
                 'st_blocks': x[9],
                 'st_atime': x[10],
                 'st_mtime': x[11],
                 'st_ctime': x[12] }
    def pack_a(self, a): return self.pack_r(a)
    def unpack_a(self, a): return (self.unpack_r(a),)

class M_fsop_exec:
    def unpack_a(self, args):
        (filename, ref, args2) = format_unpack('si*', args)
        return (filename, tree_unpack(ref, args2))
    def unpack_r(self, a): return self.unpack_a(a)
class M_misc:
    def unpack_r(self, args):
        (ref, args2) = format_unpack('i*', args)
        return tree_unpack(ref, args2)
    def unpack_a(self, a): return (self.unpack_r(a),)


def add_format(name, format):
    assert not ('packer' in methods_by_name[name])
    if isinstance(format, str):
        if len(format) == 1:
            packer = Format_str1(methods_by_name[name]['code'], format)
        else:
            packer = Format_str(methods_by_name[name]['code'], format)
    else:
        packer = format
    methods_by_name[name]['packer'] = packer

add_format('fsop_copy', '')
#add_format('r_fsop_copy', 'c')
add_format('fsop_get_dir', 'S')
add_format('fsop_get_root_dir', '')
add_format('fsop_get_obj', 'S')
add_format('fsop_log', 'S')

# These correspond to Unix system calls:
add_format('fsop_open', 'iiS')
# r_fsop_open...
add_format('fsop_stat', 'iS')
add_format('r_fsop_stat', M_r_stat(methods_by_name['r_fsop_stat']['code']))
add_format('fsop_readlink', 'S')
add_format('r_fsop_readlink', 'S')
add_format('fsop_chdir', 'S')
add_format('fsop_fchdir', 'c')
add_format('fsop_dir_fstat', 'c')
#add_format('r_fsop_dir_fstat', 'iiiiiiiiiiiii')
add_format('fsop_getcwd', '')
add_format('r_fsop_getcwd', 'S')
add_format('fsop_dirlist', 'S')
add_format('r_fsop_dirlist', M_r_fsop_dirlist())
add_format('fsop_access', 'iS')
add_format('fsop_mkdir', 'iS')
add_format('fsop_chmod', 'iiS')
add_format('fsop_chown', 'iiiS')
add_format('fsop_utime', 'iiiiiS')
add_format('fsop_rename', 'sS')
add_format('fsop_link', 'sS')
add_format('fsop_symlink', 'sS')
add_format('fsop_unlink', 'S')
add_format('fsop_rmdir', 'S')
add_format('fsop_connect', 'fS')
add_format('fsop_bind', 'fS')
add_format('fsop_exec', M_fsop_exec())

# Common response messages
add_format('okay', '')
add_format('fail', 'i')
add_format('r_cap', 'c')

add_format('r_fsop_open', 'f')
add_format('r_fsop_open_dir', 'fc')

# Files, directories and symlinks
OBJT_FILE = 1
OBJT_DIR = 2
OBJT_SYMLINK = 3
add_format('fsobj_type', '')
add_format('r_fsobj_type', 'i')
add_format('fsobj_stat', '')
add_format('r_fsobj_stat', M_r_stat(methods_by_name['r_fsobj_stat']['code']))
add_format('fsobj_utimes', 'iiii')
add_format('fsobj_chmod', 'i')

# Files
add_format('file_open', 'i')
add_format('r_file_open', 'f')
add_format('file_socket_connect', 'f')

# Directories
add_format('dir_traverse', 'S')
add_format('r_dir_traverse', 'c')
add_format('dir_list', '')
add_format('r_dir_list', M_r_dir_list())
add_format('dir_create_file', 'iiS')
add_format('r_dir_create_file', 'f')
add_format('dir_mkdir', 'iS')
add_format('dir_symlink', 'sS')
add_format('dir_rename', 'scS')
add_format('dir_link', 'scS')
add_format('dir_unlink', 'S')
add_format('dir_rmdir', 'S')
add_format('dir_socket_bind', 'Sf')

# Symlinks
add_format('symlink_readlink', '')
add_format('r_symlink_readlink', 'S')

# Executable objects
add_format('eo_is_executable', '')
add_format('eo_exec', M_misc())
add_format('r_eo_exec', 'i')

add_format('make_conn', 'iC')
add_format('r_make_conn', 'fC')
add_format('make_conn2', 'fiC')
add_format('r_make_conn2', 'C')

add_format('powerbox_req_filename', M_misc())
add_format('powerbox_result_filename', 'S')



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
            assert isinstance(arg, int), arg
            data.append(struct.pack('i', arg))
        elif p == 's':
            assert isinstance(arg, str), arg
            data.append(struct.pack('i', len(arg)))
            data.append(arg)
        elif p == 'S':
            assert isinstance(arg, str), arg
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
        elif p == '*':
            (data2, caps2, fds2) = arg
            data.append(data2)
            caps.extend(caps2)
            fds.extend(fds2)
        else:
            raise FormatStringError("Unknown format string char: %s" % p)
    return (string.join(data, ''), tuple(caps), tuple(fds))

def get_message_code(args):
    return args[0][0:int_size]

def method_unpack(message):
    (data, caps, fds) = message
    return (data[0:int_size],
            (data[int_size:],
             caps,
             fds))

def format_unpack(pattern, msg):
    (data, caps, fds) = msg
    data_pos = int_size # Skip past message ID
    caps_pos = 0
    fds_pos = 0
    args = []

    for f in pattern:
        if f == 'i':
            (v,) = struct.unpack('i', data[data_pos:data_pos+int_size])
            data_pos += int_size
        elif f == 's':
            (length,) = struct.unpack('i', data[data_pos:data_pos+int_size])
            data_pos += int_size
            v = data[data_pos:data_pos+length]
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
        elif f == '*':
            v = (data[data_pos:], caps[caps_pos:], fds[fds_pos:])
            data_pos = len(data)
            caps_pos = len(caps)
            fds_pos = len(fds)
        else:
            raise FormatStringError("Unknown format string char: %s" % f)
        args.append(v)

    # Ensure that all the data has been matched
    assert data_pos == len(data)
    assert caps_pos == len(caps)
    assert fds_pos == len(fds)
    return tuple(args)


def get_int(data, i):
    (x,) = struct.unpack('i', data[i:i+int_size])
    return x
def tree_unpack(ref, args):
    (data, caps, fds) = args
    type = ref & 7
    addr = ref >> 3
    # int
    if type == 0:
        return get_int(data, addr)
    # string
    elif type == 1:
        size = get_int(data, addr)
        return data[addr+int_size:addr+int_size+size]
    # array
    elif type == 2:
        size = get_int(data, addr)
        return [tree_unpack(get_int(data, addr + int_size * (i+1)), args)
                for i in range(size)]
    # cap
    elif type == 3:
        return caps[addr]
    # fd
    elif type == 4:
        return fds[addr]
    else:
        raise UnpackError("Bad type code in reference: %i" % type)


def pack(name, *args):
    method = methods_by_name[name]
    return method['packer'].pack_a(*args)

def unpack(args):
    method_code = get_message_code(args)
    method = methods_by_code[method_code]
    return (method['name'], method['packer'].unpack_a(args))


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

# This is a base class for Python objects that implement cap_call()
# but not other methods.  The other methods are defined in the base
# class as marshallers which call cap_call().
class Pyobj_marshal(plash_core.Pyobj):
    cap_invoke = local_cap_invoke

# This is a base class for Python objects that implement specific
# call-return methods but not cap_call() or cap_invoke().
class Pyobj_demarshal(plash_core.Pyobj):
    methods = {}

    cap_invoke = local_cap_invoke

    def cap_call(self, args):
        "Unmarshals incoming calls to invocations of specific Python methods."
        method = get_message_code(args)
        if method in self.methods:
            return self.methods[method](self, args)
        elif method in methods_by_code:
            print "cap_call received message with no handler; name:", \
                  methods_by_code[method]['name']
            raise UnmarshalError("No match")
        else:
            # NB. The exception will not actually get reported.
            # Need to add a logging/warning mechanism.
            # Need to convert (selected?) exceptions to error return values.
            print "cap_call received message with no handler; code:", method
            raise UnmarshalError("No match")

def add_marshaller(name, f):
    setattr(plash_core.Wrapper, name, f)
    setattr(Pyobj_marshal, name, f)

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
            raise UnmarshalError("No match, got '%s' wanted '%s'" % (m, result))

    add_marshaller(name, outgoing)

    method_packer = methods_by_name[name]['packer']
    result_packer = methods_by_name[result]['packer']
    result_code = methods_by_name[result]['code']
    def incoming(self, args):
        arg_list = method_packer.unpack_a(args)
        r = getattr(self, name)(*arg_list)
        return result_packer.pack_r(r)

    Pyobj_demarshal.methods[methods_by_name[name]['code']] = incoming


#add_method('fsop_copy', 'r_fsop_copy')
#add_method('fsop_get_dir', ...)
add_method('fsop_get_root_dir', 'r_cap')
#add_method('fsop_get_obj', ...)
add_method('fsop_log', 'okay')

#add_method('fsop_open', ...)
add_method('fsop_stat', 'r_fsop_stat')
add_method('fsop_readlink', 'r_fsop_readlink')
add_method('fsop_chdir', 'okay')
add_method('fsop_fchdir', 'okay')
#add_method('fsop_dir_fstat', 'r_fsop_dir_fstat')
add_method('fsop_getcwd', 'r_fsop_getcwd')
add_method('fsop_dirlist', 'r_fsop_dirlist')
add_method('fsop_access', 'okay')
add_method('fsop_mkdir', 'okay')
add_method('fsop_chmod', 'okay')
add_method('fsop_chown', 'okay')
add_method('fsop_utime', 'okay')
add_method('fsop_rename', 'okay')
add_method('fsop_link', 'okay')
add_method('fsop_symlink', 'okay')
add_method('fsop_unlink', 'okay')
add_method('fsop_rmdir', 'okay')
add_method('fsop_connect', 'okay')
add_method('fsop_bind', 'okay')

add_method('fsobj_type', 'r_fsobj_type')
add_method('fsobj_stat', 'r_fsobj_stat')
add_method('fsobj_utimes', 'okay')
add_method('fsobj_chmod', 'okay')

add_method('dir_traverse', 'r_dir_traverse')
add_method('dir_list', 'r_dir_list')
add_method('dir_create_file', 'r_dir_create_file')
add_method('dir_mkdir', 'okay')
add_method('dir_symlink', 'okay')
add_method('dir_rename', 'okay')
add_method('dir_link', 'okay')
add_method('dir_unlink', 'okay')
add_method('dir_rmdir', 'okay')
add_method('dir_socket_bind', 'okay')

add_method('eo_is_executable', 'okay')
add_method('eo_exec', 'r_eo_exec')

add_method('symlink_readlink', 'r_symlink_readlink')

add_method('make_conn2', 'r_make_conn2')


# make_conn is only defined explicitly:
# 1. to provide an example
# 2. to allow for a default argument (though this might be removed)

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

add_marshaller('make_conn', make_conn)
