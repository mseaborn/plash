
import os
import fcntl
import sys
import string
import plash
import plash_marshal
import plash_namespace as ns
from plash_process import Process_spec


# Logging proxy object
class logger(plash.Pyobj):
    def __init__(self, x1): self.x = x1
    def cap_call(self, args):
        try:
            print "call", plash_marshal.unpack(args)
        except Exception, e:
            print args, "exception:", e
        r = self.x.cap_call(args)
        try:
            print "->", plash_marshal.unpack(r)
        except Exception, e:
            print r
        return r


class Test_dir(plash.Pyobj):
    def __init__(self, x): self.x = x
    def fsobj_type(self): return 2
    def dir_traverse(self, leaf):
        print "accessed:", leaf
        return self.x

root = plash.initial_dir("/")
#root = logger(root)

test_obj = Test_dir(root)


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

p = Process_spec()
p.env = os.environ.copy()
p.caps = { 'fs_op': fs_op }
#p.setcmd('/bin/echo', 'Hello world!')
#p.setcmd('/bin/ls', '-l', '/')
p.setcmd('/bin/sh', '-c', 'echo /test/foo/*')
p.plash_setup()
pid = p.spawn()
print "entering server"
plash.run_server()
print "server done"
