
import string
import fcntl
import os
import plash_namespace as ns


# Attributes:
# env: dict listing environment variables
# cmd: pathname of command to execve()
# arg0
# argv: list of arguments to pass to execve() (not including usual 0th arg)
# fds: dict mapping FD numbers to FDs
# caps: dict mapping cap names (e.g. "fs_op") to objects
# conn_maker: object to use for creating new connection
class Process_spec:
    def __init__(self):
        self.conn_maker = ns.conn_maker

    def setcmd(self, cmd, *args):
        self.cmd = cmd
        self.arg0 = cmd
        self.args = list(args)
    
    def plash_setup(self):
        cap_names = self.caps.keys()
        fd = self.conn_maker.make_conn([self.caps[x] for x in cap_names])
        self.env['PLASH_CAPS'] = string.join(cap_names, ':')
        self.env['PLASH_COMM_FD'] = str(fd.fileno())
        fcntl.fcntl(fd.fileno(), fcntl.F_SETFD, 0) # Unset FD_CLOEXEC flag

        orig_cmd = self.cmd
        self.cmd = "/usr/lib/plash/run-as-anonymous"
        self.args = ["-s", "LD_LIBRARY_PATH=/usr/lib/plash/lib",
                     "/special/ld-linux.so.2",
                     orig_cmd] + self.args

        # This is a hack to ensure that the FD doesn't get GC'd and closed
        self.fd = fd

    def plash_setup2(self):
        cap_names = self.caps.keys()
        fd = self.conn_maker.make_conn([self.caps[x] for x in cap_names])
        self.env['PLASH_CAPS'] = string.join(cap_names, ':')
        self.env['PLASH_COMM_FD'] = str(fd.fileno())
        fcntl.fcntl(fd.fileno(), fcntl.F_SETFD, 0) # Unset FD_CLOEXEC flag

        orig_cmd = self.cmd
        self.cmd = "/usr/bin/strace"
        self.args = ["/home/mrs/projects/plash/shobj/ld.so",
                     orig_cmd] + self.args

        # This is a hack to ensure that the FD doesn't get GC'd and closed
        self.fd = fd

    def spawn(self):
        pid = os.fork()
        if pid == 0:
            os.execve(self.cmd, [self.arg0] + self.args, self.env)
            sys.exit(1)
        else:
            os.close(self.fd.fileno()) # Hack; to be removed
            return pid
