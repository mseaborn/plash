
import plash_core

_use_gtk = False

def use_gtk_mainloop():
    """Enable use of Gtk/Glib's top-level event loop, instead of
    Plash's built-in event loop.
    """
    global _use_gtk
    _use_gtk = True

def use_glib_mainloop():
    use_gtk_mainloop()


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
    if _use_gtk:
        import gtk
        while plash_core.cap_server_exporting():
            gtk.main_iteration()
    else:
        plash_core.run_server()
