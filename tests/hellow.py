
import os

import plash.env
import plash.mainloop
import plash.namespace as ns
import plash.pola_run_args
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

setup = plash.pola_run_args.ProcessSetup(p)
forwarders = setup.grant_proxy_terminal_access()

pid = p.spawn()
plash.pola_run_args.flush_and_return_status_on_child_exit(pid, [])
plash.mainloop.run_server()
