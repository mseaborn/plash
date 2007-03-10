
import os
import shutil
import subprocess
import unittest

import plash.env
import plash.mainloop
import plash.namespace as ns
import plash.pola_run_args
import plash.process


prototypes = r"""
#include <assert.h>

void t_check_zero(int return_code);
void t_check(int x);
"""

shared_code = r"""
#include <stdio.h>
#include <stdlib.h>

void t_check_zero(int return_code)
{
  if(return_code != 0) {
    perror("system call");
    exit(1);
  }
}

void t_check(int x)
{
  if(!x) {
    perror("system call");
    exit(1);
  }
}
"""


def write_file(filename, data):
    fh = open(filename, "w")
    try:
        fh.write(data)
    finally:
        fh.close()


class LogProxy(plash.marshal.Pyobj_marshal):
    """Logging proxy that records method calls made."""

    def __init__(self, obj, call_list):
        self._obj = obj
        self._calls = call_list

    def cap_call(self, args):
        self._calls.append(plash.marshal.unpack(args))
        return self._obj.cap_call(args)


class LibcTest(unittest.TestCase):

    entry = "test_base"
    code = r"""
void test_base()
{
}
"""

    def check(self):
        """Default"""
        pass

    def setUp(self):
        main_func = r"""
int main()
{
  %s();
  return 0;
}
""" % self.entry
        write_file("test-case.c", prototypes + self.code + shared_code + main_func)
        rc = subprocess.call(["gcc", "-Wall", "test-case.c", "-o", "test-case"])
        assert rc == 0
        self._method_calls = []

    def _test_main(self, f):
        if os.path.exists("tmp-dir"):
            shutil.rmtree("tmp-dir")
        os.mkdir("tmp-dir")
        start_dir = os.open(".", os.O_RDONLY)
        try:
            os.chdir("tmp-dir")
            f()
        finally:
            os.fchdir(start_dir)

    def test_native(self):
        def run():
            rc = subprocess.call(["../test-case"])
            assert rc == 0
        self._test_main(run)

    def test_pola_run(self):
        def run():
            rc = subprocess.call(["pola-run", "-B", "-fw=.", "-f=../test-case",
                                  "-e", "../test-case"])
            assert rc == 0
        self._test_main(run)

    def test_library(self):
        def run():
            calls = []
            class ProcessWithLogging(plash.process.Process_spec_ns):
                def make_fs_op(self, root_dir, logger):
                    fs_op = super(ProcessWithLogging, self).\
                            make_fs_op(root_dir, logger)
                    return LogProxy(fs_op, calls)
            class State:
                pass
            proc = ProcessWithLogging()
            proc.cwd_path = os.getcwd()
            proc.env = os.environ.copy()
            state = State()
            state.caller_root = plash.env.get_root_dir()
            state.cwd = ns.resolve_dir(state.caller_root, proc.cwd_path)
            plash.pola_run_args.handle_args(state, proc,
                                      ["-B", "-fw=.", "-f=../test-case",
                                       "--prog", "../test-case"])
            if "PLASH_LIBRARY_DIR" in os.environ:
                plash.pola_run_args.handle_args(
                    state, proc,
                    ["-f", os.environ["PLASH_LIBRARY_DIR"]])
            pid = proc.spawn()
            plash.mainloop.run_server()
            self._method_calls = [(name,) + args
                                  for name, args in calls]
            pid2, status = os.wait()
            self.assertEquals(pid, pid2)
            self.assertTrue(os.WIFEXITED(status))
            self.assertEquals(os.WEXITSTATUS(status), 0)
        self._test_main(run)
        self.check()

    def assertCalled(self, method_name, *args):
        self.assertTrue((method_name,) + args in self._method_calls,
                        self._method_calls)


class TestCreateFile(LibcTest):
    entry = "test_creat"
    code = r"""
#include <fcntl.h>
#include <unistd.h>
void test_creat()
{
  int fd = creat("file", 0777);
  t_check(fd >= 0);
  t_check_zero(close(fd));
}
"""
    def check(self):
        self.assertCalled("fsop_open", os.O_WRONLY | os.O_CREAT | os.O_TRUNC,
                          0777, "file")


class TestMkdir(LibcTest):
    entry = "test_mkdir"
    code = r"""
#include <sys/stat.h>
void test_mkdir()
{
  t_check_zero(mkdir("dir", 0777));
}
"""
    def check(self):
        self.assertCalled("fsop_mkdir", 0777, "dir")


class TestSymlink(LibcTest):
    entry = "test_symlink"
    code = r"""
#include <string.h>
#include <unistd.h>
void test_symlink()
{
  t_check_zero(symlink("dest_path", "symlink"));
}
"""
    def check(self):
        # TODO: order is reversed; confusing, should be fixed
        self.assertCalled("fsop_symlink", "symlink", "dest_path")


class TestReadlink(LibcTest):
    entry = "test_readlink"
    code = r"""
#include <string.h>
#include <unistd.h>
void test_readlink()
{
  const char *str = "dest_path";
  char buf[100];
  int got;

  t_check_zero(symlink(str, "symlink"));
  got = readlink("symlink", buf, sizeof(buf));
  t_check(got >= 0);
  assert(got == strlen(str));
  buf[got] = 0;
  assert(strcmp(buf, str) == 0);
}
"""
    def check(self):
        self.assertCalled("fsop_symlink", "symlink", "dest_path")
        self.assertCalled("fsop_readlink", "symlink")


class TestOpenOnDir(LibcTest):
    entry = "test_open_on_dir"
    code = r"""
#include <fcntl.h>
#include <unistd.h>
void test_open_on_dir()
{
  int fd = open(".", O_RDONLY);
  t_check(fd >= 0);
  t_check_zero(close(fd));
}
"""
    def check(self):
        self.assertCalled("fsop_open", os.O_RDONLY, 0, ".")


class TestGetcwd(LibcTest):
    entry = "test_getcwd"
    code = r"""
#include <stdlib.h>
#include <unistd.h>
void test_getcwd()
{
  char *str = getcwd(NULL, 0);
  t_check(str != NULL);
  free(str);
}
"""
    def check(self):
        self.assertCalled("fsop_getcwd")


class TestChdir(LibcTest):
    entry = "test_chdir"
    code = r"""
#include <sys/stat.h>
#include <unistd.h>
void test_chdir()
{
  t_check_zero(mkdir("dir", 0777));
  t_check_zero(chdir("dir"));
}
"""
    def check(self):
        self.assertCalled("fsop_chdir", "dir")


class TestReaddir(LibcTest):
    entry = "test_readdir"
    code = r"""
#include <dirent.h>
#include <stdlib.h>
void test_readdir()
{
  DIR *dir = opendir(".");
  t_check(dir != NULL);
  while(1) {
    struct dirent *ent = readdir(dir);
    if(ent == NULL)
      break;
  }
  t_check_zero(closedir(dir));
}
"""
    def check(self):
        self.assertCalled("fsop_dirlist", ".")


if __name__ == "__main__":
    unittest.main()
