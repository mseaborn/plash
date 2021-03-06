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
import signal
import subprocess
import tempfile
import unittest


def read_file(path):
    fh = open(path, "r")
    try:
        return fh.read()
    finally:
        fh.close()

def write_file(path, data):
    fh = open(path, "w")
    try:
        fh.write(data)
    finally:
        fh.close()


def check_subprocess_status(status):
    if status != 0:
        raise Exception("Subprocess exited with status %i" % status)


def stdout_from_command(args):
    proc = subprocess.Popen(args, stdout=subprocess.PIPE)
    stdout, stderr = proc.communicate()
    check_subprocess_status(proc.wait())
    return stdout


class TestCaseChdir(unittest.TestCase):

    def setUp(self):
        self._dir_fd = os.open(".", os.O_RDONLY)
        self._tmp_dir = tempfile.mkdtemp(prefix="plash-test")
        os.chdir(self._tmp_dir)

    def tearDown(self):
        os.fchdir(self._dir_fd)
        os.close(self._dir_fd)
        shutil.rmtree(self._tmp_dir)


class PolaShellTests(TestCaseChdir):

    def test_echo(self):
        proc = subprocess.Popen(
            ["pola-shell", "-c", "echo 'Hello world!'"],
            stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(stdout, "Hello world!\n")

    def test_attach(self):
        data = "test contents"
        write_file("my_file", data)
        proc = subprocess.Popen(
            ["pola-shell", "-c", "cat some_file_name=(F my_file)"],
            stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(stdout, data)

    def test_attach_name(self):
        proc = subprocess.Popen(
            ["pola-shell", "-c", "echo some_file_name=(F .)"],
            stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(stdout, "some_file_name\n")

    def test_redirect(self):
        proc = subprocess.Popen(
            ["pola-shell", "-c", "echo 'Hello world!' 1>temp_file"],
            stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(stdout, "")
        self.assertEquals(read_file("temp_file"), "Hello world!\n")

    def test_bash_exec(self):
        # Redirect stderr to suppress bash's warning about getcwd()
        proc = subprocess.Popen(
            ["pola-shell", "-c", """sh -c '/bin/echo "Successful call"'"""],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(stdout, "Successful call\n")

    def test_execobj_hello(self):
        proc = subprocess.Popen(
            ["pola-shell", "-c", """\
def myecho = capcmd exec-object /bin/echo / ;
myecho 'Hello there'
"""
             ], stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertTrue("Hello there" in stdout)

    # TODO: shell_execobj_2
    # TODO: shell_cwd_unset
    # TODO: shell_cwd_set
    # TODO: shell_execobj_cwd_unset
    # TODO: shell_execobj_cwd_set


class PolaRunTestsMixin(object):

    def test_echo(self):
        proc = subprocess.Popen([self._pola_run, "-B", "--prog", "/bin/echo",
                                 "-a", "Hello world"],
                                stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        check_subprocess_status(proc.wait())
        self.assertEquals(stdout, "Hello world\n")

    def test_echo_path(self):
        """Expects pola-run to look up the executable name in PATH."""
        proc = subprocess.Popen([self._pola_run, "-B", "--prog", "echo",
                                 "-a", "Hello world"],
                                stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        check_subprocess_status(proc.wait())
        self.assertEquals(stdout, "Hello world\n")

    def test_nopath(self):
        """Expects pola-run *not* to look up the executable name in PATH.
        Expects pola-run to fail."""
        proc = subprocess.Popen([self._pola_run, "-B", "--no-path-search",
                                 "--prog", "echo"],
                                stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertNotEqual(proc.wait(), 0)

    def test_cat(self):
        data = "Hello world!\nThis is test data."
        write_file("file", data)
        proc = subprocess.Popen(
            [self._pola_run, "-B", "--prog", "cat", "-fa", "file"],
            stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        check_subprocess_status(proc.wait())
        self.assertEquals(stdout, data)

    def test_cat_large(self):
        data = "Hello world!\nThis is test data." * (4096 / 32)
        write_file("file", data)
        proc = subprocess.Popen(
            [self._pola_run, "-B", "--prog", "cat", "-fa", "file"],
            stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        check_subprocess_status(proc.wait())
        self.assertEquals(len(stdout), len(data))
        self.assertEquals(stdout, data)

    def test_bash_exec(self):
        proc = subprocess.Popen(
            [self._pola_run, "-B", "--cwd", "/",
             "-e", "/bin/bash", "-c", "/bin/echo yeah"],
            stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        check_subprocess_status(proc.wait())
        self.assertEquals(stdout, "yeah\n")

    def test_bash_fork(self):
        proc = subprocess.Popen(
            [self._pola_run, "-B", "--cwd", "/",
             "-e", "/bin/bash", "-c", "/bin/echo yeah; /bin/true"],
            stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        check_subprocess_status(proc.wait())
        self.assertEquals(stdout, "yeah\n")

    def test_nested(self):
        proc = subprocess.Popen([self._pola_run, "-fw", "/", "-e",
                                 self._pola_run, "-fw", "/", "-e",
                                 "/bin/echo", "Hello world"],
                                stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        check_subprocess_status(proc.wait())
        self.assertEquals(stdout, "Hello world\n")

    def test_strace(self):
        proc = subprocess.Popen(
            [self._pola_run, "-B", "-e", "strace", "-c",
             "/bin/echo", "Hello world"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        check_subprocess_status(proc.wait())
        self.assertTrue("Hello world" in stdout)

    def test_script(self):
        """Test handling of "#!" scripts."""
        write_file("script", """\
#!/bin/sh
echo "script output"
""")
        for command in (["./script"], ["sh", "-c", "./script"]):
            proc = subprocess.Popen(
                [self._pola_run, "-B", "-f", ".", "-e"] + command,
                stdout=subprocess.PIPE)
            stdout, stderr = proc.communicate()
            check_subprocess_status(proc.wait())
            self.assertEquals(stdout, "script output\n")

    def test_script_arg(self):
        write_file("script", """\
#!/bin/echo argument-to-interpreter
echo "this does not get used"
""")
        for command in (["./script"], ["sh", "-c", "./script"]):
            proc = subprocess.Popen(
                [self._pola_run, "-B", "-f", ".", "-e"] + command,
                stdout=subprocess.PIPE)
            stdout, stderr = proc.communicate()
            check_subprocess_status(proc.wait())
            self.assertEquals(stdout, "argument-to-interpreter ./script\n")

    def test_script_arg_space(self):
        write_file("script", """#!/bin/echo  args with spaces  \n""")
        for command in (["./script"], ["sh", "-c", "./script"]):
            proc = subprocess.Popen(
                [self._pola_run, "-B", "-f", ".", "-e"] + command,
                stdout=subprocess.PIPE)
            stdout, stderr = proc.communicate()
            check_subprocess_status(proc.wait())
            self.assertEquals(stdout, "args with spaces   ./script\n")

    def test_getuid(self):
        # TODO: use "-B" instead of "-fw /".  See PlashIssues/Lib64Directory.
        expect = "%i\n" % os.getuid()
        self.assertEquals(stdout_from_command(["id", "-u"]), expect)
        self.assertEquals(stdout_from_command(
                ["pola-run", "-fw", "/", "-e", "id", "-u"]), expect)

    def test_getgid(self):
        # TODO: use "-B" instead of "-fw /".  See PlashIssues/Lib64Directory.
        expect = "%i\n" % os.getgid()
        self.assertEquals(stdout_from_command(["id", "-g"]), expect)
        self.assertEquals(stdout_from_command(
                ["pola-run", "-fw", "/", "-e", "id", "-g"]), expect)

    def test_return_code_exited(self):
        proc = subprocess.Popen([self._pola_run, "--cwd", "/", "-B", "-e",
                                 "sh", "-c", "exit 123"])
        self.assertEquals(proc.wait(), 123)

    def test_return_code_signalled_sigsegv(self):
        proc = subprocess.Popen([self._pola_run, "--cwd", "/", "-B", "-e",
                                 "sh", "-c", "kill -SEGV $$"])
        self.assertEquals(proc.wait(),
                          self._return_code_for_signal(signal.SIGSEGV))

    def test_return_code_signalled_sigint(self):
        proc = subprocess.Popen([self._pola_run, "--cwd", "/", "-B", "-e",
                                 "sh", "-c", "kill -INT $$"])
        self.assertEquals(proc.wait(),
                          self._return_code_for_signal(signal.SIGINT))

    def test_return_code_stopped(self):
        # Check that pola-run is not catching stop signals from child.
        proc = subprocess.Popen([
            self._pola_run, "--cwd", "/", "-B", "-e", "sh", "-c",
            "PID=$$; (sleep 0.1s; kill -CONT $PID) & kill -STOP $PID; "
            "exit 124"])
        self.assertEquals(proc.wait(), 124)

    def test_ls(self):
        proc = subprocess.Popen([self._pola_run, "-B", "-e", "ls", "--version"],
                                stdout=subprocess.PIPE)
        self.assertEquals(proc.wait(), 0)

    def test_chdir_in_forked_process(self):
        temp_dir = os.getcwd()
        proc = subprocess.Popen(
            [self._pola_run, "-B", "-fw", temp_dir,
             "-e", "/bin/bash", "-c", """
set -e
echo Foo > test_file
mkdir test_dir
bash -c 'cd test_dir && echo In subprocess && pwd'
cat test_file
pwd
"""],
            stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        check_subprocess_status(proc.wait())
        self.assertEquals(stdout, "In subprocess\n%s/test_dir\nFoo\n%s\n"
                          % (temp_dir, temp_dir))


class PolaRunCTests(PolaRunTestsMixin, TestCaseChdir):

    def setUp(self):
        super(PolaRunCTests, self).setUp()
        self._pola_run = "pola-run-c"

    def _return_code_for_signal(self, signal):
        return -signal


class PolaRunPythonTests(PolaRunTestsMixin, TestCaseChdir):

    def setUp(self):
        super(PolaRunPythonTests, self).setUp()
        self._pola_run = "pola-run"

    def test_tmp_arg(self):
        proc = subprocess.Popen([self._pola_run, "--cwd", "/", "-B", "--tmp",
                                 "-e", "sh", "-c", "> /tmp/test-tmp-file"])
        self.assertEquals(proc.wait(), 0)
        proc = subprocess.Popen([self._pola_run, "--cwd", "/", "-B",
                                 "--tmpdir", "/foo",
                                 "-e", "sh", "-c", "> /foo/test-tmp-file"])
        self.assertEquals(proc.wait(), 0)

    def _return_code_for_signal(self, signal):
        # Python pola-run's exit code does not tell you which signal
        # was raised.
        return 101

    def test_help_arg(self):
        for arg in ("--help", "-h"):
            proc = subprocess.Popen([self._pola_run, arg],
                                    stdout=subprocess.PIPE)
            self.assertEquals(proc.wait(), 0)


if __name__ == "__main__":
    unittest.main()
