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

#ifndef build_fs_h
#define build_fs_h

#include "filesysobj.h"
#include "filesysslot.h"
#include "resolve-filename.h"


struct node;
typedef struct node *fs_node_t;

fs_node_t fs_make_empty_node();
void fs_print_tree(int indent, fs_node_t node);
int fs_attach_at_pathname(fs_node_t root_node, struct dir_stack *cwd_ds,
			  seqf_t pathname, cap_t obj, int *err);
int fs_resolve_populate
  (struct filesys_obj *root_obj, fs_node_t root_node,
   struct dir_stack *cwd_ds,
   seqf_t filename, int create, int *err);

struct filesys_obj *fs_make_root(fs_node_t node);

static inline void free_node(struct node *node) {
  filesys_obj_free((cap_t) node);
}


/* This is a private data structure */

/* This data structure allows a file to be attached at the root node,
   but most code assumes that `/' is a directory, and will not do anything
   useful if it is a file! */
DECLARE_VTABLE(node_vtable);
struct node {
  struct filesys_obj hdr;
  int inode; /* Inode number to use for fabricated symlinks and directories */

  /* Symlink:  malloc'd block.  May be null.  If this is used, the
     other parameters are ignored. */
  char *symlink_dest;

  /* Slot object attached into the directory tree.  May be null. */
  /* In the first implementation, when this is non-null the `children'
     field is ignored, and this slot is attached straight into the
     directory tree.
     In the second implementation, when this is non-null and the
     `children' field is also non-null, the contents of this slot is
     assumed to be a directory and is combined with a directory
     comprising the `children' entries. */
  struct filesys_obj *attach_slot;

  /* May be empty (ie. null) */
  struct node_list *children;
};

struct node_list {
  char *name;
  struct node *node;
  struct node_list *next;
};


#endif
