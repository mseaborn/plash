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

import select

import gobject


class FDWatch(object):

    def __init__(self, parent_watch_list, fd, get_flags, callback,
                 error_callback):
        self._parent_watch_list = parent_watch_list
        self.fd = fd
        self.get_flags = get_flags
        self.flags = get_flags()
        self.callback = callback
        self.error_callback = error_callback
        self.destroyed = False
        parent_watch_list.append(self)

    def update_flags(self):
        self.flags = self.get_flags()

    def remove_watch(self):
        if not self.destroyed:
            self.destroyed = True
            self._parent_watch_list.remove(self)
            # Unset the FD so that it can be GC'd
            self.fd = None
            self.get_flags = lambda: 0
            self.callback = lambda flags: None
            self.error_callback = lambda flags: None


class GlibFDWatch(object):

    def __init__(self, parent_watch_list, fd, get_flags, callback,
                 error_callback):
        self._parent_watch_list = parent_watch_list
        self._fd = fd
        self._get_flags = get_flags
        self._flags = get_flags()
        self._callback = callback
        self._error_callback = error_callback
        self._id = None
        self.destroyed = False
        parent_watch_list.append(self)

    def unregister(self):
        if self._id is not None:
            gobject.source_remove(self._id)
            self._id = None

    def register(self):
        self.unregister()
        if not self.destroyed:
            self._id = gobject.io_add_watch(
                self._fd.fileno(), self._flags | ERROR_FLAGS, self._handler)

    def update_flags(self):
        old_flags = self._flags
        self._flags = self._get_flags()
        if old_flags != self._flags and self._id is not None:
            self.register()

    def _handler(self, fd_unused, flags):
        relevant_flags = flags & self._flags
        if relevant_flags != 0:
            self._callback(relevant_flags)
        if flags & ERROR_FLAGS != 0:
            self._error_callback(flags & ERROR_FLAGS)
            self._id = None
            self.remove_watch()
            return False # remove handler
        else:
            return True # keep handler

    def remove_watch(self):
        if not self.destroyed:
            self.destroyed = True
            self._parent_watch_list.remove(self)
            self.unregister()
            # Unset the FD so that it can be GC'd
            self._fd = None
            self._get_flags = lambda: 0
            self._callback = lambda flags: None
            self._error_callback = lambda flags: None


def poll_fds(fd_flags, timeout=None):
    poller = select.poll()
    for fd, flags in fd_flags.iteritems():
        poller.register(fd, flags)
    return poller.poll(timeout)


REQUESTABLE_FLAGS = select.POLLIN | select.POLLOUT | select.POLLPRI

ERROR_FLAGS = select.POLLERR | select.POLLHUP | select.POLLNVAL

assert gobject.IO_IN == select.POLLIN
assert gobject.IO_OUT == select.POLLOUT
assert gobject.IO_PRI == select.POLLPRI
assert gobject.IO_ERR == select.POLLERR
assert gobject.IO_HUP == select.POLLHUP
assert gobject.IO_NVAL == select.POLLNVAL


class EventLoopBase(object):

    def make_watch(self, fd, get_flags, callback):
        def error_callback(flags):
            pass
        return self.make_watch_with_error_handler(fd, get_flags, callback,
                                                  error_callback)

    def make_error_watch(self, fd, error_callback):
        def get_flags():
            return 0
        def ready_callback(flags):
            pass
        return self.make_watch_with_error_handler(fd, get_flags, ready_callback,
                                                  error_callback)


class EventLoop(EventLoopBase):

    # The design of select.poll prevents us from reusing the poll
    # instance.

    def __init__(self, poll_fds=poll_fds):
        self._watches = []
        self._poll_fds = poll_fds

    def make_watch_with_error_handler(self, fd, get_flags, callback,
                                      error_callback):
        return FDWatch(self._watches, fd, get_flags, callback, error_callback)

    def _get_fd_flags(self):
        fd_flags = {}
        for watch in self._watches:
            add_flags = watch.flags
            assert add_flags & ~REQUESTABLE_FLAGS == 0, add_flags
            # We add the FD to the poll set event if no flags are set,
            # because we can still receive results for POLLERR, POLLHUP
            # and POLLNVAL, which are not optional.
            fd = watch.fd.fileno()
            # Multiple watches may not register to handle the same
            # half (input or output) of an FD.  An input or output
            # watch must always do some input or output when the FD is
            # ready (otherwise we'd have a busy loop), and it's not
            # possible for two watches to do that.  This is just a
            # sanity check: it does not detect the case where one FD
            # is dupped into two FD slots.
            assert fd_flags.get(fd, 0) & add_flags == 0
            fd_flags[fd] = fd_flags.get(fd, 0) | add_flags
        return fd_flags

    def is_listening(self):
        # Whether there are any events we're currently interested in.
        # If not, the loop would block forever.
        return len(self._get_fd_flags()) > 0

    def will_block(self):
        ready = self._poll_fds(self._get_fd_flags(), 0)
        return len(ready) == 0

    def once(self):
        ready = dict(self._poll_fds(self._get_fd_flags()))
        self._process_ready(ready)

    def once_safely(self):
        ready = dict(self._poll_fds(self._get_fd_flags(), 0))
        assert len(ready) != 0, self._get_fd_flags()
        self._process_ready(ready)

    def run(self):
        while True:
            ready = dict(self._poll_fds(self._get_fd_flags()))
            self._process_ready(ready)

    def run_awhile(self):
        while True:
            ready = dict(self._poll_fds(self._get_fd_flags(), 0))
            if len(ready) == 0:
                break
            self._process_ready(ready)

    def _process_ready(self, ready):
        for watch in self._watches:
            fd = watch.fd.fileno()
            if fd in ready:
                flags = ready[fd]
                # Assumes the watch has not been changed since _get_fd_flags()
                relevant_flags = flags & watch.flags
                if relevant_flags != 0:
                    self._assert_still_relevant(fd, relevant_flags)
                    watch.callback(relevant_flags)
                if flags & ERROR_FLAGS != 0:
                    watch.error_callback(flags & ERROR_FLAGS)
                    watch.remove_watch()

    def _assert_still_relevant(self, fd, expected_flags):
        # Be paranoid, and check flags again.  A handler could have a
        # side effect of causing the earlier poll results to no longer
        # be relevant.
        poller = select.poll()
        poller.register(fd, expected_flags)
        ready = poller.poll(0)
        assert len(ready) == 1, ready
        fd2, current_flags = ready[0]
        assert fd2 == fd
        assert current_flags & expected_flags == expected_flags


class GlibEventLoop(EventLoopBase):

    def __init__(self):
        self._watches = []
        self._registered = True

    def make_watch_with_error_handler(self, fd, get_flags, callback,
                                      error_callback):
        watch = GlibFDWatch(self._watches, fd, get_flags, callback,
                            error_callback)
        if self._registered:
            watch.register()
        return watch

    def once_safely(self):
        may_block = False
        did_something = gobject.main_context_default().iteration(may_block)
        assert did_something

    def run(self):
        while True:
            may_block = True
            gobject.main_context_default().iteration(may_block)

    def run_awhile(self):
        while True:
            may_block = False
            did_something = gobject.main_context_default().iteration(may_block)
            if not did_something:
                break

    def will_block(self):
        return not gobject.main_context_default().pending()

    def is_listening(self):
        return len(self._watches) > 0

    def register(self):
        for watch in self._watches:
            watch.register()
        self._registered = True

    def unregister(self):
        for watch in self._watches:
            watch.unregister()
        self._registered = False
