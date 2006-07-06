
import string
import os
import plash

def get_caps_nomemoize():
    names = string.split(os.environ['PLASH_CAPS'], ';')
    fd = plash.libc_duplicate_connection()
    caps_list = plash.cap_make_connection.make_conn2(fd, len(names), [])
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

def get_return_cont():
    return get_caps()['return_cont']
