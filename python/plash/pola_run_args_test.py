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


class TestFlags(unittest.TestCase):

    def test(self):
        self.assertEqual(pola_run_args.split_flags(""),
                         [])
        self.assertEqual(pola_run_args.split_flags("al,objrw,foo"),
                         ["a", "l", "objrw", "foo"])


class TestEnvironment(unittest.TestCase):

    def setUp(self):
        self.tmp_dirs = []
        self.start_cwd = os.getcwd()

    def tearDown(self):
        for tmp_dir in self.tmp_dirs:
            shutil.rmtree(tmp_dir)
        # Restoring cwd only needed while FsObjReal corrupts cwd
        os.chdir(self.start_cwd)

    def get_temp_dir(self):
        dir_path = tempfile.mkdtemp(prefix="plash-test")
        self.tmp_dirs.append(dir_path)
        return plash.env.get_dir_from_path(dir_path)

    def test_environment_from_tmp(self):
        proc = plash.process.ProcessSpecWithNamespace()
        setup = pola_run_args.ProcessSetup(proc)
        setup._make_temp_dir = self.get_temp_dir
        setup.handle_args(["--tmp"])
        root_dir = proc.get_namespace().get_root_dir()
        tmpdir = plash.namespace.resolve_obj(root_dir, "/tmp")
        tmpdir.dir_create_file(os.O_WRONLY, 0666, "temp-file")

    def test_environment_from_tmpdir(self):
        proc = plash.process.ProcessSpecWithNamespace()
        setup = pola_run_args.ProcessSetup(proc)
        setup._make_temp_dir = self.get_temp_dir
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


if __name__ == '__main__':
    unittest.main()
