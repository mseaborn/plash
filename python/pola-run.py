#!/usr/bin/python

import sys
import os
import plash_core
import plash.env
import plash.namespace as ns
from plash.process import Process_spec
import plash.pola_run_args
from plash.pola_run_args import BadArgException


class Proc_spec(Process_spec):
    def __init__(self):
        Process_spec.__init__(self)
        
        self.root_node = ns.make_node()
        root = ns.dir_of_node(self.root_node)
        fs_op = plash_core.make_fs_op(root)
        self.caps = { 'fs_op': fs_op,
                      'conn_maker': ns.conn_maker }

        self.cmd = None
        self.arg0 = None
        self.args = []
        self.cwd = None


def usage():
    print """
Usage: pola-run [options]
  --prog <file>   Gives filename of executable to invoke
  [ -f[awls]... <path>
                  Grant access to given file or directory
  | -t[awls]... <path-dest> <path-src>
                  Attach file/dir <path-src> into namespace at <path-dest>
  | -a <string>   Add string to argument list
  | --cwd <dir>   Set the current working directory (cwd)
  | --no-cwd      Set the cwd to be undefined
  | --copy-cwd    Copy cwd from calling process
  ]...
  [-B]            Grant access to /usr, /bin and /lib
  [--x11]         Grant access to X11 Window System
  [--net]         Grant access to network config files
  [--log]         Print method calls client makes to file server
  [--pet-name <name>]
  [--powerbox]
  [-e <command> <arg>...]
"""


proc = Proc_spec()
proc.env = os.environ.copy()

class State:
    pass
state = State()
state.namespace_empty = True
state.caller_root = plash.env.get_root_dir()
state.cwd = ns.resolve_dir(state.caller_root, os.getcwd())
state.pet_name = None
state.powerbox = False

try:
    plash.pola_run_args.handle_args(state, proc, sys.argv[1:])

    # The zeroth argument defaults to being the same as the command pathname
    proc.arg0 = proc.cmd

    if proc.cmd == None:
        raise BadArgException, "No command name given (use --prog or -e)"
except BadArgException, msg:
    print "pola-run: error:", msg
    sys.exit(1)

if state.namespace_empty:
    print "pola-run: warning: namespace empty, try -B for default namespace"

plash.pola_run_args.set_fake_uids(proc)

# If the chosen cwd is present in the callee's namespace, set the cwd.
# Otherwise, leave it undefined.
if state.cwd != None:
    fs_op = proc.caps['fs_op']
    cwd_path = ns.dirstack_get_path(state.cwd)
    try:
        fs_op.fsop_chdir(cwd_path)
    except:
        pass

if state.powerbox:
    import plash.powerbox
    # The pet name defaults to the executable name
    if state.pet_name == None:
        state.pet_name = proc.cmd
    proc.caps['powerbox_req_filename'] = \
        plash.powerbox.Powerbox(user_namespace = state.caller_root,
                                app_namespace = proc.root_node,
                                pet_name = state.pet_name)

pid = proc.spawn()
# FIXME: don't want to have this special case
if state.powerbox:
    import gtk
    gtk.main()
else:
    plash_core.run_server()

# Wait for the subprocess to exit and check the exit code.
(pid2, status) = os.waitpid(pid, 0)
assert pid == pid2
if os.WIFEXITED(status):
    sys.exit(os.WEXITSTATUS(status))
else:
    # FIXME
    sys.exit(1)
