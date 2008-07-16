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
import sys
import tempfile
import testrunner
import unittest

import plash_core
import plash.env
import plash.mainloop
import plash.namespace as ns
import plash.pola_run_args
import plash.process
import plash.process_test


prototypes = r"""
#include <assert.h>

#include "test-util.h"
"""


def write_file(filename, data):
    fh = open(filename, "w")
    try:
        fh.write(data)
    finally:
        fh.close()


class TempMaker(object):

    def __init__(self):
        self._dirs = []

    def make_temp_dir(self):
        tmp_dir = tempfile.mkdtemp(prefix="testcase-tmp")
        self._dirs.append(tmp_dir)
        return tmp_dir

    def destroy(self):
        for tmp_dir in self._dirs:
            shutil.rmtree(tmp_dir)


class LogProxy(plash.marshal.Pyobj_marshal):
    """Logging proxy that records method calls made."""

    def __init__(self, obj, call_list):
        self._obj = obj
        self._calls = call_list

    def fsop_copy(self):
        return LogProxy(self._obj.fsop_copy(), self._calls)

    def cap_call(self, args):
        method_name, args_unpacked = plash.marshal.unpack(args)
        self._calls.append((method_name, args_unpacked))
        if method_name == "fsop_copy":
            result = plash.marshal.pack("r_cap", self.fsop_copy())
        else:
            result = self._obj.cap_call(args)
        # Check that we can unpack the result
        plash.marshal.unpack(result)
        return result


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


class Wildcard(object):

    def matches(self, val):
        return True


class WildcardNotNone(Wildcard):

    def matches(self, val):
        return val != None


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
        self._temp_maker = TempMaker()
        self._executable = self._compile_executable()

    def tearDown(self):
        self._temp_maker.destroy()
        plash_core.cap_close_all_connections()

    def _compile_executable(self):
        tmp_dir = self._temp_maker.make_temp_dir()
        main_func = r"""
int main()
{
  %s();
  return 0;
}
""" % self.entry
        write_file(os.path.join(tmp_dir, "test-case.c"),
                   prototypes + self.code + main_func)
        src_dir = os.path.dirname(__file__)
        rc = subprocess.call(["gcc", "-Wall", "-D_GNU_SOURCE",
                              "-I%s" % src_dir,
                              os.path.join(src_dir, "test-util.c"),
                              os.path.join(tmp_dir, "test-case.c"),
                              "-o", os.path.join(tmp_dir, "test-case")])
        assert rc == 0
        return os.path.join(tmp_dir, "test-case")

    def _test_main(self, f):
        start_dir = os.open(".", os.O_RDONLY)
        tmp_dir = self._temp_maker.make_temp_dir()
        try:
            os.chdir(tmp_dir)
            f()
        finally:
            os.fchdir(start_dir)
            os.close(start_dir)

    def test_native(self):
        def run():
            rc = subprocess.call([self._executable] + self.main_args)
            assert rc == 0
        self._test_main(run)

    def test_pola_run(self):
        def run():
            rc = subprocess.call(["pola-run", "-B", "-fw=.",
                                  "-f", self._executable,
                                  "-e", self._executable] + self.main_args)
            assert rc == 0, rc
        self._test_main(run)

    def _run_plash_process(self, proc):
        tmp_dir = self._temp_maker.make_temp_dir()
        proc.cwd_path = tmp_dir
        proc.env = os.environ.copy()
        state = plash.pola_run_args.ProcessSetup(proc)
        state.caller_root = plash.env.get_root_dir()
        state.handle_args(["--fd", "1", "--fd", "2",
                           "-B", "-fw", tmp_dir,
                           "-f", self._executable,
                           "-e", self._executable] + self.main_args)
        if "PLASH_LIBRARY_DIR" in os.environ:
            state.handle_args(["-f", os.environ["PLASH_LIBRARY_DIR"]])
        pid = proc.spawn()
        plash.mainloop.run_server()
        pid2, status = os.waitpid(pid, 0)
        self.assertEquals(pid, pid2)
        plash.process_test.check_exit_status(status)

    def test_library(self):
        def run():
            self._method_calls = []
            proc = ProcessWithLogging(self._method_calls)
            self._run_plash_process(proc)
        self._test_main(run)
        self.check()

    def test_preload_library(self):
        old_ldso = os.environ.get("PLASH_LDSO_PATH", None)
        # FsOp uses this environment variable to specify the
        # executable that subprocesses should call execve() on.
        # When using the preload library, we want to use the installed
        # ld.so, not our built ld.so.
        # TODO: make this an argument of FsOp, instead of taking it
        # from an environment variable.
        os.environ["PLASH_LDSO_PATH"] = "/usr/bin/env"
        try:
            def run():
                self._method_calls = []
                class Proc(ProcessPreloadMixin, ProcessWithLogging):
                    pass
                proc = Proc(self._method_calls)
                self._run_plash_process(proc)
            self._test_main(run)
            self.check()
        finally:
            if old_ldso is None:
                del os.environ["PLASH_LDSO_PATH"]
            else:
                os.environ["PLASH_LDSO_PATH"] = old_ldso

    def assertCalled(self, method_name, *args):
        self.assertTrue((method_name, args) in self._method_calls,
                        self._method_calls)

    def _args_match(self, pattern, args):
        if len(pattern) != len(args):
            return False
        for x1, x2 in zip(pattern, args):
            if isinstance(x1, Wildcard):
                if not x1.matches(x2):
                    return False
            else:
                if x1 != x2:
                    return False
        return True

    def assertCalledPattern(self, method_name, *pattern_args):
        for method_name2, args2 in self._method_calls:
            if (method_name == method_name2 and
                self._args_match(pattern_args, args2)):
                return
        self.fail("No match found for %r in calls: %r" %
                  (method_name, self._method_calls))

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
        self.assertCalled("fsop_open", None,
                          os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0777, "file")


class TestOpenat(LibcTest):
    entry = "test_openat"
    code = r"""
#include <fcntl.h>
#include <unistd.h>
void test_openat()
{
  int fd = openat(get_dir_fd("."), "file", O_CREAT | O_WRONLY, 0777);
  t_check(fd >= 0);
  t_check_zero(close(fd));
}
"""
    def check(self):
        self.assertCalledPattern(
            "fsop_open", WildcardNotNone(), os.O_CREAT | os.O_WRONLY,
            0777, "file")


class TestOpen64(LibcTest):
    entry = "test_open64"
    code = r"""
#include <fcntl.h>
#include <unistd.h>
void test_open64()
{
  int fd = open64("file", O_CREAT | O_WRONLY, 0777);
  t_check(fd >= 0);
  t_check_zero(close(fd));
}
"""
    def check(self):
        self.assertCalled(
            "fsop_open", None,
            os.O_WRONLY | os.O_CREAT | os.O_LARGEFILE, 0777, "file")


class TestOpenat64(LibcTest):
    entry = "test_openat64"
    code = r"""
#include <fcntl.h>
#include <unistd.h>
void test_openat64()
{
  int fd = openat64(get_dir_fd("."), "file", O_CREAT | O_WRONLY, 0777);
  t_check(fd >= 0);
  t_check_zero(close(fd));
}
"""
    def check(self):
        self.assertCalledPattern(
            "fsop_open", WildcardNotNone(),
            os.O_CREAT | os.O_WRONLY | os.O_LARGEFILE, 0777, "file")


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
        self.assertCalled("fsop_unlink", None, "file")


class TestUnlinkat(LibcTest):
    entry = "test_unlinkat"
    code = r"""
#include <fcntl.h>
#include <unistd.h>
void test_unlinkat()
{
  int fd = creat("file", 0777);
  t_check(fd >= 0);
  t_check_zero(close(fd));
  t_check_zero(unlinkat(get_dir_fd("."), "file", 0 /* flags */));
}
"""
    def check(self):
        self.assertCalledPattern("fsop_unlink", WildcardNotNone(), "file")


class TestUnlinkatOnDir(LibcTest):
    entry = "test_unlinkat_on_dir"
    code = r"""
#include <fcntl.h>
#include <unistd.h>
void test_unlinkat_on_dir()
{
  t_check_zero(mkdir("test_dir", 0777));
  t_check_zero(unlinkat(get_dir_fd("."), "test_dir", AT_REMOVEDIR));
}
"""
    def check(self):
        self.assertCalledPattern("fsop_rmdir", WildcardNotNone(), "test_dir")


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
        self.assertCalled("fsop_mkdir", None, 0777, "dir")


class TestMkdirat(LibcTest):
    entry = "test_mkdirat"
    code = r"""
#include <sys/stat.h>
void test_mkdirat()
{
  t_check_zero(mkdirat(get_dir_fd("."), "dir", 0777));
}
"""
    def check(self):
        self.assertCalledPattern("fsop_mkdir", WildcardNotNone(), 0777, "dir")


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
        self.assertCalled("fsop_rmdir", None, "dir")


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
        self.assertCalled("fsop_symlink", None, "symlink", "dest_path")


class TestSymlinkat(LibcTest):
    entry = "test_symlinkat"
    code = r"""
#include <unistd.h>
void test_symlinkat()
{
  t_check_zero(symlinkat("dest_path", get_dir_fd("."), "symlink"));
}
"""
    def check(self):
        self.assertCalledPattern("fsop_symlink", WildcardNotNone(),
                                 "symlink", "dest_path")


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
        self.assertCalled("fsop_symlink", None, "symlink", "dest_path")
        self.assertCalled("fsop_readlink", None, "symlink")


class TestReadlinkat(LibcTest):
    entry = "test_readlinkat"
    code = r"""
#include <string.h>
#include <unistd.h>
void test_readlinkat()
{
  const char *str = "dest_path";
  char buf[100];
  int got;

  t_check_zero(symlink(str, "symlink"));
  got = readlinkat(get_dir_fd("."), "symlink", buf, sizeof(buf));
  t_check(got >= 0);
  assert(got == strlen(str));
  buf[got] = 0;
  assert(strcmp(buf, str) == 0);
}
"""
    def check(self):
        self.assertCalled("fsop_symlink", None, "symlink", "dest_path")
        self.assertCalledPattern("fsop_readlink", WildcardNotNone(), "symlink")


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
        self.assertCalled("fsop_open", None, os.O_RDONLY, 0, ".")


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


class TestFchdir(LibcTest):
    entry = "test_fchdir"
    code = r"""
#include <fcntl.h>
#include <unistd.h>
void test_fchdir()
{
  int fd = open(".", O_RDONLY);
  t_check(fd >= 0);
  t_check_zero(fchdir(fd));
  t_check_zero(close(fd));
}
"""
    def check(self):
        self.assertCalledPattern("fsop_fchdir", WildcardNotNone())


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
        self.assertCalled("fsop_chmod", None, 0, 0777, "file")


class TestFchmodat(LibcTest):
    entry = "test_fchmodat"
    code = r"""
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
void test_fchmodat()
{
  int fd = creat("file", 0666);
  t_check(fd >= 0);
  t_check_zero(close(fd));
  /* Set the executable bit. */
  t_check_zero(fchmodat(get_dir_fd("."), "file", 0777, 0 /* flags */));
}
"""
    def check(self):
        self.assertCalledPattern(
            "fsop_chmod", WildcardNotNone(), 0, 0777, "file")


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
        self.assertCalled("fsop_chown", None, 0, -1, -1, "file")


class TestFchownat(LibcTest):
    entry = "test_fchownat"
    code = r"""
#include <fcntl.h>
#include <unistd.h>
void test_fchownat()
{
  int fd = creat("file", 0666);
  t_check(fd >= 0);
  t_check_zero(close(fd));
  t_check_zero(fchownat(get_dir_fd("."), "file", -1, -1, 0 /* flags */));
}
"""
    def check(self):
        self.assertCalledPattern(
            "fsop_chown", WildcardNotNone(), 0, -1, -1, "file")


class TestFork(LibcTest):
    entry = "test_fork"
    code = r"""
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
void test_fork()
{
  int pid, pid2, fd, status;
  pid = fork();
  t_check(pid >= 0);
  if(pid == 0) {
    /* Check that communications still work in the forked process. */
    fd = creat("test_file1", 0666);
    t_check(fd >= 0);
    t_check_zero(close(fd));
    _exit(42);
  }
  /* Check that communications still work in the parent process. */
  fd = creat("test_file2", 0666);
  t_check(fd >= 0);
  t_check_zero(close(fd));

  pid2 = wait(&status);
  t_check(pid2 >= 0);
  assert(pid == pid2);
  assert(WIFEXITED(status) && WEXITSTATUS(status) == 42);
}
"""
    def check(self):
        self.assertCalled("fsop_copy")
        self.assertCalled("fsop_open", None,
                          os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0666,
                          "test_file1")
        self.assertCalled("fsop_open", None,
                          os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0666,
                          "test_file2")


class TestVfork(LibcTest):
    entry = "test_vfork"
    code = r"""
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
void test_vfork()
{
  int pid, pid2, fd, status;
  pid = vfork();
  t_check(pid >= 0);
  if(pid == 0) {
    /* Check that communications still work in the forked process. */
    fd = creat("test_file1", 0666);
    t_check(fd >= 0);
    t_check_zero(close(fd));
    _exit(42);
  }
  /* Check that communications still work in the parent process. */
  fd = creat("test_file2", 0666);
  t_check(fd >= 0);
  t_check_zero(close(fd));

  pid2 = wait(&status);
  t_check(pid2 >= 0);
  assert(pid == pid2);
  assert(WIFEXITED(status) && WEXITSTATUS(status) == 42);
}
"""
    def check(self):
        self.assertCalled("fsop_copy")
        self.assertCalled("fsop_open", None,
                          os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0666,
                          "test_file1")
        self.assertCalled("fsop_open", None,
                          os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0666,
                          "test_file2")


# system() sometimes tries to inline fork().  Make sure that does not
# happen.
class TestSystem(LibcTest):
    entry = "test_system"
    code = r"""
#include <stdlib.h>
void test_system()
{
  int rc = system("exit 123");
  assert(WIFEXITED(rc));
  assert(WEXITSTATUS(rc) == 123);
}
"""
    def check(self):
        self.assertCalled("fsop_copy")
        self.assertCalled("fsop_exec", "/bin/sh", ["sh", "-c", "exit 123"])

    # Don't expect system() to be replaced properly in the preload library,
    # so disable this test.
    def test_preload_library(self):
        pass


class TestExec(LibcTest):
    entry = "test_exec"
    code = r"""
#include <unistd.h>
void test_exec()
{
  char *const argv[] = { "zeroth arg", "first arg", "second arg", NULL };
  t_check_zero(execve("/bin/true", argv, environ));
}
"""
    def check(self):
        self.assertCalled("fsop_exec", "/bin/true",
                          ["zeroth arg", "first arg", "second arg"])


class TestExecNotFound(LibcTest):
    # Check that execve() returns gracefully rather than invoking
    # ld.so when the executable does not exist.  Programs rely on this
    # for searching PATH.
    entry = "test_exec_not_found"
    code = r"""
#include <errno.h>
#include <unistd.h>
void test_exec_not_found()
{
  char *const argv1[] = { "foo", "bar", "baz", NULL };
  int rc1 = execve("/does-not-exist-1", argv1, environ);
  assert(rc1 == -1 && errno == ENOENT);

  char *const argv2[] = { "qux", "quux", "quuux", NULL };
  int rc2 = execve("/does-not-exist-2", argv2, environ);
  assert(rc2 == -1 && errno == ENOENT);
}
"""
    def check(self):
        self.assertCalled("fsop_exec", "/does-not-exist-1",
                          ["foo", "bar", "baz"])
        self.assertCalled("fsop_exec", "/does-not-exist-2",
                          ["qux", "quux", "quuux"])


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
        self.assertCalledPattern("fsop_bind", WildcardNotNone(), "socket")


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


class TestConnect(LibcTest):
    entry = "test_connect"
    code = r"""
#include <sys/socket.h>
#include <sys/un.h>
void test_connect()
{
  int fd, fd2;
  struct sockaddr_un addr;

  fd = socket(PF_UNIX, SOCK_STREAM, 0);
  t_check(fd >= 0);
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, "socket");
  t_check_zero(bind(fd, (struct sockaddr *) &addr,
                    sizeof(struct sockaddr_un)));
  t_check_zero(listen(fd, 1));

  fd2 = socket(PF_UNIX, SOCK_STREAM, 0);
  t_check(fd >= 0);
  t_check_zero(connect(fd2, (struct sockaddr *) &addr,
                       sizeof(struct sockaddr_un)));
}
"""
    def check(self):
        self.assertCalledPattern("fsop_connect", WildcardNotNone(), "socket")


class TestGetsocknameBind(LibcTest):
    entry = "test_getsockname_bind"
    code = r"""
#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>
void test_getsockname_bind()
{
  const char *pathname = "./socket";
  int fd;
  struct sockaddr_un addr, addr2;
  socklen_t len, len2;

  fd = socket(PF_UNIX, SOCK_STREAM, 0);
  t_check(fd >= 0);
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, pathname);
  len = offsetof(struct sockaddr_un, sun_path) + strlen(pathname) + 1;
  t_check_zero(bind(fd, (struct sockaddr *) &addr, len));

  len2 = sizeof(struct sockaddr_un);
  t_check_zero(getsockname(fd, &addr2, &len2));
  assert(len == len2);
  assert(memcmp(&addr, &addr2, len2) == 0);
}
"""


class TestGetsocknameConnect(LibcTest):
    entry = "test_getsockname_connect"
    code = r"""
#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>
void test_getsockname_connect()
{
  const char *pathname = "./socket";
  int fd, fd2;
  struct sockaddr_un addr, addr2;
  socklen_t len, len2;

  fd = socket(PF_UNIX, SOCK_STREAM, 0);
  t_check(fd >= 0);
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, pathname);
  len = offsetof(struct sockaddr_un, sun_path) + strlen(pathname) + 1;
  t_check_zero(bind(fd, (struct sockaddr *) &addr, len));
  t_check_zero(listen(fd, 1));

  fd2 = socket(PF_UNIX, SOCK_STREAM, 0);
  t_check(fd >= 0);
  t_check_zero(connect(fd2, (struct sockaddr *) &addr, len));

  len2 = sizeof(struct sockaddr_un);
  t_check_zero(getsockname(fd2, &addr2, &len2));
  assert(len == len2);
  assert(memcmp(&addr, &addr2, len2) == 0);
}
"""
    def test_native(self):
        # The code fails when run on Linux natively because Linux
        # returns a sockaddr without a filename for the FD set up by
        # connect().  It returns a length of 2 (equal to
        # offsetof(struct sockaddr_un, sun_path)).
        # Perhaps PlashGlibc should be changed to do the same, and
        # store the socket pathname on bind() but not connect().
        pass


class TestRename(LibcTest):
    entry = "test_rename"
    code = r"""
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
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
        self.assertCalled("fsop_rename", None, None, "new_file", "orig_file")


class TestRenameat(LibcTest):
    entry = "test_renameat"
    code = r"""
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
void test_renameat()
{
  int fd = creat("orig_file", 0777);
  t_check(fd >= 0);
  t_check_zero(close(fd));
  t_check_zero(renameat(get_dir_fd("."), "orig_file",
                        get_dir_fd("."), "new_file"));
}
"""
    def check(self):
        self.assertCalledPattern(
            "fsop_rename", WildcardNotNone(), WildcardNotNone(),
            "new_file", "orig_file")


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
        self.assertCalled("fsop_link", None, None, "new_file", "orig_file")


class TestLinkat(LibcTest):
    entry = "test_linkat"
    code = r"""
#include <fcntl.h>
#include <unistd.h>
void test_linkat()
{
  int fd = creat("orig_file", 0777);
  t_check(fd >= 0);
  t_check_zero(close(fd));
  t_check_zero(linkat(get_dir_fd("."), "orig_file",
                      get_dir_fd("."), "new_file", 0 /* flags */));
}
"""
    def check(self):
        self.assertCalledPattern(
            "fsop_link", WildcardNotNone(), WildcardNotNone(),
            "new_file", "orig_file")


class TestAccess(LibcTest):
    entry = "test_access"
    code = r"""
#include <unistd.h>
#include <fcntl.h>
void test_access()
{
  int fd = creat("test_file", 0777);
  t_check(fd >= 0);
  t_check_zero(close(fd));
  t_check_zero(access("test_file", R_OK | W_OK));
}
"""
    def check(self):
        self.assertCalled("fsop_access", None, os.R_OK | os.W_OK, "test_file")


class TestFaccessat(LibcTest):
    entry = "test_faccessat"
    code = r"""
#include <unistd.h>
#include <fcntl.h>
void test_faccessat()
{
  int fd = creat("test_file", 0777);
  t_check(fd >= 0);
  t_check_zero(close(fd));
  t_check_zero(faccessat(get_dir_fd("."), "test_file", R_OK | W_OK, 0));
}
"""
    def check(self):
        self.assertCalledPattern("fsop_access", WildcardNotNone(),
                                 os.R_OK | os.W_OK, "test_file")


class TestStat(LibcTest):
    entry = "test_stat"
    code = r"""
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
void test_stat()
{
  int fd = creat("test_file", 0777);
  t_check(fd >= 0);
  t_check_zero(close(fd));
  struct stat st;
  t_check_zero(stat("test_file", &st));
}
"""
    def check(self):
        # TODO: decode nofollow argument as a bool not an int
        self.assertCalled("fsop_stat", None, 0, "test_file")


class TestLstat(LibcTest):
    entry = "test_lstat"
    code = r"""
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
void test_lstat()
{
  int fd = creat("test_file", 0777);
  t_check(fd >= 0);
  t_check_zero(close(fd));
  struct stat st;
  t_check_zero(lstat("test_file", &st));
}
"""
    def check(self):
        # TODO: decode nofollow argument as a bool not an int
        self.assertCalled("fsop_stat", None, 1, "test_file")


class TestFstatat(LibcTest):
    entry = "test_fstatat"
    code = r"""
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
void test_fstatat()
{
  int fd = creat("test_file", 0777);
  t_check(fd >= 0);
  t_check_zero(close(fd));
  struct stat st;
  t_check_zero(fstatat(get_dir_fd("."), "test_file", &st, 0));
}
"""
    def check(self):
        self.assertCalledPattern("fsop_stat", WildcardNotNone(), 0,
                                 "test_file")


class TestFstatOnFile(LibcTest):
    entry = "test_fstat_on_file"
    code = r"""
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
void test_fstat_on_file()
{
  int fd = creat("test_file", 0777);
  t_check(fd >= 0);
  struct stat st;
  t_check_zero(fstat(fd, &st));
  t_check_zero(close(fd));
}
"""
    def check(self):
        pass


class TestFstatOnDir(LibcTest):
    entry = "test_fstat_on_dir"
    code = r"""
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
void test_fstat_on_dir()
{
  int fd = open(".", O_RDONLY);
  t_check(fd >= 0);
  struct stat st;
  t_check_zero(fstat(fd, &st));
  t_check_zero(close(fd));
}
"""
    def check(self):
        self.assertCalledPattern("fsop_dir_fstat", WildcardNotNone())


class TestUtime(LibcTest):
    entry = "test_utime"
    code = r"""
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>
void test_utime()
{
  int fd = creat("test_file", 0777);
  t_check(fd >= 0);
  t_check_zero(close(fd));
  struct utimbuf buf;
  buf.actime = 123;
  buf.modtime = 456;
  t_check_zero(utime("test_file", &buf));
}
"""
    def check(self):
        self.assertCalled("fsop_utime", None, 0, 123, 0, 456, 0, "test_file")


class TestUtimes(LibcTest):
    entry = "test_utimes"
    code = r"""
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
void test_utimes()
{
  int fd = creat("test_file", 0777);
  t_check(fd >= 0);
  t_check_zero(close(fd));
  struct timeval times[2];
  times[0].tv_sec = 123;
  times[0].tv_usec = 321;
  times[1].tv_sec = 456;
  times[1].tv_usec = 654;
  t_check_zero(utimes("test_file", times));
}
"""
    def check(self):
        self.assertCalled("fsop_utime", None, 0, 123, 321, 456, 654,
                          "test_file")


class TestFutimesat(LibcTest):
    entry = "test_futimesat"
    code = r"""
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
void test_futimesat()
{
  int fd = creat("test_file", 0666);
  t_check(fd >= 0);
  t_check_zero(close(fd));
  struct timeval times[2];
  times[0].tv_sec = 123;
  times[0].tv_usec = 321;
  times[1].tv_sec = 456;
  times[1].tv_usec = 654;
  t_check_zero(futimesat(get_dir_fd("."), "test_file", times));
}
"""
    def check(self):
        self.assertCalledPattern("fsop_utime", WildcardNotNone(), 0,
                                 123, 321, 456, 654, "test_file")


class TestTruncate(LibcTest):
    entry = "test_truncate"
    code = r"""
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
void test_truncate()
{
  int fd = open("test_file", O_CREAT, 0777);
  t_check(fd >= 0);
  t_check_zero(truncate("test_file", 123));

  struct stat st;
  t_check_zero(fstat(fd, &st));
  assert(st.st_size == 123);
  t_check_zero(close(fd));
}
"""
    def check(self):
        self.assertCalled("fsop_open", None, os.O_CREAT, 0777, "test_file")
        self.assertCalled("fsop_open", None, os.O_WRONLY, 0, "test_file")


class TestTruncate64(LibcTest):
    entry = "test_truncate64"
    code = r"""
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
void test_truncate64()
{
  int fd = open("test_file", O_CREAT, 0777);
  t_check(fd >= 0);
  t_check_zero(truncate64("test_file", 123));

  struct stat st;
  t_check_zero(fstat(fd, &st));
  assert(st.st_size == 123);
  t_check_zero(close(fd));
}
"""
    def check(self):
        self.assertCalled("fsop_open", None, os.O_CREAT, 0777, "test_file")
        self.assertCalled("fsop_open", None, os.O_WRONLY | os.O_LARGEFILE,
                          0, "test_file")


class TestGetuid(LibcTest):
    entry = "test_getuid"
    code = r"""
#include <stdlib.h>
#include <unistd.h>
void test_getuid()
{
  t_check_zero(setenv("PLASH_FAKE_UID", "4001", 1));
  t_check_zero(setenv("PLASH_FAKE_GID", "4002", 1));
  t_check_zero(setenv("PLASH_FAKE_EUID", "4003", 1));
  t_check_zero(setenv("PLASH_FAKE_EGID", "4004", 1));

  /* Getters should return faked UID/GID values. */
  assert(getuid() == 4001);
  assert(getgid() == 4002);
  assert(geteuid() == 4003);
  assert(getegid() == 4004);
}
"""
    def check(self):
        function_names = [prefix + suffix
                          for prefix in ("get", "gete")
                          for suffix in ("uid", "gid")]
        for function_name in function_names:
            self.assertCalled("fsop_log", function_name)

    def test_native(self):
        # Test checks for hard-coded UID/GIDs, so doesn't work natively.
        pass


class TestUnfakedGetuid(LibcTest):
    entry = "test_getuid_unfaked"
    code = r"""
#include <stdlib.h>
#include <unistd.h>
void test_getuid_unfaked()
{
  t_check_zero(unsetenv("PLASH_FAKE_UID"));
  t_check_zero(unsetenv("PLASH_FAKE_GID"));
  t_check_zero(unsetenv("PLASH_FAKE_EUID"));
  t_check_zero(unsetenv("PLASH_FAKE_EGID"));

  /* This is mainly checking that these do not enter infinite recursion,
     which is a risk when getuid() calls kernel_getuid(). */
  getuid();
  getgid();
  geteuid();
  getegid();
}
"""
    def check(self):
        function_names = [prefix + suffix
                          for prefix in ("get", "gete")
                          for suffix in ("uid", "gid")]
        for function_name in function_names:
            self.assertCalled("fsop_log", function_name)


class TestSetuid(LibcTest):
    entry = "test_setuid"
    code = r"""
#include <unistd.h>
void test_setuid()
{
  /* These setters should all be no-ops. */
  t_check_zero(setuid(1234));
  t_check_zero(setgid(1234));
  t_check_zero(seteuid(1234));
  t_check_zero(setegid(1234));
  t_check_zero(setreuid(1234, 2345));
  t_check_zero(setregid(1234, 2345));
  t_check_zero(setresuid(1234, 2345, 3456));
  t_check_zero(setresgid(1234, 2345, 3456));
}
"""
    def test_native(self):
        # Test doesn't work natively.  TODO: maybe these calls should
        # only work when the UID/GID matches the current faked
        # UID/GID?
        pass

    def check(self):
        function_names = [prefix + suffix
                          for prefix in ("set", "sete", "setre", "setres")
                          for suffix in ("uid", "gid")]
        for function_name in function_names:
            self.assertCalled("fsop_log", function_name)


def compile_into_one_executable(cases, tmp_dir):
    entry_points = [case.entry for case in cases]
    test_case_prototypes = ""
    for case in cases:
        test_case_prototypes += "void %s(void);\n" % case.entry
    c_files = []
    for case in cases:
        filename = os.path.join(tmp_dir, "%s.c" % case.entry)
        write_file(filename, test_case_prototypes + prototypes + case.code)
        c_files.append(filename)
    code = test_case_prototypes
    code += r"""
#include <stdlib.h>
struct test_case {
  const char *name;
  void (*func)(void);
};
"""
    code += ("struct test_case test_cases[] = { %s, { NULL, NULL } };\n"
             % ", ".join(["""{ "%s", %s }""" % (name, name)
                          for name in entry_points]))
    main_func = r"""
#include <string.h>
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
    write_file(os.path.join(tmp_dir, "test-cases.c"),
               prototypes + code + main_func)
    src_dir = os.path.dirname(__file__)
    rc = subprocess.call(["gcc", "-Wall", "-D_GNU_SOURCE",
                          "-I%s" % src_dir,
                          os.path.join(src_dir, "test-util.c"),
                          os.path.join(tmp_dir, "test-cases.c")] + c_files +
                         ["-o", os.path.join(tmp_dir, "test-case")])
    assert rc == 0
    return os.path.join(tmp_dir, "test-case")


def get_test_cases(module, temp_maker):
    cases = [x for x in module.__dict__.values()
             if isinstance(x, type) and issubclass(x, LibcTest)]
    tmp_dir = temp_maker.make_temp_dir()
    executable = compile_into_one_executable(cases, tmp_dir)
    tests = []
    for case in cases:
        class Case(case):
            def _compile_executable(self):
                return executable
            main_args = [case.entry]
        Case.__name__ = case.__name__
        tests.extend(testrunner.get_cases_from_unittest_class(Case))
    return tests

def run_tests(suite):
    runner = unittest.TextTestRunner(verbosity=1)
    runner.run(suite)


if __name__ == "__main__":
    if sys.argv[1:2] == ["--slow"]:
        sys.argv.pop(1)
        unittest.main()
    else:
        temp_maker = TempMaker()
        try:
            cases = get_test_cases(__import__("__main__"), temp_maker)
            run_tests(testrunner.make_test_suite_from_cases(cases))
        finally:
            temp_maker.destroy()
