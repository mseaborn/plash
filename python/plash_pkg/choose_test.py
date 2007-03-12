
import unittest

import plash_pkg_choose


class ChooseTest(unittest.TestCase):

    def test(self):
        chooser = plash_pkg_choose.DepChooser()

        def make_pkg(d):
            d["version"] = "1.0-1"
            return d
        editor = make_pkg({"package": "editor",
                           "depends": "libgtk"})
        libgtk = make_pkg({"package": "libgtk",
                           "depends": "libcircular"})
        libcircular = make_pkg({"package": "libcircular",
                                "depends": "libgtk"})
        libuseless = make_pkg({"package": "libuseless"})
        bash = make_pkg({"package": "bash",
                         "essential": "yes"})
        for pkg in [editor, libgtk, libcircular, libuseless, bash]:
            chooser.add_avail_package(pkg)

        chooser.search_deps("editor")
        chooser.add_essential_packages()
        self.assertEquals(chooser.get_output(),
                          [libcircular, libgtk, editor, bash])


if __name__ == "__main__":
    unittest.main()
