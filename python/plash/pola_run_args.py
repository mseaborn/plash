
import string
import sys
import os
import plash_core
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

    # -f<flags> <filename>
    def bind_at_same(flags_arg, args):
        flags = handle_flags(flags_arg)
        filename = get_arg(args, '-f')
        ns.resolve_populate(state.caller_root, proc.root_node,
                            filename, cwd=state.cwd, flags=flags['build_fs'])
        state.namespace_empty = False
        if flags['a']:
            proc.args.append(filename)

    # -t<flags> <dest_filename> <src_filename>
    def bind_at(flags_arg, args):
        flags = handle_flags(flags_arg)
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
    def add_arg(args):
        x = get_arg(args, '-a')
        proc.args.append(x)

    # -e [<command>] <args...>
    # This option consumes the remaining arguments in the list.
    def command_and_args(args):
        # If --prog has not already been used, the first argument is taken
        # as the program executable filename.
        if proc.cmd == None:
            proc.cmd = get_arg(args, '-e')
        # Take the rest as literal arguments.
        while(len(args) > 0):
            proc.args.append(args.pop(0))

    # -B: Provide default environment.
    def default_env(args):
        handle_args(state, proc,
                    ["-fl", "/usr",
                     "-fl", "/bin",
                     "-fl", "/lib",
                     "-fl,objrw", "/dev/null",
                     "-fl,objrw", "/dev/tty"
                     ])

    def set_executable(args):
        if proc.cmd != None:
            raise BadArgException, "--prog argument used multiple times"
        proc.cmd = get_arg(args, '--prog')

    def set_cwd(args):
        path = args.pop(0)
        state.cwd = ns.resolve_dir(state.caller_root, path, cwd=state.cwd)

    def unset_cwd(args):
        state.cwd = None

    def copy_cwd(args):
        state.cwd = ns.resolve_dir(state.caller_root, os.getcwd())

    # --env NAME=VALUE
    # Sets an environment variable.
    def add_environ_var(args):
        x = args.pop(0)
        index = x.find('=')
        if index == -1:
            raise BadArgException, "Bad --env parameter: %s" % x
        name = x[:index]
        value = x[index+1:]
        proc.env[name] = value

    # --x11: Grant access to X Window System displays
    def grant_x11_access(args):
        ns.resolve_populate(state.caller_root, proc.root_node,
                            "/tmp/.X11-unix/",
                            flags=ns.FS_FOLLOW_SYMLINKS | ns.FS_OBJECT_RW)
        if 'XAUTHORITY' in os.environ:
            ns.resolve_populate(state.caller_root, proc.root_node,
                                os.environ['XAUTHORITY'],
                                flags=ns.FS_FOLLOW_SYMLINKS)

    def grant_network_access(args):
        handle_args(state, proc,
                    ["-fl", "/etc/resolv.conf",
                     "-fl", "/etc/hosts",
                     "-fl", "/etc/services"])

    def enable_logging(args):
        fd = plash_core.wrap_fd(os.dup(2))
        proc.logger = ns.make_log_from_fd(fd)

    def log_to_file(args):
        log_filename = args.pop(0)
        fd = plash_core.wrap_fd(
            os.open(log_filename, os.O_WRONLY | os.O_CREAT | os.O_TRUNC))
        proc.logger = ns.make_log_from_fd(fd)

    def debug(args):
        raise BadArgException, "not implemented"

    def set_pet_name(args):
        state.pet_name = get_arg(args, "--pet-name")

    def enable_powerbox(args):
        state.powerbox = True

    def help(args):
        usage()
        sys.exit(0)

    options = {}
    def add_option(name, fun):
        options[name] = (fun, False)
    def add_option_t(name, fun):
        options[name] = (fun, True)
    
    add_option_t("f", bind_at_same)
    add_option_t("t", bind_at)
    add_option("a", add_arg)
    add_option("e", command_and_args)
    add_option("B", default_env)
    add_option("h", help)
    add_option("prog", set_executable)
    add_option("cwd", set_cwd)
    add_option("no-cwd", unset_cwd)
    add_option("copy-cwd", copy_cwd)
    add_option("env", add_environ_var)
    add_option("x11", grant_x11_access)
    add_option("net", grant_network_access)
    add_option("log", enable_logging)
    add_option("log-file", log_to_file)
    add_option("debug", debug)
    add_option("pet-name", set_pet_name)
    add_option("powerbox", enable_powerbox)
    add_option("help", help)

    arg = args.pop(0)
    if arg == "" or arg[0] != "-":
        raise BadArgException, "Unknown argument: %s" % arg
    elif arg[1] in options:
        fun, allow_trailing = options[arg[1]]
        if allow_trailing:
            fun(arg[2:], args)
        else:
            fun(args)
    elif arg[1] == "-" and arg[2:] in options:
        fun, allow_trailing = options[arg[2:]]
        assert not allow_trailing
        fun(args)
    else:
        raise BadArgException, "Unrecognised argument: %s" % arg


def set_fake_uids(proc):
    proc.env['PLASH_FAKE_UID'] = str(os.getuid())
    proc.env['PLASH_FAKE_GID'] = str(os.getgid())
    proc.env['PLASH_FAKE_EUID'] = str(os.getuid())
    proc.env['PLASH_FAKE_EGID'] = str(os.getgid())
