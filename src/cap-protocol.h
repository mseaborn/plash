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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA.  */

#ifndef cap_protocol_h
#define cap_protocol_h


#include "region.h"
#include "filesysobj.h"

/* `name' is just used for debugging; it's not freed. */
cap_t *cap_make_connection(region_t r, int sock_fd,
			   cap_seq_t export, int import_count,
			   const char *name);
/* This handles connections until there are no more objects exported.
   It prints a warning if imported objects remain. */
void cap_run_server(void);
int cap_server_exporting(void);
/* Returns 0 when there are no connections left to handle: */
int cap_run_server_step(void);
void cap_close_all_connections(void);

void cap_print_connections_info(FILE *fp);

#ifdef GC_DEBUG
void cap_mark_exported_objects(void);
#endif

/* This is a more complex interface for listening on connections for
   when you need to use select() on some other file descriptors at the
   same time. */
void cap_add_select_fds(int *max_fd,
			fd_set *read_fds,
			fd_set *write_fds,
			fd_set *except_fds);
void cap_handle_select_result(fd_set *read_fds,
			      fd_set *write_fds,
			      fd_set *except_fds);

#if 0
int cap_relocate_fd(int old_fd);
#endif


/* For cap-call-return.c: */

void local_obj_invoke(struct filesys_obj *obj, struct cap_args args);

void generic_obj_call(struct filesys_obj *obj, region_t r,
		      struct cap_args args, struct cap_args *result);

struct return_state {
  region_t r;
  int returned;
  /* When `returned' is set, the struct this points to is filled out: */
  struct cap_args *result;
};

cap_t make_return_cont(struct return_state *s);


#endif
