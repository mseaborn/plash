
# This script is equivalent to:
# pola-run \
#   -f /bin \
#   -f /lib \
#   -f /usr \
#   -f,objrw /dev/null \
#   --prog /bin/echo -a 'Hello world!'

import os
import plash_core
import plash_env
import plash_namespace as ns
from plash_process import Process_spec


# Create a file namespace for the subprocess:
# First, get the root directory of our own file namespace.
caller_root = plash_env.get_root_dir()

# Create an empty namespace.
root_node = ns.make_node()

# Populate the namespace by binding selected directories taken
# from our root directory.
# Bind read-only versions of the directories "/bin", "/lib" and
# "/usr"; this is important if the caller has write access to these,
# such as when the caller is root.
ns.attach_at_path(root_node, "/bin",
                  ns.make_read_only_proxy(ns.resolve_obj(caller_root, "/bin")))
ns.attach_at_path(root_node, "/lib",
                  ns.make_read_only_proxy(ns.resolve_obj(caller_root, "/lib")))
ns.attach_at_path(root_node, "/usr",
                  ns.make_read_only_proxy(ns.resolve_obj(caller_root, "/usr")))
ns.attach_at_path(root_node, "/dev/null",
                  ns.resolve_obj(caller_root, "/dev/null"))

# Get a directory object for this namespace.
root = ns.dir_of_node(root_node)
# Create an fs_op object for that root directory.
# The fs_op object provides POSIX pathname-based calls,
# and keeps track of the current working directory.
fs_op = plash_core.make_fs_op(root)


# Create a template for spawning a limited-authority subprocess.
p = Process_spec()

# The subprocess is given the same environment variables as this process.
# Alternatively, we could leave this empty.
# Some extra variables will be filled out for us:
# PLASH_COMM_FD, PLASH_CAPS, and LD_LIBRARY_PATH.
p.env = os.environ.copy()

# Specify the capabilities to be passed to the new process.
# These objects are exported to the subprocess via a connection
# that is set up for us.
# fs_op provides the file namespace.
# conn_maker allows the subprocess to create new object protocol
# connections, so that it does not have to forward requests itself
# when it fork()s a subprocess.
p.caps = { 'fs_op': fs_op,
           'conn_maker': ns.conn_maker }

# Set the pathname of the command and its arguments.
p.setcmd('/bin/echo', 'Hello world!')

# Spawn the subprocess.  This does fork() and execve().
# It sets up the file descriptor for the connection that the
# new process uses to make requests.
pid = p.spawn()

# Go into an event loop.  Handle object invocations on the connection
# that we created.  This returns when no more objects are being
# exported (when the references are dropped).
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