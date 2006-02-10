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
#include "serialise.h"
#include "filesysobj.h"
#include "cap-protocol.h"
#include "cap-utils.h"
#include "plash-libc.h"


DECLARE_VTABLE(exec_obj_vtable);
struct exec_obj {
  struct filesys_obj hdr;
};

void exec_obj_free(struct filesys_obj *obj)
{
}

#define STAT_DEVICE 101

int exec_obj_stat(struct filesys_obj *obj, struct stat *st, int *err)
{
  st->st_dev = STAT_DEVICE;
  st->st_ino = 1; /* FIXME */
  st->st_mode = S_IFREG | 0111;
  st->st_nlink = 1;
  st->st_uid = 0;
  st->st_gid = 0;
  st->st_rdev = 0;
  st->st_size = 0;
  st->st_blksize = 1024;
  st->st_blocks = 0;
  st->st_atime = 0;
  st->st_mtime = 0;
  st->st_ctime = 0;
  return 0;
}

struct fd_mapping {
  int fd_no;
  int fd;
};

void exec_obj_call(struct filesys_obj *obj1, region_t r,
		   struct cap_args args, struct cap_args *result)
{
  {
    seqf_t data = flatten_reuse(r, args.data);
    int ok = 1;
    m_str(&ok, &data, "Exep");
    m_end(&ok, &data);
    if(ok) {
      result->data = mk_string(r, "Okay");
      result->caps = caps_empty;
      result->fds = fds_empty;
      return;
    }
  }
  {
    seqf_t data = flatten_reuse(r, args.data);
    bufref_t args_ref;
    int ok = 1;
    m_str(&ok, &data, "Exeo");
    m_int(&ok, &data, &args_ref);
    if(ok) {
      char **argv = 0, **env = 0;
      struct fd_mapping *fds = 0;
      int fds_count;
      cap_t root_dir = 0;
      seqf_t cwd;
      int got_cwd = 0;

      struct arg_m_buf argbuf = { data, args.caps, args.fds };
      int i;
      int size;
      const bufref_t *a;
      if(argm_array(&argbuf, args_ref, &size, &a)) goto error;
      for(i = 0; i < size; i++) {
	bufref_t tag_ref, arg_ref;
	seqf_t tag;
	if(argm_pair(&argbuf, a[i], &tag_ref, &arg_ref) ||
	   argm_str(&argbuf, tag_ref, &tag)) goto error;
	
	if(seqf_equal(tag, seqf_string("Argv"))) {
	  int i;
	  int count;
	  const bufref_t *a;
	  if(argm_array(&argbuf, arg_ref, &count, &a)) goto error;
	  argv = region_alloc(r, (count + 1) * sizeof(char *));
	  for(i = 0; i < count; i++) {
	    seqf_t str;
	    if(argm_str(&argbuf, a[i], &str)) goto error;
	    argv[i] = region_strdup_seqf(r, str);
	  }
	  argv[count] = 0;
	}
	else if(seqf_equal(tag, seqf_string("Env."))) {
	  int i;
	  int count;
	  const bufref_t *a;
	  if(argm_array(&argbuf, arg_ref, &count, &a)) goto error;
	  env = region_alloc(r, (count + 1) * sizeof(char *));
	  for(i = 0; i < count; i++) {
	    seqf_t str;
	    if(argm_str(&argbuf, a[i], &str)) goto error;
	    env[i] = region_strdup_seqf(r, str);
	  }
	  env[count] = 0;
	}
	else if(seqf_equal(tag, seqf_string("Fds."))) {
	  int i;
	  const bufref_t *a;
	  if(argm_array(&argbuf, arg_ref, &fds_count, &a)) goto error;
	  fds = region_alloc(r, fds_count * sizeof(struct fd_mapping));
	  for(i = 0; i < fds_count; i++) {
	    bufref_t no_ref, fd_ref;
	    if(argm_pair(&argbuf, a[i], &no_ref, &fd_ref) ||
	       argm_int(&argbuf, no_ref, &fds[i].fd_no) ||
	       argm_fd(&argbuf, fd_ref, &fds[i].fd)) goto error;
	  }
	}
	else if(seqf_equal(tag, seqf_string("Root"))) {
	  if(argm_cap(&argbuf, arg_ref, &root_dir)) goto error;
	}
	else if(seqf_equal(tag, seqf_string("Cwd."))) {
	  if(argm_str(&argbuf, arg_ref, &cwd)) goto error;
	  got_cwd = 1;
	}
	else goto error;
      }

      if(!argv || !env || !fds || !root_dir) goto error;
      {
	int i;
	for(i = 0; i < fds_count; i++) {
	  if(fds[i].fd_no == 1) {
	    const char *msg = "Hello, world!\n";
	    write(fds[i].fd, msg, strlen(msg));
	    break;
	  }
	}
      }
      if(0) {
	int pid = fork();
	if(pid == 0) {
	  execve("/foo", argv, env);
	}
      }
    }
  }
 error:
  marshall_cap_call(obj1, r, args, result);
  
  /*caps_free(args.caps);
  close_fds(args.fds);
  result->data = mk_string(r, "RMsg");
  result->caps = caps_empty;
  result->fds = fds_empty;*/
}

struct filesys_obj_vtable exec_obj_vtable = {
  /* .free = */ exec_obj_free,

  /* .cap_invoke = */ local_obj_invoke,
  /* .cap_call = */ exec_obj_call,
  /* .single_use = */ 0,

  /* .type = */ objt_file,
  /* .stat = */ exec_obj_stat,
  /* .utimes = */ dummy_utimes,
  /* .chmod = */ dummy_chmod,
  /* .open = */ dummy_open,
  /* .connect = */ dummy_socket_connect,
  /* .traverse = */ dummy_traverse,
  /* .list = */ dummy_list,
  /* .create_file = */ dummy_create_file,
  /* .mkdir = */ dummy_mkdir,
  /* .symlink = */ dummy_symlink,
  /* .rename = */ dummy_rename_or_link,
  /* .link = */ dummy_rename_or_link,
  /* .unlink = */ dummy_unlink,
  /* .rmdir = */ dummy_rmdir,
  /* .socket_bind = */ dummy_socket_bind,
  /* .readlink = */ dummy_readlink,
  1
};


int main(int argc, const char *argv[])
{
  region_t r;
  cap_t *caps;
  char *var;
  int sock_fd;
  int count, i;
  seqf_t cap_list, elt, list;
  cap_t fs_server = 0, fs_op_maker = 0, conn_maker = 0, union_dir_maker = 0,
    fab_dir_maker = 0, return_cont = 0;
  
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
    else if(seqf_equal(elt, seqf_string("union_dir_maker"))) {
      union_dir_maker = caps[i];
      union_dir_maker->refcount++;
    }
    else if(seqf_equal(elt, seqf_string("fab_dir_maker"))) {
      fab_dir_maker = caps[i];
      fab_dir_maker->refcount++;
    }
    else if(seqf_equal(elt, seqf_string("return_cont"))) {
      return_cont = caps[i];
      return_cont->refcount++;
    }
    i++;
  }

  assert(fs_server);
  assert(fs_op_maker);
  assert(conn_maker);
  assert(union_dir_maker);
  assert(fab_dir_maker);
  assert(return_cont);
  {
    cap_t exec_obj;
    cap_t root_dir;
    cap_t extra_dir;
    cap_t new_root_dir;
    cap_t new_fs_server;
    struct cap_args result;

    {
      struct exec_obj *obj = amalloc(sizeof(struct exec_obj));
      obj->hdr.refcount = 1;
      obj->hdr.vtable = &exec_obj_vtable;
      exec_obj = (struct filesys_obj *) obj;
    }

    cap_call(fs_server, r,
	     cap_args_make(mk_string(r, "Grtd"),
			   caps_empty, fds_empty),
	     &result);
    if(expect_cap1(result, &root_dir) < 0) {
      printf("get-root failed\n");
      return 1;
    }

    cap_invoke(return_cont,
	       cap_args_make(mk_string(r, "Okay"),
			     mk_caps1(r, exec_obj),
			     fds_empty));
  }
  return 0;
}
