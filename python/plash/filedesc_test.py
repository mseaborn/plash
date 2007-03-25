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

import errno
import os
import sys
import traceback
import unittest

# Ensure that we don't need X to run
del os.environ["DISPLAY"]

import gobject

import plash.filedesc
import plash.mainloop


def forked(func):
    """Runs function in child process."""
    pid = os.fork()
    if pid == 0:
        try:
            func()
        except:
            try:
                traceback.print_exc()
            except:
                pass
        os._exit(1)
    return pid


class Proc(object):
    
    """Creates a subprocess that forks immediately but only runs when
    we explicitly tell it to continue.  This allows us to close FDs in
    the parent before the child continues, which makes it possible to
    guarantee that the child will see a broken pipe."""

    def __init__(self, func):
        pipe_read, pipe_write = os.pipe()
        def process():
            os.close(pipe_write)
            os.read(pipe_read, 1)
            func()
        self.pid = forked(process)
        self.pipe_write = pipe_write

    def start(self):
        os.write(self.pipe_write, "x")
        os.close(self.pipe_write)


class ProcSet(object):

    """Wait for a set of processes to exit successfully."""

    def __init__(self):
        self._pids = {}
        self._count = 0

    def add(self, pid):
        self._pids[pid] = None
        self._count += 1

    def wait(self):
        while self._count > 0:
            pid, status = os.wait()
            assert pid in self._pids
            assert self._pids[pid] is None
            self._count -= 1
            self._pids[pid] = status
            assert os.WIFEXITED(status)
            assert os.WEXITSTATUS(status) == 0


# Returns readable FD
def echo(text):
    pipe_read, pipe_write = os.pipe()
    def process():
        os.close(pipe_read)
        os.write(pipe_write, text)
        os._exit(0)
    proc = Proc(process)
    os.close(pipe_write)
    return proc, pipe_read

def mainloop():
    while plash.mainloop._forwarders > 0:
        sys.stdout.flush()
        gobject.main_context_default().iteration()

def forward(fd):
    pipe_read, pipe_write = os.pipe()
    def process():
        os.close(pipe_read)
        plash.filedesc.ForwardFD(fd, pipe_write, buf_size=2)
        mainloop()
        os._exit(0)
    proc = Proc(process)
    os.close(pipe_write)
    os.close(fd)
    return proc, pipe_read


class ForwardTest(unittest.TestCase):

    def test_forwarding(self):
        pids = ProcSet()
        proc1, tmp_fd = echo("hello world")
        proc2, fd = forward(tmp_fd)
        for p in [proc1, proc2]:
            pids.add(p.pid)
            p.start()
        got = ""
        while True:
            buf = os.read(fd, 100)
            if len(buf) == 0:
                break
            got += buf
        self.assertEquals(got, "hello world")
        pids.wait()

    def test_epipe(self):
        """Check that we get EPIPE."""
        pipe_read, pipe_write = os.pipe()
        os.close(pipe_read)
        try:
            os.write(pipe_write, "text")
        except OSError, ex:
            self.assertEquals(ex.errno, errno.EPIPE)
        else:
            self.fail("Didn't get EPIPE")

    def test_broken_pipe(self):
        """Check that forwarding to a broken pipe does not kill the
        server or cause a traceback to be printed."""
        pids = ProcSet()
        proc1, tmp_fd = echo("hello world")
        proc2, fd = forward(tmp_fd)
        os.close(fd)
        for p in [proc1, proc2]:
            pids.add(p.pid)
            p.start()
        pids.wait()

    # TODO: check that the forwarder does not block when the pipe's
    # reader is not reading


if __name__ == "__main__":
    unittest.main()
