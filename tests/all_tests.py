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

"""
Usage: python all_tests.py [-v|--verbose]
"""

import getopt
import imp
import os
import sys
import unittest

import libc_test
import testrunner


top_dirs = ["python", "tests"]


def get_files_recursively(path):
    if os.path.isfile(path):
        yield path
    elif os.path.isdir(path):
        for leafname in os.listdir(path):
            for filename in get_files_recursively(os.path.join(path, leafname)):
                yield filename


def get_test_case_modules(search_paths):
    for top_path in search_paths:
        filenames = [path for path in get_files_recursively(top_path)
                     if path.endswith("_test.py")]
        if len(filenames) == 0:
            print 'Warning: no test modules found under "%s"' % top_path
        for path in filenames:
            # TODO: use __import__ instead, which means
            # modules will only get imported once.  That only
            # works when the modules are in sys.path though.
            module_name = path.replace("/", "_").replace(".", "_")
            yield imp.load_source(module_name, path)


def main(args):
    verbosity = 1
    options, args = getopt.getopt(args, "vh", ["verbose", "help"])
    for opt, value in options:
        if opt in ("-v", "--verbose"):
            verbosity = 2
        if opt in ("-h", "--help"):
            print __doc__
            return 0
    if len(args) != 0:
        search_paths = args
    else:
        search_paths = top_dirs

    loader = unittest.defaultTestLoader
    suite = unittest.TestSuite()
    temp_maker = libc_test.TempMaker()
    try:
        for module in get_test_case_modules(search_paths):
            if hasattr(module, "get_test_cases"):
                suite.addTests(testrunner.make_test_suite_from_cases(
                        module.get_test_cases(module, temp_maker)))
            else:
                suite.addTests(testrunner.load_tests_from_module(module))
        runner = unittest.TextTestRunner(verbosity=verbosity)
        result = runner.run(suite)
        return not result.wasSuccessful()
    finally:
        temp_maker.destroy()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
