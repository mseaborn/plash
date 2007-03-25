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

import string
import os

import plash_core
import plash.marshal


# Whether we are running under the Plash environment
under_plash = 'PLASH_CAPS' in os.environ

def get_caps_nomemoize():
    names = string.split(os.environ['PLASH_CAPS'], ';')
    fd = plash_core.libc_duplicate_connection()
    caps_list = plash_core.cap_make_connection.make_conn2(fd, len(names), [])
    caps = {}
    for i in range(len(names)):
        caps[names[i]] = caps_list[i]
    return caps

got_caps = None
def get_caps():
    global got_caps
    if got_caps == None:
        got_caps = get_caps_nomemoize()
    return got_caps

def get_root_dir():
    if under_plash:
        return get_caps()['fs_op'].fsop_get_root_dir()
    else:
        return plash_core.initial_dir('/')

def get_dir_from_path(pathname):
    if under_plash:
        return get_caps()["fs_op"].fsop_get_dir(pathname)
    else:
        return plash_core.initial_dir(pathname)

def get_return_cont():
    return get_caps()['return_cont']
