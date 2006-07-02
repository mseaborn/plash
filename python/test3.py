
import os
import fcntl
import sys
import string
import traceback
import plash
import plash_marshal
import plash_marshal as m
import plash_namespace as ns
from plash_process import Process_spec


# Logging proxy object
class logger(plash_marshal.Pyobj_marshal):
    def __init__(self, x1): self.x = x1
    def cap_call(self, args):
        try:
            print "call", plash_marshal.unpack(args)
        except Exception, e:
            print args, "exception:", e
        try:
            r = self.x.cap_call(args)
        except:
            print "Exception in cap_call:",
            traceback.print_exc()
            return
        try:
            print "->", plash_marshal.unpack(r)
        except Exception, e:
            print r
        return r

indent = ['>']

class logger_indent(plash_marshal.Pyobj_marshal):
    def __init__(self, x1): self.x = x1
    def cap_call(self, args):
        try:
            print indent[0], "call", plash_marshal.unpack(args)
        except Exception, e:
            print indent[0], args, "exception:", e
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
        try:
            print indent[0], "->", plash_marshal.unpack(r)
        except Exception, e:
            print indent[0], r
        return r

logger = logger_indent


class Test_dir(plash_marshal.Pyobj_demarshal):
    def __init__(self, x): self.x = x
    def fsobj_type(self): return m.OBJT_DIR
    def dir_traverse(self, leaf):
        print "accessed:", leaf
        return self.x

root = plash.initial_dir("/")
#root = logger(root)

class Fab_dir(plash_marshal.Pyobj_demarshal):
    def __init__(self, dict): self.dict = dict
    def fsobj_type(self): return m.OBJT_DIR
    def fsobj_stat(self): return root.fsobj_stat() # FIXME
    def dir_traverse(self, leaf): return self.dict[leaf]
    def dir_list(self):
        return [{ 'inode': 0, 'type': 0, 'name': name }
                for name in self.dict.keys()]

test_obj = logger(Fab_dir({ "foo": root,
                            "bar": root }))


ns.resolve_dir(root, "/bin")
try:
    ns.resolve_dir(root, "/foo")
except:
    print "Can't resolve /foo"

root_node = ns.make_node()
#ns.attach_at_path(root_node, "/", root)
ns.resolve_populate(root, root_node, "/bin")
ns.resolve_populate(root, root_node, "/lib")
ns.resolve_populate(root, root_node, "/usr")
ns.attach_at_path(root_node, "/test", test_obj)
root2 = ns.dir_of_node(root_node)
print "dir type:", root.fsobj_type()
print "traverse:", root.dir_traverse("lib")

assert root.fsobj_type() == 2
assert isinstance(root.dir_traverse("lib"), plash.Wrapper)
#print "traverse2:", root.dir_traverse("doesnt-exist")

# root2 = logger(root2)
fs_op = plash.make_fs_op(root2)


fs_op = logger(fs_op)
#print fs_op.fsop_chdir('/lib')
#print fs_op.fsop_getcwd()

p = Process_spec()
p.env = os.environ.copy()
p.caps = { 'fs_op': fs_op }
#p.setcmd('/bin/echo', 'Hello world!')
#p.setcmd('/bin/ls', '-l', '/')
#p.setcmd('/bin/sh', '-c', 'echo /test/foo/*')
p.setcmd('/bin/ls', '-li', '/test')
p.plash_setup()
pid = p.spawn()
print "entering server"
plash.run_server()
print "server done"
