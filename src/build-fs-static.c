/* Copyright (C) 2004, 2005 Mark Seaborn

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

#include "region.h"
#include "filesysobj.h"
#include "filesysobj-fab.h"
#include "filesysobj-readonly.h"
#include "build-fs.h"


/* Returns an owning reference. */
struct filesys_obj *fs_build_fs(struct node *node)
{
  if(node->symlink_dest) {
    struct fab_symlink *sym =
      filesys_obj_make(sizeof(struct fab_symlink), &fab_symlink_vtable);
    sym->dest = dup_seqf(seqf_string(node->symlink_dest));
    sym->inode = node->inode;
    return make_read_only_slot((struct filesys_obj *) sym);
  }
  else if(node->attach_slot) {
    return inc_ref(node->attach_slot);
  }
  else {
    /* Construct directory */
    struct s_fab_dir *dir;
    struct slot_list *nlist = 0;
    struct node_list *list;
    for(list = node->children; list; list = list->next) {
      struct slot_list *n = amalloc(sizeof(struct obj_list));
      n->name = strdup(list->name);
      n->slot = fs_build_fs(list->node);
      n->next = nlist;
      nlist = n;
    }
    dir = filesys_obj_make(sizeof(struct s_fab_dir), &s_fab_dir_vtable);
    dir->entries = nlist;
    dir->inode = node->inode;
    return make_read_only_slot((struct filesys_obj *) dir);
  }
}

/* Older version.  See fs_make_root() */
struct filesys_obj *fs_make_root_orig(struct node *node)
{
  struct filesys_obj *root_slot = fs_build_fs(node);
  struct filesys_obj *root = root_slot->vtable->slot_get(root_slot);
  filesys_obj_free(root_slot);
  return root;
}
