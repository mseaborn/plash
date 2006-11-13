#!/usr/bin/python

import sys
import os
import plash_core
import plash.env
import plash.namespace as ns
from plash.process import Process_spec


class BadArgException(Exception):
    pass

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


def handle_flags(str):
    x = { 'build_fs': 0,
          'a': False }
    for c in str:
        if c == 'a':
            x['a'] = True
        elif c == 'l':
            x['build_fs'] |= ns.FS_FOLLOW_SYMLINKS
        elif c == 's':
            # Ignored
            pass
        elif c == 'w':
            x['build_fs'] |= ns.FS_SLOT_RWC
        elif c == 'objrw':
            x['build_fs'] |= ns.FS_OBJECT_RW
        else:
            raise BadArgException, "Unrecognised flag argument: %c" % c
    return x


def handle_args(proc, args):
    args = args[:]
    while len(args) > 0:
        handle_arg(proc, args)


def get_arg(args, name):
    if len(args) == 0:
        raise BadArgException, "%s expected an argument" % name
    return args.pop(0)

def handle_arg(proc, args):
    arg = args.pop(0)
    if arg == '' or arg[0] != '-':
        raise BadArgException, "Unknown argument: %s" % arg

    # -f<flags> <filename>
    if arg[1] == 'f':
        flags = handle_flags(arg[2:])
        filename = get_arg(args, '-f')
        ns.resolve_populate(state.caller_root, proc.root_node,
                            filename, cwd=state.cwd, flags=flags['build_fs'])
        state.namespace_empty = False
        if flags['a']:
            proc.args.append(filename)

    # -t<flags> <dest_filename> <src_filename>
    elif arg[1] == 't':
        flags = handle_flags(arg[2:])
        dest_filename = get_arg(args, '-t')
        src_filename = get_arg(args, '-t')
        obj = ns.resolve_obj(state.caller_root, src_filename,
                             cwd=state.cwd,
                             nofollow=(flags['build_fs'] & ns.FS_FOLLOW_SYMLINKS) != 0)
        if not ((flags['build_fs'] & ns.FS_OBJECT_RW != 0) or
                (flags['build_fs'] & ns.FS_SLOT_RWC != 0)):
            obj = ns.make_read_only_proxy(obj)
        ns.attach_at_path(proc.root_node, dest_filename, obj)
        state.namespace_empty = False
        if flags['a']:
            proc.args.append(dest_filename)

    # -a <string>
    # Append string to argument list.
    elif arg == '-a':
        x = get_arg(args, '-a')
        proc.args.append(x)

    # -e [<command>] <args...>
    # This option consumes the remaining arguments in the list.
    elif arg[1] == 'e':
        # If --prog has not already been used, the first argument is taken
        # as the program executable filename.
        if proc.cmd == None:
            proc.cmd = get_arg(args, '-e')
        # Take the rest as literal arguments.
        while(len(args) > 0):
            proc.args.append(args.pop(0))

    # -B: Provide default environment.
    elif arg[1] == 'B':
        handle_args(proc,
                    ["-fl", "/usr",
                     "-fl", "/bin",
                     "-fl", "/lib",
                     #"-fl,objrw", "/dev/null",
                     #"-fl,objrw", "/dev/tty"
                     ])

    # -h: Help
    elif arg[1] == 'h':
        usage()
        sys.exit(0)

    elif arg == '--prog':
        if proc.cmd != None:
            raise BadArgException, "--prog argument used multiple times"
        proc.cmd = get_arg(args, '--prog')

    elif arg == '--cwd':
        path = args.pop(0)
        state.cwd = ns.resolve_dir(state.caller_root, path, cwd=state.cwd)

    elif arg == '--no-cwd':
        state.cwd = None

    elif arg == '--copy-cwd':
        state.cwd = ns.resolve_dir(state.caller_root, os.getcwd())

    # --env NAME=VALUE
    # Sets an environment variable.
    elif arg == '--env':
        x = args.pop(0)
        index = x.find('=')
        if index == -1:
            raise BadArgException, "Bad --env parameter: %s" % x
        name = x[:index]
        value = x[index+1:]
        proc.env[name] = value

    # --x11: Grant access to X Window System displays
    elif arg == '--x11':
        ns.resolve_populate(state.caller_root, proc.root_node,
                            "/tmp/.X11-unix/",
                            flags=ns.FS_FOLLOW_SYMLINKS | ns.FS_OBJECT_RW)
        if 'XAUTHORITY' in os.environ:
            ns.resolve_populate(state.caller_root, proc.root_node,
                                os.environ['XAUTHORITY'],
                                flags=ns.FS_FOLLOW_SYMLINKS)

    elif arg == '--net':
        handle_args(proc,
                    ["-fl", "/etc/resolv.conf",
                     "-fl", "/etc/hosts",
                     "-fl", "/etc/services"])

    elif arg == '--log':
        raise BadArgException, "not implemented"
    
    elif arg == '--debug':
        raise BadArgException, "not implemented"

    elif arg == '--pet-name':
        state.pet_name = get_arg(args, "--pet-name")

    elif arg == '--powerbox':
        state.powerbox = True

    elif arg == '--help':
        usage()
        sys.exit(0)

    else:
        raise BadArgException, "Unrecognised argument: %s" % arg


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
    handle_args(proc, sys.argv[1:])

    # REMOVE
    ns.resolve_populate(state.caller_root, proc.root_node, "/usr")
    ns.resolve_populate(state.caller_root, proc.root_node, "/dev/null", flags=ns.FS_OBJECT_RW)

    # The zeroth argument defaults to being the same as the command pathname
    proc.arg0 = proc.cmd

    if proc.cmd == None:
        raise BadArgException, "No command name given (use --prog or -e)"
except BadArgException, msg:
    print "pola-run: error:", msg
    sys.exit(1)

if state.namespace_empty:
    print "pola-run: warning: namespace empty, try -B for default namespace"

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
