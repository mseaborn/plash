
import os
import unittest

import plash_core
import plash.env
import plash.namespace


class LogTest(unittest.TestCase):
    
    def test1(self):
        dir_obj = plash.env.get_root_dir()
        plash.namespace.make_fs_op(dir_obj)

    def test2(self):
        dir_obj = plash.env.get_root_dir()
        fd = os.open("/dev/null", os.O_WRONLY | os.O_CREAT | os.O_TRUNC)
        log = plash.namespace.make_log_from_fd(plash_core.wrap_fd(fd))
        plash.namespace.make_fs_op(dir_obj, log)


if __name__ == "__main__":
    unittest.main()
