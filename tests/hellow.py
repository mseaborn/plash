
import os

import plash.env
import plash.mainloop
import plash.namespace as ns
import plash.process


caller_root = plash.env.get_root_dir()
root_node = ns.make_node()
ns.resolve_populate(caller_root, root_node, "/bin")
ns.resolve_populate(caller_root, root_node, "/lib")
ns.resolve_populate(caller_root, root_node, "/usr")
ns.resolve_populate(caller_root, root_node, "/dev/null", flags=ns.FS_OBJECT_RW)
# This is for when running under tests:
if 'PLASH_LIBRARY_DIR' in os.environ:
    ns.resolve_populate(caller_root, root_node,
                        os.environ['PLASH_LIBRARY_DIR'],
                        flags=ns.FS_FOLLOW_SYMLINKS)
root = ns.dir_of_node(root_node)
fs_op = ns.make_fs_op(root)

p = plash.process.ProcessSpec()
p.env = os.environ.copy()
p.caps = { 'fs_op': fs_op,
           'conn_maker': ns.conn_maker }
p.setcmd('/bin/echo', 'Hello world!')
pid = p.spawn()
plash.mainloop.run_server()

(pid2, status) = os.waitpid(pid, 0)
assert pid == pid2
if os.WIFEXITED(status):
    print "exited with status:", os.WEXITSTATUS(status)
elif os.WIFSIGNALED(status):
    print "exited with signal:", os.WTERMSIG(status)
else:
    print "unknown exit status:", status
