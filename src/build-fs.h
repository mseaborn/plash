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

struct node;

struct node *make_empty_node();
void free_node(struct node *node);
void print_tree(int indent, struct node *node);
int attach_at_pathname(struct node *root_node, struct dir_stack *cwd_ds,
		       seqf_t pathname, cap_t obj, int *err);
int resolve_populate
  (struct filesys_obj *root_obj, struct node *root_node,
   struct dir_stack *cwd_ds,
   seqf_t filename, int create, int *err);
struct filesys_obj *build_fs(struct node *node);

#endif
