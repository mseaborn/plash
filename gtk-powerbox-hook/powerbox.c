/* Copyright (C) 2007, 2008 Mark Seaborn

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

#include "cap-utils.h"
#include "filesysobj.h"
#include "marshal.h"
#include "region.h"
#include "serialise.h"

#include "powerbox.h"


DECLARE_VTABLE(pb_return_cont_vtable);

struct pb_return_cont {
  struct filesys_obj hdr;
  gboolean called;
  void (*callback)(void *handle, struct pb_result *result);
  void *handle;
};

struct pb_result {
  gboolean was_cancelled;
  char *filename;
};


static cap_t powerbox_req = NULL;


static void init()
{
  static gboolean initialised = FALSE;
  if(!initialised) {
    get_process_caps("powerbox_req_filename", &powerbox_req, NULL);
    initialised = TRUE;
  }
}


static void pb_return_cont_free(struct filesys_obj *obj)
{
  struct pb_return_cont *cont = (void *) obj;
  if(!cont->called) {
    cont->called = TRUE;
    struct pb_result *result = amalloc(sizeof(struct pb_result));
    result->was_cancelled = TRUE;
    result->filename = NULL;
    cont->callback(cont->handle, result);
  }
}

static void pb_return_cont_invoke(struct filesys_obj *obj, struct cap_args args)
{
  struct pb_return_cont *cont = (void *) obj;
  region_t r = region_make();
  if(!cont->called) {
    cont->called = TRUE;

    seqf_t data = flatten_reuse(r, args.data);
    int ok = 1;
    m_int_const(&ok, &data, METHOD_POWERBOX_RESULT_FILENAME);
    if(ok) {
      struct pb_result *result = amalloc(sizeof(struct pb_result));
      result->was_cancelled = FALSE;
      result->filename = strdup_seqf(data);
      cont->callback(cont->handle, result);
    }
    else {
      struct pb_result *result = amalloc(sizeof(struct pb_result));
      result->was_cancelled = TRUE;
      result->filename = NULL;
      cont->callback(cont->handle, result);
    }
  }
  region_free(r);
}

#include "out-vtable-powerbox-for-gtk.h"


gboolean pb_is_available(void)
{
  init();
  return powerbox_req != NULL;
}

gboolean pb_request_send(gboolean want_save, gboolean want_directory,
			 int parent_window_id,
			 void (*callback)(void *handle,
					  struct pb_result *result),
			 void *handle)
{
  region_t r = region_make();
  argmkbuf_t argbuf = argbuf_make(r);

  int args_size = 10;
  bufref_t args[args_size];
  int arg_count = 0;

  struct pb_return_cont *cont =
    filesys_obj_make(sizeof(struct pb_return_cont), &pb_return_cont_vtable);
  cont->called = FALSE;
  cont->callback = callback;
  cont->handle = handle;

  if(want_save) {
    bufref_t *a;
    args[arg_count++] = argmk_array(argbuf, 1, &a);
    a[0] = argmk_str(argbuf, mk_string(r, "Save"));
  }
  if(want_directory) {
    bufref_t *a;
    args[arg_count++] = argmk_array(argbuf, 1, &a);
    a[0] = argmk_str(argbuf, mk_string(r, "Wantdir"));
  }
  if(parent_window_id != 0) {
    args[arg_count++] =
      argmk_pair(argbuf, argmk_str(argbuf, mk_string(r, "Transientfor")),
		 argmk_int(argbuf, parent_window_id));
  }

  assert(arg_count < args_size);
  init();
  int i;
  bufref_t *a;
  bufref_t arg_list = argmk_array(argbuf, arg_count, &a);
  for(i = 0; i < arg_count; i++)
    a[i] = args[i];
  cap_invoke(powerbox_req,
	     cap_args_dc(cat4(r, mk_int(r, METHOD_CALL),
			      mk_int(r, METHOD_POWERBOX_REQ_FILENAME),
			      mk_int(r, arg_list),
			      argbuf_data(argbuf)),
			 mk_caps1(r, (struct filesys_obj *) cont)));
  region_free(r);
  return TRUE;
}


gboolean pb_result_was_cancelled(struct pb_result *result)
{
  return result->was_cancelled;
}

char *pb_result_get_filename(struct pb_result *result)
{
  return result->filename;
}

void pb_result_free(struct pb_result *result)
{
  free(result->filename);
  free(result);
}
