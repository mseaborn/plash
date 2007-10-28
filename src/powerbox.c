/* Copyright (C) 2005 Mark Seaborn

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

#include <stdio.h>
#include <unistd.h>

#include <gtk/gtkwindow.h>
#include <gtk/gtk.h>
#include <glib.h>

#include "region.h"
#include "filesysobj.h"
#include "cap-protocol.h"
#include "cap-utils.h"
#include "serialise.h"
#include "marshal.h"
#include "build-fs.h"


char *region_strdup(region_t r, const char *str)
{
  int len = strlen(str) + 1;
  char *copy = region_alloc(r, len);
  memcpy(copy, str, len);
  return copy;
}

char *region_printf(region_t r, const char *fmt, ...)
{
  char *text, *copy;
  int got;
  va_list list;
  va_start(list, fmt);
  got = vasprintf(&text, fmt, list);
  assert(got >= 0);
  va_end(list);
  copy = region_strdup(r, text);
  free(text);
  return copy;
}

DECLARE_VTABLE(powerbox_vtable);
struct powerbox {
  struct filesys_obj hdr;
  char *app_pet_name;
  /* Note that the Gtk code uses libc to access the filesystem, so
     user_root needs to be the same as libc's root directory!
     It is a parameter here for convenience only. */
  struct filesys_obj *user_root; /* The user's namespace */
  struct node *app_root; /* The application process's namespace */
};

struct powerbox_window {
  struct powerbox *pb;
  cap_t return_cont;
  GtkWidget *window;
  GtkWidget *read_only;
};

/* object-type: file or dir or either */
/* ("wantDir", ())
   ("File", ())
   ("Exts", list of extensions)
   ("Dir", dir)
   ("Leaf", ..)
   ("save")
   ("multiple")
   ("read-only")

   =>
   ("filename", ..)

   cancel
*/

/* user_root and app_root are owning references.
   pet_name is a non-owning reference. */
cap_t powerbox_make(const char *pet_name,
		    struct filesys_obj *user_root,
		    struct node *app_root)
{
  struct powerbox *pb = filesys_obj_make(sizeof(struct powerbox), &powerbox_vtable);
  pb->app_pet_name = pet_name ? strdup(pet_name) : "";
  pb->user_root = user_root;
  pb->app_root = app_root;
  return (cap_t) pb;
}

void powerbox_free(struct filesys_obj *obj1)
{
  struct powerbox *pb = (void *) obj1;
  free(pb->app_pet_name);
  filesys_obj_free(pb->user_root);
  free_node(pb->app_root);
}

static void dialog_response(GtkDialog *dialog, gint arg, gpointer obj)
{
  struct powerbox_window *w = (struct powerbox_window *) obj;
  struct powerbox *pb = w->pb;
  region_t r = region_make();
  
  if(arg == GTK_RESPONSE_ACCEPT) {
    int ro = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->read_only));
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(w->window));
    if(filename) {
      int err;
      if(fs_resolve_populate(pb->user_root, pb->app_root, NULL /* cwd */,
			     seqf_string(filename),
			     (ro ? FS_READ_ONLY : FS_SLOT_RWC)
			     | FS_FOLLOW_SYMLINKS, &err) < 0) {
	fprintf(stderr, _("powerbox: error in resolving filename \"%s\": %s\n"),
		filename, strerror(err));
      }
      else {
	cap_invoke(w->return_cont,
		   cap_args_d(cat2(r, mk_int(r, METHOD_POWERBOX_RESULT_FILENAME),
				   mk_string(r, filename))));
      }
      g_free(filename);
    }
  }
  else if(arg == GTK_RESPONSE_CANCEL) {
    cap_invoke(w->return_cont,
	       cap_args_d(mk_int(r, METHOD_POWERBOX_RESULT_CANCEL)));
  }
  else {
    fprintf(stderr, "powerbox: unknown response code\n");
  }
  gtk_widget_destroy(w->window);
  filesys_obj_free(w->return_cont);
  filesys_obj_free((cap_t) w->pb);
  free(w);
  region_free(r);
}

void powerbox_invoke(struct filesys_obj *obj1, struct cap_args args)
{
  struct powerbox *pb = (struct powerbox *) obj1;
  region_t r = region_make();
  {
    seqf_t data = flatten_reuse(r, args.data);
    bufref_t args_ref;
    int ok = 1;
    m_int_const(&ok, &data, METHOD_CALL);
    m_int_const(&ok, &data, METHOD_POWERBOX_REQ_FILENAME);
    m_int(&ok, &data, &args_ref);
    if(ok && args.caps.size == 1 && args.fds.count == 0) {
      int i, size;
      const bufref_t *a;

      cap_t return_cont = args.caps.caps[0];
      struct arg_m_buf argbuf =
	{ data, { args.caps.caps+1, args.caps.size-1 }, args.fds };

      struct powerbox_window *w;
      GtkWidget *vbox, *label;
      const char *start_dir = "/";
      const char *desc = "<No reason given>";
      int save_mode = 0;
      int want_dir = 0;
      GtkFileChooserAction action;
      const char *title;
      const char *button_text;
      int parent_window_id = 0;

      if(argm_array(&argbuf, args_ref, &size, &a)) goto error;
      for(i = 0; i < size; i++) {
	const bufref_t *args;
	int arity;
	seqf_t tag;
	if(argm_array(&argbuf, a[i], &arity, &args) ||
	   arity < 1 ||
	   argm_str(&argbuf, args[0], &tag)) goto error;

	if(arity == 2 && seqf_equal(tag, seqf_string("Startdir"))) {
	  seqf_t dir;
	  struct filesys_obj *root;
	  struct dir_stack *ds;
	  int err;
	  
	  if(argm_str(&argbuf, args[1], &dir)) goto error;

	  /* Check whether the requested start directory is present in
	     the application's file namespace.  If it's not, we don't
	     use it.  This guards against the application tricking the
	     user into opening a file with an expected name from an
	     unexpected directory. */
	  root = fs_make_root(pb->app_root);
	  ds = resolve_dir(r, root, NULL /* cwd */, dir, SYMLINK_LIMIT, &err);
	  if(ds) {
	    start_dir = region_strdup_seqf(r, dir);
	  }
	  else {
	    fprintf(stderr, "powerbox: not using requested start directory, \"%s\": not in application's namespace\n", region_strdup_seqf(r, dir));
	  }
	  dir_stack_free(ds);
	  filesys_obj_free(root);
	}
	else if(arity == 2 && seqf_equal(tag, seqf_string("Desc"))) {
	  seqf_t text;
	  if(argm_str(&argbuf, args[1], &text)) goto error;
	  desc = region_strdup_seqf(r, text);
	}
	else if(arity == 1 && seqf_equal(tag, seqf_string("Save"))) {
	  save_mode = TRUE;
	}
	else if(arity == 1 && seqf_equal(tag, seqf_string("Wantdir"))) {
	  want_dir = TRUE;
	}
	else if(arity == 2 && seqf_equal(tag, seqf_string("Transientfor"))) {
	  if(argm_int(&argbuf, args[1], &parent_window_id)) goto error;
	}
	else {
	  fprintf(stderr, "powerbox: unknown arg \"%s\"\n",
		  region_strdup_seqf(r, tag));
	  goto error;
	}
      }

      /* The cwd will be somewhere random: whatever was last visited
	 by filesysobj-real.c.  We reset it, because Gtk takes the cwd
	 to present as a location to visit in its navigation bar, even
	 if we do gtk_file_chooser_set_current_folder() below. */
      if(chdir("/") < 0) {
	/* Warning: */
	perror("powerbox: chdir");
      }

      w = amalloc(sizeof(struct powerbox_window));
      w->pb = (struct powerbox *) inc_ref((cap_t) pb);
      w->return_cont = return_cont;
      if(want_dir) {
	if(save_mode) {
	  action = GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER;
	  title = region_printf(r, _("<%s>: Create and select directory"), pb->app_pet_name);
	  button_text = GTK_STOCK_SAVE;
	}
	else {
	  action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
	  title = region_printf(r, _("<%s>: Select directory"), pb->app_pet_name);
	  button_text = GTK_STOCK_OPEN;
	}
      }
      else {
	if(save_mode) {
	  action = GTK_FILE_CHOOSER_ACTION_SAVE;
	  title = region_printf(r, _("<%s>: Save file"), pb->app_pet_name);
	  button_text = GTK_STOCK_SAVE;
	}
	else {
	  action = GTK_FILE_CHOOSER_ACTION_OPEN;
	  title = region_printf(r, _("<%s>: Open file"), pb->app_pet_name);
	  button_text = GTK_STOCK_OPEN;
	}
      }
      w->window =
	gtk_file_chooser_dialog_new(title,
				    NULL /* parent window */,
				    action,
				    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				    button_text, GTK_RESPONSE_ACCEPT,
				    NULL);
      w->read_only = gtk_check_button_new_with_label(_("Grant only read access"));
      gtk_widget_show(w->read_only);

      label = gtk_label_new(desc);
      gtk_misc_set_alignment(GTK_MISC(label), 0, 0); /* Align left */
      gtk_widget_show(label);
      
      vbox = gtk_vbox_new(FALSE, 5);
      gtk_container_add(GTK_CONTAINER(vbox), w->read_only);
      gtk_container_add(GTK_CONTAINER(vbox), label);
      gtk_widget_show(vbox);
      
      gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(w->window), vbox);

      /* When this is a save dialog, require confirmation for
	 overwriting a file.  Requires Gtk 2.8. */
#if 0
      if(save_mode) {
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(w->window), TRUE);
      }
#endif
      
      // gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(w->window), TRUE);
      // gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(w->window), TRUE);
      
      if(!gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(w->window),
					      start_dir)) {
	fprintf(stderr, "powerbox: can't set folder to \"%s\"\n", start_dir);
      }

      g_signal_connect(G_OBJECT(w->window), "response",
		       G_CALLBACK(dialog_response), w);

      /* Widget needs to be realized:  X window must be created before
	 we can set the property on it. */
      gtk_widget_realize(w->window);
      if(parent_window_id) {
	gdk_property_change(GTK_WIDGET(w->window)->window,
			    gdk_atom_intern("WM_TRANSIENT_FOR", FALSE),
			    gdk_atom_intern("WINDOW", FALSE), 32,
			    GDK_PROP_MODE_REPLACE,
			    (guchar *) &parent_window_id, 1);
      }
      
      gtk_widget_show(w->window);

      region_free(r);
      return;
    }
  }
 error:
  region_free(r);
  local_obj_invoke(obj1, args);
}

#include "out-vtable-powerbox.h"
