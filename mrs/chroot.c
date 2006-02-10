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

#include <stdio.h>
#include <unistd.h>

#include "region.h"
#include "filesysobj.h"
#include "cap-protocol.h"
#include "plash-libc.h"


static int parse_cap_list(seqf_t list, seqf_t *elt, seqf_t *rest)
{
  if(list.size > 0) {
    int i = 0;
    while(i < list.size) {
      if(list.data[i] == ';') {
	elt->data = list.data;
	elt->size = i;
	rest->data = list.data + i + 1;
	rest->size = list.size - i - 1;
	return 1;
      }
      i++;
    }
    elt->data = list.data;
    elt->size = i;
    rest->data = 0;
    rest->size = 0;
    return 1;
  }
  else return 0;
}

extern char **environ;

int main(int argc, const char *argv[])
{
  region_t r;
  cap_t *caps;
  char *var;
  int sock_fd;
  int count, i;
  seqf_t cap_list, elt, list;
  cap_t fs_server = 0, fs_op_maker = 0, conn_maker = 0;

  if(argc < 3) {
    fprintf(stderr, "Usage: chroot <dir> <executable> <args...>\n");
    return 1;
  }
  
  var = getenv("PLASH_CAPS");
  if(!var) { fprintf(stderr, "no caps\n"); return 1; }
  sock_fd = plash_libc_duplicate_connection();
  if(sock_fd < 0) { fprintf(stderr, "can't dup\n"); return 1; }

  cap_list = seqf_string(var);
  list = cap_list;
  count = 0;
  /* Count the number of capabilities listed */
  while(parse_cap_list(list, &elt, &list)) count++;
  
  r = region_make();
  caps = cap_make_connection(r, sock_fd, caps_empty, count, "to-server");

  /* Unpack the capabilities */
  list = cap_list;
  i = 0;
  while(parse_cap_list(list, &elt, &list)) {
    assert(i < count);
    if(seqf_equal(elt, seqf_string("fs_op"))) {
      fs_server = caps[i];
      fs_server->refcount++;
    }
    else if(seqf_equal(elt, seqf_string("fs_op_maker"))) {
      fs_op_maker = caps[i];
      fs_op_maker->refcount++;
    }
    else if(seqf_equal(elt, seqf_string("conn_maker"))) {
      conn_maker = caps[i];
      conn_maker->refcount++;
    }
    i++;
  }

  assert(fs_server);
  assert(fs_op_maker);
  assert(conn_maker);
  {
    cap_t root_dir;
    cap_t new_fs_server;

    cap_seq_t caps;
    seqt_t reply;
    cap_seq_t reply_caps;
    fds_t reply_fds;

    fs_server->vtable->cap_call(fs_server, r,
				cat2(r, mk_string(r, "Gdir"),
				     mk_string(r, argv[1])),
				caps_empty, fds_empty,
				&reply, &reply_caps, &reply_fds);
    close_fds(reply_fds);
    {
      seqf_t msg = flatten_reuse(r, reply);
      int ok = 1;
      m_str(&ok, &msg, "Okay");
      m_end(&ok, &msg);
      if(ok && reply_caps.size == 1) {
	root_dir = reply_caps.caps[0];
      }
      else {
	printf("get-root failed\n");
	return 1;
      }
    }
    
    fs_op_maker->vtable->cap_call(fs_op_maker, r,
				   mk_string(r, "Mkfs"),
				   mk_caps1(r, root_dir), fds_empty,
				   &reply, &reply_caps, &reply_fds);
    close_fds(reply_fds);
    {
      seqf_t msg = flatten_reuse(r, reply);
      int ok = 1;
      m_str(&ok, &msg, "Okay");
      m_end(&ok, &msg);
      if(ok && reply_caps.size == 1) {
	new_fs_server = reply_caps.caps[0];
      }
      else {
	printf("mkfs failed\n");
	return 1;
      }
    }

    {
      int count = 3;
      cap_t *a = region_alloc(r, count * sizeof(cap_t));
      a[0] = new_fs_server;
      a[1] = conn_maker;
      a[2] = fs_op_maker;
      caps.caps = a;
      caps.size = count;
    }
    conn_maker->vtable->cap_call(conn_maker, r,
				 cat2(r, mk_string(r, "Mkco"), mk_int(r, 0)),
				 caps, fds_empty,
				 &reply, &reply_caps, &reply_fds);
    {
      seqf_t msg = flatten_reuse(r, reply);
      int ok = 1;
      m_str(&ok, &msg, "Okay");
      m_end(&ok, &msg);
      if(ok && reply_fds.count == 1) {
	char buf[20];
	snprintf(buf, sizeof(buf), "%i", reply_fds.fds[0]);
	setenv("PLASH_COMM_FD", buf, 1);
	setenv("PLASH_CAPS", "fs_op;conn_maker;fs_op_maker", 1);
      }
      else {
	printf("mkco failed\n");
	return 1;
      }
    }
  }
  region_free(r);
  plash_libc_reset_connection();
  execve(argv[2], (char **) argv + 2, environ);
  perror("chroot: exec");
  return 1;
}
