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

#ifndef cap_protocol_h
#define cap_protocol_h


#include "region.h"
#include "filesysobj.h"

/* `name' is just used for debugging; it's not freed. */
cap_t *cap_make_connection(region_t r, int sock_fd,
			   cap_seq_t export, int import_count,
			   const char *name);
void cap_run_server(void);
/* Returns 0 when there are no connections left to handle: */
int cap_run_server_step(void);
void cap_close_all_connections(void);

void local_obj_invoke(struct filesys_obj *obj,
		      seqt_t data1, cap_seq_t cap_args, fds_t fd_args);

void generic_obj_call(struct filesys_obj *obj, region_t r,
		      seqt_t data, cap_seq_t cap_args, fds_t fd_args,
		      seqt_t *r_data, cap_seq_t *r_cap_args, fds_t *r_fd_args);


#endif
