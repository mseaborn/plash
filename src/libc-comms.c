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
#include "cap-utils.h"


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

static char *strdup(const char *str)
{
  int len = strlen(str);
  char *x = amalloc(len + 1);
  memcpy(x, str, len + 1);
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
    char *var = glibc_getenv("PLASH_COMM_FD");
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

static int initialised = 0;
int comm_sock = -1;
char *process_caps_names = 0;
cap_seq_t process_caps = { 0, 0 }; /* uses a malloc'd block */
cap_t fs_server = 0;
cap_t conn_maker = 0;
cap_t fs_op_maker = 0; /* not used by libc itself, but needs to be passed on by fork() */

int plash_init()
{
  if(!initialised) {
    region_t r;
    cap_t *caps;
    char *var;
    int count, i;
    seqf_t cap_list, elt, list;
    
    var = glibc_getenv("PLASH_COMM_FD");
    if(!var) { __set_errno(ENOSYS); return -1; }
    comm_sock = my_atoi(var);

    var = glibc_getenv("PLASH_CAPS");
    if(!var) { __set_errno(ENOSYS); return -1; }
    process_caps_names = strdup(var);

    cap_list = seqf_string(var);
    list = cap_list;
    count = 0;
    /* Count the number of capabilities listed */
    while(parse_cap_list(list, &elt, &list)) count++;

    r = region_make();
    caps = cap_make_connection(r, comm_sock, caps_empty, count, "to-server");

    {
      cap_t *a = amalloc(count * sizeof(cap_t));
      memcpy(a, caps, count * sizeof(cap_t));
      process_caps.caps = a;
      process_caps.size = count;
    }

    /* Unpack the capabilities */
    list = cap_list;
    i = 0;
    while(parse_cap_list(list, &elt, &list)) {
      assert(i < count);
      if(seqf_equal(elt, seqf_string("fs_op"))) {
	fs_server = inc_ref(caps[i]);
      }
      else if(seqf_equal(elt, seqf_string("conn_maker"))) {
	conn_maker = inc_ref(caps[i]);
      }
      else if(seqf_equal(elt, seqf_string("fs_op_maker"))) {
	fs_op_maker = inc_ref(caps[i]);
      }
      i++;
    }
    region_free(r);
    initialised = 1;
  }
  return 0;
}

static void free_slot(cap_t *x) {
  if(*x) {
    filesys_obj_free(*x);
    *x = 0;
  }
}
/* Calling this will force the code to look at PLASH_COMM_FD and
   PLASH_CAPS again. */
/* EXPORT: plash_libc_reset_connection */
void plash_libc_reset_connection()
{
  if(initialised) {
    cap_close_all_connections();

    caps_free(process_caps);
    free((void *) process_caps.caps);
    process_caps = caps_empty;
    free(process_caps_names);
    process_caps_names = 0;

    free_slot(&fs_server);
    free_slot(&conn_maker);
    free_slot(&fs_op_maker);

    initialised = 0;
  }
}

int req_and_reply(region_t r, seqt_t msg, seqf_t *reply)
{
  struct cap_args result;
  if(plash_init() < 0) return -1;
  if(!fs_server) { __set_errno(ENOSYS); return -1; }
  cap_call(fs_server, r,
	   cap_args_make(msg, caps_empty, fds_empty),
	   &result);
  caps_free(result.caps);
  close_fds(result.fds);
  *reply = flatten_reuse(r, result.data);
  return 0;
}

int req_and_reply_with_fds2(region_t r, seqt_t msg, fds_t fds,
			    seqf_t *reply, fds_t *reply_fds)
{
  struct cap_args result;
  if(plash_init() < 0) return -1;
  if(!fs_server) { __set_errno(ENOSYS); return -1; }
  cap_call(fs_server, r,
	   cap_args_make(msg, caps_empty, fds),
	   &result);
  caps_free(result.caps);
  *reply = flatten_reuse(r, result.data);
  *reply_fds = result.fds;
  return 0;
}

int req_and_reply_with_fds(region_t r, seqt_t msg,
			   seqf_t *reply, fds_t *reply_fds)
{
  return req_and_reply_with_fds2(r, msg, fds_empty, reply, reply_fds);
}

/* EXPORT: plash_libc_duplicate_connection */
int plash_libc_duplicate_connection()
{
  region_t r;
  int i, fd;
  struct cap_args result;
  if(plash_init() < 0) return -1;
  if(!conn_maker) { __set_errno(ENOSYS); return -1; }

  r = region_make();
  for(i = 0; i < process_caps.size; i++) process_caps.caps[i]->refcount++;
  cap_call(conn_maker, r,
	   cap_args_make(cat2(r, mk_string(r, "Mkco"), mk_int(r, 0)),
			 process_caps, fds_empty),
	   &result);
  if(expect_fd1(result, &fd) < 0) {
    region_free(r);
    return -1;
  }
  region_free(r);
  return fd;
}

#endif


#include "out-aliases_libc-comms.h"
