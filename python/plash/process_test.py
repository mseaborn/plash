
import unittest

from plash.process import add_to_path


class TestPath(unittest.TestCase):

    def test(self):
        self.assertEquals(add_to_path("/foo", "/bar:/qux"),
                          "/foo:/bar:/qux")
        self.assertEquals(add_to_path("/foo", "/bar:/foo"),
                          "/foo:/bar")
        self.assertEquals(add_to_path("/foo", ""),
                          "/foo")


if __name__ == "__main__":
    unittest.main()
