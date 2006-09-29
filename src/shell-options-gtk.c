/* Copyright (C) 2004, 2005 Mark Seaborn

   This file is part of Plash.

   Plash is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   Plash is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with Plash; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
   USA.  */

#include <gtk/gtk.h>

#include "cap-utils.h"
#include "shell-options.h"


struct window_info {
  cap_t opts;
  GtkWidget *window;
  GtkWidget *o_log_summary;
  GtkWidget *o_log_messages;
  GtkWidget *o_log_into_xterm;
  GtkWidget *o_print_fs_tree;
  GtkWidget *o_enable_x11;
};

void window_ok(GtkWidget *widget, gpointer data)
{
  struct window_info *w = (void *) data;
  cap_t opts = w->opts;
  region_t r = region_make();

  set_option(r, opts, "log_summary",
	     gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->o_log_summary)));
  set_option(r, opts, "log_messages",
	     gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->o_log_messages)));
  set_option(r, opts, "log_into_xterm",
	     gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->o_log_into_xterm)));
  set_option(r, opts, "print_fs_tree",
	     gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->o_print_fs_tree)));
  set_option(r, opts, "enable_x11",
	     gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->o_enable_x11)));
  region_free(r);

  /*
  w->state->log_summary = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->o_log_summary));
  w->state->log_messages = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->o_log_messages));
  w->state->log_into_xterm = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->o_log_into_xterm));
  w->state->print_fs_tree = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->o_print_fs_tree));
  */
  gtk_widget_destroy(w->window);
}

gint window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
  return TRUE;
}

void window_destroy(GtkWidget *widget, gpointer data)
{
  gtk_main_quit();
}

void option_window(cap_t opts)
{
  region_t r = region_make();
  struct window_info w;
  GtkWidget *widget;
  GtkWidget *vbox;
  GtkWidget *button;

  w.opts = opts;
  
  w.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(G_OBJECT(w.window), "delete_event",
		   G_CALLBACK(window_delete_event), NULL);
  g_signal_connect(G_OBJECT(w.window), "destroy",
		   G_CALLBACK(window_destroy), NULL);

  widget = gtk_check_button_new_with_label(_("Log calls, single line each"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
			       get_option(r, opts, "log_summary"));
  gtk_widget_show(widget);
  w.o_log_summary = widget;

  widget = gtk_check_button_new_with_label(_("Log messages in/out"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
			       get_option(r, opts, "log_messages"));
  gtk_widget_show(widget);
  w.o_log_messages = widget;

  widget = gtk_check_button_new_with_label(_("Send log data to new xterm"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
			       get_option(r, opts, "log_into_xterm"));
  gtk_widget_show(widget);
  w.o_log_into_xterm = widget;

  widget = gtk_check_button_new_with_label(_("Print constructed filesystem tree"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
			       get_option(r, opts, "print_fs_tree"));
  gtk_widget_show(widget);
  w.o_print_fs_tree = widget;

  widget = gtk_check_button_new_with_label(_("Grant access to X11 Window System by default"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
			       get_option(r, opts, "enable_x11"));
  gtk_widget_show(widget);
  w.o_enable_x11 = widget;

  button = gtk_button_new_with_label(_("Okay"));
  gtk_widget_show(button);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(window_ok), &w);
  
  vbox = gtk_vbox_new(0, 5);
  gtk_box_pack_start(GTK_BOX(vbox), w.o_log_summary, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), w.o_log_messages, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), w.o_log_into_xterm, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), w.o_print_fs_tree, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), w.o_enable_x11, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);

  gtk_container_add(GTK_CONTAINER(w.window), vbox);
  gtk_widget_show(vbox);
  
  gtk_widget_show(w.window);

  region_free(r);
  
  gtk_main();
}


#define PROG_NAME "plash-opts-gtk"
#define PROG PROG_NAME ": "

int main(int argc, char *argv[])
{
  region_t r;
  cap_t fs_server;

  if(argc != 2) {
    printf(_("Usage: %s /x=shell_options\n"), PROG_NAME);
    return 1;
  }

  if(!gtk_init_check(&argc, &argv)) {
    fprintf(stderr, _("failed to initialise gtk\n"));
    return 1;
  }

  if(get_process_caps("fs_op", &fs_server, 0) < 0) return 1;

  {
    cap_t options_obj;
    struct cap_args result;

    r = region_make();
    cap_call(fs_server, r,
	     cap_args_make(cat2(r, mk_string(r, "Gobj"),
				mk_string(r, argv[1])),
			   caps_empty, fds_empty),
	     &result);
    filesys_obj_free(fs_server);
    if(expect_cap1(result, &options_obj) < 0) {
      fprintf(stderr, PROG _("couldn't get options object\n"));
      return 1;
    }
    option_window(options_obj);
  }
  return 0;
}
