
import string
import fcntl
import os
import plash
import plash_namespace as ns
import plash_env
import sys


def kernel_execve(cmd, argv, env):
    return plash.kernel_execve(cmd, tuple(argv),
                               tuple([x[0]+"="+x[1] for x in env.items()]))

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
        self.env['PLASH_CAPS'] = string.join(cap_names, ';')
        self.env['PLASH_COMM_FD'] = str(fd.fileno())
        fcntl.fcntl(fd.fileno(), fcntl.F_SETFD, 0) # Unset FD_CLOEXEC flag

        orig_cmd = self.cmd
        if plash_env.under_plash:
            self.cmd = "/run-as-anonymous"
        else:
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
        plash_dir = "/home/mrs/projects/plash"
        self.args = ["-f",
                     "-c", "-o", "/dev/null",
                     "-E", "LD_LIBRARY_PATH="+plash_dir+"/lib",
                     plash_dir+"/shobj/ld.so",
                     orig_cmd] + self.args

        # This is a hack to ensure that the FD doesn't get GC'd and closed
        self.fd = fd

    def spawn(self):
        self.plash_setup()
        if plash_env.under_plash:
            my_execve = kernel_execve
        else:
            my_execve = os.execve
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
                plash.cap_close_all_connections()
                try:
                    my_execve(self.cmd, [self.arg0] + self.args, self.env)
                except:
                    pass
                print "execve failed"
            except:
                pass
            sys.exit(1)
        else:
            os.close(self.fd.fileno()) # Hack; to be removed
            return pid
