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

import os
import unittest

from plash.process import add_to_path
import plash.comms.stream
import plash.filedesc
import plash.mainloop
import plash.process


class ProcessExitError(Exception):

    pass


def check_exit_status(status):
    if os.WIFEXITED(status):
        if os.WEXITSTATUS(status) != 0:
            raise ProcessExitError("Process exited with status %i" %
                                   os.WEXITSTATUS(status))
    else:
        raise ProcessExitError("Status %i" % status)


class TestPath(unittest.TestCase):

    def test(self):
        self.assertEquals(add_to_path("/foo", "/bar:/qux"),
                          "/foo:/bar:/qux")
        self.assertEquals(add_to_path("/foo", "/bar:/foo"),
                          "/foo:/bar")
        self.assertEquals(add_to_path("/foo", ""),
                          "/foo")


class ProcessTest(unittest.TestCase):

    def test(self):
        proc = plash.process.ProcessSpecWithNamespace()
        # Copying env is needed for test environment using wrapper.sh
        proc.env = os.environ.copy()
        proc.get_namespace().attach_at_path("/", plash.env.get_root_dir())
        pipe_read, pipe_write = plash.comms.stream.make_pipe()
        proc.fds[1] = pipe_write
        proc.fds[2] = plash.filedesc.dup_and_wrap_fd(2)
        proc.setcmd("/bin/echo", "hello world")
        pid = proc.spawn()
        plash.mainloop.run_server()
        pid2, status = os.waitpid(pid, 0)
        self.assertEquals(pid2, pid)
        check_exit_status(status)
        self.assertEquals(os.read(pipe_read.fileno(), 1000), "hello world\n")


if __name__ == "__main__":
    unittest.main()
