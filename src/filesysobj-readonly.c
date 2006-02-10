/* Copyright (C) 2004 Mark Seaborn

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

/* Necessary to get O_DIRECTORY and O_NOFOLLOW */
/* #define __USE_GNU */
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>

#include "filesysobj.h"
#include "cap-protocol.h"


extern struct filesys_obj_vtable readonly_obj_vtable;

struct readonly_obj {
  struct filesys_obj hdr;
  struct filesys_obj *x; /* The object being proxied */
};

/* Takes an owning reference. */
struct filesys_obj *make_read_only_proxy(struct filesys_obj *x)
{
  struct readonly_obj *obj = amalloc(sizeof(struct readonly_obj));
  obj->hdr.refcount = 1;
  obj->hdr.vtable = &readonly_obj_vtable;
  obj->x = x;
  return (struct filesys_obj *) obj;
}


void readonly_free(struct filesys_obj *obj1)
{
  struct readonly_obj *obj = (void *) obj1;
  filesys_obj_free(obj->x);
}

int readonly_type(struct filesys_obj *obj1)
{
  struct readonly_obj *obj = (void *) obj1;
  return obj->x->vtable->type(obj->x);
}

int readonly_stat(struct filesys_obj *obj1, struct stat *buf, int *err)
{
  struct readonly_obj *obj = (void *) obj1;
  return obj->x->vtable->stat(obj->x, buf, err);
}

int readonly_open(struct filesys_obj *obj1, int flags, int *err)
{
  struct readonly_obj *obj = (void *) obj1;
  /* These flags are not allowed; they may not be relevant:
     O_CREAT | O_EXCL | O_TRUNC | O_APPEND | O_DIRECTORY | O_NOFOLLOW */

  if((flags & O_ACCMODE) == O_RDONLY &&
     /* Only these flags are allowed.  This check is probably unnecessary: */
     (flags & ~(O_ACCMODE | O_NOCTTY | O_NONBLOCK | O_NDELAY | O_SYNC |
		O_LARGEFILE)) == 0) {
    return obj->x->vtable->open(obj->x, flags, err);
  }
  else {
    *err = EACCES;
    return -1;
  }
}

struct filesys_obj *readonly_traverse(struct filesys_obj *obj1, const char *leaf)
{
  struct readonly_obj *obj = (void *) obj1;
  struct filesys_obj *t = obj->x->vtable->traverse(obj->x, leaf);
  if(t) return make_read_only_proxy(t);
  else return 0;
}

int readonly_list(struct filesys_obj *obj1, region_t r, seqt_t *result, int *err)
{
  struct readonly_obj *obj = (void *) obj1;
  return obj->x->vtable->list(obj->x, r, result, err);
}

int readonly_readlink(struct filesys_obj *obj1, region_t r, seqf_t *result, int *err)
{
  struct readonly_obj *obj = (void *) obj1;
  return obj->x->vtable->readlink(obj->x, r, result, err);
}


#include "out-vtable-filesysobj-readonly.h"
