
import os
import sys
import plash_core
import plash.env
import plash.namespace as ns
from plash.process import Process_spec


args = sys.argv[1:]
if len(args) < 3:
    print "Usage: %s <write-layer-dir> <read-layer-dir> <prog> <args...>" % sys.argv[0]
    sys.exit(1)

caller_root = plash.env.get_root_dir()
caller_cwd_path = os.getcwd()
caller_cwd = ns.resolve_dir(caller_root, caller_cwd_path)


write_layer = ns.resolve_obj(caller_root, args[0], cwd=caller_cwd)
read_layer1 = ns.resolve_obj(caller_root, args[1], cwd=caller_cwd)
read_layer2 = caller_root

root_node = ns.make_node()

ns.attach_at_path(root_node, "/",
                  ns.make_cow_dir(write_layer,
                                  ns.make_union_dir(read_layer1,
                                                    read_layer2)))
ns.attach_at_path(root_node, "/tmp",
                  ns.resolve_obj(caller_root, "/tmp"))
ns.attach_at_path(root_node, "/dev",
                  ns.resolve_obj(caller_root, "/dev"))

fs_op = plash.make_fs_op(ns.dir_of_node(root_node))
fs_op.fsop_chdir(caller_cwd_path)
p = Process_spec()
p.setcmd(*args[2:])
p.env = os.environ.copy()
p.env['PLASH_FAKE_UID'] = str(os.getuid())
p.env['PLASH_FAKE_GID'] = str(os.getgid())
p.env['PLASH_FAKE_EUID'] = str(os.getuid())
p.env['PLASH_FAKE_EGID'] = str(os.getgid())
p.caps = { 'fs_op': fs_op,
           'conn_maker': ns.conn_maker }
pid = p.spawn()
plash_core.run_server()

# Wait for the subprocess to exit and check the exit code.
(pid2, status) = os.waitpid(pid, 0)
assert pid == pid2
if os.WIFEXITED(status):
    print "exited with status:", os.WEXITSTATUS(status)
elif os.WIFSIGNALED(status):
    print "exited with signal:", os.WTERMSIG(status)
else:
    print "unknown exit status:", status
