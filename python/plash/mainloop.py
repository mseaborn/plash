# Copyright (C) 2006, 2007 Mark Seaborn
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

import plash_core

_use_glib = False

def use_gtk_mainloop():
    use_glib_mainloop()

def use_glib_mainloop():
    """Enable use of Gtk/Glib's top-level event loop, instead of
    Plash's built-in event loop.
    """
    global _use_glib
    _use_glib = True


_reasons_count = 0

class ReasonToRun(object):

    def __init__(self):
        global _reasons_count
        _reasons_count += 1
        self._alive = True

    def __del__(self):
        self.dispose()

    def dispose(self):
        if self._alive:
            self._alive = False
            global _reasons_count
            _reasons_count -= 1


def run_server():
    """Run the top-level loop.  Returns when this process is no longer
    exporting any object references.
    """
    if _use_glib:
        import gobject
        while plash_core.cap_server_exporting() or _reasons_count > 0:
            gobject.main_context_default().iteration()
    else:
        plash_core.run_server()
