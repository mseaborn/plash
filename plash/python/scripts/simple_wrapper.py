#!/usr/bin/python

# Copyright (C) 2007 Mark Seaborn
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
Usage: simple_wrapper executable args...

This tools runs the specified command in a Plash environment with no
sandboxing.  This is used in the glibc build process for running
processes using the newly-built ld.so and libc.so.
"""

import os
import sys

import plash_core
import plash.env
import plash.mainloop
import plash.namespace
import plash.process


class Process(plash.process.ProcessSpec):

    def _set_up_library_path(self):
        pass

    def _set_up_sandbox_prog(self):
        pass


def main(args):
    if len(args) == 0:
        print __doc__
        return 1
    cwd_path = os.getcwd()
    proc = Process()
    proc.cmd = args[0]
    proc.args = args[1:]
    proc.env = os.environ.copy()
    root_dir = plash.env.get_root_dir()
    proc.caps["conn_maker"] = proc.conn_maker
    proc.caps["fs_op"] = plash.namespace.make_fs_op(root_dir)
    proc.caps["fs_op"].fsop_chdir(cwd_path)
    for fd in (0, 1, 2):
        proc.fds[fd] = plash_core.wrap_fd(os.dup(fd))
    pid = proc.spawn()
    plash.mainloop.run_server()
    pid2, status = os.wait()
    if os.WIFEXITED(status):
        return os.WEXITSTATUS(status)
    else:
        return 127


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
