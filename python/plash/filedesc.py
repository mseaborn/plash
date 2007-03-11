
import gobject
import os
import select

import plash.mainloop
import plash_core


class ForwardFD(object):

    def __init__(self, src_fd, dest_fd, buf_size=1024):
        self._src_fd = src_fd
        self._dest_fd = dest_fd
        self._buf_size = buf_size
        self._buf = ""
        self._ended = False
        plash.mainloop.use_glib_mainloop()
        plash.mainloop.inc_forwarders_count()
        self._read()

    def _read(self):
        gobject.io_add_watch(self._src_fd,
                             gobject.IO_IN | gobject.IO_PRI |
                             gobject.IO_ERR | gobject.IO_HUP,
                             self._read_callback)

    def _read_callback(self, source, cb_condition):
        assert len(self._buf) == 0
        try:
            data = os.read(self._src_fd, self._buf_size)
        except:
            self._tidy_up()
            raise
        if len(data) == 0:
            # End of stream
            self._tidy_up()
        else:
            self._buf = data
            gobject.io_add_watch(self._dest_fd,
                                 gobject.IO_OUT |
                                 gobject.IO_ERR | gobject.IO_HUP,
                                 self._write_callback)
        # Remove from event handler list
        return False

    def _write_callback(self, dest, cb_condition):
        if cb_condition & gobject.IO_ERR:
            self._tidy_up()
            return False
        if len(self._buf) == 0:
            return False
        try:
            written = os.write(self._dest_fd, self._buf)
        except:
            self._tidy_up()
            raise
        else:
            if written == len(self._buf):
                self._buf = ""
                self._read()
                # Remove from event handler list
                return False
            else:
                # More left to write, so leave in handler list
                self._buf = self._buf[written:]
                return True

    def _tidy_up(self):
        plash.mainloop.dec_forwarders_count()
        os.close(self._src_fd)
        os.close(self._dest_fd)
        self._ended = True

    def flush(self):
        if not self._ended:
            rlist, wlist, xlist = \
                   select.select([self._src_fd], [self._dest_fd], [], 0)
            if self._src_fd in rlist and self._dest_fd in wlist:
                data = os.read(self._src_fd, self._buf_size)
                self._buf += data
                written = os.write(self._dest_fd, self._buf)
                self._buf = self._buf[written:]
            self._tidy_up()


def proxy_input_fd(fd):
    pipe_read, pipe_write = os.pipe()
    forwarder = ForwardFD(fd, pipe_write)
    return (plash_core.wrap_fd(pipe_read), forwarder)

def proxy_output_fd(fd):
    pipe_read, pipe_write = os.pipe()
    forwarder = ForwardFD(pipe_read, fd)
    return (plash_core.wrap_fd(pipe_write), forwarder)
