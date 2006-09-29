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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA.  */

/* When a process is forked, all the connections it had have to be
   closed in the child process, so any objects that were imported on
   those connections become unusable (their methods will just return
   error codes).

   Reconnectable objects let you wrap these objects.  Just before
   forking, a new connection is set up to import the wrapped objects.
   The child process uses this connection to replace the wrapped
   references with their newly-imported counterparts.

   It is also useful to wrap local objects.  If a process P implements
   an object X, you can wrap X so that when P forks a child process P2,
   P2 uses P's copy of X, typically importing it via an intermediary
   server process.
*/

#include "filesysobj.h"
#include "reconnectable-obj.h"


struct reconnectable_obj reconnectable_list =
  { .head = 1, .next = &reconnectable_list, .prev = &reconnectable_list,
    .hdr = { 0, 0 }, .x = 0 };

DECLARE_VTABLE(reconnectable_obj_vtable);

cap_t make_reconnectable(cap_t x)
{
  struct reconnectable_obj *obj =
    filesys_obj_make(sizeof(struct reconnectable_obj), &reconnectable_obj_vtable);
  obj->head = 0;
  obj->x = x;

  /* Insert into list: */
  obj->prev = &reconnectable_list;
  obj->next = reconnectable_list.next;
  reconnectable_list.next->prev = obj;
  reconnectable_list.next = obj;

  return (cap_t) obj;
}

void reconnectable_obj_free(struct filesys_obj *obj1)
{
  struct reconnectable_obj *obj = (void *) obj1;

  /* Remove from list: */
  obj->prev->next = obj->next;
  obj->next->prev = obj->prev;

  filesys_obj_free(obj->x);
}

#ifdef GC_DEBUG
void reconnectable_obj_mark(struct filesys_obj *obj1)
{
  struct reconnectable_obj *obj = (void *) obj1;
  filesys_obj_mark(obj->x);
}
#endif

void reconnectable_obj_invoke(struct filesys_obj *obj1, struct cap_args args)
{
  struct reconnectable_obj *obj = (void *) obj1;
  obj->x->vtable->cap_invoke(obj->x, args);
}

void reconnectable_obj_call(struct filesys_obj *obj1, region_t r,
			    struct cap_args args, struct cap_args *result)
{
  struct reconnectable_obj *obj = (void *) obj1;
  obj->x->vtable->cap_call(obj->x, r, args, result);
}

#include "out-vtable-reconnectable-obj.h"
