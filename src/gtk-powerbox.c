/* Copyright (C) 2005 Mark Seaborn

   This file is part of Plash, the Principle of Least Authority Shell.

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
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
#include <gtk/gtklabel.h>

#include "region.h"
#include "filesysobj.h"
#include "cap-utils.h"
#include "serialise.h"
#include "marshal.h"


#define MOD_MSG "gtk-powerbox: "


#define TYPE_FILE_POWERBOX    (file_powerbox_get_type())
#define FILE_POWERBOX(obj)    (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_FILE_POWERBOX, FilePowerbox))
#define IS_FILE_POWERBOX(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_FILE_POWERBOX))

typedef struct _FilePowerboxClass FilePowerboxClass;
typedef struct _FilePowerbox FilePowerbox;

struct _FilePowerboxClass
{
  GtkObjectClass parent_class;

  /* Copied from GtkDialog */
  void (* response) (FilePowerbox *dialog, gint response_id);
};

enum FilePowerboxProp {
  PROP_ACTION = 1,
  PROP_LAST = PROP_ACTION
};

enum PowerboxState {
  STATE_UNSENT = 1,
  STATE_SENT,
  STATE_GOT_REPLY
};

struct _FilePowerbox
{
  GtkObject parent_instance;

  enum PowerboxState state;
  GtkFileChooserAction action;
  /* Filled out when state == STATE_SENT: */
  GMainLoop *loop;
  /* Filled out when state == STATE_GOT_REPLY: */
  char *filename;
};

guint sig_response;

GType file_powerbox_get_type(void);


static GType (*orig_gtk_file_chooser_dialog_get_type)(void);
static void (*orig_gtk_widget_show)(GtkWidget *widget);
static void (*orig_gtk_widget_show_all)(GtkWidget *widget);
static gint (*orig_gtk_dialog_run)(GtkDialog *dialog);
static GtkWidget* (*orig_gtk_message_dialog_new)(GtkWindow *parent, GtkDialogFlags flags, GtkMessageType type, GtkButtonsType buttons, const gchar *message_format, ...);

static cap_t powerbox_req = NULL;


static void lookup_sym(const char *name, void *fun_ptr)
{
  void **ptr = fun_ptr;
  void *val = dlsym(RTLD_NEXT, name);
  if(!val) {
    fprintf(stderr, MOD_MSG "symbol \"%s\" not defined\n", name);
    exit(1);
  }
  *ptr = val;
}

void init()
{
  static int initialised = 0;
  if(!initialised) {
    fprintf(stderr, MOD_MSG "init\n");
    
    lookup_sym("gtk_file_chooser_dialog_get_type",
	       &orig_gtk_file_chooser_dialog_get_type);
    lookup_sym("gtk_widget_show",
	       &orig_gtk_widget_show);
    lookup_sym("gtk_widget_show_all",
	       &orig_gtk_widget_show_all);
    lookup_sym("gtk_dialog_run",
	       &orig_gtk_dialog_run);
    lookup_sym("gtk_message_dialog_new",
	       &orig_gtk_message_dialog_new);

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
  pb->state = STATE_UNSENT;
  pb->action = GTK_FILE_CHOOSER_ACTION_OPEN;
  pb->loop = NULL;
  pb->filename = NULL;
  
  // GtkFileChooserDialogPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (dialog,
  //                                                                  GTK_TYPE_FILE_CHOOSER_DIALOG,
  //                                                                  GtkFileChooserDialogPrivate);
  // dialog->priv = priv;
  // dialog->priv->default_width = -1;
  // dialog->priv->default_height = -1;
  // dialog->priv->resize_horizontally = TRUE;
  // dialog->priv->resize_vertically = TRUE;

  // gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

  /* We do a signal connection here rather than overriding the method in
   * class_init because GtkDialog::response is a RUN_LAST signal.  We want *our*
   * handler to be run *first*, regardless of whether the user installs response
   * handlers of his own.
   */
  // g_signal_connect (dialog, "response",
  //                   G_CALLBACK (response_cb), NULL);
}

static void
file_powerbox_finalize(GObject *object)
{
  FilePowerbox *pb = FILE_POWERBOX(object);
  fprintf(stderr, MOD_MSG "finalize\n");
  g_free(pb->filename);
  g_main_loop_unref (pb->loop);
  // G_OBJECT_CLASS(parent_class)->finalize(object);
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

  // gobject_class->constructor = file_powerbox_constructor;
  gobject_class->set_property = file_powerbox_set_property;
  gobject_class->get_property = file_powerbox_get_property;
  gobject_class->finalize = file_powerbox_finalize;

  /* See GtkDialog */
  sig_response =
    g_signal_new ("response",
                  G_OBJECT_CLASS_TYPE (class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (FilePowerboxClass, response),
                  NULL, NULL,
                  gtk_marshal_NONE__INT, /* was _gtk_marshal_NONE__INT */
                  G_TYPE_NONE, 1,
                  G_TYPE_INT);

  /* See gtkfilechooser.c */
  g_object_class_install_property (gobject_class,
				   PROP_ACTION,
				   g_param_spec_enum ("action",
						      P_("Action"),
						      P_("The type of operation that the file selector is performing"),
						      GTK_TYPE_FILE_CHOOSER_ACTION,
						      GTK_FILE_CHOOSER_ACTION_OPEN,
						      GTK_PARAM_READWRITE));
  
  // parent_class = g_type_class_peek_parent (class);

  // _file_powerbox_install_properties (gobject_class);

  // g_type_class_add_private (class, sizeof (GtkFileChooserDialogPrivate));
}

// static void
// file_powerbox_dialog_iface_init (GtkFileChooserIface *iface)
// {
//   // ...
// }

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

      type = g_type_register_static (GTK_TYPE_OBJECT, "FilePowerbox",
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
  // init();
  // return orig_gtk_file_chooser_dialog_get_type();
}

GType gtk_file_chooser_get_type(void)
{
  fprintf(stderr, MOD_MSG "gtk_file_chooser_get_type\n");
  return file_powerbox_get_type();
}

static GtkWidget *
file_powerbox_new(const char *title, GtkWindow *parent,
		  GtkFileChooserAction action)
{
  GtkObject *result = g_object_new(TYPE_FILE_POWERBOX,
				   "action", action,
				   NULL);
    fprintf(stderr, MOD_MSG "new\n");

  /* NB. It's not really a GtkWidget. */
  return (GtkWidget *) result;
}

/* 'parent' is ignored */
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
  const char *button_text;
  va_list args;
  va_start(args, first_button_text);
  while(button_text) {
    int id = va_arg(args, gint);
    fprintf(stderr, MOD_MSG "arg: %s, %i\n", button_text, id);
    button_text = va_arg(args, const char *);
  }
  va_end(args);
  return file_powerbox_new(title, parent, action);
}

GtkWidget *
gtk_file_chooser_dialog_new_with_backend (const gchar          *title,
                                          GtkWindow            *parent,
                                          GtkFileChooserAction  action,
                                          const gchar          *backend,
                                          const gchar          *first_button_text,
                                          ...)
{
  return file_powerbox_new(title, parent, action);
}

gboolean
gtk_file_chooser_set_current_folder (GtkFileChooser *chooser,
                                     const gchar    *filename)
{
  g_return_val_if_fail(IS_FILE_POWERBOX(chooser), FALSE);
  fprintf(stderr, MOD_MSG "set_folder\n");
  return TRUE; /* successful */
}

void
gtk_file_chooser_add_filter (GtkFileChooser *chooser,
                             GtkFileFilter  *filter)
{
  g_return_if_fail(IS_FILE_POWERBOX(chooser));
  fprintf(stderr, MOD_MSG "add_filter: NO-OP\n");
}

void
gtk_file_chooser_set_extra_widget (GtkFileChooser *chooser,
                                   GtkWidget      *extra_widget)
{
  g_return_if_fail(IS_FILE_POWERBOX(chooser));
  fprintf(stderr, MOD_MSG "set_extra_widget: NO-OP\n");
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

DECLARE_VTABLE(pb_return_cont_vtable);
struct pb_return_cont {
  struct filesys_obj hdr;
  FilePowerbox *pb;
};
void pb_return_cont_free(struct filesys_obj *obj)
{
  struct pb_return_cont *c = (void *) obj;
  g_object_unref(c->pb);
}
void pb_return_cont_invoke(struct filesys_obj *obj, struct cap_args args)
{
  struct pb_return_cont *c = (void *) obj;
  region_t r = region_make();
  {
    seqf_t data = flatten_reuse(r, args.data);
    int ok = 1;
    m_int_const(&ok, &data, METHOD_POWERBOX_RESULT_FILENAME);
    if(ok) {
      fprintf(stderr, MOD_MSG "got reply\n");
      
      c->pb->filename = strdup_seqf(data);
      if(c->pb->loop) { g_main_loop_quit(c->pb->loop); }
      g_signal_emit(c->pb, sig_response, 0, GTK_RESPONSE_OK);

      g_object_unref(c->pb);
      c->pb = NULL;
      
      region_free(r);
      return;
    }
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

  init();
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

/* We treat the gtk_widget_show() method as opening the powerbox and
   assume that all the powerbox request's parameters have been filled
   out by this point.  Send the powerbox request message.  For other
   objects, fall through to the usual case. */
void
gtk_widget_show (GtkWidget *widget)
{
  if(IS_FILE_POWERBOX(widget)) {
    FilePowerbox *pb = FILE_POWERBOX(widget);
    fprintf(stderr, MOD_MSG "show\n");
    file_powerbox_send_request(pb);
  }
  else {
    init();
    orig_gtk_widget_show(widget);
  }
}

void
gtk_widget_show_all (GtkWidget *widget)
{
  if(IS_FILE_POWERBOX(widget)) {
    FilePowerbox *pb = FILE_POWERBOX(widget);
    fprintf(stderr, MOD_MSG "show_all\n");
    file_powerbox_send_request(pb);
  }
  else {
    init();
    orig_gtk_widget_show_all(widget);
  }
}

gint
gtk_dialog_run (GtkDialog *dialog)
{
  if(IS_FILE_POWERBOX(dialog)) {
    FilePowerbox *pb = FILE_POWERBOX(dialog);

    fprintf(stderr, MOD_MSG "dialog_run\n");
    pb->loop = g_main_loop_new (NULL, FALSE);
    GDK_THREADS_LEAVE ();  
    g_main_loop_run (pb->loop);
    fprintf(stderr, MOD_MSG "loop exited\n");
    GDK_THREADS_ENTER ();
    
    return GTK_RESPONSE_ACCEPT; // GTK_RESPONSE_CANCEL
  }
  else {
    init();
    return orig_gtk_dialog_run(dialog);
  }
}


/* Copied from gtkmessagedialog.c */
GtkWidget*
gtk_message_dialog_new (GtkWindow     *parent,
                        GtkDialogFlags flags,
                        GtkMessageType type,
                        GtkButtonsType buttons,
                        const gchar   *message_format,
                        ...)
{
  GtkWidget *widget;
  GtkDialog *dialog;
  gchar* msg = NULL;
  va_list args;

  if(IS_FILE_POWERBOX(parent)) { parent = NULL; }
  g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), NULL);

  widget = g_object_new (GTK_TYPE_MESSAGE_DIALOG,
                         "message-type", type,
                         "buttons", buttons,
                         NULL);
  dialog = GTK_DIALOG (widget);

  if (flags & GTK_DIALOG_NO_SEPARATOR)
    {
      g_warning ("The GTK_DIALOG_NO_SEPARATOR flag cannot be used for GtkMessageDialog");
      flags &= ~GTK_DIALOG_NO_SEPARATOR;
    }

  if (message_format)
    {
      va_start (args, message_format);
      msg = g_strdup_vprintf (message_format, args);
      va_end (args);

      gtk_label_set_text (GTK_LABEL (GTK_MESSAGE_DIALOG (widget)->label),
                          msg);

      g_free (msg);
    }

  if (parent != NULL)
    gtk_window_set_transient_for (GTK_WINDOW (widget),
                                  GTK_WINDOW (parent));
  
  if (flags & GTK_DIALOG_MODAL)
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  if (flags & GTK_DIALOG_DESTROY_WITH_PARENT)
    gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

  if (flags & GTK_DIALOG_NO_SEPARATOR)
    gtk_dialog_set_has_separator (dialog, FALSE);

  return widget;
}
