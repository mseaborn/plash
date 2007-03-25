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

class ProcessWithLogging(plash.process.ProcessSpecWithNamespace):
    """Process constructor that logs the calls the process makes."""

    def __init__(self, call_list):
        self._call_list = call_list
        super(ProcessWithLogging, self).__init__()

    def make_fs_op(self, root_dir, logger):
        fs_op = super(ProcessWithLogging, self).\
                make_fs_op(root_dir, logger)
        return LogProxy(fs_op, self._call_list)

class ProcessPreloadMixin(object):

    def _set_up_library_path(self):
        build_dir = os.environ["PLASH_BUILD_DIR"]
        self.env["LD_PRELOAD"] = \
            os.path.join(build_dir, "shobj/preload-libc.so")

    def _set_up_sandbox_prog(self):
        # This is a hack to restore the cwd.
        # Needed because cwd gets corrupted.
        # Not bad to set cwd, but it should be done in the
        # forked process instead.
        os.chdir(self.cwd_path)


class LibcTest(unittest.TestCase):

    entry = "test_base"
    code = r"""
void test_base()
{
}
"""
    main_args = []

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
            rc = subprocess.call(["../test-case"] + self.main_args)
            assert rc == 0
        self._test_main(run)

    def test_pola_run(self):
        def run():
            rc = subprocess.call(["pola-run", "-B", "-fw=.", "-f=../test-case",
                                  "-e", "../test-case"] + self.main_args)
            assert rc == 0
        self._test_main(run)

    def _run_plash_process(self, proc):
        proc.cwd_path = os.getcwd()
        proc.env = os.environ.copy()
        state = plash.pola_run_args.ProcessSetup(proc)
        state.caller_root = plash.env.get_root_dir()
        state.cwd = ns.resolve_dir(state.caller_root, proc.cwd_path)
        state.handle_args(["-B", "-fw=.", "-f=../test-case",
                           "-e", "../test-case"] + self.main_args)
        if "PLASH_LIBRARY_DIR" in os.environ:
            state.handle_args(["-f", os.environ["PLASH_LIBRARY_DIR"]])
        pid = proc.spawn()
        plash.mainloop.run_server()
        pid2, status = os.wait()
        self.assertEquals(pid, pid2)
        self.assertTrue(os.WIFEXITED(status))
        self.assertEquals(os.WEXITSTATUS(status), 0)

    def test_library(self):
        def run():
            self._method_calls = []
            proc = ProcessWithLogging(self._method_calls)
            self._run_plash_process(proc)
        self._test_main(run)
        self.check()

    def test_preload_library(self):
        def run():
            self._method_calls = []
            class Proc(ProcessPreloadMixin, ProcessWithLogging):
                pass
            proc = Proc(self._method_calls)
            self._run_plash_process(proc)
        self._test_main(run)
        self.check()

    def assertCalled(self, method_name, *args):
        self.assertTrue((method_name, args) in self._method_calls,
                        self._method_calls)

    def assertCalledMatches(self, method_name, fun):
        for method, args in self._method_calls:
            if method == method_name and fun(args):
                return
        self.fail("No match found for %s" % method_name)

    def assertNotCalled(self, method_name):
        for method, args in self._method_calls:
            if method == method_name:
                self.fail("Method %s was called" % method_name)


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


class TestUnlink(LibcTest):
    entry = "test_unlink"
    code = r"""
#include <fcntl.h>
#include <unistd.h>
void test_unlink()
{
  int fd = creat("file", 0777);
  t_check(fd >= 0);
  t_check_zero(close(fd));
  t_check_zero(unlink("file"));
}
"""
    def check(self):
        self.assertCalled("fsop_unlink", "file")


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


class TestRmdir(LibcTest):
    entry = "test_rmdir"
    code = r"""
#include <sys/stat.h>
#include <unistd.h>
void test_rmdir()
{
  t_check_zero(mkdir("dir", 0777));
  t_check_zero(rmdir("dir"));
}
"""
    def check(self):
        self.assertCalled("fsop_rmdir", "dir")


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


class TestChmod(LibcTest):
    entry = "test_chmod"
    code = r"""
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
void test_chmod()
{
  int fd = creat("file", 0666);
  t_check(fd >= 0);
  t_check_zero(close(fd));
  /* Set the executable bit. */
  t_check_zero(chmod("file", 0777));
}
"""
    def check(self):
        self.assertCalled("fsop_chmod", 0, 0777, "file")


class TestChown(LibcTest):
    entry = "test_chown"
    code = r"""
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
void test_chown()
{
  int fd = creat("file", 0666);
  t_check(fd >= 0);
  t_check_zero(close(fd));
  /* Not attempting to change the owner/group. */
  t_check_zero(chown("file", -1, -1));
}
"""
    def check(self):
        self.assertCalled("fsop_chown", 0, -1, -1, "file")


class TestFork(LibcTest):
    entry = "test_fork"
    code = r"""
#include <unistd.h>
#include <sys/wait.h>
void test_fork()
{
  int pid, pid2, status;
  pid = fork();
  t_check(pid >= 0);
  if(pid == 0)
    _exit(42);
  pid2 = wait(&status);
  t_check(pid2 >= 0);
  assert(pid == pid2);
  assert(WIFEXITED(status) && WEXITSTATUS(status) == 42);
}
"""
    def check(self):
        self.assertCalled("fsop_copy")


class TestBind(LibcTest):
    entry = "test_bind"
    code = r"""
#include <sys/socket.h>
#include <sys/un.h>
void test_bind()
{
  int fd;
  struct sockaddr_un addr;

  fd = socket(PF_UNIX, SOCK_STREAM, 0);
  t_check(fd >= 0);
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, "socket");
  t_check_zero(bind(fd, (struct sockaddr *) &addr,
                    sizeof(struct sockaddr_un)));
}
"""
    def check(self):
        self.assertCalledMatches("fsop_bind",
                                 lambda (fd, name): name == "socket")


class TestBindAbstract(LibcTest):
    """Use bind() with the Linux-specific "abstract" socket namespace."""
    entry = "test_bind_abstract"
    code = r"""
#include <sys/socket.h>
#include <sys/un.h>
void test_bind_abstract()
{
  int fd;
  struct sockaddr_un addr;

  fd = socket(PF_UNIX, SOCK_STREAM, 0);
  t_check(fd >= 0);
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  addr.sun_path[0] = '\0';
  strcpy(addr.sun_path + 1, "/tmp/test-socket");
  t_check_zero(bind(fd, (struct sockaddr *) &addr,
                    sizeof(struct sockaddr_un)));
}
"""
    def check(self):
        self.assertNotCalled("fsop_bind")


class TestRename(LibcTest):
    entry = "test_rename"
    code = r"""
#include <stdio.h>
void test_rename()
{
  int fd = creat("orig_file", 0777);
  t_check(fd >= 0);
  t_check_zero(close(fd));
  t_check_zero(rename("orig_file", "new_file"));
}
"""
    def check(self):
        # FIXME: argument ordering is confusing
        self.assertCalled("fsop_rename", "new_file", "orig_file")


class TestHardLink(LibcTest):
    entry = "test_hard_link"
    code = r"""
#include <fcntl.h>
#include <unistd.h>
void test_hard_link()
{
  int fd = creat("orig_file", 0777);
  t_check(fd >= 0);
  t_check_zero(close(fd));
  t_check_zero(link("orig_file", "new_file"));
}
"""
    def check(self):
        # FIXME: argument ordering is confusing
        self.assertCalled("fsop_link", "new_file", "orig_file")


def get_test_suite(module):
    cases = [x for x in module.__dict__.values()
             if isinstance(x, type) and issubclass(x, LibcTest)]
    entry_points = [case.entry for case in cases]
    code = ""
    for case in cases:
        code += case.code
    code += r"""
struct test_case {
  const char *name;
  void (*func)(void);
};
"""
    code += ("struct test_case test_cases[] = { %s, { NULL, NULL } };\n"
             % ", ".join(["""{ "%s", %s }""" % (name, name)
                          for name in entry_points]))
    main_func = r"""
int main(int argc, char **argv)
{
  struct test_case *test_case;
  assert(argc == 2);
  for(test_case = test_cases; test_case->name != NULL; test_case++) {
    if(strcmp(test_case->name, argv[1]) == 0) {
      test_case->func();
      return 0;
    }
  }
  return 1;
}
"""
    write_file("test-cases.c", prototypes + code + shared_code + main_func)
    rc = subprocess.call(["gcc", "-Wall", "test-cases.c", "-o", "test-case"])
    assert rc == 0

    suite = unittest.TestSuite()
    for case in cases:
        class Case(case):
            def setUp(self):
                pass
            main_args = [case.entry]
        suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(Case))
    return suite

def run_tests(suite):
    runner = unittest.TextTestRunner(verbosity=1)
    runner.run(suite)


if __name__ == "__main__":
    run_tests(get_test_suite(__import__("__main__")))
