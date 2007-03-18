
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


_forwarders = 0

def inc_forwarders_count():
    global _forwarders
    _forwarders += 1

def dec_forwarders_count():
    global _forwarders
    _forwarders -= 1


def run_server():
    """Run the top-level loop.  Returns when this process is no longer
    exporting any object references.
    """
    if _use_glib:
        import gobject
        while plash_core.cap_server_exporting():
            gobject.main_context_default().iteration()
    else:
        plash_core.run_server()
