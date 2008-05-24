/* Copyright (C) 2004, 2005, 2006 Mark Seaborn

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
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
/* #include <dirent.h> We have our own types */
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "region.h"
#include "serialise.h"
#include "comms.h"
#include "libc-comms.h"
#include "plash-libc.h"
#include "cap-protocol.h"
#include "cap-utils.h"
#include "marshal.h"
#include "marshal-pack.h"
#include "kernel-fd-ops.h"


pid_t new_fork(void);
int new_plash_libc_kernel_execve(const char *cmd_filename, char *argv[],
				 char *envp[]);
int new_execve(const char *cmd_filename, char *const argv[],
	       char *const envp[]);


static int clone_connection(void)
{
  int fd = -1;
  region_t r = region_make();

  plash_libc_lock();
  if(plash_init() < 0)
    goto exit;
  if(!fs_server || !conn_maker) {
    __set_errno(ENOSYS);
    goto exit;
  }

  struct cap_args result;
  cap_t new_fs_server;
  cap_call(fs_server, r,
	   cap_args_d(mk_int(r, METHOD_FSOP_COPY)),
	   &result);
  if(!pl_unpack(r, result, METHOD_R_CAP, "c", &new_fs_server)) {
    __set_errno(ENOSYS);
    goto exit;
  }

  cap_t *a = region_alloc(r, process_caps.size * sizeof(cap_t));
  seqf_t list = seqf_string(process_caps_names);
  seqf_t elt;
  int i = 0;
  while(parse_cap_list(list, &elt, &list)) {
    assert(i < process_caps.size);
    if(seqf_equal(elt, seqf_string("fs_op"))) {
      a[i] = new_fs_server;
    }
    else {
      a[i] = process_caps.caps[i];
    }
    i++;
  }
  assert(i == process_caps.size);

  cap_t *imports; /* Contents not used */
  fd = conn_maker->vtable->make_conn(conn_maker, r,
				     cap_seq_make(a, process_caps.size),
				     0 /* import_count */, &imports);
  filesys_obj_free(new_fs_server);
  if(fd < 0) {
    __set_errno(ENOSYS);
  }
 exit:
  plash_libc_unlock();
  region_free(r);
  return fd;
}


export_weak_alias(new_fork, fork);
export_weak_alias(new_fork, __fork);
export_weak_alias(new_fork, vfork);
export_weak_alias(new_fork, __vfork);
export(new_fork, __libc_fork);
export(new_fork, __GI___fork);
export(new_fork, __GI___vfork);

/* vfork() just calls normal fork(). */
/* If there is an error in duplicating the server's connection, we have
   the choice of carrying on with the fork and stopping any communication
   with the server in the child process, or giving an error now.  I have
   chosen the latter. */
pid_t new_fork(void)
{
  int fd = clone_connection();
  if(fd < 0)
    return -1;

  pid_t pid = kernel_fork();
  if(pid == 0) {
    /* Now running in subprocess.  Note that no other threads are
       running at this point. */
    int comm_sock_saved = comm_sock;
    /* This sets comm_sock to -1.  We save comm_sock and restore it. */
    plash_libc_reset_connection();
    comm_sock = comm_sock_saved;
    
    if(kernel_dup2(fd, comm_sock) < 0) {
      if(libc_debug) fprintf(stderr, "libc: fork(): dup2() failed\n");
      /* Fail quietly at this point. */
      unsetenv("PLASH_COMM_FD");
    }
  }
  kernel_close(fd);
  return pid;
}


static int exec_object(cap_t obj, int argc, const char **argv,
		       const char **envp)
{
  region_t r = region_make();
  argmkbuf_t argbuf = argbuf_make(r);
  struct cap_args result;
  bufref_t argv_arg, env_arg, root_arg, cwd_arg, fds_arg, args;
  int got_cwd;

  /* Pack main argv arguments. */
  {
    bufref_t *a;
    int i;
    argv_arg = argmk_array(argbuf, argc, &a);
    for(i = 0; i < argc; i++) {
      a[i] = argmk_str(argbuf, mk_string(r, argv[i]));
    }
  }

  /* Pack the environment arguments. */
  {
    bufref_t *a;
    int i, count;
    for(count = 0; envp[count]; count++) /* nothing */;
    env_arg = argmk_array(argbuf, count, &a);
    for(i = 0; i < count; i++) {
      a[i] = argmk_str(argbuf, mk_string(r, envp[i]));
    }
  }

  /* Get the root directory object. */
  {
    cap_t root_dir;
    cap_call(fs_server, r,
	     cap_args_d(mk_int(r, METHOD_FSOP_GET_ROOT_DIR)),
	     &result);
    if(!pl_unpack(r, result, METHOD_R_CAP, "c", &root_dir)) {
      filesys_obj_free(obj);
      region_free(r);
      __set_errno(ENOSYS);
      return -1;
    }
    root_arg = argmk_cap(argbuf, root_dir);
  }

  /* Get current working directory. */
  {
    char *cwd = getcwd(0, 0);
    if(cwd) {
      cwd_arg = argmk_str(argbuf,
			  mk_leaf(r, region_dup_seqf(r, seqf_string(cwd))));
      free(cwd);
      got_cwd = 1;
    }
    else got_cwd = 0;
  }

  /* Work out what file descriptors the process has.  The kernel
     doesn't offer a way to list them (except using /proc, but we don't
     have access to that), so we just try using dup() on each
     low-numbered FD in turn. */
  {
    /* fds[i] gives the FD number that FD i has been copied to.
       If FD i was not present, fds[i] = -1.
       If a FD was copied to FD x, fds[x] = -2. */
    int limit = 100;
    bufref_t *a;
    int i, j, got_fds = 0;
    int *fds = region_alloc(r, limit * sizeof(int));
    for(i = 0; i < limit; i++) fds[i] = -1;

    /* Also make sure that we don't pass on the socket that's connected
       to the server. */
    if(0 <= comm_sock && comm_sock < limit) fds[comm_sock] = -2;

    for(i = 0; i < limit; i++) {
      if(fds[i] == -1) {
	int fd = dup(i);
	if(fd < 0 && errno != EBADF) {
	  for(i = 0; i < limit; i++) if(fds[i] >= 0) close(fds[i]);
	  filesys_obj_free(obj);
	  argbuf_free_refs(argbuf);
	  region_free(r); /* must come after free_refs */
	  return -1;
	}
	if(fd >= 0) {
	  fds[i] = fd;
	  fds[fd] = -2;
	  got_fds++;
	}
      }
    }
    fds_arg = argmk_array(argbuf, got_fds, &a);
    for(i = 0, j = 0; i < limit; i++) {
      if(fds[i] >= 0) {
	assert(j < got_fds);
	a[j] = argmk_pair(argbuf, argmk_int(argbuf, i),
			  argmk_fd(argbuf, fds[i]));
	j++;
      }
    }
    assert(j == got_fds);
  }

  {
    int pgid = getpgid(0);
    int i;
    bufref_t *a;
    args = argmk_array(argbuf, 4 + (got_cwd ? 1:0) + (pgid > 0 ? 1:0), &a);
    a[0] = argmk_pair(argbuf,
		      argmk_str(argbuf, mk_string(r, "Argv")),
		      argv_arg);
    a[1] = argmk_pair(argbuf,
		      argmk_str(argbuf, mk_string(r, "Env.")),
		      env_arg);
    a[2] = argmk_pair(argbuf,
		      argmk_str(argbuf, mk_string(r, "Fds.")),
		      fds_arg);
    a[3] = argmk_pair(argbuf,
		      argmk_str(argbuf, mk_string(r, "Root")),
		      root_arg);
    i = 4;
    if(got_cwd) {
      a[i++] = argmk_pair(argbuf,
			  argmk_str(argbuf, mk_string(r, "Cwd.")),
			  cwd_arg);
    }
    if(pgid > 0) {
      a[i++] = argmk_pair(argbuf,
			  argmk_str(argbuf, mk_string(r, "Pgid")),
			  argmk_int(argbuf, pgid));
    }
  }
  cap_call(obj, r,
	   cap_args_make(cat3(r, mk_int(r, METHOD_EO_EXEC),
			      mk_int(r, args),
			      argbuf_data(argbuf)),
			 argbuf_caps(argbuf),
			 argbuf_fds(argbuf)),
	   &result);
  {
    seqf_t msg = flatten_reuse(r, result.data);
    int rc;
    int ok = 1;
    m_int_const(&ok, &msg, METHOD_R_EO_EXEC);
    m_int(&ok, &msg, &rc);
    m_end(&ok, &msg);
    if(ok && result.caps.size == 0 && result.fds.count == 0) {
      /* FIXME: should notify when process is running.
	 Then we can close all FDs here. */
      exit(rc);
    }
  }
  caps_free(result.caps);
  close_fds(result.fds);
  region_free(r);
  __set_errno(EIO);
  return -1;
}

static int unpack_string_array(region_t r, struct arg_m_buf argbuf, int argref,
			       int *argc_result, char ***argv_result)
{
  int argc;
  const bufref_t *array;
  if(argm_array(&argbuf, argref, &argc, &array))
    return -1;
  char **argv = region_alloc(r, (argc + 1) * sizeof(char *));
  int i;
  for(i = 0; i < argc; i++) {
    seqf_t arg;
    if(argm_str(&argbuf, array[i], &arg))
      return -1;
    argv[i] = region_strdup_seqf(r, arg);
  }
  argv[argc] = NULL;
  *argc_result = argc;
  *argv_result = argv;
  return 0;
}

static char *int_to_string(region_t r, int value)
{
  char buf[40];
  int rc = snprintf(buf, sizeof(buf), "%i", value);
  assert(rc > 0 && rc != sizeof(buf));
  return region_strdup(r, buf);
}

static int unpack_exec_result(region_t r, int argref,
			      seqf_t packed_data, fds_t fds,
			      int *argc, char ***argv)
{
  struct arg_m_buf argbuf = { packed_data, caps_empty, fds };
  bufref_t args, exec_fds;
  if(argm_pair(&argbuf, argref, &args, &exec_fds))
    return -1;
  if(unpack_string_array(r, argbuf, args, argc, argv))
    return -1;

  int fds_count;
  const bufref_t *fds_array;
  if(argm_array(&argbuf, exec_fds, &fds_count, &fds_array))
    return -1;
  /* Substitute file descriptor arguments into the argv, overwriting
     placeholder arguments. */
  int i;
  for(i = 0; i < fds_count; i++) {
    bufref_t argv_index_ref, fd_ref;
    int argv_index;
    int fd;
    if(argm_pair(&argbuf, fds_array[i], &argv_index_ref, &fd_ref) ||
       argm_int(&argbuf, argv_index_ref, &argv_index) ||
       argm_fd(&argbuf, fd_ref, &fd))
      return -1;
    if(!(0 <= argv_index && argv_index < *argc))
      return -1;
    (*argv)[argv_index] = int_to_string(r, fd);
  }
  return 0;
}


export(new_plash_libc_kernel_execve, plash_libc_kernel_execve);

int new_plash_libc_kernel_execve(const char *cmd_filename, char *argv[],
				 char *envp[])
{
  return kernel_execve(cmd_filename, argv, envp);
}


export_weak_alias(new_execve, execve);
export(new_execve, __execve);

/* This execs an executable using a particular dynamic linker.
   Won't work for setuid executables. */
/* We don't need to do anything special to hand off the connection to
   the server to the executable that's taking over the process.
   The socket should be in a consistent state: there should not be any
   bytes waiting on the socket to read, nor should we have buffered any
   unprocessed bytes from the socket, because the server is only supposed
   to send us replies. */
/* The const qualifiers glibc uses for execve are somewhat stupid. */
int new_execve(const char *cmd_filename, char *const argv[], char *const envp[])
{
  region_t r = region_make();
  struct cap_args result;
  if(libc_debug) fprintf(stderr, "libc: execve()\n");

  /* Pack the arguments. */
  argmkbuf_t argbuf = argbuf_make(r);
  bufref_t args;
  int argc, i;
  /* Count the arguments. */
  for(argc = 0; argv[argc]; argc++) /* nothing */;
  {
    bufref_t *a;
    args = argmk_array(argbuf, argc, &a);
    for(i = 0; i < argc; i++) {
      a[i] = argmk_str(argbuf, mk_string(r, argv[i]));
    }
  }

  plash_libc_lock();
  if(plash_init() < 0) { __set_errno(ENOSYS); goto error; }
  if(!fs_server) {
    if(libc_debug) fprintf(stderr, "libc: execve: fs_server not defined\n");
    __set_errno(ENOSYS);
    goto error;
  }

  /* Unset the close-on-exec flag. */
  if(fcntl(comm_sock, F_SETFD, 0) < 0) { goto error; }
      
  cap_call(fs_server, r,
	   pl_pack(r, METHOD_FSOP_EXEC, "siS", seqf_string(cmd_filename),
		   args, flatten_reuse(r, argbuf_data(argbuf))),
	   &result);
  seqf_t cmd_filename2;
  int argv2_ref;
  seqf_t argv2_packed;
  fds_t exec_fds;
  if(pl_unpack(r, result, METHOD_R_FSOP_EXEC, "siSF", &cmd_filename2,
	       &argv2_ref, &argv2_packed, &exec_fds)) {
    int argc2;
    char **argv2;
    if(unpack_exec_result(r, argv2_ref, argv2_packed, exec_fds,
			  &argc2, &argv2)) {
      __set_errno(EIO);
      goto error;
    }
    kernel_execve(region_strdup_seqf(r, cmd_filename2), argv2, envp);
    close_fds(exec_fds);
    goto error;
  }
  cap_t exec_obj;
  if(pl_unpack(r, result, METHOD_R_FSOP_EXEC_OBJECT, "c", &exec_obj)) {
    exec_object(exec_obj, argc, (const char **) argv, (const char **) envp);
    goto error;
  }
  set_errno_from_result(r, result);
 error:
  plash_libc_unlock();
  region_free(r);
  return -1;
}
