
import errno
import fcntl
import os
import string

import plash_core
import plash.namespace as ns
import plash.env


STDERR_FILENO = 2


def kernel_execve(cmd, argv, env):
    return plash_core.kernel_execve(cmd, tuple(argv),
                               tuple([x[0]+"="+x[1] for x in env.items()]))

def add_to_path(dir, path):
    """Insert string <dir> into colon-separated list <path> if it is not
    already present.  Used for LD_LIBRARY_PATH."""
    elts = string.split(path, ":")
    if dir not in elts:
        elts.insert(0, dir)
        return string.join(elts, ":")
    return path

# Attributes:
# env: dict listing environment variables
# cmd: pathname of command to execve()
# arg0
# args: list of arguments to pass to execve() (not including usual 0th arg)
# fds: dict mapping FD numbers to FDs
# caps: dict mapping cap names (e.g. "fs_op") to objects
# conn_maker: object to use for creating new connection
class Process_spec(object):
    
    def __init__(self):
        self.env = {}
        self.cmd = None
        self.arg0 = None
        self.args = []
        self.caps = {}
        self.conn_maker = ns.conn_maker
        self.fds = {}
        self.next_fd = 3

    def setcmd(self, cmd, *args):
        self.cmd = cmd
        self.arg0 = cmd
        self.args = list(args)

    def add_fd(self, fd):
        """Add a file descriptor.  Takes FD object, returns the FD
        number assigned."""
        while self.next_fd in self.fds:
            self.next_fd += 1
        fd_number = self.next_fd
        self.next_fd += 1
        self.fds[fd_number] = fd
        return fd_number
    
    def plash_setup(self):
        cap_names = self.caps.keys()
        fd = self.conn_maker.make_conn([self.caps[x] for x in cap_names])
        fd_number = self.add_fd(fd)
        self.env['PLASH_CAPS'] = string.join(cap_names, ';')
        self.env['PLASH_COMM_FD'] = str(fd_number)

        # Ensure that certain environment variables get preserved
        # across the invocation of a setuid program.
        def preserve_env(args):
            for var in ['LD_LIBRARY_PATH', 'LD_PRELOAD']:
                if var in self.env:
                    args.extend(["-s", var + '=' + self.env[var]])

        if not plash.env.under_plash:
            if 'PLASH_SANDBOX_PROG' in os.environ:
                prefix_cmd = [os.environ['PLASH_SANDBOX_PROG']]
                # prefix_cmd = string.split(os.environ['PLASH_SANDBOX_PROGV'], ":")
            else:
                self.env['LD_LIBRARY_PATH'] = \
                    add_to_path("/usr/lib/plash/lib",
                                self.env.get('LD_LIBRARY_PATH', ''))
                prefix_cmd = ["/usr/lib/plash/run-as-anonymous"]
                preserve_env(prefix_cmd)
                prefix_cmd.append("/special/ld-linux.so.2")
        else:
            if 'PLASH_P_SANDBOX_PROG' in os.environ:
                prefix_cmd = [os.environ['PLASH_P_SANDBOX_PROG']]
            else:
                prefix_cmd = ["/run-as-anonymous"]
                preserve_env(prefix_cmd)

        orig_cmd = self.cmd
        self.cmd = prefix_cmd[0]
        self.args = prefix_cmd[1:] + [orig_cmd] + self.args

    def set_post_defaults(self):
        if self.arg0 is None:
            self.arg0 = self.cmd

    def _set_up_fds(self):
        """Called after fork() in the child process."""
        max_fd = 3
        for dest_number, fd in self.fds.iteritems():
            max_fd = max(max_fd, dest_number + 1)
        # Duplicate the FDs to temporary numbers that won't conflict
        # with where they will be assigned.
        keep = set()
        mappings = []
        for dest_number, fd in self.fds.iteritems():
            temp_fd = fcntl.fcntl(fd.fileno(), fcntl.F_DUPFD, max_fd)
            keep.add(temp_fd)
            mappings.append((dest_number, temp_fd))
        for fd in range(3, 1024):
            if fd not in keep:
                try:
                    os.close(fd)
                except OSError, ex:
                    if ex.errno == errno.EBADF:
                        pass
                    else:
                        raise
        # Copy the file descriptors into place.
        for dest_fd, src_fd in mappings:
            os.dup2(src_fd, dest_fd)
            os.close(src_fd)

    def spawn(self):
        self.set_post_defaults()
        self.plash_setup()
        if plash.env.under_plash:
            my_execve = kernel_execve
        else:
            my_execve = os.execve
        assert isinstance(self.cmd, str)
        for arg in [self.arg0] + self.args:
            assert isinstance(arg, str)
        # Doing this in Python is a little risky.  We must ensure that
        # no freeing of Plash objects occurs before the call to
        # cap_close_all_connections() in the newly-forked process,
        # otherwise both processes could try to use the same connection
        # to send the "Drop" message, leading to a protocol violation.
        # We're probably OK with the refcounting C implementation of
        # Python, but another implementation might do GC at any point.
        pid = os.fork()
        if pid == 0:
            try:
                plash_core.cap_close_all_connections()
                try:
                    self._set_up_fds()
                    my_execve(self.cmd, [self.arg0] + self.args, self.env)
                except Exception, ex:
                    os.write(STDERR_FILENO, "%s\n" % str(ex))
                os.write(STDERR_FILENO, "execve failed\n")
            except:
                pass
            os._exit(1)
        else:
            # We must drop our references to the client's FDs so that
            # we do not hold its connections open even after the
            # client has exited.
            self.fds.clear()
            return pid


class NotFoundInPathError(Exception):
    pass


class Process_spec_ns(Process_spec):

    def __init__(self):
        Process_spec.__init__(self)
        self.caps["conn_maker"] = self.conn_maker
        self.root_node = ns.make_node()
        self.root_dir = None
        self.cwd_path = None
        self.logger = None

    def _resolve_obj(self, pathname):
        if self.cwd_path is None:
            return ns.resolve_obj(self.root_dir, pathname)
        else:
            return ns.resolve_obj(self.root_dir,
                                  os.path.join(self.cwd_path, pathname))

    def _look_up_in_path(self, name, search_path):
        for dir_path in search_path.split(":"):
            full_path = os.path.join(dir_path, name)
            try:
                unused_obj = self._resolve_obj(full_path)
            except ns.ReturnUnmarshalError:
                pass
            else:
                return full_path
        raise NotFoundInPathError(name, search_path)

    def _resolve_executable(self):
        if "/" not in self.cmd:
            self.cmd = self._look_up_in_path(self.cmd, self.env["PATH"])

    def _set_up_script(self):
        exec_obj = self._resolve_obj(self.cmd)
        fd = exec_obj.file_open(os.O_RDONLY)
        fh = os.fdopen(os.dup(fd.fileno()))
        try:
            line = fh.readline(1024)
        finally:
            fh.close()
        line = line.rstrip("\n")
        if line.startswith("#!"):
            line = line[2:]
            self.args.insert(0, self.cmd)
            # A single argument may appear after the executable name.
            # It may contain spaces.
            if " " in line:
                executable_path, rest = line.split(" ", 1)
                self.cmd = executable_path
                self.args.insert(0, rest.lstrip(" "))
            else:
                self.cmd = line

    def make_fs_op(self, root_dir, logger):
        return ns.make_fs_op(root_dir, logger)

    def plash_setup(self):
        self.root_dir = ns.dir_of_node(self.root_node)
        self._resolve_executable()
        self._set_up_script()
        fs_op = self.make_fs_op(self.root_dir, self.logger)
        self.caps["fs_op"] = fs_op
        # If the chosen cwd is present in the callee's namespace, set the cwd.
        # Otherwise, leave it undefined.
        if self.cwd_path is not None:
            try:
                fs_op.fsop_chdir(self.cwd_path)
            except plash.marshal.UnmarshalError:
                pass
        Process_spec.plash_setup(self)
