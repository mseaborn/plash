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

#ifndef filesysslot_h
#define filesysslot_h


#include "filesysobj.h"

/* Slots have now been merged into filesys_objs. */
/* This is the same as struct filesys_obj; it may be removed in future: */
struct filesys_slot {
  int refcount;
  struct filesys_obj_vtable *vtable;
};

#if 0
struct filesys_slot {
  int refcount;
  struct filesys_slot_vtable *vtable;
};

struct filesys_slot_vtable {
  void (*free)(struct filesys_slot *obj);
  struct filesys_obj *(*get)(struct filesys_slot *obj); /* Returns 0 if slot is currently empty */
  int (*slot_create_file)(struct filesys_slot *obj, int flags, int mode,
			  int *err);
  int (*slot_mkdir)(struct filesys_slot *slot, int mode, int *err);
  int (*slot_symlink)(struct filesys_slot *slot, const char *oldpath, int *err);
  int (*slot_unlink)(struct filesys_slot *slot, int *err);
  int (*slot_rmdir)(struct filesys_slot *slot, int *err);
  int (*slot_socket_bind)(struct filesys_slot *slot, int sock_fd, int *err);
  int sentinel; /* For type checking purposes */
};
#endif

struct filesys_generic_slot {
  struct filesys_obj hdr;
  struct filesys_obj *dir;
  char *leaf;
};

struct filesys_read_only_slot {
  struct filesys_obj hdr;
  struct filesys_obj *obj;
};

// void filesys_slot_free(struct filesys_slot *slot);
#define filesys_slot_free(x) filesys_obj_free(x)

/* Takes ownership of dir and leaf */
struct filesys_obj *make_generic_slot(struct filesys_obj *dir, char *leaf);
/* Takes ownership of the obj reference */
struct filesys_obj *make_read_only_slot(struct filesys_obj *obj);


#endif
