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

import gc
import inspect
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import types
import unittest


# This is a hack that unittest uses to prune tracebacks.
# Stack frames from modules with this set are omitted.
__unittest = 1


class TestCaseBase(object):

    @classmethod
    def get_test_cases(cls):
        raise NotImplementedError()


class TestCase(TestCaseBase):

    @classmethod
    def get_test_cases(cls):
        tests = []

        def add_test(method_name):
            test_name = "%s.%s.%s" % (cls.__module__, cls.__name__, method_name)
            def test_body():
                instance = cls()
                instance.setUp()
                try:
                    getattr(instance, method_name)()
                    instance.check_for_pending_failures()
                finally:
                    instance.tearDown()
            tests.append((test_name, test_body))

        for method_name, method in inspect.getmembers(cls):
            if method_name.startswith("test_"):
                add_test(method_name)
        return tests

    def setUp(self):
        self.__on_teardown = []
        self.__pending_failures = []

    def mark_failure(self, message):
        self.__pending_failures.append(message)

    def check_for_pending_failures(self):
        if len(self.__pending_failures) > 0:
            raise AssertionError("Queued errors: %r" % self.__pending_failures)

    def on_teardown(self, callback):
        self.__on_teardown.append(callback)

    def make_temp_dir(self):
        temp_dir = tempfile.mkdtemp(prefix="unittest-%s" %
                                    self.__class__.__module__)
        self.on_teardown(lambda: shutil.rmtree(temp_dir))
        return temp_dir

    def tearDown(self):
        for callback in reversed(self.__on_teardown):
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

    def assertRaises(self, exception_class, func, *args, **kwargs):
        try:
            func(*args, **kwargs)
        except exception_class:
            pass
        else:
            raise AssertionError("Did not raise %r" % exception_class)


class CombinationTestCase(TestCase):

    @classmethod
    def get_test_cases(cls):
        tests = []

        def add_test(method_names):
            test_name = "%s.%s.%s" % (cls.__module__, cls.__name__,
                                      "/".join(method_names))
            def test_body():
                instance = cls()
                instance.setUp()
                try:
                    for method_name in method_names:
                        getattr(instance, method_name)()
                    instance.check_for_pending_failures()
                finally:
                    instance.tearDown()
            tests.append((test_name, test_body))

        for method_names in cls.get_method_combinations():
            add_test(method_names)
        return tests

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


def get_fd_set():
    # os.listdir() will return the FD for the temporarily-opened
    # directory, so we need to filter afterwards.
    return set(int(fd) for fd in os.listdir("/proc/self/fd")
               if os.path.exists("/proc/self/fd/%s" % fd))


def get_fd_description(fd):
    return os.readlink("/proc/self/fd/%i" % fd)


def check_for_fd_leak(func):
    garbage_before = gc.garbage[:]
    fds_before = get_fd_set()
    func()
    while True:
        fds_after = get_fd_set()
        fds_leaked = fds_after.difference(fds_before)
        if len(fds_leaked) == 0:
            break
        # In the future it might be useful to record which tests
        # created cycles that required collecting.
        # Also, running a collection is relatively slow, so these leak
        # checks could be made optional.
        collected = gc.collect()
        if collected == 0:
            break
    garbage_after = gc.garbage[:]
    garbage_leaked = set(garbage_after).difference(garbage_before)
    if len(garbage_leaked) > 0:
        # This leak could be attributed to the wrong test if the test
        # responsible does not trigger a collection but a later one
        # does.
        raise AssertionError(
            "Unfreeable cycles leaked (found in gc.garbage): %r" %
            garbage_leaked)
    if len(fds_leaked) > 0:
        descriptions = [(fd, get_fd_description(fd)) for fd in fds_leaked]
        raise AssertionError("FDs leaked: %s" % descriptions)


def strace_wrapper(func):
    proc = subprocess.Popen(["strace", "-p", str(os.getpid())])
    # There is a race condition here: we want to wait until strace has
    # attached to this process.
    time.sleep(0.1)
    try:
        func()
    finally:
        os.kill(proc.pid, signal.SIGTERM)


def get_cases_from_unittest_class(cls):
    tests = []

    def add_test(method_name):
        test_name = "%s.%s.%s" % (cls.__module__, cls.__name__, method_name)
        def test_body():
            instance = cls(method_name) # Argument not actually used
            instance.setUp()
            try:
                getattr(instance, method_name)()
            finally:
                instance.tearDown()
        tests.append((test_name, test_body))

    for method_name in dir(cls):
        if method_name.startswith("test"):
            add_test(method_name)
    return tests


def make_test_suite_from_cases(tests):
    suite = unittest.TestSuite()
    def add_test(test_name, test_func):
        def test_wrapper(results):
            run_single_test(results, test_name,
                            lambda: check_for_fd_leak(test_func))
        suite.addTest(test_wrapper)
    for test_name, test_func in tests:
        add_test(test_name, test_func)
    return suite


def load_tests_from_module(module):
    suite = unittest.TestSuite()
    for name, obj in module.__dict__.iteritems():
        if isinstance(obj, type) and issubclass(obj, TestCaseBase):
            suite.addTest(make_test_suite_from_cases(obj.get_test_cases()))
        elif (isinstance(obj, (type, types.ClassType)) and
              issubclass(obj, unittest.TestCase)):
            suite.addTest(make_test_suite_from_cases(
                    get_cases_from_unittest_class(obj)))
    return suite
