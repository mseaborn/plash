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

#ifndef server_h
#define server_h


#include "filesysobj.h"


struct dir_stack {
  int refcount;
  struct filesys_obj *dir; /* Only those with OBJT_DIR */
  /* parent may be null if this is the root.
     In that case, name is null too. */
  struct dir_stack *parent;
  char *name;
};

void dir_stack_free(struct dir_stack *st);

struct dir_stack *dir_stack_root(struct filesys_obj *dir);
seqt_t string_of_cwd(region_t r, struct dir_stack *dir);

struct dir_stack *resolve_dir
  (region_t r, struct filesys_obj *root, struct dir_stack *cwd,
   seqf_t filename, int symlink_limit, int *err);

struct filesys_obj *resolve_file
  (region_t r, struct filesys_obj *root, struct dir_stack *cwd,
   seqf_t filename, int symlink_limit, int nofollow, int *err);

int exec_for_scripts
  (region_t r,
   struct filesys_obj *root, struct dir_stack *cwd,
   const char *cmd, int exec_fd, int argc, const char **argv,
   int *exec_fd_out, int *argc_out, const char ***argv_out,
   int *err);

int process_open(struct filesys_obj *root, struct dir_stack *cwd,
		 seqf_t pathname, int flags, int mode, int *err);


struct process {
  int sock_fd;
  struct filesys_obj *root;
  /* cwd may be null: processes may have an undefined current directory. */
  struct dir_stack *cwd;
};

struct process_list {
  int id; /* 0 for the list head */
  struct comm *comm;
  struct process *proc;
  struct process_list *prev, *next;
};

struct server_state {
  struct process_list list;
  int next_proc_id;

  /* Arguments to select(): */
  int max_fd;
  fd_set set;

  FILE *log; /* 0 if not doing logging */
  int log_summary, log_messages;
};

/* This is the error given when trying to access something through the
   cwd when no cwd is defined.
   One choice is EACCES ("Permission denied"). */
#define E_NO_CWD_DEFINED EACCES

struct filesys_obj *initial_dir(const char *pathname);
struct process *process_create(void);
void init_server_state(struct server_state *state);
void add_process(struct server_state *state, struct process *initial_proc);
void remove_process(struct process_list *node);
void run_server(struct server_state *state);

#define SYMLINK_LIMIT 100


#endif
