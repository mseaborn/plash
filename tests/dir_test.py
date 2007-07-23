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
import socket
import tempfile
import unittest

import plash_core
import plash.env
import plash.marshal
import plash.marshal as marshal
import plash.namespace


def make_unix_socket():
    sock = socket.socket(socket.AF_UNIX)
    return plash_core.wrap_fd(os.dup(sock.fileno()))


class FsObjRealTests(unittest.TestCase):

    def setUp(self):
        self.dir_path = tempfile.mkdtemp(prefix="plash-test")
        fh = open(os.path.join(self.dir_path, "file"), "w")
        fh.close()
        os.symlink("dest_path", os.path.join(self.dir_path, "symlink"))
        os.mkdir(os.path.join(self.dir_path, "subdir"))
        self.dir_obj = plash.env.get_dir_from_path(self.dir_path)

    def tearDown(self):
        shutil.rmtree(self.dir_path)

    def assert_cwd_unchanged(self, func):
        saved_dir_fd = os.open(".", os.O_RDONLY)
        try:
            cwd_path_before = os.getcwd()
            func()
            cwd_path_after = os.getcwd()
            self.assertEquals(cwd_path_before, cwd_path_after)
        finally:
            # Restore cwd so that other tests can run normally.
            # getcwd() fails if the cwd has been deleted.
            os.fchdir(saved_dir_fd)
            os.close(saved_dir_fd)

    def test_type(self):
        self.assertEquals(self.dir_obj.fsobj_type(), marshal.OBJT_DIR)

    def test_traverse_file(self):
        def f():
            obj = self.dir_obj.dir_traverse("file")
            self.assertEquals(obj.fsobj_type(), marshal.OBJT_FILE)
        self.assert_cwd_unchanged(f)

    def test_traverse_dir(self):
        def f():
            obj = self.dir_obj.dir_traverse("subdir")
            self.assertEquals(obj.fsobj_type(), marshal.OBJT_DIR)
        self.assert_cwd_unchanged(f)

    def test_traverse_symlink(self):
        def f():
            obj = self.dir_obj.dir_traverse("symlink")
            self.assertEquals(obj.fsobj_type(), marshal.OBJT_SYMLINK)
        self.assert_cwd_unchanged(f)

    def test_traverse_nonexistent(self):
        def f():
            # FIXME: should return or raise something reasonable
            self.assertRaises(
                marshal.UnmarshalError,
                lambda: self.dir_obj.dir_traverse("notexist"))
        self.assert_cwd_unchanged(f)

    def test_traverse_dot_path(self):
        self.assertRaises(
            marshal.UnmarshalError,
            lambda: self.dir_obj.dir_traverse("."))

    def test_traverse_dot_dot_path(self):
        self.assertRaises(
            marshal.UnmarshalError,
            lambda: self.dir_obj.dir_traverse(".."))

    def test_file_open(self):
        def f():
            self.dir_obj.dir_traverse("file").file_open(os.O_RDONLY)
        self.assert_cwd_unchanged(f)

    def test_symlink_readlink(self):
        def f():
            self.assertEquals(
                self.dir_obj.dir_traverse("symlink").symlink_readlink(),
                "dest_path")
        self.assert_cwd_unchanged(f)

    def test_dir_list(self):
        def f():
            listing = self.dir_obj.dir_list()
            self.assertEquals(set([entry["name"] for entry in listing]),
                              set(["file", "subdir", "symlink"]))
        self.assert_cwd_unchanged(f)

    def test_multiple_dir_list(self):
        # Re-using a directory FD without resetting its position can
        # potentially cause dir_list to return an empty list.
        def f():
            listing = self.dir_obj.dir_list()
            self.assertEquals(set([entry["name"] for entry in listing]),
                              set(["file", "subdir", "symlink"]))
        self.assert_cwd_unchanged(f)
        self.assert_cwd_unchanged(f)

    def test_dir_create_file(self):
        def f():
            self.dir_obj.dir_create_file(os.O_WRONLY, 0666, "new_file")
        self.assert_cwd_unchanged(f)

    def test_dir_mkdir(self):
        def f():
            self.dir_obj.dir_mkdir(0666, "test_dir")
        self.assert_cwd_unchanged(f)

    def test_dir_symlink(self):
        def f():
            self.dir_obj.dir_symlink("test_symlink", "dest_path")
        self.assert_cwd_unchanged(f)

    def test_dir_unlink(self):
        def f():
            self.dir_obj.dir_unlink("file")
        self.assert_cwd_unchanged(f)

    def test_dir_rmdir(self):
        def f():
            self.dir_obj.dir_rmdir("subdir")
        self.assert_cwd_unchanged(f)

    def test_dir_socket_bind(self):
        def f():
            self.dir_obj.dir_socket_bind("socket", make_unix_socket())
        self.assert_cwd_unchanged(f)

    def test_dir_socket_connect(self):
        def f():
            sock = socket.socket(socket.AF_UNIX)
            fd = plash_core.wrap_fd(os.dup(sock.fileno()))
            self.dir_obj.dir_socket_bind("socket", fd)
            sock.listen(1)
            self.dir_obj.dir_traverse("socket").file_socket_connect(
                make_unix_socket())
        self.assert_cwd_unchanged(f)

    def test_dir_socket_connect_on_non_socket(self):
        def f():
            self.assertRaises(
                marshal.UnmarshalError,
                lambda: self.dir_obj.dir_traverse("file").
                            file_socket_connect(make_unix_socket()))
        self.assert_cwd_unchanged(f)

    def test_file_chmod(self):
        def f():
            self.dir_obj.dir_traverse("file").fsobj_chmod(0777)
        self.assert_cwd_unchanged(f)

    def test_dir_chmod(self):
        def f():
            self.dir_obj.fsobj_chmod(0777)
        self.assert_cwd_unchanged(f)

    def test_file_utimes(self):
        def f():
            self.dir_obj.dir_traverse("file").fsobj_utimes(123, 456, 789, 123)
        self.assert_cwd_unchanged(f)

    def test_dir_utimes(self):
        def f():
            self.dir_obj.fsobj_utimes(123, 456, 789, 123)
        self.assert_cwd_unchanged(f)

    def test_rename(self):
        def f():
            self.dir_obj.dir_rename("file", self.dir_obj, "file2")
        self.assert_cwd_unchanged(f)

    def test_link(self):
        def f():
            self.dir_obj.dir_link("file", self.dir_obj, "file2")
        self.assert_cwd_unchanged(f)


class TestDirMixin(object):

    def setUp(self):
        self._tmp_dirs = []

    def get_real_temp_dir(self):
        dir_path = tempfile.mkdtemp(prefix="plash-test")
        self._tmp_dirs.append(dir_path)
        return plash.env.get_dir_from_path(dir_path)

    def tearDown(self):
        for tmp_dir in self._tmp_dirs:
            shutil.rmtree(tmp_dir)

    def check_dir_listing(self, dir, list_expect):
        list_got = [item['name'] for item in dir.dir_list()]
        list_got.sort()
        list_expect.sort()
        self.assertEquals(list_got, list_expect)

    def check_utimes(self, fun):
        "Set the object's atime/mtime and read them back"
        # NB. Currently stat information is cached.
        # We work around this by re-getting the object.
        fun().fsobj_utimes(123456789, 0, 234567890, 0)
        stat = fun().fsobj_stat()
        self.assertEquals(stat['st_atime'], 123456789)
        self.assertEquals(stat['st_mtime'], 234567890)

    def check_empty_writable_dir(self, dir, recurse=True):
        # Check directory listing - should be empty to start
        list = []
        self.check_dir_listing(dir, list)

        # Create a file

        # FIXME: arg ordering
        fd = dir.dir_create_file(os.O_WRONLY, 0777, "test-file")
        # Close the file descriptor.  On NFS, if a file is deleted without
        # being closed, NFS shows a temporary file in the directory listing,
        # with a name like ".nfs021d13370001a79f", which stops the listing
        # checks from succeeding.
        del fd
        list.append("test-file")
        self.check_dir_listing(dir, list)

        file = dir.dir_traverse("test-file")
        assert(file.fsobj_type(), plash.marshal.OBJT_FILE)

        # Create a subdirectory

        # FIXME: arg ordering
        dir.dir_mkdir(0777, "test-dir")
        list.append("test-dir")
        self.check_dir_listing(dir, list)

        subdir = dir.dir_traverse("test-dir")
        self.assertEquals(subdir.fsobj_type(), plash.marshal.OBJT_DIR)
        self.check_utimes(lambda: dir.dir_traverse("test-dir"))

        # Does the subdirectory also pass this test?
        if recurse:
            self.check_empty_writable_dir(subdir, recurse=False)

        # Create a symlink

        dir.dir_symlink("test-symlink", "destination-path")
        list.append("test-symlink")
        self.check_dir_listing(dir, list)

        symlink = dir.dir_traverse("test-symlink")
        self.assertEquals(symlink.fsobj_type(), plash.marshal.OBJT_SYMLINK)
        self.assertEquals(symlink.symlink_readlink(), "destination-path")

        # Remove the symlink
        dir.dir_unlink("test-symlink")
        list.remove("test-symlink")
        self.check_dir_listing(dir, list)

        # Remove the file
        dir.dir_unlink("test-file")
        list.remove("test-file")
        self.check_dir_listing(dir, list)

        # Remove the subdirectory
        dir.dir_rmdir("test-dir")
        list.remove("test-dir")
        self.check_dir_listing(dir, list)

        self.assertEquals(list, [])

    def test_dir(self):
        self.check_empty_writable_dir(self.get_temp_dir())

    def test_same_dir_rename(self):
        dir = self.get_temp_dir()
        fd = dir.dir_create_file(os.O_WRONLY, 0666, "file")
        stat1 = dir.dir_traverse("file").fsobj_stat()
        dir.dir_rename("file", dir, "dest")
        stat2 = dir.dir_traverse("dest").fsobj_stat()
        self.assertEquals(stat1, stat2)


class TestRealDir(TestDirMixin, unittest.TestCase):

    def get_temp_dir(self):
        return self.get_real_temp_dir()

    def test_cross_dir_hard_link(self):
        dir1 = self.get_temp_dir()
        dir2 = self.get_temp_dir()
        fd = dir1.dir_create_file(os.O_WRONLY, 0666, "file")
        dir1.dir_link("file", dir2, "dest")
        stat1 = dir1.dir_traverse("file").fsobj_stat()
        stat2 = dir2.dir_traverse("dest").fsobj_stat()
        self.assertEquals(stat1, stat2)

    def test_cross_dir_rename(self):
        dir1 = self.get_temp_dir()
        dir2 = self.get_temp_dir()
        fd = dir1.dir_create_file(os.O_WRONLY, 0666, "file")
        stat1 = dir1.dir_traverse("file").fsobj_stat()
        dir1.dir_rename("file", dir2, "dest")
        stat2 = dir2.dir_traverse("dest").fsobj_stat()
        self.assertEquals(stat1, stat2)


class TestCowDir(TestDirMixin, unittest.TestCase):

    def get_temp_dir(self):
        dir1 = self.get_real_temp_dir()
        dir2 = self.get_real_temp_dir()
        return plash.namespace.make_cow_dir(dir1, dir2)

    def test_cow_dir(self):
        read = self.get_real_temp_dir()
        write = self.get_real_temp_dir()

        # Set up the initial state of the read-only and read/write layers
        read.dir_mkdir(0777, "mydir")
        read.dir_traverse("mydir").dir_mkdir(0777, "dir1")
        read.dir_mkdir(0777, "only-going-to-be-in-read")
        write.dir_mkdir(0777, "only-in-write")

        # Now create the COW directory...
        cow_dir = plash.namespace.make_cow_dir(write, read)
        self.assertEquals(cow_dir.fsobj_type(), plash.marshal.OBJT_DIR)

        # Traverse this but never create anything in it
        cow_dir.dir_traverse("only-going-to-be-in-read")

        self.check_utimes(lambda: cow_dir.dir_traverse("mydir"))

        subdir = cow_dir.dir_traverse("mydir")
        self.assertEquals(subdir.fsobj_type(), plash.marshal.OBJT_DIR)
        # This will realize mydir
        subdir.dir_mkdir(0777, "dir2")
        # This uses already-realized mydir
        subdir.dir_mkdir(0777, "dir3")

        self.check_dir_listing(subdir, ["dir1", "dir2", "dir3"])

        subdir = cow_dir.dir_traverse("only-in-write")
        self.assertEquals(subdir.fsobj_type(), plash.marshal.OBJT_DIR)
        subdir.dir_mkdir(0777, "dir4")


if __name__ == "__main__":
    unittest.main()
