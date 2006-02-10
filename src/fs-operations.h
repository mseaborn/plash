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

#ifndef fs_operations_h
#define fs_operations_h


#include "filesysobj.h"

int process_open(struct filesys_obj *root, struct dir_stack *cwd,
		 seqf_t pathname, int flags, int mode, int *err,
		 int *dummy_fd);

int open_executable_file(struct filesys_obj *obj, seqf_t cmd_filename, int *err);

int exec_for_scripts
  (region_t r,
   struct filesys_obj *root, struct dir_stack *cwd,
   const char *cmd, int exec_fd, int argc, const char **argv,
   int *exec_fd_out, int *argc_out, const char ***argv_out,
   int *err);

/* This could be merged into struct fs_op_object now */
struct process {
  struct filesys_obj *root;
  /* cwd may be null: processes may have an undefined current directory. */
  struct dir_stack *cwd;
};

struct server_shared {
  int refcount;
  int next_id;

  FILE *log; /* 0 if not doing logging */
  int log_summary, log_messages;
};
void server_shared_free(struct server_shared *s);

struct fs_op_object {
  struct filesys_obj hdr;
  struct process p;
  struct server_shared *shared;
  int id;
};

void handle_fs_op_message(region_t r, struct process *proc,
			  seqf_t msg_orig, fds_t fds_orig,
			  seqt_t *reply, fds_t *reply_fds,
			  seqt_t *log_msg, seqt_t *log_reply);

cap_t make_fs_op_server(struct server_shared *shared,
			struct filesys_obj *root, struct dir_stack *cwd);

cap_t fs_op_maker_make(struct server_shared *shared);

cap_t conn_maker_make(void);
cap_t union_dir_maker_make(void);
cap_t fab_dir_maker_make(void);


#endif
