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

#ifndef filesysslot_h
#define filesysslot_h


#include "filesysobj.h"

struct filesys_slot {
  int refcount;
  struct filesys_slot_vtable *vtable;
};

struct filesys_slot_vtable {
  void (*free)(struct filesys_slot *obj);
  struct filesys_obj *(*get)(struct filesys_slot *obj); /* Returns 0 if slot is currently empty */
  int (*create_file)(struct filesys_slot *obj, int flags, int mode, int *err);
  int (*mkdir)(struct filesys_slot *slot, int mode, int *err);
  int (*unlink)(struct filesys_slot *slot, int *err);
  int (*rmdir)(struct filesys_slot *slot, int *err);
  int sentinel; /* For type checking purposes */
};

struct filesys_generic_slot {
  struct filesys_slot hdr;
  struct filesys_obj *dir;
  char *leaf;
};

struct filesys_read_only_slot {
  struct filesys_slot hdr;
  struct filesys_obj *obj;
};

void filesys_slot_free(struct filesys_slot *slot);

/* Takes ownership of dir and leaf */
struct filesys_slot *make_generic_slot(struct filesys_obj *dir, char *leaf);
/* Takes ownership of the obj reference */
struct filesys_slot *make_read_only_slot(struct filesys_obj *obj);


#endif
