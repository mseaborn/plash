
import os
import shutil
import tempfile
import unittest

import plash_core
import plash.marshal
import plash.namespace


class TestDirMixin(object):

    def setUp(self):
        self._tmp_dirs = []

    def get_real_temp_dir(self):
        dir_path = tempfile.mkdtemp(prefix="plash-test")
        self._tmp_dirs.append(dir_path)
        return plash_core.initial_dir(dir_path)

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
