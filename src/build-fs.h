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
fs_node_t tree_traverse(fs_node_t node, const char *name);
int attach_ro_obj(fs_node_t node, cap_t obj);
int attach_rw_slot(fs_node_t node, struct filesys_obj *obj);
int fs_attach_at_pathname(fs_node_t root_node, struct dir_stack *cwd_ds,
			  seqf_t pathname, cap_t obj, int *err);

/* Flags for use with fs_resolve_populate().
   If neither FS_SLOT_RW nor FS_OBJECT_RW are both set, it will attach
   a read-only version of the object in a read-only slot.
   If FS_SLOT_RW is set, the slot is attached, with read-write-create
   access (and FS_OBJECT_RW is ignored).
   Otherwise, if FS_OBJECT_RW is set, the writable version of the object
   is attached in a read-only slot -- this is useful for devices and
   sockets (such as /dev/null and /tmp/.X11-unix/X0) where you want to
   grant the ability to open with write access or connect to a socket,
   without granting the ability to delete the object from the slot and
   replace it.
   If FS_FOLLOW_SYMLINKS is set, the function will follow symlinks in
   all parts of the pathname, and potentially grant access to multiple
   objects or slots as a result. */
#define FS_READ_ONLY        0x0
#define FS_SLOT_RWC         0x1
#define FS_OBJECT_RW        0x2
#define FS_FOLLOW_SYMLINKS  0x4

int fs_resolve_populate
  (struct filesys_obj *root_obj, fs_node_t root_node,
   struct dir_stack *cwd_ds,
   seqf_t filename, int flags, int *err);

struct filesys_obj *fs_make_root(fs_node_t node);

static inline void free_node(struct node *node) {
  filesys_obj_free((cap_t) node);
}
#ifdef GC_DEBUG
static inline void mark_node(struct node *node) {
  filesys_obj_mark((cap_t) node);
}
#endif


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
