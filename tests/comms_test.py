
import gobject
import socket
import unittest

import plash_core
import plash.mainloop
import plash.marshal


class CommsTest(unittest.TestCase):

    def test_freeing_last_reference(self):
        sock_pair = socket.socketpair()
        # Junk data should not be read because the connection is
        # closed down first.
        sock_pair[1].send("junk data")
        sock_pair[1].close()
        method, args = plash.marshal.unpack(
            plash_core.cap_make_connection.cap_call(
                plash.marshal.pack("make_conn2",
                                   plash_core.wrap_fd(sock_pair[0].fileno()),
                                   1, [])))
        assert method == "r_make_conn2"
        del args
        # "may_block=False" argument to iteration() method not supported.
        # Adding an idle callback is a workaround.
        def idle_callback():
            return False # remove callback
        gobject.idle_add(idle_callback)
        gobject.main_context_default().iteration()


if __name__ == "__main__":
    unittest.main()
