
import plash
import struct

def unpack2(fmt, string):
    size = struct.calcsize(fmt)
    rest = string[size:]
    return (struct.unpack(fmt, string[0:size]), rest)

def unpack_method(args):
    (data, caps, fds) = args
    ((method,), rest) = unpack2('4s', data)
    return (method, (rest, caps, fds))

def fsobj_type(self): return self.cap_call(('Otyp', (), ()))
def dir_list(self):
    fmt = "iii"
    size = struct.calcsize(fmt)
    (method, (data, _, _)) = unpack_method(self.cap_call(('Olst', (), ())))
    entries = []
    i = 4
    if method == 'Okay':
        while i < len(data):
            a = struct.unpack(fmt, data[i:i+size])
            i += size
            name = data[i : i + a[2]]
            i += a[2]
            entries.append({ 'inode': a[0], 'type': a[1], 'name': name })
        return entries
    else:
        raise 'Unrecognised response'

plash.Wrapper.fsobj_type = fsobj_type
plash.Wrapper.dir_list = dir_list
