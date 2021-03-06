
import os
import plash_core
import plash.marshal as pm


def assert_equal(got, expect):
    if got != expect:
        print "MISMATCH:"
        print "GOT:\n%s" % got
        print "EXPECTED:\n%s" % expect
        raise AssertionError


def check_dir_listing(dir, list_expect):
    list_got = [item['name'] for item in dir.dir_list()]
    list_got.sort()
    list_expect.sort()
    assert_equal(list_got, list_expect)


def test_utimes(fun):
    "Set the object's atime/mtime and read them back"
    # NB. Currently stat information is cached.
    # We work around this by re-getting the object.
    fun().fsobj_utimes(123456789, 0, 234567890, 0)
    stat = fun().fsobj_stat()
    assert_equal(stat['st_atime'], 123456789)
    assert_equal(stat['st_mtime'], 234567890)


def test_empty_writable_dir(dir, recurse=True):
    
    # Check directory listing - should be empty to start
    list = []
    check_dir_listing(dir, list)

    # Create a file

    # FIXME: arg ordering
    fd = dir.dir_create_file(os.O_WRONLY, 0777, "test-file")
    # Close the file descriptor.  On NFS, if a file is deleted without
    # being closed, NFS shows a temporary file in the directory listing,
    # with a name like ".nfs021d13370001a79f", which stops the listing
    # checks from succeeding.
    del fd
    list.append("test-file")
    check_dir_listing(dir, list)

    file = dir.dir_traverse("test-file")
    assert(file.fsobj_type(), pm.OBJT_FILE)

    # Create a subdirectory

    # FIXME: arg ordering
    dir.dir_mkdir(0777, "test-dir")
    list.append("test-dir")
    check_dir_listing(dir, list)

    subdir = dir.dir_traverse("test-dir")
    assert_equal(subdir.fsobj_type(), pm.OBJT_DIR)
    test_utimes(lambda: dir.dir_traverse("test-dir"))

    # Does the subdirectory also pass this test?
    if recurse:
        test_empty_writable_dir(subdir, recurse=False)

    # Create a symlink

    dir.dir_symlink("test-symlink", "destination-path")
    list.append("test-symlink")
    check_dir_listing(dir, list)

    symlink = dir.dir_traverse("test-symlink")
    assert_equal(symlink.fsobj_type(), pm.OBJT_SYMLINK)
    assert_equal(symlink.symlink_readlink(), "destination-path")

    # Remove the symlink
    dir.dir_unlink("test-symlink")
    list.remove("test-symlink")
    check_dir_listing(dir, list)

    # Remove the file
    dir.dir_unlink("test-file")
    list.remove("test-file")
    check_dir_listing(dir, list)

    # Remove the subdirectory
    dir.dir_rmdir("test-dir")
    list.remove("test-dir")
    check_dir_listing(dir, list)
    
    assert_equal(list, [])


def test_cow_dir(write, read):

    # Set up the initial state of the read-only and read/write layers
    read.dir_mkdir(0777, "mydir")
    read.dir_traverse("mydir").dir_mkdir(0777, "dir1")
    read.dir_mkdir(0777, "only-going-to-be-in-read")
    write.dir_mkdir(0777, "only-in-write")

    # Now create the COW directory...
    cow_dir = ns.make_cow_dir(write, read)
    assert_equal(cow_dir.fsobj_type(), pm.OBJT_DIR)

    # Traverse this but never create anything in it
    cow_dir.dir_traverse("only-going-to-be-in-read")
    
    test_utimes(lambda: cow_dir.dir_traverse("mydir"))

    subdir = cow_dir.dir_traverse("mydir")
    assert_equal(subdir.fsobj_type(), pm.OBJT_DIR)
    # This will realize mydir
    subdir.dir_mkdir(0777, "dir2")
    # This uses already-realized mydir
    subdir.dir_mkdir(0777, "dir3")

    check_dir_listing(subdir, ["dir1", "dir2", "dir3"])

    subdir = cow_dir.dir_traverse("only-in-write")
    assert_equal(subdir.fsobj_type(), pm.OBJT_DIR)
    subdir.dir_mkdir(0777, "dir4")


fd = os.open(".", os.O_RDONLY)

dir = plash_core.initial_dir(".")
test_empty_writable_dir(dir)
print "ok"

import plash.namespace as ns
os.fchdir(fd)
os.mkdir("write")
os.mkdir("read")
test_cow_dir(plash_core.initial_dir("write"),
             plash_core.initial_dir("read"))
print "ok"
