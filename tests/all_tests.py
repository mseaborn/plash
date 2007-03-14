
"""
Usage: python all_tests.py [-v|--verbose]
"""

import getopt
import imp
import os
import sys
import unittest


top_dirs = ["python", "tests"]

def get_test_case_modules():
    for top_dir in top_dirs:
        for dirpath, dirnames, leafnames in os.walk(top_dir):
            for leafname in leafnames:
                if leafname.endswith("_test.py"):
                    path = os.path.join(dirpath, leafname)
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
        print __doc__
        return 1

    loader = unittest.defaultTestLoader
    suite = unittest.TestSuite()
    for module in get_test_case_modules():
        suite.addTests(loader.loadTestsFromModule(module))
    runner = unittest.TextTestRunner(verbosity=verbosity)
    result = runner.run(suite)
    return not result.wasSuccessful()

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
