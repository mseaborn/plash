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

import os
import shutil
import subprocess
import tempfile
import unittest


def write_file(filename, data):
    fh = open(filename, "w")
    try:
        fh.write(data)
    finally:
        fh.close()


class TempDirTestCase(unittest.TestCase):

    def setUp(self):
        self._temp_dirs = []

    def tearDown(self):
        for temp_dir in self._temp_dirs:
            shutil.rmtree(temp_dir)

    def make_temp_dir(self):
        temp_dir = tempfile.mkdtemp(prefix="tmp-%s-" % self.__class__.__name__)
        self._temp_dirs.append(temp_dir)
        return temp_dir


class SimpleWrapperTest(unittest.TestCase):

    def test_exit_code(self):
        rc = subprocess.call(["simple_wrapper.py", "/bin/sh", "-c", "exit 42"])
        self.assertEquals(rc, 42)

    def test_signal_exit_code(self):
        rc = subprocess.call(["simple_wrapper.py", "/bin/sh", "-c", "kill $$"])
        self.assertEquals(rc, 127)


class TestWrapperTest(TempDirTestCase):

    def test_exit_code(self):
        rc = subprocess.call(["test_wrapper.py", "/bin/sh", "-c", "exit 42"])
        self.assertEquals(rc, 42)

    def test_signal_exit_code(self):
        proc = subprocess.Popen(["test_wrapper.py", "/bin/sh", "-c", "kill $$"],
                                stderr=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(stderr, "process killed by signal 15 (SIGTERM)\n")
        self.assertEquals(proc.wait(), 127)

    def test_invoking_static_executable(self):
        # This only works with run-uninstalled.sh (when not using the
        # chroot jail) because the static executable is invoked
        # directly.  TODO: invoke executable via the chainloader.
        if "PLASH_SANDBOX_PROG" not in os.environ:
            return

        temp_dir = self.make_temp_dir()
        write_file(os.path.join(temp_dir, "hello.c"), """
#include <stdio.h>
int main()
{
  printf("hello\\n");
  return 12;
}
""")
        rc = subprocess.call(
            ["gcc", "-static", os.path.join(temp_dir, "hello.c"),
             "-o", os.path.join(temp_dir, "hello")])
        self.assertEquals(rc, 0)
        proc = subprocess.Popen(
            ["test_wrapper.py",
             "/bin/bash", "-c", os.path.join(temp_dir, "hello")],
            stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(proc.wait(), 12)
        self.assertEquals(stdout, "hello\n")


if __name__ == "__main__":
    unittest.main()
