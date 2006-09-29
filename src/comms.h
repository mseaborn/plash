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

#ifndef comms_h
#define comms_h

#include "region.h"


struct comm {
  int sock;
  
  char *buf;
  int buf_size; /* Allocated size of buf */
  int pos; /* Position of message being received; number of consumed bytes in buf */
  int got; /* Number of bytes received after pos */

  int *fds_buf;
  int fds_buf_size;
  int fds_pos;
  int fds_got;
};

#define COMM_END 0
#define COMM_AVAIL 1
#define COMM_UNAVAIL 2

struct comm *comm_init(int sock);
void comm_free(struct comm *comm);
int comm_read(struct comm *comm, int *err);
int comm_try_get(struct comm *comm, seqf_t *result_data, fds_t *result_fds);
int comm_get(struct comm *comm, seqf_t *result_data, fds_t *result_fds);
int comm_send(region_t r, int sock, seqt_t msg, fds_t fds);


#endif
