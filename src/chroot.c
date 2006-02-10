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
#include "cap-utils.h"
#include "plash-libc.h"


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
    struct cap_args result;

    cap_call(fs_server, r,
	     cap_args_make(cat2(r, mk_string(r, "Gdir"),
				mk_string(r, argv[1])),
			   caps_empty, fds_empty),
	     &result);
    if(expect_cap1(result, &root_dir) < 0) {
      printf("get-root failed\n");
      return 1;
    }
    
    cap_call(fs_op_maker, r,
	     cap_args_make(mk_string(r, "Mkfs"),
			   mk_caps1(r, root_dir), fds_empty),
	     &result);
    if(expect_cap1(result, &new_fs_server) < 0) {
      printf("mkfs failed\n");
      return 1;
    }

    {
      int count = 3;
      cap_t *a = region_alloc(r, count * sizeof(cap_t));
      a[0] = new_fs_server;
      a[1] = conn_maker;
      a[2] = fs_op_maker;
      cap_call(conn_maker, r,
	       cap_args_make(cat2(r, mk_string(r, "Mkco"), mk_int(r, 0)),
			     cap_seq_make(a, count),
			     fds_empty),
	       &result);
    }
    {
      int fd;
      char buf[20];
      if(expect_fd1(result, &fd) < 0) {
	printf("mkco failed\n");
	return 1;
      }
      snprintf(buf, sizeof(buf), "%i", fd);
      setenv("PLASH_COMM_FD", buf, 1);
      setenv("PLASH_CAPS", "fs_op;conn_maker;fs_op_maker", 1);
    }
  }
  region_free(r);
  plash_libc_reset_connection();
  execve(argv[2], (char **) argv + 2, environ);
  perror("chroot: exec");
  return 1;
}
