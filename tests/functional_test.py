
import os
import subprocess
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


class TestCaseChdir(unittest.TestCase):

    def setUp(self):
        self._dir_fd = os.open(".", os.O_RDONLY)
        os.chdir("out-test")

    def tearDown(self):
        os.fchdir(self._dir_fd)
        os.close(self._dir_fd)


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


pola_run = "pola-run-py"


class PolaRunTests(TestCaseChdir):

    def test_echo(self):
        proc = subprocess.Popen([pola_run, "-B", "--prog", "/bin/echo",
                                 "-a", "Hello world"],
                                stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(stdout, "Hello world\n")

    def test_echo_path(self):
        """Expects pola-run to look up the executable name in PATH."""
        proc = subprocess.Popen([pola_run, "-B", "--prog", "echo",
                                 "-a", "Hello world"],
                                stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(stdout, "Hello world\n")

    def test_nopath(self):
        """Expects pola-run *not* to look up the executable name in PATH.
        Expects pola-run to fail."""
        proc = subprocess.Popen([pola_run, "-B", "--no-path-search",
                                 "--prog", "echo"],
                                stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertNotEqual(proc.wait(), 0)

    def test_cat(self):
        data = "Hello world!\nThis is test data."
        write_file("file", data)
        proc = subprocess.Popen(
            [pola_run, "-B", "--prog", "cat", "-fa", "file"],
            stdout=subprocess.PIPE)
        self.assertEquals(proc.communicate()[0], data)

    def test_bash_exec(self):
        proc = subprocess.Popen(
            [pola_run, "-B", "--cwd", "/",
             "-e", "/bin/bash", "-c", "/bin/echo yeah"],
            stdout=subprocess.PIPE)
        self.assertEquals(proc.communicate()[0], "yeah\n")

    def test_bash_fork(self):
        proc = subprocess.Popen(
            [pola_run, "-B", "--cwd", "/",
             "-e", "/bin/bash", "-c", "/bin/echo yeah; /bin/true"],
            stdout=subprocess.PIPE)
        self.assertEquals(proc.communicate()[0], "yeah\n")

    def test_strace(self):
        proc = subprocess.Popen(
            [pola_run, "-B", "-e", "strace", "-c",
             "/bin/echo", "Hello world"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)
        self.assertTrue("Hello world" in proc.communicate()[0])

    def test_script(self):
        """Test handling of "#!" scripts."""
        write_file("script", """\
#!/bin/sh
echo "script output"
""")
        proc = subprocess.Popen(
            [pola_run, "-B", "-f", ".", "-e", "./script"],
            stdout=subprocess.PIPE)
        self.assertEquals(proc.communicate()[0], "script output\n")

    def test_script_arg(self):
        write_file("script", """\
#!/bin/echo argument-to-interpreter
echo "this does not get used"
""")
        proc = subprocess.Popen(
            [pola_run, "-B", "-f", ".", "-e", "./script"],
            stdout=subprocess.PIPE)
        self.assertEquals(proc.communicate()[0],
                          "argument-to-interpreter ./script\n")

    def test_script_arg_space(self):
        write_file("script", """#!/bin/echo  args with spaces  \n""")
        proc = subprocess.Popen(
            [pola_run, "-B", "-f", ".", "-e", "./script"],
            stdout=subprocess.PIPE)
        self.assertEquals(proc.communicate()[0],
                          "args with spaces   ./script\n")


if __name__ == "__main__":
    unittest.main()
