
INCLUDES = $(LIBPLASH_CFLAGS) $(GTK_CFLAGS)

lib_LTLIBRARIES = libgtk-powerbox-hook.la

libgtk_powerbox_hook_la_CFLAGS = -Wall
libgtk_powerbox_hook_la_LDFLAGS = -avoid-version -Wl,-z,defs
libgtk_powerbox_hook_la_LIBADD = $(LIBPLASH_LIBS) $(GTK_LIBS)
libgtk_powerbox_hook_la_SOURCES = gtk-powerbox.c powerbox.c
