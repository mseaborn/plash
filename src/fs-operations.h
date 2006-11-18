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

#ifndef fs_operations_h
#define fs_operations_h


#include "filesysobj.h"
#include "resolve-filename.h"

int process_open(struct filesys_obj *root, struct dir_stack *cwd,
		 seqf_t pathname, int flags, int mode, int *err);

/* This could be merged into struct fs_op_object now */
struct process {
  struct filesys_obj *root;
  /* cwd may be null: processes may have an undefined current directory. */
  struct dir_stack *cwd;
};

struct fs_op_object {
  struct filesys_obj hdr;
  struct process p;
  struct filesys_obj *log; /* May be NULL */
};

cap_t make_fs_op_server(struct filesys_obj *log,
			struct filesys_obj *root, struct dir_stack *cwd);

cap_t fs_op_maker_make(struct filesys_obj *log);

cap_t conn_maker_make(void);
cap_t union_dir_maker_make(void);
cap_t fab_dir_maker_make(void);


#endif
