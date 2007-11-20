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

import inspect
import shutil
import sys
import tempfile
import unittest


# This is a hack that unittest uses to prune tracebacks.
# Stack frames from modules with this set are omitted.
__unittest = 1


class TestCaseBase(object):

    @classmethod
    def get_test_suite(cls):
        raise NotImplementedError()


class TestCase(TestCaseBase):

    @classmethod
    def get_test_suite(cls):
        suite = unittest.TestSuite()

        def add_test(method_name):
            test_name = "%s.%s.%s" % (cls.__module__, cls.__name__, method_name)
            def test_body():
                instance = cls()
                instance.setUp()
                try:
                    getattr(instance, method_name)()
                finally:
                    instance.tearDown()
            def test_wrapper(results):
                run_single_test(results, test_name, test_body)
            suite.addTest(test_wrapper)

        for method_name, method in inspect.getmembers(cls):
            if method_name.startswith("test_"):
                add_test(method_name)
        return suite

    def setUp(self):
        self._on_teardown = []

    def on_teardown(self, callback):
        self._on_teardown.append(callback)

    def make_temp_dir(self):
        temp_dir = tempfile.mkdtemp(prefix="unittest-%s" %
                                    self.__class__.__module__)
        self.on_teardown(lambda: shutil.rmtree(temp_dir))
        return temp_dir

    def tearDown(self):
        for callback in reversed(self._on_teardown):
            callback()

    def assertEquals(self, x, y):
        if not x == y:
            raise AssertionError("%r != %r" % (x, y))

    def assertNotEquals(self, x, y):
        if x == y:
            raise AssertionError("%r == %r" % (x, y))

    def assertTrue(self, x):
        self.assertEquals(x, True)

    def assertFalse(self, x):
        self.assertEquals(x, False)


class CombinationTestCase(TestCase):

    @classmethod
    def get_test_suite(cls):
        suite = unittest.TestSuite()

        def add_test(method_names):
            test_name = "%s.%s.%s" % (cls.__module__, cls.__name__,
                                      "/".join(method_names))
            def test_body():
                instance = cls()
                instance.setUp()
                try:
                    for method_name in method_names:
                        getattr(instance, method_name)()
                finally:
                    instance.tearDown()
            def test_wrapper(results):
                run_single_test(results, test_name, test_body)
            suite.addTest(test_wrapper)

        for method_names in cls.get_method_combinations():
            add_test(method_names)
        return suite

    @classmethod
    def get_method_combinations(cls):
        def by_prefix(prefix):
            return [method_name
                    for method_name, unused in inspect.getmembers(cls)
                    if method_name.startswith(prefix)]
        for setup_method in by_prefix("setup_"):
            for test_method in by_prefix("test_"):
                yield [setup_method, test_method]


# Similar logic to unittest.TestCase.run(), except that
# unittest.TestCase.run() treats exceptions in setUp/tearDown()
# differently for the purposes of failure/error classification.  But
# that classification is not terribly important.  setUp/tearDown() are
# not primitive concepts any more.
def run_single_test(test_results, name, test_func):
    test_name = TestCaseName(name)
    test_results.startTest(test_name)
    try:
        try:
            test_func()
        except KeyboardInterrupt:
            raise
        except AssertionError:
            test_results.addFailure(test_name, sys.exc_info())
        except:
            test_results.addError(test_name, sys.exc_info())
        else:
            test_results.addSuccess(test_name)
    finally:
        test_results.stopTest(test_name)


class TestCaseName(object):

    # unittest.TestResult expects to get passed test instances, in
    # order to get names of tests.  In our system, tests and their
    # names are decoupled.  This class gives us something to pass to
    # TestResult.

    failureException = AssertionError

    def __init__(self, name_string):
        self._name = name_string

    def __str__(self):
        return self._name

    def shortDescription(self):
        return None


def load_tests_from_module(module):
    suite = unittest.TestSuite()
    for name, obj in module.__dict__.iteritems():
        if isinstance(obj, type) and issubclass(obj, TestCaseBase):
            suite.addTest(obj.get_test_suite())
    return suite
