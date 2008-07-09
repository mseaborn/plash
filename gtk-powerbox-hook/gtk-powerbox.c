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

/* For RTLD_NEXT */
#define _GNU_SOURCE

#include <stdlib.h>
#include <dlfcn.h>
#include <gtk/gtkfilechooser.h>
#include <gtk/gtkfilechooserdialog.h>
#include <gtk/gtkmarshal.h>
#include <gtk/gtkdialog.h>

#include <gtk/gtkmessagedialog.h>

#include <gdk/gdkwindow.h>
#include <gdk/gdkx.h>

#include "powerbox.h"


#define MOD_MSG "gtk-powerbox: "

#define LOG_NO_OP(name) fprintf(stderr, MOD_MSG "NO-OP: " name "\n");


#define TYPE_FILE_POWERBOX    (file_powerbox_get_type())
#define FILE_POWERBOX(obj)    (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_FILE_POWERBOX, FilePowerbox))
#define IS_FILE_POWERBOX(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_FILE_POWERBOX))

typedef struct _FilePowerboxClass FilePowerboxClass;
typedef struct _FilePowerbox FilePowerbox;

struct _FilePowerboxClass
{
  GtkDialogClass parent_class;
};

enum FilePowerboxProp {
  PROP_ACTION = 1,
  PROP_LOCAL_ONLY,
  PROP_SELECT_MULTIPLE,
  PROP_LAST = PROP_SELECT_MULTIPLE
};

enum PowerboxState {
  STATE_UNSENT = 1,
  STATE_SENT,
  STATE_GOT_REPLY
};

struct _FilePowerbox
{
  GtkDialog parent_instance;
  
  enum PowerboxState state;
  GtkFileChooserAction action;
  gboolean local_only;
  gboolean select_multiple;
  /* Filled out when state == STATE_GOT_REPLY: */
  char *filename;
};

GType file_powerbox_get_type(void);

static GObjectClass *parent_class;

static void
file_powerbox_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec);
static void
file_powerbox_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec);
static void file_powerbox_map(GtkWidget *widget);


static void
file_powerbox_init(FilePowerbox *pb)
{
  fprintf(stderr, MOD_MSG "init instance\n");
  pb->state = STATE_UNSENT;
  pb->action = GTK_FILE_CHOOSER_ACTION_OPEN;
  pb->local_only = TRUE;
  pb->select_multiple = FALSE;
  pb->filename = NULL;
}

static void
file_powerbox_finalize(GObject *object)
{
  FilePowerbox *pb = FILE_POWERBOX(object);
  fprintf(stderr, MOD_MSG "finalize instance\n");
  g_free(pb->filename);
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
file_powerbox_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  FilePowerbox *pb = FILE_POWERBOX(object);
  switch(prop_id) {
    case PROP_ACTION:
      pb->action = g_value_get_enum(value);
      break;
    case PROP_LOCAL_ONLY:
      pb->local_only = g_value_get_boolean(value);
      break;
    case PROP_SELECT_MULTIPLE:
      pb->select_multiple = g_value_get_boolean(value);
      break;
      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
file_powerbox_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  FilePowerbox *pb = FILE_POWERBOX(object);
  switch(prop_id) {
    case PROP_ACTION:
      g_value_set_enum(value, pb->action);
      break;
    case PROP_LOCAL_ONLY:
      g_value_set_boolean(value, pb->local_only);
      break;
    case PROP_SELECT_MULTIPLE:
      g_value_set_boolean(value, pb->select_multiple);
      break;
      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

#define P_(x) x

/* See gtkprivate.h */
#define GTK_PARAM_READWRITE G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB

static void
file_powerbox_class_init (FilePowerboxClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  fprintf(stderr, MOD_MSG "init class\n");

  parent_class = g_type_class_peek_parent (class);

  // gobject_class->constructor = file_powerbox_constructor;
  gobject_class->set_property = file_powerbox_set_property;
  gobject_class->get_property = file_powerbox_get_property;
  gobject_class->finalize = file_powerbox_finalize;

  widget_class->map = file_powerbox_map;

  /* See gtkfilechooser.c */
  g_object_class_install_property (gobject_class,
				   PROP_ACTION,
				   g_param_spec_enum ("action",
						      P_("Action"),
						      P_("The type of operation that the file selector is performing"),
						      GTK_TYPE_FILE_CHOOSER_ACTION,
						      GTK_FILE_CHOOSER_ACTION_OPEN,
						      GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_LOCAL_ONLY,
				   g_param_spec_boolean ("local-only",
							 P_("Local Only"),
							 P_("Whether the selected file(s) should be limited to local file: URLs"),
							 TRUE,
							 GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
				   PROP_SELECT_MULTIPLE,
				   g_param_spec_boolean ("select-multiple",
							 P_("Select Multiple"),
							 P_("Whether to allow multiple files to be selected"),
							 FALSE,
							 GTK_PARAM_READWRITE));
}

GType file_powerbox_get_type(void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo info =
      {
        sizeof (FilePowerboxClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) file_powerbox_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (FilePowerbox),
        0,              /* n_preallocs */
        (GInstanceInitFunc) file_powerbox_init,
      };

//       static const GInterfaceInfo file_chooser_info =
//       {
//         (GInterfaceInitFunc) file_powerbox_dialog_iface_init, /* interface_init */
//         NULL,                                                       /* interface_finalize */
//         NULL                                                        /* interface_data */
//       };

      type = g_type_register_static (GTK_TYPE_DIALOG, "FilePowerbox",
				     &info, 0);
//       g_type_add_interface_static (type,
//                                    GTK_TYPE_FILE_CHOOSER,
//                                    &info);
    }
  return type;
}

GType gtk_file_chooser_dialog_get_type(void)
{
  fprintf(stderr, MOD_MSG "gtk_file_chooser_dialog_get_type\n");
  return file_powerbox_get_type();
}

GType gtk_file_chooser_get_type(void)
{
  fprintf(stderr, MOD_MSG "gtk_file_chooser_get_type\n");
  return file_powerbox_get_type();
}

static GtkWidget *
file_powerbox_new(const char *title, GtkWindow *parent,
		  GtkFileChooserAction action,
		  const char *button_text,
		  va_list args)
{
  GtkWidget *result = g_object_new(TYPE_FILE_POWERBOX,
				   "title", title,
				   "action", action,
				   NULL);
  fprintf(stderr, MOD_MSG "new\n");

  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (result), parent);
  
  while(button_text) {
    int response_id = va_arg(args, gint);
    fprintf(stderr, MOD_MSG "arg: %s, %i\n", button_text, response_id);
    gtk_dialog_add_button (GTK_DIALOG (result), button_text, response_id);
    button_text = va_arg(args, const char *);
  }

  return result;
}

/* button text and argument list is ignored. */
/* 'action' determines whether we're opening or saving, and whether we want
   a file or directory. */
/* title could be used */
GtkWidget *
gtk_file_chooser_dialog_new (const gchar         *title,
                             GtkWindow           *parent,
                             GtkFileChooserAction action,
                             const gchar         *first_button_text,
                             ...)
{
  GtkWidget *result;
  va_list args;
  va_start(args, first_button_text);
  result = file_powerbox_new(title, parent, action, first_button_text, args);
  va_end(args);
  return result;
}

GtkWidget *
gtk_file_chooser_dialog_new_with_backend (const gchar          *title,
                                          GtkWindow            *parent,
                                          GtkFileChooserAction  action,
                                          const gchar          *backend,
                                          const gchar          *first_button_text,
                                          ...)
{
  GtkWidget *result;
  va_list args;
  va_start(args, first_button_text);
  result = file_powerbox_new(title, parent, action, first_button_text, args);
  va_end(args);
  return result;
}

void
gtk_file_chooser_set_action (GtkFileChooser       *chooser,
			     GtkFileChooserAction  action)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "action", action, NULL);
}

GtkFileChooserAction
gtk_file_chooser_get_action (GtkFileChooser *chooser)
{
  GtkFileChooserAction action;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "action", &action, NULL);

  return action;
}

void
gtk_file_chooser_set_local_only (GtkFileChooser *chooser,
				 gboolean        local_only)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "local-only", local_only, NULL);
}

gboolean
gtk_file_chooser_get_local_only (GtkFileChooser *chooser)
{
  gboolean local_only;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "local-only", &local_only, NULL);

  return local_only;
}

void
gtk_file_chooser_set_select_multiple (GtkFileChooser *chooser,
				      gboolean        select_multiple)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "select-multiple", select_multiple, NULL);
}

gboolean
gtk_file_chooser_get_select_multiple (GtkFileChooser *chooser)
{
  gboolean select_multiple;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "select-multiple", &select_multiple, NULL);

  return select_multiple;
}

gboolean
gtk_file_chooser_set_current_folder (GtkFileChooser *chooser,
                                     const gchar    *filename)
{
  g_return_val_if_fail(IS_FILE_POWERBOX(chooser), FALSE);
  LOG_NO_OP("set_folder");
  return TRUE; /* successful */
}

void
gtk_file_chooser_set_current_name  (GtkFileChooser *chooser,
				    const gchar    *name)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (name != NULL);
  LOG_NO_OP("set_current_name");
}

void
gtk_file_chooser_add_filter (GtkFileChooser *chooser,
                             GtkFileFilter  *filter)
{
  g_return_if_fail(IS_FILE_POWERBOX(chooser));
  LOG_NO_OP("add_filter");
}

void
gtk_file_chooser_remove_filter (GtkFileChooser *chooser,
				GtkFileFilter  *filter)
{
  LOG_NO_OP("remove_filter");
}

GSList*
gtk_file_chooser_list_filters (GtkFileChooser *chooser)
{
  LOG_NO_OP("list_filters");
  return NULL;
}

void
gtk_file_chooser_set_filter (GtkFileChooser *chooser,
			     GtkFileFilter  *filter)
{
  LOG_NO_OP("set_filter");
}

void
gtk_file_chooser_set_extra_widget (GtkFileChooser *chooser,
                                   GtkWidget      *extra_widget)
{
  g_return_if_fail(IS_FILE_POWERBOX(chooser));
  LOG_NO_OP("set_extra_widget");
}

gchar *
gtk_file_chooser_get_filename (GtkFileChooser *chooser)
{
  FilePowerbox *pb;
  g_return_val_if_fail(IS_FILE_POWERBOX(chooser), NULL);
  pb = FILE_POWERBOX(chooser);
  
  fprintf(stderr, MOD_MSG "get_filename: %s\n", pb->filename);
  return g_strdup(pb->filename);
}

gchar *
gtk_file_chooser_get_uri (GtkFileChooser *chooser)
{
  FilePowerbox *pb;
  g_return_val_if_fail(IS_FILE_POWERBOX(chooser), NULL);
  pb = FILE_POWERBOX(chooser);

  if(pb->filename) {
    char *uri;
    if(asprintf(&uri, "file:%s", pb->filename) < 0) { return NULL; }
    fprintf(stderr, MOD_MSG "get_uri: %s\n", uri);
    return uri;
  }
  else {
    fprintf(stderr, MOD_MSG "get_uri failed\n");
    return NULL;
  }
}

gboolean
gtk_file_chooser_set_uri (GtkFileChooser *chooser,
			  const char     *uri)
{
  g_return_val_if_fail (IS_FILE_POWERBOX (chooser), FALSE);
  LOG_NO_OP("set_uri");
  return FALSE;
}

gboolean
gtk_file_chooser_select_uri (GtkFileChooser *chooser,
			     const char     *uri)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);
  LOG_NO_OP("select_uri");
  return FALSE;
}

void
gtk_file_chooser_unselect_uri (GtkFileChooser *chooser,
			       const char     *uri)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (uri != NULL);
  LOG_NO_OP("unselect_uri");
}

GSList *
gtk_file_chooser_get_uris (GtkFileChooser *chooser)
{
  GSList *list = NULL;
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);
  list = g_slist_prepend(list, gtk_file_chooser_get_uri(chooser));
  return list;
}

void
gtk_file_chooser_unselect_all (GtkFileChooser *chooser)
{

  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  LOG_NO_OP("unselect_all");
}

gboolean
gtk_file_chooser_add_shortcut_folder (GtkFileChooser    *chooser,
				      const char        *folder,
				      GError           **error)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (folder != NULL, FALSE);
  LOG_NO_OP("add_shortcut_folder");
  return FALSE;
}

gboolean
gtk_file_chooser_remove_shortcut_folder (GtkFileChooser    *chooser,
					 const char        *folder,
					 GError           **error)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);
  g_return_val_if_fail (folder != NULL, FALSE);
  LOG_NO_OP("remove_shortcut_folder");
  return FALSE;
}

#if 1
/* Not available before 2.8, so supply our own.
   Makes assumptions about GtkDialog's private data. */
typedef struct _ResponseData ResponseData;
struct _ResponseData
{
  gint response_id;
};

gint
gtk_dialog_get_response_for_widget(GtkDialog *dialog, GtkWidget *widget)
{
  ResponseData *rd = g_object_get_data (G_OBJECT (widget),
                                        "gtk-dialog-response-data");
  return rd ? rd->response_id : GTK_RESPONSE_NONE;
}
#endif

static GtkResponseType get_response_code(FilePowerbox *pb)
{
  GList *children, *list;
  GtkResponseType rc = GTK_RESPONSE_NONE;
  g_return_val_if_fail(IS_FILE_POWERBOX(pb), GTK_RESPONSE_NONE);
  g_return_val_if_fail(GTK_IS_DIALOG(pb), GTK_RESPONSE_NONE);

  fprintf(stderr, MOD_MSG "response codes:\n");
  children = gtk_container_get_children(GTK_CONTAINER(GTK_DIALOG(pb)->action_area));
  for(list = children; list; list = list->next) {
    GtkResponseType rc2 = gtk_dialog_get_response_for_widget(GTK_DIALOG(pb), GTK_WIDGET(list->data));
    fprintf(stderr, MOD_MSG "response code %i\n", rc);

    /* Use the first widget's response code.  This seems to be the
       rightmost widget.  Is this right? */
    if(rc == GTK_RESPONSE_NONE) { rc = rc2; }
  }
  g_list_free(children);
  return rc;
}

static void on_powerbox_reply(void *handle, struct pb_result *result)
{
  FilePowerbox *pb = handle;

  if(pb_result_was_cancelled(result)) {
    gtk_dialog_response(GTK_DIALOG(pb), GTK_RESPONSE_CANCEL);
  }
  else {
    g_free(pb->filename);
    pb->filename = pb_result_get_filename(result);
    gtk_dialog_response(GTK_DIALOG(pb), get_response_code(pb));
  }
  g_object_unref(pb);
}

static void file_powerbox_send_request(FilePowerbox *pb)
{
  gboolean save, dir;
  int parent_window_id = 0;

  switch(pb->action) {
    case GTK_FILE_CHOOSER_ACTION_OPEN:
    default:
      save = 0;
      dir = 0;
      break;
    case GTK_FILE_CHOOSER_ACTION_SAVE:
      save = 1;
      dir = 0;
      break;
    case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
      save = 0;
      dir = 1;
      break;
    case GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER:
      save = 1;
      dir = 1;
      break;
  }

  /* If the GtkDialog is a transient window, find its parent.  Tell
     the powerbox manager to mark its window with WM_TRANSIENT_FOR
     instead. */
  GtkWindow *parent = GTK_WINDOW(pb)->transient_parent;
  if(parent) {
    parent_window_id = GDK_WINDOW_XID(GTK_WIDGET(parent)->window);
  }
  else {
    fprintf(stderr, MOD_MSG "transient_parent is NULL\n");
  }

  get_response_code(pb); // call for debugging output only

  g_object_ref(pb);
  pb_request_send(save, dir, parent_window_id, on_powerbox_reply, pb);
}

static void
file_powerbox_map (GtkWidget *widget)
{
  /* Normally we'd do the following, but we aim to prevent the real
     window from being opened! */
  /* GTK_WIDGET_CLASS (parent_class)->map (widget); */

  fprintf(stderr, MOD_MSG "map window\n");
  file_powerbox_send_request(FILE_POWERBOX(widget));
}
