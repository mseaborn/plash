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
#include "marshal.h"
#include "marshal-exec.h"
#include "marshal-pack.h"


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

#ifdef GC_DEBUG
void exec_obj_mark(struct filesys_obj *obj1)
{
  struct exec_obj *obj = (void *) obj1;
  filesys_obj_mark(obj->root_dir);
  filesys_obj_mark(obj->conn_maker);
  filesys_obj_mark(obj->fs_op_maker);
  filesys_obj_mark(obj->union_dir_maker);
}
#endif

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

void handle_process_status(void *x, int status)
{
  if(!WIFSTOPPED(status)) {
    cap_t return_cont = x;
    region_t r = region_make();
    cap_invoke(return_cont,
	       cap_args_make(cat2(r, mk_int(r, METHOD_R_EO_EXEC), mk_int(r, status)),
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
    m_int_const(&ok, &data, METHOD_EO_EXEC);
    m_int(&ok, &data, &args_ref);
    if(ok && args.caps.size >= 1) {
      int err, i;

      cap_t return_cont = args.caps.caps[0];
      
      struct arg_m_buf argbuf =
	{ data, { args.caps.caps+1, args.caps.size-1 }, args.fds };
      struct exec_args ea;
      if(unpack_exec_args(r, argbuf, args_ref, &ea) < 0) goto error;
      if(!ea.argv || !ea.env || !ea.fds || !ea.root_dir) goto error;

      {
	struct fd_array fds2 = { 0, 0 };
	struct cap_args result;
	cap_t new_root_dir;
	cap_t new_fs_server;
	int sock_fd, sock_fd_no;
	cap_t *a, *import;
	int cap_count;
	int pid;
      
	for(i = 0; i < ea.fds_count; i++) {
	  array_set_fd(r, &fds2, ea.fds[i].fd_no, ea.fds[i].fd);
	}

	cap_call(obj->union_dir_maker, r,
		 pl_pack(r, METHOD_MAKE_UNION_DIR, "cc",
			 inc_ref(obj->root_dir), inc_ref(ea.root_dir)),
		 &result);
	if(!pl_unpack(r, result, METHOD_R_CAP, "c", &new_root_dir)) {
	  err = EIO;
	  fprintf(stderr, "make_union_dir failed\n");
	  pl_args_free(&result);
	  goto exec_error;
	}

	cap_call(obj->fs_op_maker, r,
		 pl_pack(r, METHOD_MAKE_FS_OP, "c", new_root_dir),
		 &result);
	if(!pl_unpack(r, result, METHOD_R_CAP, "c", &new_fs_server)) {
	  err = EIO;
	  fprintf(stderr, "make_fs_op failed\n");
	  pl_args_free(&result);
	  goto exec_error;
	}

	/* If we were passed a cwd (current working directory) argument,
	   set the cwd.  If this fails, abort starting the new process. */
	if(ea.got_cwd) {
	  cap_call(new_fs_server, r,
		   cap_args_d(cat2(r, mk_int(r, METHOD_FSOP_CHDIR),
				   mk_leaf(r, ea.cwd))),
		   &result);
	  {
	    seqf_t msg = flatten_reuse(r, result.data);
	    int ok = 1;
	    m_int_const(&ok, &msg, METHOD_OKAY);
	    m_end(&ok, &msg);
	    if(!(ok && result.caps.size == 0 && result.fds.count == 0)) {
	      pl_args_free(&result);
	      err = EIO;
	      goto exec_error;
	    }
	  }
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

	pid = fork();
	if(pid == 0) {
	  char buf[20];
	  fds_t inst_fds = { fds2.fds, fds2.count };
	  
	  /* Close sockets */
	  cap_close_all_connections();
	  unsetenv("PLASH_COMM_FD");
	  assert(plash_libc_reset_connection);
	  plash_libc_reset_connection();
	  
	  if(install_fds(inst_fds) < 0) { exit(1); }
	  
	  snprintf(buf, sizeof(buf), "%i", sock_fd_no);
	  if(setenv("PLASH_COMM_FD", buf, 1) < 0 ||
	     setenv("PLASH_CAPS", "fs_op;conn_maker;fs_op_maker", 1) < 0) {
	    fprintf(stderr, "setenv failed");
	    exit(1);
	  }
	  plash_libc_reset_connection();

	  if(ea.pgid > 0) {
	    if(setpgid(0, ea.pgid) < 0) perror("exec-object: setpgid");
	  }
	  
	  execve(obj->cmd, ea.argv, environ /* FIXME */);
	  fprintf(stderr, "exec-object: exec(\"%s\"): %s\n", obj->cmd, strerror(errno));
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
  // struct exec_obj *obj = (void *) obj1;
  {
    seqf_t data = flatten_reuse(r, args.data);
    int ok = 1;
    m_int_const(&ok, &data, METHOD_EO_IS_EXECUTABLE);
    m_end(&ok, &data);
    if(ok) {
      result->data = mk_int(r, METHOD_OKAY);
      result->caps = caps_empty;
      result->fds = fds_empty;
      return;
    }
  }
  marshal_cap_call(obj1, r, args, result);
}

#include "out-vtable-exec-object.h"


int main(int argc, const char *argv[])
{
  region_t r = region_make();
  cap_t fs_server, fs_op_maker, conn_maker, union_dir_maker, return_cont;

  if(argc != 3) {
    printf(_("exec-object is only useful when run from Plash using the "
	     "capcmd expression.\n"));
    printf(_("Usage: %s <executable-pathname> <root-dir>\n"), argv[0]);
    return 1;
  }

  if(get_process_caps("fs_op", &fs_server,
		      "fs_op_maker", &fs_op_maker,
		      "conn_maker", &conn_maker,
		      "union_dir_maker", &union_dir_maker,
		      "return_cont", &return_cont,
		      0) < 0) return 1;

  {
    cap_t exec_obj;
    cap_t root_dir;
    struct cap_args result;

    cap_call(fs_server, r,
	     cap_args_make(cat2(r, mk_string(r, "Gdir"),
				mk_string(r, argv[2])),
			   caps_empty, fds_empty),
	     &result);
    filesys_obj_free(fs_server);
    if(expect_cap1(result, &root_dir) < 0) {
      fprintf(stderr, _("exec-object: get-root failed\n"));
      return 1;
    }

    {
      struct exec_obj *obj =
	filesys_obj_make(sizeof(struct exec_obj), &exec_obj_vtable);
      obj->root_dir = root_dir;
      obj->cmd = strdup(argv[1]);
      obj->conn_maker = conn_maker;
      obj->fs_op_maker = fs_op_maker;
      obj->union_dir_maker = union_dir_maker;
      exec_obj = (struct filesys_obj *) obj;
    }

    cap_invoke(return_cont,
	       cap_args_make(mk_int(r, METHOD_OKAY),
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

  printf(_("exec-object server for `%s' finished\n"), argv[1]);
  return 0;
}
