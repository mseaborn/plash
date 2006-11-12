
import os
import fcntl
import stat
import sys
import string
import traceback
import plash_core
import plash_marshal
import plash_marshal as m
import plash_namespace as ns
import plash_process
from plash_process import Process_spec
from plash_logger import logger


class Test_dir(plash_marshal.Pyobj_demarshal):
    def __init__(self, x): self.x = x
    def fsobj_type(self): return m.OBJT_DIR
    def dir_traverse(self, leaf):
        print "accessed:", leaf
        return self.x

root = plash_core.initial_dir("/")
#root = logger(root)

next_inode = [1]

class Exec_obj(plash_marshal.Pyobj_demarshal):
    def __init__(self):
        self.inode = next_inode[0]
        next_inode[0] += 1
    def fsobj_type(self): return m.OBJT_FILE
    def fsobj_stat(self):
        return { 'st_dev': 1, # Could pick a different device number
                 'st_ino': self.inode,
                 'st_mode': stat.S_IFREG | 0777,
                 'st_nlink': 0,
                 'st_uid': 0,
                 'st_gid': 0,
                 'st_rdev': 0,
                 'st_size': 0,
                 'st_blksize': 1024,
                 'st_blocks': 0,
                 'st_atime': 0,
                 'st_mtime': 0,
                 'st_ctime': 0 }
    def eo_is_executable(self): return ()
    def eo_exec(self, args):
        args = dict(args)
        fds = dict(args['Fds.'])
        stdout = fds[1]
        os.write(stdout.fileno(), "Hello world!\n")
        os.write(stdout.fileno(), "This is an executable object.\n")
        return 0

class Fab_dir(plash_marshal.Pyobj_demarshal):
    def __init__(self, dict):
        self.dict = dict
        self.inode = next_inode[0]
        next_inode[0] += 1
    def fsobj_type(self): return m.OBJT_DIR
    def fsobj_stat(self):
        return { 'st_dev': 1, # Could pick a different device number
                 'st_ino': self.inode,
                 'st_mode': stat.S_IFDIR | 0777,
                 'st_nlink': 0,
                 'st_uid': 0,
                 'st_gid': 0,
                 'st_rdev': 0,
                 'st_size': 0,
                 'st_blksize': 1024,
                 'st_blocks': 0,
                 'st_atime': 0,
                 'st_mtime': 0,
                 'st_ctime': 0 }
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
ns.resolve_populate(root, root_node, "/dev/null", flags=ns.FS_OBJECT_RW)
ns.attach_at_path(root_node, "/test", test_obj)
ns.attach_at_path(root_node, "/mybin/exec-obj", logger(Exec_obj()))
root2 = ns.dir_of_node(root_node)
print "dir type:", root.fsobj_type()
print "traverse:", root.dir_traverse("lib")

assert root.fsobj_type() == 2
assert isinstance(root.dir_traverse("lib"), plash_core.Wrapper)
#print "traverse2:", root.dir_traverse("doesnt-exist")

# root2 = logger(root2)
fs_op = plash_core.make_fs_op(root2)


fs_op = logger(fs_op)
#print fs_op.fsop_chdir('/lib')
#print fs_op.fsop_getcwd()

p = Process_spec()
p.env = os.environ.copy()
p.caps = { 'fs_op': fs_op,
           'conn_maker': ns.conn_maker }
#p.setcmd('/bin/echo', 'Hello world!')
#p.setcmd('/bin/ls', '-l', '/')
#p.setcmd('/bin/sh', '-c', 'echo /test/foo/*')
#p.setcmd('/bin/ls', '-li', '/test')
#p.setcmd('/bin/sh', '-c', '/bin/echo Hello!')
#p.setcmd('/bin/echo', 'Hello!')
#p.setcmd('/usr/bin/strace', '/bin/echo', 'Hello world!')
#p.setcmd('/usr/bin/perl', '-e', 'print "Hello, world!\\n"')
#p.setcmd('/bin/sh', '-c', "/mybin/exec-obj arg1 arg2; echo bar")
p.setcmd('/bin/sh', '-c', """/mybin/exec-obj arg1 arg2 | perl -ne 'print "Your message: $_"' """)
pid = p.spawn()
print "entering server"
plash_core.run_server()
print "server done"

(pid2, status) = os.waitpid(pid, 0)
assert pid == pid2
if os.WIFEXITED(status):
    print "exited with status:", os.WEXITSTATUS(status)
elif os.WIFSIGNALED(status):
    print "exited with signal:", os.WTERMSIG(status)
else:
    print "unknown exit status:", status
