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

/* Used to get `environ' declared */
#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#include "region.h"
#include "serialise.h"
#include "filesysobj.h"
#include "cap-protocol.h"
#include "cap-utils.h"
#include "plash-libc.h"
#include "shell-fds.h"
#include "shell-wait.h"


DECLARE_VTABLE(exec_obj_vtable);
struct exec_obj {
  struct filesys_obj hdr;
  cap_t root_dir, conn_maker, fs_op_maker, union_dir_maker;
  char *cmd;
};

void exec_obj_free(struct filesys_obj *obj1)
{
  struct exec_obj *obj = (void *) obj1;
  filesys_obj_free(obj->root_dir);
  filesys_obj_free(obj->conn_maker);
  filesys_obj_free(obj->fs_op_maker);
  filesys_obj_free(obj->union_dir_maker);
  free(obj->cmd);
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

int refuse_open(struct filesys_obj *obj, int flags, int *err)
{
  *err = EACCES;
  return -1;
}

struct fd_mapping {
  int fd_no;
  int fd;
};

void handle_process_status(void *x, int status)
{
  if(!WIFSTOPPED(status)) {
    cap_t return_cont = x;
    region_t r = region_make();
    cap_invoke(return_cont,
	       cap_args_make(cat2(r, mk_string(r, "Okay"), mk_int(r, status)),
			     caps_empty,
			     fds_empty));
    filesys_obj_free(return_cont);
    region_free(r);
  }
}

/* Handles invoke so that it can invoke the return continuation later. */
void exec_obj_invoke(struct filesys_obj *obj1, struct cap_args args)
{
  struct exec_obj *obj = (void *) obj1;
  region_t r = region_make();
  {
    seqf_t data = flatten_reuse(r, args.data);
    bufref_t args_ref;
    int ok = 1;
    m_str(&ok, &data, "Call");
    m_str(&ok, &data, "Exeo");
    m_int(&ok, &data, &args_ref);
    if(ok && args.caps.size >= 1) {
      int err;
      char **argv = 0, **env = 0;
      struct fd_mapping *fds = 0;
      int fds_count;
      cap_t root_dir = 0;
      seqf_t cwd;
      int got_cwd = 0;
      int pgid = 0;

      cap_t return_cont = args.caps.caps[0];
      
      struct arg_m_buf argbuf =
	{ data, { args.caps.caps+1, args.caps.size-1 }, args.fds };
      int i;
      int size;
      const bufref_t *a;

      // arg_print(stderr, &argbuf, args_ref);

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
	else if(seqf_equal(tag, seqf_string("Pgid"))) {
	  if(argm_int(&argbuf, arg_ref, &pgid)) goto error;
	}
	else goto error;
      }

      if(!argv || !env || !fds || !root_dir) goto error;

      {
	struct fd_array fds2 = { 0, 0 };
	cap_t new_root_dir;
	cap_t new_fs_server;
	int sock_fd, sock_fd_no;
	cap_t *a, *import;
	int cap_count;
	int pid;
      
	for(i = 0; i < fds_count; i++) {
	  array_set_fd(r, &fds2, fds[i].fd_no, fds[i].fd);
	}

	new_root_dir = obj->union_dir_maker->vtable->make_union_dir(obj->union_dir_maker, obj->root_dir, root_dir);
	if(!new_root_dir) {
	  err = EIO;
	  goto exec_error;
	}

	filesys_obj_check(obj->root_dir);
	new_fs_server = obj->fs_op_maker->vtable->make_fs_op(obj->fs_op_maker, new_root_dir);
	filesys_obj_free(new_root_dir);
	if(!new_fs_server) {
	  err = EIO;
	  goto exec_error;
	}

	cap_count = 3;
	a = region_alloc(r, cap_count * sizeof(cap_t));
	a[0] = new_fs_server;
	a[1] = obj->conn_maker;
	a[2] = obj->fs_op_maker;
	sock_fd = obj->conn_maker->vtable->make_conn(obj->conn_maker, r, cap_seq_make(a, cap_count), 0 /* import_count */, &import);
	filesys_obj_free(new_fs_server);
	if(sock_fd < 0) {
	  err = EIO;
	  goto exec_error;
	}
	region_add_finaliser(r, finalise_close_fd, (void *) sock_fd);

	sock_fd_no = array_get_free_index(&fds2);
	array_set_fd(r, &fds2, sock_fd_no, sock_fd);
	{
	  char buf[20];
	  snprintf(buf, sizeof(buf), "%i", sock_fd_no);
	  setenv("PLASH_COMM_FD", buf, 1);
	  setenv("PLASH_CAPS", "fs_op;conn_maker;fs_op_maker", 1);
	}

	pid = fork();
	if(pid == 0) {
	  fds_t inst_fds = { fds2.fds, fds2.count };
	  
	  /* Close sockets */
	  cap_close_all_connections();
	  plash_libc_reset_connection();
	  
	  install_fds(inst_fds);

	  if(pgid > 0) {
	    if(setpgid(0, pgid) < 0) perror("exec-object: setpgid");
	  }
	  
	  execve(obj->cmd, argv, environ /* FIXME */);
	  fprintf(stderr, "exec-object: %s: %s\n", obj->cmd, strerror(errno));
	  exit(1);
	}
	if(pid < 0) { err = errno; goto exec_error; }
	else {
	  w_add_process(pid, handle_process_status, return_cont);
	}
      }
      caps_free(argbuf.caps); /* free all caps except return_cont */
      close_fds(args.fds);
      region_free(r);
      return;

    exec_error:
      cap_invoke(return_cont,
		 cap_args_make(cat2(r, mk_string(r, "Fail"),
				    mk_int(r, err)),
			       caps_empty,
			       fds_empty));
      filesys_obj_free(return_cont);
      caps_free(argbuf.caps); /* free all caps except return_cont */
      close_fds(args.fds);
      region_free(r);
      return;
    }
  }
 error:
  region_free(r);
  local_obj_invoke(obj1, args);
}

void exec_obj_call(struct filesys_obj *obj1, region_t r,
		   struct cap_args args, struct cap_args *result)
{
  struct exec_obj *obj = (void *) obj1;
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
 error:
  marshal_cap_call(obj1, r, args, result);
  
  /*caps_free(args.caps);
  close_fds(args.fds);
  result->data = mk_string(r, "RMsg");
  result->caps = caps_empty;
  result->fds = fds_empty;*/
}

#include "out-vtable-exec-object.h"


int get_cap(seqf_t elt, cap_t x, const char *name, cap_t *store)
{
  if(seqf_equal(elt, seqf_string(name))) {
    if(*store) filesys_obj_free(*store);
    *store = inc_ref(x);
    return 1;
  }
  return 0;
}

int main(int argc, const char *argv[])
{
  region_t r;
  cap_t *caps;
  char *var;
  int sock_fd;
  int count, i;
  seqf_t cap_list, elt, list;
  cap_t fs_server = 0, fs_op_maker = 0, conn_maker = 0, union_dir_maker = 0,
    // fab_dir_maker = 0,
    return_cont = 0;

  if(argc != 3) {
    printf("exec-object is only useful when run from Plash using the "
	   "capcmd expression.\n");
    printf("Usage: %s <executable-pathname> <root-dir>\n", argv[0]);
    return 1;
  }
  
  var = getenv("PLASH_CAPS");
  if(!var) { fprintf(stderr, "exec-object: no caps\n"); return 1; }
  sock_fd = plash_libc_duplicate_connection();
  if(sock_fd < 0) { fprintf(stderr, "exec-object: can't dup\n"); return 1; }

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
    get_cap(elt, caps[i], "fs_op", &fs_server) ||
    get_cap(elt, caps[i], "fs_op_maker", &fs_op_maker) ||
    get_cap(elt, caps[i], "conn_maker", &conn_maker) ||
    // get_cap(elt, caps[i], "fab_dir_maker", &fab_dir_maker) ||
    get_cap(elt, caps[i], "union_dir_maker", &union_dir_maker) ||
    get_cap(elt, caps[i], "return_cont", &return_cont);

    filesys_obj_free(caps[i]);
    i++;
  }

  assert(fs_server);
  assert(fs_op_maker);
  assert(conn_maker);
  assert(union_dir_maker);
  // assert(fab_dir_maker);
  assert(return_cont);
  {
    cap_t exec_obj;
    cap_t root_dir;
    cap_t extra_dir;
    cap_t new_root_dir;
    cap_t new_fs_server;
    struct cap_args result;

    cap_call(fs_server, r,
	     cap_args_make(cat2(r, mk_string(r, "Gdir"),
				mk_string(r, argv[2])),
			   caps_empty, fds_empty),
	     &result);
    filesys_obj_free(fs_server);
    if(expect_cap1(result, &root_dir) < 0) {
      fprintf(stderr, "exec-object: get-root failed\n");
      return 1;
    }

    {
      struct exec_obj *obj = amalloc(sizeof(struct exec_obj));
      obj->hdr.refcount = 1;
      obj->hdr.vtable = &exec_obj_vtable;
      obj->root_dir = root_dir;
      obj->cmd = strdup(argv[1]);
      obj->conn_maker = conn_maker;
      obj->fs_op_maker = fs_op_maker;
      obj->union_dir_maker = union_dir_maker;
      exec_obj = (struct filesys_obj *) obj;
    }

    cap_invoke(return_cont,
	       cap_args_make(mk_string(r, "Okay"),
			     mk_caps1(r, exec_obj),
			     fds_empty));
    filesys_obj_free(return_cont);
  }

  w_setup();
  // cap_run_server();
  while(cap_run_server_step()) {
    while(1) {
      int status;
      /* WUNTRACED is unnecessary but doesn't hurt. */
      int pid = waitpid(-1, &status, WUNTRACED | WNOHANG);
      if(pid < 0 && errno != ECHILD) perror("exec-object: waitpid");
      if(pid <= 0) break;
      w_handle_process_status(pid, status);
    }
  }

  printf("exec-object server for `%s' finished\n", argv[1]);
  return 0;
}
