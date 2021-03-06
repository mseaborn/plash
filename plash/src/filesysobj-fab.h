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

#ifndef filesysobj_fab_h
#define filesysobj_fab_h

#include "filesysobj.h"
#include "filesysslot.h"


/* Device number reported by stat.  FIXME: avoid clashes? */
#define FAB_OBJ_STAT_DEVICE 100


/* Abstract version that "assoc" operates on. */
struct list {
  char *name;
  void *val;
  struct list *next;
};

struct list *assoc(struct list *list, const char *name);


struct obj_list {
  char *name;
  struct filesys_obj *obj;
  struct obj_list *next;
};

struct fab_dir {
  struct filesys_obj hdr;
  struct obj_list *entries; /* Owned by the fab_dir */
  int inode;
};

struct fab_symlink {
  struct filesys_obj hdr;
  seqf_t dest; /* malloc'd block, owned by this object */
  int inode;
};

DECLARE_VTABLE(fab_dir_vtable);
DECLARE_VTABLE(fab_symlink_vtable);


/* This is a version built from slots rather than filesys_objs */

struct slot_list {
  char *name;
  struct filesys_obj *slot;
  struct slot_list *next;
};

struct s_fab_dir {
  struct filesys_obj hdr;
  struct slot_list *entries; /* Owned by the s_fab_dir */
  int inode;
};

DECLARE_VTABLE(s_fab_dir_vtable);
DECLARE_VTABLE(s_fab_symlink_vtable);


#endif
