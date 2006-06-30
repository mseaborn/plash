
import os
import fcntl
import sys
import string
import plash
import plash_marshal
import plash_namespace as ns


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


# Attributes:
# env: dict listing environment variables
# cmd: pathname of command to execve()
# arg0
# argv: list of arguments to pass to execve() (not including usual 0th arg)
# fds: dict mapping FD numbers to FDs
# caps: dict mapping cap names (e.g. "fs_op") to objects
# conn_maker: object to use for creating new connection
class Process_spec:
    def __init__(self):
        self.conn_maker = ns.conn_maker

    def setcmd(self, cmd, *args):
        self.cmd = cmd
        self.arg0 = cmd
        self.args = list(args)
    
    def plash_setup(self):
        cap_names = self.caps.keys()
        fd = self.conn_maker.make_conn([self.caps[x] for x in cap_names])
        self.env['PLASH_CAPS'] = string.join(cap_names, ':')
        self.env['PLASH_COMM_FD'] = str(fd.fileno())
        fcntl.fcntl(fd.fileno(), fcntl.F_SETFD, 0) # Unset FD_CLOEXEC flag

        orig_cmd = self.cmd
        self.cmd = "/usr/lib/plash/run-as-anonymous"
        self.args = ["-s", "LD_LIBRARY_PATH=/usr/lib/plash/lib",
                     "/special/ld-linux.so.2",
                     orig_cmd] + self.args

        # This is a hack to ensure that the FD doesn't get GC'd and closed
        self.fd = fd

    def plash_setup2(self):
        cap_names = self.caps.keys()
        fd = self.conn_maker.make_conn([self.caps[x] for x in cap_names])
        self.env['PLASH_CAPS'] = string.join(cap_names, ':')
        self.env['PLASH_COMM_FD'] = str(fd.fileno())
        fcntl.fcntl(fd.fileno(), fcntl.F_SETFD, 0) # Unset FD_CLOEXEC flag

        orig_cmd = self.cmd
        self.cmd = "/usr/bin/strace"
        self.args = ["/home/mrs/projects/plash/shobj/ld.so",
                     orig_cmd] + self.args

        # This is a hack to ensure that the FD doesn't get GC'd and closed
        self.fd = fd

    def spawn(self):
        pid = os.fork()
        if pid == 0:
            os.execve(self.cmd, [self.arg0] + self.args, self.env)
            sys.exit(1)
        else:
            os.close(self.fd.fileno()) # Hack; to be removed
            return pid


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
