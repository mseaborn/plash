# Copyright (C) 2006, 2007 Mark Seaborn
#
# This file is part of Plash.
#
# Plash is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of
# the License, or (at your option) any later version.
#
# Plash is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with Plash; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
# USA.

import gobject
import string
import sys
import tempfile
import os

import plash_core
import plash.filedesc
import plash.namespace as ns


STDIN_FILENO = 0
STDOUT_FILENO = 1
STDERR_FILENO = 2


class BadArgException(Exception):

    pass


class UsageException(Exception):

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


class OptionWithSingleArg(object):

    def __init__(self, func):
        self._func = func

    def handle_option(self, name, trailing, args):
        if trailing == "":
            self._func(get_arg(args, name))
        elif trailing.startswith("="):
            self._func(trailing[1:])
        else:
            raise BadArgException("Unrecognised argument: %s%s" %
                                  (name, trailing))


class OptionTrailing(object):

    def __init__(self, func):
        self._func = func

    def handle_option(self, name, trailing, args):
        self._func(trailing, args)


class OptionNoTrailing(object):

    def __init__(self, func):
        self._func = func

    def handle_option(self, name, trailing, args):
        if trailing != "":
            raise BadArgException("Unrecognised argument: %s%s" %
                                  (name, trailing))
        self._func(args)


class ProcessSetup(object):
    """Processes the command line arguments accepted by pola-run.
    Grant a process a subset of the caller's authority."""

    def __init__(self, proc, parent_environ=None):
        self.proc = proc
        self.caller_root = None
        self.cwd = None
        self.namespace_empty = True
        self.powerbox = False
        self.pet_name = None
        if parent_environ is None:
            parent_environ = os.environ
        self._parent_environ = parent_environ
        self._options = {
            "f": OptionTrailing(self.bind_at_same),
            "t": OptionTrailing(self.bind_at),
            "a": OptionWithSingleArg(self.add_arg),
            "e": OptionNoTrailing(self.command_and_args),
            "B": OptionNoTrailing(self.default_env),
            "h": OptionNoTrailing(self.help),
            "prog": OptionWithSingleArg(self.set_executable),
            "cwd": OptionWithSingleArg(self.set_cwd),
            "no-cwd": OptionNoTrailing(self.unset_cwd),
            "copy-cwd": OptionNoTrailing(self.copy_cwd),
            "env": OptionWithSingleArg(self.add_environ_var),
            "fd": OptionWithSingleArg(self.add_fd),
            "x11": OptionNoTrailing(self.grant_x11_access),
            "net": OptionNoTrailing(self.grant_network_access),
            "tmp": OptionNoTrailing(self.grant_tmp),
            "tmpdir": OptionWithSingleArg(self.grant_tmp_dir),
            "log": OptionNoTrailing(self.enable_logging),
            "log-file": OptionWithSingleArg(self.log_to_file),
            "debug": OptionNoTrailing(self.debug),
            "pet-name": OptionWithSingleArg(self.set_pet_name),
            "powerbox": OptionNoTrailing(self.enable_powerbox),
            "help": OptionNoTrailing(self.help),
            }

    # -f<flags> <filename>
    def bind_at_same(self, flags_arg, args):
        if "=" in flags_arg:
            flags_arg, filename = flags_arg.split("=", 1)
        else:
            filename = get_arg(args, "-f")
        flags = handle_flags(flags_arg)
        self.proc.get_namespace().resolve_populate(
            self.caller_root, filename, cwd=self.cwd, flags=flags['build_fs'])
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
        self.proc.get_namespace().attach_at_path(dest_filename, obj)
        self.namespace_empty = False
        if flags['a']:
            self.proc.args.append(dest_filename)

    # -a <string>
    # Append string to argument list.
    def add_arg(self, arg):
        self.proc.args.append(arg)

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
                     ])

    def set_executable(self, arg):
        if self.proc.cmd != None:
            raise BadArgException, "--prog argument used multiple times"
        self.proc.cmd = arg

    def set_cwd(self, dir_path):
        self.cwd = ns.resolve_dir(self.caller_root, dir_path, cwd=self.cwd)

    def unset_cwd(self, args):
        self.cwd = None

    def copy_cwd(self, args):
        self.cwd = ns.resolve_dir(self.caller_root, os.getcwd())

    # --env NAME=VALUE
    # Sets an environment variable.
    def add_environ_var(self, arg):
        if "=" not in arg:
            raise BadArgException("Bad --env parameter: %s" % arg)
        name, value = arg.split("=", 1)
        self.proc.env[name] = value

    def grant_proxy_terminal_access(self):
        self.proc.fds[STDIN_FILENO], forwarder1 = \
            plash.filedesc.proxy_input_fd(os.dup(STDIN_FILENO))
        self.proc.fds[STDOUT_FILENO], forwarder2 = \
            plash.filedesc.proxy_output_fd(os.dup(STDOUT_FILENO))
        self.proc.fds[STDERR_FILENO], forwarder3 = \
            plash.filedesc.proxy_output_fd(os.dup(STDERR_FILENO))
        return (forwarder2, forwarder3)

    # -fd <fd-number>
    # Add file descriptor.
    def add_fd(self, arg):
        fd = int(arg)
        self.proc.fds[fd] = plash_core.wrap_fd(os.dup(fd))

    # --x11: Grant access to X Window System displays
    # TODO: Pass through only one Xauthority entry, instead of passing
    # them all.  Grant only one X11-unix socket, instead of granting
    # them all.
    def grant_x11_access(self, args):
        # Note that /tmp/.X11-unix might not exist if the system is
        # using only TCP sockets or does not even use X.
        # TODO: don't use FS_OBJECT_RW, because that potentially
        # allows directory entries to be added and removed.  We only
        # need to grant the ability to connect to sockets, which
        # FS_READ_ONLY blocks.
        try:
            self.proc.get_namespace().resolve_populate(
                self.caller_root, "/tmp/.X11-unix/",
                flags=ns.FS_FOLLOW_SYMLINKS | ns.FS_OBJECT_RW)
        except plash.namespace.ReturnUnmarshalError:
            pass
        if "DISPLAY" in self._parent_environ:
            self.proc.env["DISPLAY"] = self._parent_environ["DISPLAY"]

        xauthority_file = self._parent_environ.get("XAUTHORITY")
        if xauthority_file is not None:
            self.proc.env["XAUTHORITY"] = xauthority_file
        if xauthority_file is None and "HOME" in self._parent_environ:
            xauthority_file = os.path.join(self._parent_environ["HOME"],
                                           ".Xauthority")
        if xauthority_file is not None:
            try:
                self.proc.get_namespace().resolve_populate(
                    self.caller_root, xauthority_file,
                    flags=ns.FS_FOLLOW_SYMLINKS)
            except plash.namespace.ReturnUnmarshalError:
                pass

    def grant_network_access(self, args):
        self.handle_args(
                    ["-fl", "/etc/resolv.conf",
                     "-fl", "/etc/hosts",
                     "-fl", "/etc/services"])

    def _make_temp_dir(self):
        dir_path = tempfile.mkdtemp()
        return plash.env.get_dir_from_path(dir_path)

    def grant_tmp(self, args):
        self.proc.get_namespace().attach_at_path("/tmp", self._make_temp_dir())

    def grant_tmp_dir(self, dest_path):
        self.proc.get_namespace().attach_at_path(dest_path,
                                                 self._make_temp_dir())

    def enable_logging(self, args):
        fd = plash_core.wrap_fd(os.dup(2))
        self.proc.logger = ns.make_log_from_fd(fd)

    def log_to_file(self, log_filename):
        fd = plash_core.wrap_fd(
            os.open(log_filename, os.O_WRONLY | os.O_CREAT | os.O_TRUNC))
        self.proc.logger = ns.make_log_from_fd(fd)

    def debug(self, args):
        raise BadArgException, "not implemented"

    def set_pet_name(self, name):
        self.pet_name = name

    def enable_powerbox(self, args):
        self.powerbox = True

    def help(self, args):
        raise UsageException()

    def handle_arg(self, args):
        arg = args.pop(0)
        if not arg.startswith("-"):
            raise BadArgException("Unknown argument: %s" % arg)
        if arg[1] in self._options:
            handler = self._options[arg[1]]
            handler.handle_option(arg[:2], arg[2:], args)
            return
        if arg.startswith("--"):
            if "=" in arg:
                arg_name, rest = arg.split("=", 1)
                trailing = "=" + rest
            else:
                arg_name = arg
                trailing = ""
            handler = self._options.get(arg_name[2:], None)
            if handler is not None:
                handler.handle_option(arg_name, trailing, args)
                return
        raise BadArgException("Unrecognised argument: %s" % arg)

    def handle_args(self, args):
        args = args[:]
        while len(args) > 0:
            self.handle_arg(args)


def set_fake_uids(proc):
    proc.env['PLASH_FAKE_UID'] = str(os.getuid())
    proc.env['PLASH_FAKE_GID'] = str(os.getgid())
    proc.env['PLASH_FAKE_EUID'] = str(os.getuid())
    proc.env['PLASH_FAKE_EGID'] = str(os.getgid())


def return_status_and_fork_to_background(exit_status):
    pid = os.fork()
    if pid != 0:
        os._exit(exit_status)


def flush_multiple_forwarders(fd_forwarders, callback):
    # It might be better to implement this using promises.
    counter = [len(fd_forwarders)]
    def on_flush():
        counter[0] -= 1
        if counter[0] == 0:
            callback()

    for forwarder in fd_forwarders:
        forwarder.flush(on_flush)


def convert_wait_status(status):
    if os.WIFEXITED(status):
        return os.WEXITSTATUS(status)
    elif os.WIFSIGNALED(status):
        return 101
    else:
        return 102


def on_flushed_child_exit(pid, fd_forwarders, callback):
    reason_to_run = plash.mainloop.ReasonToRun()

    def exit_callback(pid_unused, status):
        reason_to_run.dispose()
        def on_flush():
            callback(status)
        flush_multiple_forwarders(fd_forwarders, on_flush)

    gobject.child_watch_add(pid, exit_callback)


def flush_and_return_status_on_child_exit(pid, fd_forwarders):
    def callback(status):
        return_status_and_fork_to_background(convert_wait_status(status))

    on_flushed_child_exit(pid, fd_forwarders, callback)
