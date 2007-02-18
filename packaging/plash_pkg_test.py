
import unittest

import plash_pkg


class SourcesListTest(unittest.TestCase):

    def test1(self):
        line = "deb http://ftp.debian.org/debian etchy main universe"
        self.assertEquals(
            list(plash_pkg.read_sources_list_line(line)),
            [{"S_base": "http://ftp.debian.org/debian",
              "S_component": "dists/etchy/main"},
             {"S_base": "http://ftp.debian.org/debian",
              "S_component": "dists/etchy/universe"},
             ])

    def test2(self):
        line = "deb http://ftp.debian.org/debian dists/etchy/main"
        self.assertEquals(
            list(plash_pkg.read_sources_list_line(line)),
            [{"S_base": "http://ftp.debian.org/debian",
              "S_component": "dists/etchy/main"}])


class BlockTest(unittest.TestCase):

    def test1(self):
        data = ["Field: content",
                "Another-field: more contents"]
        self.assertEquals(plash_pkg.block_fields(data),
                          {"field": "content",
                           "another-field": "more contents"})

    def test2(self):
        data = ["Field1: multi-line",
                " contents",
                "Field2: line1",
                " line2"]
        self.assertEquals(plash_pkg.block_fields(data),
                          {"field1": "multi-line\ncontents",
                           "field2": "line1\nline2"})

    def test3(self):
        data = ["not a proper field"]
        self.assertRaises(plash_pkg.BadControlLineError,
                          plash_pkg.block_fields,
                          data)


if __name__ == "__main__":
    unittest.main()
