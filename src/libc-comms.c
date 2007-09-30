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

#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#include "region.h"
#include "comms.h"
#include "libc-comms.h"
#include "cap-protocol.h"
#include "cap-utils.h"
#include "marshal.h"


void new_plash_libc_reset_connection(void);
int new_plash_libc_duplicate_connection(void);


/* Read a non-negative integer from a string.
   If str is NULL, returns -1.
   If str does not contain a non-negative integer, returns -1. */
int my_atoi(const char *str)
{
  int x = 0;
  if(str == NULL) {
    return -1;
  }
  for(; *str; str++) {
    if('0' <= *str && *str <= '9') x = x*10 + (*str - '0');
    else return -1;
  }
  return x;
}

static char *my_strdup(const char *str)
{
  int len = strlen(str);
  char *x = amalloc(len + 1);
  memcpy(x, str, len + 1);
  return x;
}


pthread_mutex_t libc_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

static int initialised = 0;
int comm_sock = -1;
char *process_caps_names = 0;
cap_seq_t process_caps = { 0, 0 }; /* uses a malloc'd block */
cap_t fs_server = 0;
cap_t conn_maker = 0;
cap_t fs_op_maker = 0; /* not used by libc itself, but needs to be passed on by fork() */
int libc_debug = FALSE;

int plash_init()
{
  if(!initialised) {
    region_t r;
    cap_t *caps;
    char *var;
    int count, i;
    seqf_t cap_list, elt, list;

    var = getenv("PLASH_LIBC_DEBUG");
    if(var) { libc_debug = TRUE; }
    
    var = getenv("PLASH_COMM_FD");
    if(!var) {
#ifdef ENABLE_LOGGING
      if(libc_debug) fprintf(stderr, "libc: PLASH_COMM_FD not set\n");
#endif
      __set_errno(ENOSYS);
      return -1;
    }
    comm_sock = my_atoi(var);
#ifdef ENABLE_LOGGING
    if(libc_debug) fprintf(stderr, "libc: init: comm_sock=%i\n", comm_sock);
#endif

    var = getenv("PLASH_CAPS");
    if(!var) {
#ifdef ENABLE_LOGGING
      if(libc_debug) fprintf(stderr, "libc: PLASH_CAPS not set\n");
#endif
      __set_errno(ENOSYS);
      return -1;
    }
    process_caps_names = my_strdup(var);

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


export(new_plash_libc_reset_connection, plash_libc_reset_connection);

/* Calling this will force the code to look at PLASH_COMM_FD and
   PLASH_CAPS again. */
void new_plash_libc_reset_connection(void)
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

    /* This allows dup2() and close() to clobber the file descriptor
       from this point on. */
    comm_sock = -1;
    
    initialised = 0;
  }
}

int libc_get_fs_op(cap_t *result)
{
  if(plash_init() < 0)
    return -1;
  if(!fs_server) {
    __set_errno(ENOSYS);
    return -1;
  }
  *result = fs_server;
  return 0;
}

int req_and_reply(region_t r, seqt_t msg, seqf_t *reply)
{
  struct cap_args result;
  if(plash_init() < 0) return -1;
  if(!fs_server) { __set_errno(ENOSYS); return -1; }

  plash_libc_lock();

  cap_call(fs_server, r,
	   cap_args_make(msg, caps_empty, fds_empty),
	   &result);
  caps_free(result.caps);
  close_fds(result.fds);
  *reply = flatten_reuse(r, result.data);

  plash_libc_unlock();
  return 0;
}

int req_and_reply_with_fds2(region_t r, seqt_t msg, fds_t fds,
			    seqf_t *reply, fds_t *reply_fds)
{
  struct cap_args result;
  if(plash_init() < 0) return -1;
  if(!fs_server) { __set_errno(ENOSYS); return -1; }

  plash_libc_lock();

  cap_call(fs_server, r,
	   cap_args_make(msg, caps_empty, fds),
	   &result);
  caps_free(result.caps);
  *reply = flatten_reuse(r, result.data);
  *reply_fds = result.fds;

  plash_libc_unlock();
  return 0;
}

void libc_log(const char *msg)
{
  region_t r = region_make();
  seqf_t reply;
  req_and_reply(r, cat2(r, mk_int(r, METHOD_FSOP_LOG),
			mk_string(r, msg)),
		&reply);
  region_free(r);
}

export(new_plash_libc_duplicate_connection, plash_libc_duplicate_connection);

int new_plash_libc_duplicate_connection(void)
{
  region_t r;
  int fd;
  cap_t *import; /* Contents not used */
  if(plash_init() < 0) return -1;
  if(!conn_maker) { __set_errno(ENOSYS); return -1; }

  r = region_make();
  fd = conn_maker->vtable->make_conn(conn_maker, r,
				     process_caps, 0 /* import_count */,
				     &import);
  region_free(r);
  return fd;
}
