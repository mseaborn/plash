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

import os
import shutil
import subprocess
import tempfile
import unittest

import plash.mainloop
import plash.pola_run_args
import plash.process

import libc_test


# TODO: remove when FsObjReal stops changing the cwd
__file__ = os.path.abspath(__file__)


class ThreadTest(unittest.TestCase):

    def setUp(self):
        self.tmp_dir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmp_dir)

    def _compile(self):
        dir_path = os.path.dirname(__file__)
        executable = os.path.join(self.tmp_dir, "test-case")
        rc = subprocess.call(["gcc", "-Wall",
                              os.path.join(dir_path, "thread-test.c"),
                              os.path.join(dir_path, "test-util.c"),
                              "-lpthread",
                              "-o", executable])
        assert rc == 0, rc
        return executable

    def test_sequential_native(self):
        test_prog = self._compile()
        rc = subprocess.call([test_prog, "test_sequential"],
                             cwd=self.tmp_dir)
        assert rc == 0, rc

    def test_parallel_native(self):
        test_prog = self._compile()
        rc = subprocess.call([test_prog, "test_parallel"],
                             cwd=self.tmp_dir)
        assert rc == 0, rc

    def _run_test(self, arg):
        test_prog = self._compile()
        proc = plash.process.ProcessSpecWithNamespace()
        proc.cwd_path = self.tmp_dir
        proc.env = os.environ.copy()
        state = plash.pola_run_args.ProcessSetup(proc)
        state.grant_proxy_terminal_access()
        state.caller_root = plash.env.get_root_dir()
        state.handle_args(["-B", "-fw", self.tmp_dir,
                           "-e", test_prog, arg])
        if "PLASH_LIBRARY_DIR" in os.environ:
            state.handle_args(["-f", os.environ["PLASH_LIBRARY_DIR"]])
        pid = proc.spawn()
        plash.mainloop.run_server()
        pid2, status = os.wait()
        self.assertEquals(pid, pid2)
        libc_test.check_exit_status(status)

    def test_sequential(self):
        self._run_test("test_sequential")

    def test_parallel(self):
        self._run_test("test_parallel")


if __name__ == "__main__":
    unittest.main()
