/* Copyright (C) 2004 Mark Seaborn

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

#include "filesysobj.h"
#include "cap-utils.h"
#include "log-proxy.h"


DECLARE_VTABLE(log_proxy_vtable);
struct log_proxy {
  struct filesys_obj hdr;
  cap_t x;
};

cap_t make_log_proxy(cap_t x)
{
  struct log_proxy *obj =
    filesys_obj_make(sizeof(struct log_proxy), &log_proxy_vtable);
  obj->x = x;
  return (cap_t) obj;
}

void log_proxy_free(struct filesys_obj *obj1)
{
  struct log_proxy *obj = (void *) obj1;
  filesys_obj_free(obj->x);
}

#ifdef GC_DEBUG
void log_proxy_mark(struct filesys_obj *obj1)
{
  struct log_proxy *obj = (void *) obj1;
  filesys_obj_mark(obj->x);
}
#endif

void print_args(struct cap_args args)
{
  region_t r = region_make();
  fprintf(stderr, "%i caps, %i FDs, data:\n", args.caps.size, args.fds.count);
  fprint_data(stderr, flatten_reuse(r, args.data));
  region_free(r);
}

void log_proxy_call(struct filesys_obj *obj1, region_t r, struct cap_args args,
		    struct cap_args *result)
{
  struct log_proxy *obj = (void *) obj1;
  
  fprintf(stderr, "args:\n");
  print_args(args);

  cap_call(obj->x, r, args, result);

  fprintf(stderr, "result:\n");
  print_args(*result);
  fprintf(stderr, "\n");

  {
    cap_t *caps = region_alloc(r, result->caps.size * sizeof(cap_t));
    int i;
    for(i = 0; i < result->caps.size; i++) {
      caps[i] = make_log_proxy(result->caps.caps[i]);
    }
    result->caps = cap_seq_make(caps, result->caps.size);
  }
}

#include "out-vtable-log-proxy.h"
