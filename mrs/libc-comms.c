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

#include <errno.h>

#include "region.h"
#include "comms.h"
#include "libc-comms.h"
#include "cap-protocol.h"


char *glibc_getenv(const char *name);


static int my_atoi(const char *str)
{
  int x = 0;
  for(; *str; str++) {
    if('0' <= *str && *str <= '9') x = x*10 + (*str - '0');
    else return -1;
  }
  return x;
}


#ifdef SIMPLE_SERVER

int comm_sock = -1;
struct comm *comm = 0;

static int init()
{
  if(comm_sock == -2) {
    /* This could happen when something goes wrong in fork(). */
    __set_errno(EIO); return -1;
  }
  if(comm_sock == -1) {
    char *var = glibc_getenv("COMM_FD");
    if(var) comm_sock = my_atoi(var);
    else { __set_errno(ENOSYS); return -1; }
  }
  if(!comm) {
    comm = comm_init(comm_sock);
  }
  return 0;
}

/* Send a message, expecting a reply without FDs. */
int req_and_reply_with_fds(region_t r, seqt_t msg,
			   seqf_t *reply, fds_t *reply_fds)
{
  if(init() < 0) return -1;
  if(comm_send(r, comm_sock, msg, fds_empty) < 0) return -1;
  if(comm_get(comm, reply, reply_fds) <= 0) return -1;
  return 0;
}
int req_and_reply_with_fds2(region_t r, seqt_t msg, fds_t fds,
			    seqf_t *reply, fds_t *reply_fds)
{
  if(init() < 0) return -1;
  if(comm_send(r, comm_sock, msg, fds) < 0) return -1;
  if(comm_get(comm, reply, reply_fds) <= 0) return -1;
  return 0;
}
int req_and_reply(region_t r, seqt_t msg, seqf_t *reply)
{
  fds_t fds;
  if(req_and_reply_with_fds(r, msg, reply, &fds) < 0) return -1;
  close_fds(fds);
  return 0;
}

#else

int comm_sock = -1;
cap_t fs_server = 0;

static int init()
{
  if(!fs_server) {
    region_t r;
    cap_t *caps;
    
    char *var = glibc_getenv("COMM_FD");
    if(!var) { __set_errno(ENOSYS); return -1; }
    comm_sock = my_atoi(var);

    r = region_make();
    caps = cap_make_connection(r, comm_sock, caps_empty, 1, "to-server");
    fs_server = caps[0];
    region_free(r);
  }
  return 0;
}

int req_and_reply(region_t r, seqt_t msg, seqf_t *reply)
{
  seqt_t reply1;
  cap_seq_t reply_caps;
  fds_t reply_fds;
  init();
  fs_server->vtable->cap_call(fs_server, r, msg, caps_empty, fds_empty,
			      &reply1, &reply_caps, &reply_fds);
  caps_free(reply_caps);
  close_fds(reply_fds);
  *reply = flatten_reuse(r, reply1);
  return 0;
}

int req_and_reply_with_fds2(region_t r, seqt_t msg, fds_t fds,
			    seqf_t *reply, fds_t *reply_fds)
{
  seqt_t reply1;
  cap_seq_t reply_caps;
  init();
  fs_server->vtable->cap_call(fs_server, r, msg, caps_empty, fds,
			      &reply1, &reply_caps, reply_fds);
  caps_free(reply_caps);
  *reply = flatten_reuse(r, reply1);
  return 0;
}

int req_and_reply_with_fds(region_t r, seqt_t msg,
			   seqf_t *reply, fds_t *reply_fds)
{
  return req_and_reply_with_fds2(r, msg, fds_empty, reply, reply_fds);
}

#endif
