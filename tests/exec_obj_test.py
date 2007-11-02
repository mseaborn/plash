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
import stat
import unittest

import plash.env
import plash.mainloop
import plash.marshal
import plash.pola_run_args
import plash.process


class ExecObj(plash.marshal.Pyobj_demarshal):

    def __init__(self, call_list):
        self._call_list = call_list

    def fsobj_type(self):
        return plash.marshal.OBJT_FILE

    def fsobj_stat(self):
        return {"st_dev": 1, # Could pick a different device number
                "st_ino": 1, # TODO: allocate inode number
                "st_mode": stat.S_IFREG | 0777,
                "st_nlink": 0,
                "st_uid": 0,
                "st_gid": 0,
                "st_rdev": 0,
                "st_size": 0,
                "st_blksize": 1024,
                "st_blocks": 0,
                "st_atime": 0,
                "st_mtime": 0,
                "st_ctime": 0}

    def eo_is_executable(self):
        return ()

    def eo_exec(self, args):
        self._call_list.append(args)
        return 123


class ExecutableObjectTest(unittest.TestCase):

    def test_libc_exec_call(self):
        calls = []
        proc = plash.process.ProcessSpecWithNamespace()
        proc.cwd_path = "/"
        proc.env = os.environ.copy()
        proc.env["TEST_ENV_VAR"] = "example contents"
        proc.get_namespace().attach_at_path("/", plash.env.get_root_dir())
        proc.get_namespace().attach_at_path("/test/exec_obj", ExecObj(calls))
        proc.setcmd("bash", "-c", "exec /test/exec_obj arg1 arg2")
        helper = plash.pola_run_args.ProcessSetup(proc)
        helper.handle_args(["--fd", "0", "--fd", "1", "--fd", "2"])
        pid = proc.spawn()
        plash.mainloop.run_server()
        pid2, status = os.wait()
        self.assertEquals(pid, pid2)
        self.assertTrue(os.WIFEXITED(status))
        self.assertEquals(os.WEXITSTATUS(status), 123)
        self.assertEquals(len(calls), 1)
        args = dict(calls[0])
        self.assertEquals(sorted(args.keys()),
                          ["Argv", "Cwd.", "Env.", "Fds.", "Pgid", "Root"])
        self.assertEquals(args["Argv"], ["/test/exec_obj", "arg1", "arg2"])
        self.assertEquals(args["Cwd."], "/")
        self.assertTrue("TEST_ENV_VAR=example contents" in args["Env."],
                        args["Env."])
        self.assertEquals(sorted(dict(args["Fds."]).keys()),
                          [0, 1, 2])
        self.assertEquals(args["Pgid"], os.getpgid(0))
        # TODO: compare directly, instead of comparing reprs.
        # Python wrappers of C Plash objects don't preserve EQ.
        self.assertEquals(repr(args["Root"]), repr(proc.root_dir))


if __name__ == "__main__":
    unittest.main()
