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

#include "region.h"
#include "filesysobj.h"
#include "cap-utils.h"
#include "serialise.h"
#include "marshal.h"


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
  PROP_LAST = PROP_LOCAL_ONLY
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


static cap_t powerbox_req = NULL;


static void init()
{
  static int initialised = 0;
  if(!initialised) {
    fprintf(stderr, MOD_MSG "get_process_caps\n");

    if(get_process_caps("powerbox_req_filename", &powerbox_req,
			NULL) < 0) {
      exit(1);
    }

    initialised = 1;
  }
}


static void
file_powerbox_init(FilePowerbox *pb)
{
  fprintf(stderr, MOD_MSG "init instance\n");
  pb->state = STATE_UNSENT;
  pb->action = GTK_FILE_CHOOSER_ACTION_OPEN;
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

DECLARE_VTABLE(pb_return_cont_vtable);
struct pb_return_cont {
  struct filesys_obj hdr;
  FilePowerbox *pb;
};
static void pb_return_cont_free(struct filesys_obj *obj)
{
  struct pb_return_cont *c = (void *) obj;
  if(c->pb) {
    fprintf(stderr, MOD_MSG "cont_free: got no reply\n");
    gtk_dialog_response(GTK_DIALOG(c->pb), GTK_RESPONSE_CANCEL);
    g_object_unref(c->pb);
  }
}
static void pb_return_cont_invoke(struct filesys_obj *obj, struct cap_args args)
{
  struct pb_return_cont *c = (void *) obj;
  region_t r = region_make();
  if(c->pb) {
    seqf_t data = flatten_reuse(r, args.data);
    int ok = 1;
    m_int_const(&ok, &data, METHOD_POWERBOX_RESULT_FILENAME);
    if(ok) {
      fprintf(stderr, MOD_MSG "got reply\n");
      c->pb->filename = strdup_seqf(data);
      gtk_dialog_response(GTK_DIALOG(c->pb), get_response_code(c->pb));
    }
    else {
      fprintf(stderr, MOD_MSG "got unrecognised reply\n");
      gtk_dialog_response(GTK_DIALOG(c->pb), GTK_RESPONSE_CANCEL);
    }
    g_object_unref(c->pb);
    c->pb = NULL;
  }
  region_free(r);
}

#include "out-vtable-powerbox-for-gtk.h"

static void file_powerbox_send_request(FilePowerbox *pb)
{
  region_t r = region_make();
  argmkbuf_t argbuf = argbuf_make(r);

  int save, dir;
  int args_size = 10;
  bufref_t args[args_size];
  int arg_count = 0;

  struct pb_return_cont *cont =
    filesys_obj_make(sizeof(struct pb_return_cont), &pb_return_cont_vtable);
  g_object_ref(pb);
  cont->pb = pb;

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
  if(save) {
    bufref_t *a;
    args[arg_count++] = argmk_array(argbuf, 1, &a);
    a[0] = argmk_str(argbuf, mk_string(r, "Save"));
  }
  if(dir) {
    bufref_t *a;
    args[arg_count++] = argmk_array(argbuf, 1, &a);
    a[0] = argmk_str(argbuf, mk_string(r, "Wantdir"));
  }

  /* If the GtkDialog is a transient window, find its parent.  Tell
     the powerbox manager to mark its window with WM_TRANSIENT_FOR
     instead. */
  {
    GtkWindow *parent = GTK_WINDOW(pb)->transient_parent;
    if(parent) {
      int window_id = GDK_WINDOW_XID(GTK_WIDGET(parent)->window);
      args[arg_count++] =
	argmk_pair(argbuf, argmk_str(argbuf, mk_string(r, "Transientfor")),
		   argmk_int(argbuf, window_id));
    }
    else {
      fprintf(stderr, MOD_MSG "transient_parent is NULL\n");
    }
  }

  assert(arg_count < args_size);
  init();
  get_response_code(pb); // call for debugging output only
  {
    int i;
    bufref_t *a;
    bufref_t arg_list = argmk_array(argbuf, arg_count, &a);
    for(i = 0; i < arg_count; i++) { a[i] = args[i]; }
    cap_invoke(powerbox_req,
	       cap_args_dc(cat4(r, mk_string(r, "Call"),
				mk_int(r, METHOD_POWERBOX_REQ_FILENAME),
				mk_int(r, arg_list),
				argbuf_data(argbuf)),
			   mk_caps1(r, (struct filesys_obj *) cont)));
  }
  region_free(r);
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
