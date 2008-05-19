
import os
import re
import subprocess
import unittest


class ChainloaderTest(unittest.TestCase):

    def test_1_entering_and_doing_syscall(self):
        proc = subprocess.Popen(["./test-entry"], stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(stdout, "Entering C function works\n")
        self.assertEquals(proc.wait(), 123)

    def test_2_reading_argv_and_env(self):
        proc = subprocess.Popen(["env", "-i", "FOO=BAR", "BAZ=QUX",
                                 "./test-argv", "arg1", "arg2", "arg3"],
                                stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(proc.wait(), 123)
        self.assertEquals(stdout, """\
argv:
"./test-argv"
"arg1"
"arg2"
"arg3"
env:
"FOO=BAR"
"BAZ=QUX"
""")

    def test_3_filename_chainloader(self):
        proc = subprocess.Popen(["./chainloader-fixed", "/bin/echo",
                                 "Hello world"],
                                stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(proc.wait(), 0)
        self.assertEquals(stdout, "Hello world\n")

    def _get_system_defines(self):
        # There has to be a better way of testing the architecture
        # than this.
        proc = subprocess.Popen("cpp -dM </dev/null", shell=True,
                                stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(proc.wait(), 0)
        return stdout.split("\n")

    def _get_ld_so(self):
        if "#define __x86_64__ 1" in self._get_system_defines():
            return "/lib/ld-linux-x86-64.so.2"
        else:
            return "/lib/ld-linux.so.2"

    def test_4_fd_chainloader(self):
        fd = os.open(self._get_ld_so(), os.O_RDONLY)
        proc = subprocess.Popen(["./chainloader", str(fd), "/bin/echo",
                                 "Hello", "world"],
                                stdout=subprocess.PIPE)
        os.close(fd)
        stdout, stderr = proc.communicate()
        self.assertEquals(proc.wait(), 0)
        self.assertEquals(stdout, "Hello world\n")

    def test_5_fd_chainloader_no_args(self):
        proc = subprocess.Popen(["./chainloader"], stderr=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(proc.wait(), 127)
        assert stderr != ""

    def test_fd_chainloader_environ(self):
        proc = subprocess.Popen(
            ["bash", "-c", "env -i FOO=BAR BAZ=QUX "
             "./chainloader 10 /usr/bin/env 10<%s" % self._get_ld_so()],
            stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(proc.wait(), 0)
        self.assertEquals(stdout, "FOO=BAR\nBAZ=QUX\n")

    def test_fd_chainloader_invalid_fd(self):
        proc = subprocess.Popen(["./chainloader", "10"],
                                stderr=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(proc.wait(), 127)
        assert stderr != ""

    def test_fd_chainloader_invalid_elf(self):
        proc = subprocess.Popen(["sh", "-c", "./chainloader 10 10</dev/zero"],
                                stderr=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(proc.wait(), 127)
        assert stderr != ""

    def test_chainloader_proc_cmdline(self):
        proc = subprocess.Popen(
            ["bash", "-c",
             "./chainloader 10 /bin/cat /proc/self/cmdline 10<%s"
             % self._get_ld_so()], stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(proc.wait(), 0)
        # This is what the kernel reports.  It looks like it is using
        # the data portion after the auxv and ignoring the argv
        # pointer array (which the chainloader modifies).
        expected = ["./chainloader", "10", "/bin/cat", "/proc/self/cmdline"]
        self.assertEquals(stdout,
                          "".join(string + "\0" for string in expected))

    def parse_maps(self, maps_data):
        for line in maps_data.split("\n"):
            if line == "":
                continue
            match = re.match("(?P<start>[0-9a-f]+)-(?P<end>[0-9a-f]+) "
                             "(?P<flags>....) "
                             "(?P<offset>[0-9a-f]+) "
                             "(?P<device>..:..) "
                             "(?P<inode>[0-9]+) *"
                             "(?P<filename>.*)$", line)
            assert match is not None, line
            yield match

    def stack_flags(self, maps_data):
        return [mapping.group("flags")
                for mapping in self.parse_maps(maps_data)
                if mapping.group("filename") == "[stack]"]

    def test_non_executable_stack(self):
        proc = subprocess.Popen(
            ["bash", "-c",
             "./chainloader 10 /bin/cat /proc/self/maps 10<%s"
             % self._get_ld_so()], stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(proc.wait(), 0)
        self.assertEquals(self.stack_flags(stdout), ["rw-p"])

    def test_heap_using_sbrk(self):
        proc = subprocess.Popen(["bash", "-c",
                                 "./chainloader 10 10<%s ./test-sbrk"
                                 % self._get_ld_so()])
        self.assertEquals(proc.wait(), 0)

    def test_heap_mapping(self):
        proc = subprocess.Popen(
            ["bash", "-c",
             "./chainloader 10 /bin/cat /proc/self/maps 10<%s"
             % self._get_ld_so()], stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(proc.wait(), 0)
        self.assertEquals(len([mapping for mapping in self.parse_maps(stdout)
                               if mapping.group("filename") == "[heap]"]), 1)

    def test_bash(self):
        # bash is unusual in that it provides its own malloc implementation
        # that uses sbrk().
        proc = subprocess.Popen(
            ["bash", "-c",
             "./chainloader 10 10<%s /bin/bash -c 'echo bash works'"
             % self._get_ld_so()], stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(proc.wait(), 0)
        self.assertEquals(stdout, "bash works\n")

    def _find_load_address(self, command, pathname):
        proc = subprocess.Popen(command, stdout=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        self.assertEquals(proc.wait(), 0)
        maps = [mapping for mapping in self.parse_maps(stdout)
                if mapping.group("filename") == os.path.realpath(pathname)]
        assert len(maps) > 0
        return maps[0].group("start")

    def assert_in(self, x, y):
        if x not in y:
            raise AssertionError("%r not in %r" % (x, y))

    def test_initial_load_position(self):
        ld_so = self._get_ld_so()
        ld_so_address = self._find_load_address(
            [ld_so, "/bin/cat", "/proc/self/maps"], ld_so)
        chainloader_address = self._find_load_address(
            ["bash", "-c", "./chainloader 10 10<%s /bin/cat /proc/self/maps"
             % ld_so], "chainloader")
        self.assert_in(ld_so_address,
                       ("80000000", # 32-bit kernel
                        "56555000", # 32-bit process on 64-bit kernel
                        "555555554000", # 64-bit process on 64-bit kernel
                        ))
        self.assertEquals(chainloader_address, "80000000")


if __name__ == "__main__":
    unittest.main()
