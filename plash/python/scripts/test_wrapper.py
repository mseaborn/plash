#!/usr/bin/python

# Copyright (C) 2008 Mark Seaborn
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

"""
Usage: test_wrapper executable args...

Wrapper for running "make check" on glibc.  Contains hacks to enable
invoking static executables and invoking ld.so directly.
"""

import os
import signal
import subprocess
import sys

import plash_core
import plash.env
import plash.mainloop
import plash.marshal
import plash.namespace
import plash.process


# This is a hack to make invoking static executables work during test
# cases.  This should be rolled into FsOp.
class FsOpWrapper(plash.marshal.Pyobj_marshal):

    def __init__(self, wrapped_obj):
        self._obj = wrapped_obj

    def fsop_copy(self):
        return FsOpWrapper(self._obj.fsop_copy())

    def _is_static(self, filename):
        if not os.path.exists(filename):
            return False
        pathname = os.path.join(self._obj.fsop_getcwd(), filename)
        fh = open(pathname)
        try:
            if fh.read(2) == "#!":
                return False
        finally:
            fh.close()
        proc = subprocess.Popen(["readelf", "-l", pathname],
                                stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        proc.wait()
        return "INTERP" not in stdout

    def cap_call(self, args):
        method_name, args_unpacked = plash.marshal.unpack(args)
        if method_name == "fsop_exec":
            filename, argv = args_unpacked
            if self._is_static(filename):
                return plash.marshal.pack("r_fsop_exec", filename, argv, [])
            return self._obj.cap_call(args)
        elif method_name == "fsop_copy":
            return plash.marshal.pack("r_cap", self.fsop_copy())
        else:
            return self._obj.cap_call(args)


# Map signal numbers to names.  It would be useful if the signal
# module provided this.  Python does not appear to provide a wrapper
# for strsignal() either (which provides signal descriptions).
signal_names = dict(
    (getattr(signal, name), name)
    for name in dir(signal)
    if name.startswith("SIG") and not name.startswith("SIG_"))


def main(args):
    if len(args) == 0:
        print __doc__
        return 1
    cwd_path = os.getcwd()
    proc = plash.process.ProcessSpec()
    proc.cmd = args[0]
    proc.args = args[1:]
    proc.env = os.environ.copy()
    root_dir = plash.env.get_root_dir()
    proc.caps["conn_maker"] = proc.conn_maker
    proc.caps["fs_op"] = FsOpWrapper(plash.namespace.make_fs_op(root_dir))
    proc.caps["fs_op"].fsop_chdir(cwd_path)
    for fd in (0, 1, 2):
        proc.fds[fd] = plash_core.wrap_fd(os.dup(fd))
    pid = proc.spawn()
    plash.mainloop.run_server()
    pid2, status = os.wait()
    if os.WIFEXITED(status):
        return os.WEXITSTATUS(status)
    elif os.WIFSIGNALED(status):
        sig = os.WTERMSIG(status)
        sys.stderr.write("process killed by signal %i (%s)\n" %
                         (sig, signal_names.get(sig, "unknown")))
    return 127


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
