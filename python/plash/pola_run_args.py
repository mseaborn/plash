
import string
import sys
import os

import plash_core
import plash.filedesc
import plash.namespace as ns


STDIN_FILENO = 0
STDOUT_FILENO = 1
STDERR_FILENO = 2


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


def get_arg(args, name):
    if len(args) == 0:
        raise BadArgException, "%s expected an argument" % name
    return args.pop(0)


class ProcessSetup(object):
    """Processes the command line arguments accepted by pola-run.
    Grant a process a subset of the caller's authority."""

    def __init__(self, proc):
        self.proc = proc
        self.caller_root = None
        self.cwd = None
        self.namespace_empty = True
        self.powerbox = False
        self.pet_name = None

    # -f<flags> <filename>
    def bind_at_same(self, flags_arg, args):
        if "=" in flags_arg:
            flags_arg, filename = flags_arg.split("=", 1)
        else:
            filename = get_arg(args, "-f")
        flags = handle_flags(flags_arg)
        ns.resolve_populate(self.caller_root, self.proc.root_node,
                            filename, cwd=self.cwd, flags=flags['build_fs'])
        self.namespace_empty = False
        if flags['a']:
            self.proc.args.append(filename)

    # -t<flags> <dest_filename> <src_filename>
    def bind_at(self, flags_arg, args):
        flags = handle_flags(flags_arg)
        dest_filename = get_arg(args, '-t')
        src_filename = get_arg(args, '-t')
        obj = ns.resolve_obj(self.caller_root, src_filename,
                             cwd=self.cwd,
                             nofollow=(flags['build_fs'] & ns.FS_FOLLOW_SYMLINKS) != 0)
        if not ((flags['build_fs'] & ns.FS_OBJECT_RW != 0) or
                (flags['build_fs'] & ns.FS_SLOT_RWC != 0)):
            obj = ns.make_read_only_proxy(obj)
        ns.attach_at_path(self.proc.root_node, dest_filename, obj)
        self.namespace_empty = False
        if flags['a']:
            self.proc.args.append(dest_filename)

    # -a <string>
    # Append string to argument list.
    def add_arg(self, args):
        x = get_arg(args, '-a')
        self.proc.args.append(x)

    # -e [<command>] <args...>
    # This option consumes the remaining arguments in the list.
    def command_and_args(self, args):
        # If --prog has not already been used, the first argument is taken
        # as the program executable filename.
        if self.proc.cmd == None:
            self.proc.cmd = get_arg(args, '-e')
        # Take the rest as literal arguments.
        while(len(args) > 0):
            self.proc.args.append(args.pop(0))

    # -B: Provide default environment.
    def default_env(self, args):
        self.handle_args(
                    ["-fl", "/usr",
                     "-fl", "/bin",
                     "-fl", "/lib",
                     "-fl,objrw", "/dev/null",
                     "-fl,objrw", "/dev/tty"
                     ])

    def set_executable(self, args):
        if self.proc.cmd != None:
            raise BadArgException, "--prog argument used multiple times"
        self.proc.cmd = get_arg(args, '--prog')

    def set_cwd(self, args):
        path = args.pop(0)
        self.cwd = ns.resolve_dir(self.caller_root, path, cwd=self.cwd)

    def unset_cwd(self, args):
        self.cwd = None

    def copy_cwd(self, args):
        self.cwd = ns.resolve_dir(self.caller_root, os.getcwd())

    # --env NAME=VALUE
    # Sets an environment variable.
    def add_environ_var(self, args):
        x = args.pop(0)
        index = x.find('=')
        if index == -1:
            raise BadArgException, "Bad --env parameter: %s" % x
        name = x[:index]
        value = x[index+1:]
        self.proc.env[name] = value

    # -fd <fd-number>
    # Add file descriptor.
    def add_fd(self, args):
        fd = int(get_arg(args, "--fd"))
        self.proc.fds[fd] = plash_core.wrap_fd(os.dup(fd))

    # --x11: Grant access to X Window System displays
    def grant_x11_access(self, args):
        ns.resolve_populate(self.caller_root, self.proc.root_node,
                            "/tmp/.X11-unix/",
                            flags=ns.FS_FOLLOW_SYMLINKS | ns.FS_OBJECT_RW)
        if 'XAUTHORITY' in os.environ:
            ns.resolve_populate(self.caller_root, self.proc.root_node,
                                os.environ['XAUTHORITY'],
                                flags=ns.FS_FOLLOW_SYMLINKS)

    def grant_network_access(self, args):
        self.handle_args(
                    ["-fl", "/etc/resolv.conf",
                     "-fl", "/etc/hosts",
                     "-fl", "/etc/services"])

    def enable_logging(self, args):
        fd = plash_core.wrap_fd(os.dup(2))
        self.proc.logger = ns.make_log_from_fd(fd)

    def log_to_file(self, args):
        log_filename = args.pop(0)
        fd = plash_core.wrap_fd(
            os.open(log_filename, os.O_WRONLY | os.O_CREAT | os.O_TRUNC))
        self.proc.logger = ns.make_log_from_fd(fd)

    def debug(self, args):
        raise BadArgException, "not implemented"

    def set_pet_name(self, args):
        self.pet_name = get_arg(args, "--pet-name")

    def enable_powerbox(self, args):
        self.powerbox = True

    def help(self, args):
        usage()
        sys.exit(0)

    options = {}

    def handle_arg(self, args):
        arg = args.pop(0)
        if arg == "" or arg[0] != "-":
            raise BadArgException, "Unknown argument: %s" % arg
        elif arg[1] in self.options:
            method_name, allow_trailing = self.options[arg[1]]
            if allow_trailing:
                getattr(self, method_name)(arg[2:], args)
            else:
                getattr(self, method_name)(args)
        elif arg[1] == "-" and arg[2:] in self.options:
            method_name, allow_trailing = self.options[arg[2:]]
            assert not allow_trailing
            getattr(self, method_name)(args)
        else:
            raise BadArgException, "Unrecognised argument: %s" % arg

    def handle_args(self, args):
        args = args[:]
        while len(args) > 0:
            self.handle_arg(args)


def add_option(name, method_name):
    ProcessSetup.options[name] = (method_name, False)
def add_option_t(name, method_name):
    ProcessSetup.options[name] = (method_name, True)

add_option_t("f", "bind_at_same")
add_option_t("t", "bind_at")
add_option("a", "add_arg")
add_option("e", "command_and_args")
add_option("B", "default_env")
add_option("h", "help")
add_option("prog", "set_executable")
add_option("cwd", "set_cwd")
add_option("no-cwd", "unset_cwd")
add_option("copy-cwd", "copy_cwd")
add_option("env", "add_environ_var")
add_option("fd", "add_fd")
add_option("x11", "grant_x11_access")
add_option("net", "grant_network_access")
add_option("log", "enable_logging")
add_option("log-file", "log_to_file")
add_option("debug", "debug")
add_option("pet-name", "set_pet_name")
add_option("powerbox", "enable_powerbox")
add_option("help", "help")


def set_fake_uids(proc):
    proc.env['PLASH_FAKE_UID'] = str(os.getuid())
    proc.env['PLASH_FAKE_GID'] = str(os.getgid())
    proc.env['PLASH_FAKE_EUID'] = str(os.getuid())
    proc.env['PLASH_FAKE_EGID'] = str(os.getgid())

def proxy_terminal(proc):
    proc.fds[STDIN_FILENO], forwarder1 = \
        plash.filedesc.proxy_input_fd(os.dup(STDIN_FILENO))
    proc.fds[STDOUT_FILENO], forwarder2 = \
        plash.filedesc.proxy_output_fd(os.dup(STDOUT_FILENO))
    proc.fds[STDERR_FILENO], forwarder3 = \
        plash.filedesc.proxy_output_fd(os.dup(STDERR_FILENO))
    return (forwarder2, forwarder3)
