
import string
import sys
import os
import plash.namespace as ns


class BadArgException(Exception):
    pass


def split_flags(flag_str):
    lst = string.split(flag_str, ",")
    return list(lst[0]) + lst[1:]

def handle_flags(flag_str):
    x = { 'build_fs': 0,
          'a': False }
    for c in split_flags(flag_str):
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


def handle_args(state, proc, args):
    args = args[:]
    while len(args) > 0:
        handle_arg(state, proc, args)


def get_arg(args, name):
    if len(args) == 0:
        raise BadArgException, "%s expected an argument" % name
    return args.pop(0)

def handle_arg(state, proc, args):
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
        handle_args(state, proc,
                    ["-fl", "/usr",
                     "-fl", "/bin",
                     "-fl", "/lib",
                     "-fl,objrw", "/dev/null",
                     "-fl,objrw", "/dev/tty"
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
        handle_args(state, proc,
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


def set_fake_uids(proc):
    proc.env['PLASH_FAKE_UID'] = str(os.getuid())
    proc.env['PLASH_FAKE_GID'] = str(os.getgid())
    proc.env['PLASH_FAKE_EUID'] = str(os.getuid())
    proc.env['PLASH_FAKE_EGID'] = str(os.getgid())
