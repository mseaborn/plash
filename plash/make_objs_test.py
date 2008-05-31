
import os
import shutil
import tempfile
import unittest

import make_objs


def write_file(filename, data):
    fh = open(filename, "w")
    try:
        fh.write(data)
    finally:
        fh.close()


class TestGccTarget(unittest.TestCase):

    def setUp(self):
        self._tmpdir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self._tmpdir)

    def test_building(self):
        src = os.path.join(self._tmpdir, "foo.c")
        dest = os.path.join(self._tmpdir, "foo.o")
        write_file(src, "int main() { return 0; }")
        target = make_objs.GccTarget(src, dest, [])
        self.assertFalse(target.is_up_to_date())
        target.build()
        self.assertTrue(target.is_up_to_date())


if __name__ == "__main__":
    unittest.main()
