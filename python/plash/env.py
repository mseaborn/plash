
import string
import os
import plash_core

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

def get_return_cont():
    return get_caps()['return_cont']
