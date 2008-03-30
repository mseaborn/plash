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
import shutil
import tempfile
import unittest

import plash.env
import plash.namespace
import plash.pola_run_args as pola_run_args
import plash.process


def write_file(filename, data):
    fh = open(filename, "w")
    try:
        fh.write(data)
    finally:
        fh.close()


class TestFlags(unittest.TestCase):

    def test(self):
        self.assertEqual(pola_run_args.split_flags(""),
                         [])
        self.assertEqual(pola_run_args.split_flags("al,objrw,foo"),
                         ["a", "l", "objrw", "foo"])


class TestEnvironment(unittest.TestCase):

    def setUp(self):
        self.tmp_dirs = []

    def tearDown(self):
        for tmp_dir in self.tmp_dirs:
            shutil.rmtree(tmp_dir)

    def make_temp_dir(self):
        dir_path = tempfile.mkdtemp(prefix="plash-test")
        self.tmp_dirs.append(dir_path)
        return dir_path

    def make_temp_dirobj(self):
        return plash.env.get_dir_from_path(self.make_temp_dir())

    def make_temp_file(self, data):
        filename = os.path.join(self.make_temp_dir(), "tempfile")
        write_file(filename, data)
        return filename

    def test_environment_from_tmp(self):
        proc = plash.process.ProcessSpecWithNamespace()
        setup = pola_run_args.ProcessSetup(proc)
        setup._make_temp_dir = self.make_temp_dirobj
        setup.handle_args(["--tmp"])
        root_dir = proc.get_namespace().get_root_dir()
        tmpdir = plash.namespace.resolve_obj(root_dir, "/tmp")
        tmpdir.dir_create_file(os.O_WRONLY, 0666, "temp-file")

    def test_environment_from_tmpdir(self):
        proc = plash.process.ProcessSpecWithNamespace()
        setup = pola_run_args.ProcessSetup(proc)
        setup._make_temp_dir = self.make_temp_dirobj
        setup.handle_args(["--tmpdir", "/foo"])
        root_dir = proc.get_namespace().get_root_dir()
        tmpdir = plash.namespace.resolve_obj(root_dir, "/foo")
        tmpdir.dir_create_file(os.O_WRONLY, 0666, "temp-file")

    def test_help_arg(self):
        setup = pola_run_args.ProcessSetup(None)
        self.assertRaises(pola_run_args.UsageException,
                          lambda: setup.handle_args(["--help"]))
        self.assertRaises(pola_run_args.UsageException,
                          lambda: setup.handle_args(["-h"]))

    def test_grant_x11_access(self):
        xauthority_file = self.make_temp_file("auth cookies")
        env = {"DISPLAY": ":123",
               "XAUTHORITY": xauthority_file}
        proc = plash.process.ProcessSpecWithNamespace()
        setup = pola_run_args.ProcessSetup(proc, parent_environ=env)
        setup.caller_root = plash.env.get_root_dir()
        setup.handle_args(["--x11"])
        self.assertEquals(proc.env["DISPLAY"], ":123")
        self.assertEquals(proc.env["XAUTHORITY"], xauthority_file)

        root_dir = proc.get_namespace().get_root_dir()
        fd = plash.namespace.resolve_obj(root_dir, xauthority_file).\
            file_open(os.O_RDONLY)
        self.assertEquals(os.fdopen(fd.fileno()).read(), "auth cookies")

    def test_grant_x11_access_ssh_case(self):
        home_dir = self.make_temp_dir()
        xauthority_file = os.path.join(home_dir, ".Xauthority")
        write_file(xauthority_file, "auth cookies")
        env = {"DISPLAY": ":123",
               "HOME": home_dir}
        proc = plash.process.ProcessSpecWithNamespace()
        setup = pola_run_args.ProcessSetup(proc, parent_environ=env)
        setup.caller_root = plash.env.get_root_dir()
        setup.handle_args(["--x11"])
        self.assertEquals(proc.env["DISPLAY"], ":123")
        self.assertTrue("XAUTHORITY" not in env)

        # SSH does not set XAUTHORITY.  But the file should still get
        # granted by pola-run.
        root_dir = proc.get_namespace().get_root_dir()
        fd = plash.namespace.resolve_obj(root_dir, xauthority_file).\
            file_open(os.O_RDONLY)
        self.assertEquals(os.fdopen(fd.fileno()).read(), "auth cookies")

    def test_grant_x11_access_without_x11(self):
        env = {}
        proc = plash.process.ProcessSpecWithNamespace()
        setup = pola_run_args.ProcessSetup(proc, parent_environ=env)
        setup.caller_root = self.make_temp_dirobj()
        setup.handle_args(["--x11"])
        self.assertTrue("DISPLAY" not in proc.env)
        self.assertTrue("XAUTHORITY" not in proc.env)

    def test_grant_x11_access_without_x11_with_home(self):
        env = {"HOME": self.make_temp_dir()}
        proc = plash.process.ProcessSpecWithNamespace()
        setup = pola_run_args.ProcessSetup(proc, parent_environ=env)
        setup.caller_root = self.make_temp_dirobj()
        setup.handle_args(["--x11"])
        self.assertTrue("DISPLAY" not in proc.env)
        self.assertTrue("XAUTHORITY" not in proc.env)


if __name__ == '__main__':
    unittest.main()
