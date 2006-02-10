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

#ifndef resolve_filename_h
#define resolve_filename_h


#include "filesysobj.h"


/* This is the error given when trying to access something through the
   cwd when no cwd is defined.
   One choice is EACCES ("Permission denied"). */
#define E_NO_CWD_DEFINED EACCES

#define SYMLINK_LIMIT 100


struct dir_stack {
  struct filesys_obj hdr;
  struct filesys_obj *dir; /* Only those with OBJT_DIR */
  /* parent may be null if this is the root.
     In that case, name is null too. */
  struct dir_stack *parent;
  char *name;
};

struct dir_stack *dir_stack_root(struct filesys_obj *dir);
struct dir_stack *dir_stack_make(struct filesys_obj *dir,
				 struct dir_stack *parent, char *name);
static inline void dir_stack_free(struct dir_stack *st)
{
  filesys_obj_free((struct filesys_obj *) st);
}
static inline struct filesys_obj *dir_stack_downcast(struct dir_stack *st)
{
  return (void *) st;
}
struct dir_stack *dir_stack_upcast(struct filesys_obj *obj);

seqt_t string_of_cwd(region_t r, struct dir_stack *dir);

struct dir_stack *resolve_dir
  (region_t r, struct filesys_obj *root, struct dir_stack *cwd,
   seqf_t filename, int symlink_limit, int *err);

struct filesys_obj *resolve_file
  (region_t r, struct filesys_obj *root, struct dir_stack *cwd,
   seqf_t filename, int symlink_limit, int nofollow, int *err);


struct resolved_slot {
  struct filesys_obj *dir; /* ref owned by this struct */
  char *leaf; /* malloc'd, owned by this struct */
};

#define CREATE_ONLY 2
#define RESOLVED_FILE_OR_SYMLINK 1
#define RESOLVED_DIR 2
#define RESOLVED_EMPTY_SLOT 3

void free_resolved_slot(struct resolved_slot *slot);

int resolve_obj(region_t r, struct filesys_obj *root, struct dir_stack *cwd,
		seqf_t filename, int symlink_limit,
		int nofollow, int create,
		void **result, int *err);

struct filesys_obj *resolve_obj_simple
  (struct filesys_obj *root, struct dir_stack *cwd,
   seqf_t pathname, int symlink_limit, int nofollow, int *err);
struct resolved_slot *resolve_empty_slot
  (struct filesys_obj *root, struct dir_stack *cwd,
   seqf_t pathname, int symlink_limit, int *err);
struct resolved_slot *resolve_any_slot
  (struct filesys_obj *root, struct dir_stack *cwd,
   seqf_t pathname, int symlink_limit, int *err);


#endif
