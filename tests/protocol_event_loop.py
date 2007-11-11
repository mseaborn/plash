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


class FDWatch(object):

    def __init__(self, parent_watch_list, fd, get_flags, callback,
                 error_callback):
        self._parent_watch_list = parent_watch_list
        self.fd = fd
        self.get_flags = get_flags
        self.callback = callback
        self.error_callback = error_callback
        self.destroyed = False
        parent_watch_list.append(self)

    def remove_watch(self):
        if not self.destroyed:
            self.destroyed = True
            self._parent_watch_list.remove(self)


def poll_fds(fd_flags, timeout=None):
    poller = select.poll()
    for fd, flags in fd_flags.iteritems():
        poller.register(fd, flags)
    return poller.poll(timeout)


REQUESTABLE_FLAGS = select.POLLIN | select.POLLOUT | select.POLLPRI

ERROR_FLAGS = select.POLLERR | select.POLLHUP | select.POLLNVAL


class EventLoop(object):

    # The design of select.poll prevents us from reusing the poll
    # instance.

    def __init__(self):
        self._watches = []

    def make_watch(self, fd, get_flags, callback):
        def error_callback(flags):
            pass
        return FDWatch(self._watches, fd, get_flags, callback, error_callback)

    def make_error_watch(self, fd, error_callback):
        def get_flags():
            return 0
        def ready_callback(flags):
            pass
        return FDWatch(self._watches, fd, get_flags, ready_callback,
                       error_callback)

    def _get_fd_flags(self):
        fd_flags = {}
        for watch in self._watches:
            add_flags = watch.get_flags()
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
        ready = poll_fds(self._get_fd_flags(), 0)
        return len(ready) == 0

    def once(self):
        ready = dict(poll_fds(self._get_fd_flags()))
        self._process_ready(ready)

    def once_safely(self):
        ready = dict(poll_fds(self._get_fd_flags(), 0))
        assert len(ready) != 0, self._get_fd_flags()
        self._process_ready(ready)

    def run_awhile(self):
        while True:
            ready = dict(poll_fds(self._get_fd_flags(), 0))
            if len(ready) == 0:
                break
            self._process_ready(ready)

    def _process_ready(self, ready):
        for watch in self._watches:
            fd = watch.fd.fileno()
            if fd in ready:
                flags = ready[fd]
                # Assumes the watch has not been changed since _get_fd_flags()
                relevant_flags = flags & watch.get_flags()
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
