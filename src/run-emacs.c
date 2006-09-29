/* Copyright (C) 2005 Mark Seaborn

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

/* Used to get `environ' declared */
#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>

#include "region.h"
#include "filesysobj.h"
#include "cap-protocol.h"
#include "cap-utils.h"
#include "plash-libc.h"
#include "marshal.h"
#include "marshal-exec.h"
#include "marshal-pack.h"
#include "build-fs.h"
#include "parse-filename.h"


/* If `path' is a cwd-relative pathname, expands it relative to `dir'.
   Otherwise, `path', unmodified, is the result. */
/* Returns 0 for success, -1 for an invalid (empty) pathname. */
int absolute_pathname(region_t r, seqf_t dir, seqf_t path, seqf_t *result)
{
  int end;
  seqf_t temp;
  switch(filename_parse_start(path, &end, &temp)) {
    case FILENAME_ROOT:
      *result = path;
      return 0;
    case FILENAME_CWD:
      if(dir.size > 0) {
	seqt_t got;
	if(dir.data[dir.size - 1] == '/') {
	  got = cat2(r, mk_leaf(r, dir), mk_leaf(r, path));
	}
	else {
	  got = cat3(r, mk_leaf(r, dir), mk_string(r, "/"), mk_leaf(r, path));
	}
	*result = flatten(r, got);
	return 0;
      }
      else return -1; /* Error */
    default: return -1; /* Error */
  }
}


DECLARE_VTABLE(edit_obj_vtable);
struct edit_obj {
  struct filesys_obj hdr;
  fs_node_t root_node;

  /* File descriptor of pipe for sending messages to Emacs.  May be -1
     if we haven't received it from Emacs yet. */
  int emacs_fd; 
};

void edit_obj_free(struct filesys_obj *obj1)
{
  struct edit_obj *obj = (void *) obj1;
  free_node(obj->root_node);
  if(obj->emacs_fd >= 0) close(obj->emacs_fd);
}

#ifdef GC_DEBUG
void edit_obj_mark(struct filesys_obj *obj1)
{
  struct edit_obj *obj = (void *) obj1;
  mark_node(obj->root_node);
}
#endif

#define STAT_DEVICE 101

int edit_obj_stat(struct filesys_obj *obj, struct stat *st, int *err)
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

int find_fd(struct exec_args *ea, int fd_number)
{
  int i;
  for(i = 0; i < ea->fds_count; i++) {
    if(ea->fds[i].fd_no == fd_number) {
      return ea->fds[i].fd;
    }
  }
  return -1;
}

/* Handles invoke so that it can invoke the return continuation later. */
void edit_obj_invoke(struct filesys_obj *obj1, struct cap_args args)
{
  struct edit_obj *obj = (void *) obj1;
  region_t r = region_make();
  {
    seqf_t data = flatten_reuse(r, args.data);
    bufref_t args_ref;
    int ok = 1;
    m_str(&ok, &data, "Call");
    m_int_const(&ok, &data, METHOD_EO_EXEC);
    m_int(&ok, &data, &args_ref);
    if(ok && args.caps.size >= 1) {
      int i;

      cap_t return_cont = args.caps.caps[0];
      
      struct arg_m_buf argbuf =
	{ data, { args.caps.caps+1, args.caps.size-1 }, args.fds };
      struct exec_args ea;
      if(unpack_exec_args(r, argbuf, args_ref, &ea) < 0) goto error;
      if(!ea.argv || !ea.env || !ea.fds || !ea.root_dir) goto error;

      /* FIXME: this is a hack.  I have cut corners: there is not a
	 separate executable object for this.  There should be. */
      if(ea.argv[0] && ea.argv[1] && !strcmp(ea.argv[1], "--serv")) {
	int stdout = find_fd(&ea, STDOUT_FILENO);
	if(stdout < 0) {
	  printf("stdout not found\n");
	}
	else {
	  int fd = dup(stdout);
	  if(fd < 0) perror("dup");
	  else {
	    if(obj->emacs_fd >= 0) close(obj->emacs_fd);
	    obj->emacs_fd = fd;
	    printf("emacs has registered a communications channel with run-emacs\n");
	  }
	  /* Now we *deliberately* do not free return_cont, because we don't
	     want this process to exit.  Unfortunately, this risks leaving a
	     process lying around if Emacs dies without being able to kill
	     the child process it created. */
	}
      }
      else if(!ea.argv[0] || !ea.argv[1]) {
	/* Empty argument list:  print usage info */
	int stdout = find_fd(&ea, STDOUT_FILENO);
	const char *usage =
	  "Usage: <prog> <filenames>...\n"
	  "or better: <prog> => <filenames>... &\n";
	if(stdout >= 0) {
	  write(stdout, usage, strlen(usage));
	}
	cap_invoke(return_cont,
		   cap_args_make(cat2(r, mk_int(r, METHOD_R_EO_EXEC),
				      mk_int(r, 0)),
				 caps_empty, fds_empty));
	filesys_obj_free(return_cont);
      }
      else {
	i = 0;
	if(ea.argv[i]) i++;
	while(ea.argv[i]) {
	  const char *relative_filename = ea.argv[i];
	  const char *filename;
	  seqf_t full_filename;
	  int err;
	  struct dir_stack *ds_cwd;

	  if(ea.got_cwd) {
	    if(absolute_pathname(r, ea.cwd, seqf_string(relative_filename),
				 &full_filename) < 0) {
	      printf("bad pathname\n");
	      continue;
	    }
	    filename = region_strdup_seqf(r, full_filename);
	  }
	  else {
	    filename = relative_filename;
	  }
	  
	  ds_cwd = dir_stack_root(ea.root_dir);
	  /* FIXME: print to the stdout that we're passed in the method call */
	  printf("file: %s\n", filename);
	  if(fs_resolve_populate(ea.root_dir, obj->root_node, ds_cwd,
				 seqf_string(filename),
				 FS_SLOT_RWC | FS_FOLLOW_SYMLINKS, &err) < 0) {
	    printf("error resolving filename `%s': %s\n", filename,
		   strerror(err));
	  }
	  dir_stack_free(ds_cwd);

	  if(obj->emacs_fd >= 0) {
	    seqf_t buf = flatten(r, mk_printf(r, "%s\004", filename));
	    printf("sending to emacs\n");
	    /* Disable SIGPIPE signals which would kill this process --
	       we'd rather handle the error here.  I would use send() and
	       its MSG_NOSIGNAL flag, but it doesn't work on pipe FDs. */
	    signal(SIGPIPE, SIG_IGN);
	    while(buf.size > 0) {
	      int sent = write(obj->emacs_fd, buf.data, buf.size);
	      if(sent < 0) {
		fprintf(stderr, "error sending message to Emacs: %s\n",
			strerror(errno));
		break;
	      }
	      if(sent == 0) { printf("error sending\n"); break; }
	      buf.data += sent;
	      buf.size -= sent;
	    }
	  }
	  else {
	    printf("emacs has not registered a communications channel -- can't ask it to open file\n");
	  }
	  
	  i++;
	}
	printf("Emacs file namespace:\n");
	fs_print_tree(0, obj->root_node);
	cap_invoke(return_cont,
		   cap_args_make(cat2(r, mk_int(r, METHOD_R_EO_EXEC),
				      mk_int(r, 0)),
				 caps_empty, fds_empty));
	filesys_obj_free(return_cont);
      }
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

void edit_obj_call(struct filesys_obj *obj1, region_t r,
		   struct cap_args args, struct cap_args *result)
{
  // struct edit_obj *obj = (void *) obj1;
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

#include "out-vtable-run-emacs.h"


int main(int argc, const char *argv[])
{
  region_t r = region_make();
  cap_t fs_server, fs_op_maker, conn_maker, return_cont;
  cap_t orig_root_dir;
  fs_node_t root_node;
  cap_t root_dir;
  cap_t new_fs_server;

  if(get_process_caps("fs_op", &fs_server,
		      "fs_op_maker", &fs_op_maker,
		      "conn_maker", &conn_maker,
		      "return_cont", &return_cont,
		      0) < 0) return 1;

  /* Get root directory */
  {
    struct cap_args result;
    cap_call(fs_server, r,
	     cap_args_make(mk_int(r, METHOD_FSOP_GET_ROOT_DIR),
			   caps_empty, fds_empty),
	     &result);
    if(!pl_unpack(r, result, METHOD_R_CAP, "c", &orig_root_dir)) {
      fprintf(stderr, "run-emacs: get-root failed\n");
      return 1;
    }
  }
  filesys_obj_free(fs_server);

  /* Create filesystem namespace to give to Emacs */
  root_node = fs_make_empty_node();
  attach_rw_slot(root_node, make_read_only_slot(orig_root_dir));
  root_dir = fs_make_root(root_node);

  /* Create fs_op object */
  {
    struct cap_args result;
    cap_call(fs_op_maker, r,
	     cap_args_make(mk_int(r, METHOD_MAKE_FS_OP),
			   mk_caps1(r, root_dir), fds_empty),
	     &result);
    if(expect_cap1(result, &new_fs_server) < 0) {
      fprintf(stderr, "run-emacs: mkfs failed\n");
      return 1;
    }
  }

  /* Set cwd to '/' -- XEmacs doesn't work if the cwd is undefined */
  {
    struct cap_args result;
    cap_call(new_fs_server, r,
	     cap_args_make(cat2(r, mk_int(r, METHOD_FSOP_CHDIR),
				mk_string(r, "/")),
			   caps_empty, fds_empty),
	     &result);
    if(expect_ok(result) < 0) { fprintf(stderr, "run-emacs: warning: can't set cwd\n"); }
  }

  {
    int sock_fd1, sock_fd;
    cap_t *import;
    
    int count = 3;
    cap_t *a = region_alloc(r, count * sizeof(cap_t));
    a[0] = new_fs_server;
    a[1] = conn_maker;
    a[2] = fs_op_maker;
    sock_fd1 =
      conn_maker->vtable->make_conn(conn_maker, r, cap_seq_make(a, count),
				    0, &import);
    if(sock_fd1 < 0) {
      fprintf(stderr, "run-emacs: can't create connection\n");
      return 1;
    }

    /* This is a hack to deal with the fact that XEmacs closes file
       descriptors 0 through to 64 (inclusive) when it forks a child
       process.  The real solution is to have libc virtualise FDs, or
       at least move its FD out of the way. */
    sock_fd = fcntl(sock_fd1, F_DUPFD, 100);
    close(sock_fd1);
    if(sock_fd < 0) {
      perror("fcntl/dup");
      return 1;
    }
    
    filesys_obj_free(new_fs_server);
    filesys_obj_free(conn_maker);
    filesys_obj_free(fs_op_maker);
    {
      int pid = fork();
      if(pid == 0) {
	char buf[20];
	const char *prog = "/usr/bin/xemacs";
	int extra_args = 4;
	
	const char **args = alloca(sizeof(char *) * (argc + extra_args + 1));
	int i;
	args[0] = prog;
	args[1] = "-l";
	args[2] = "/usr/share/emacs/site-lisp/plash/plash-gnuserv.el";
	args[3] = "-f";
	args[4] = "plash-gnuserv-start";
	for(i = 1; i < argc; i++) args[extra_args + i] = argv[i];
	args[extra_args + argc] = 0;
	
	snprintf(buf, sizeof(buf), "%i", sock_fd);
	setenv("PLASH_COMM_FD", buf, 1);
	setenv("PLASH_CAPS", "fs_op;conn_maker;fs_op_maker", 1);
	assert(plash_libc_reset_connection);
	plash_libc_reset_connection();
	region_free(r);
	
	execve(prog, (char **) args, environ);
	fprintf(stderr, "exec on \"%s\" failed: %s\n",
		prog, strerror(errno));
	exit(1);
      }
    }
  }
  {
    cap_t edit_obj;
    struct edit_obj *obj =
      filesys_obj_make(sizeof(struct edit_obj), &edit_obj_vtable);
    obj->root_node = root_node;
    obj->emacs_fd = -1;
    edit_obj = (struct filesys_obj *) obj;

    /* Attach the executable object into the tree for Emacs to call. */
    {
      struct node *n = tree_traverse(tree_traverse(root_node, "bin"), "plash-gnuserv");
      attach_rw_slot(n, make_read_only_slot(edit_obj));
    }

    cap_invoke(return_cont,
	       cap_args_make(mk_int(r, METHOD_OKAY),
			     mk_caps1(r, edit_obj),
			     fds_empty));
    filesys_obj_free(return_cont);
  }

  cap_run_server();
  printf("run-emacs: edit obj dropped\n");
  return 0;
}
