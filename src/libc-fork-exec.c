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

/* For getpgid() */
#define _GNU_SOURCE

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


/* EXPORT: new_fork => WEAK:fork WEAK:__fork WEAK:vfork WEAK:__vfork __libc_fork __GI___fork __GI___vfork */
/* vfork() just calls normal fork(). */
/* If there is an error in duplicating the server's connection, we have
   the choice of carrying on with the fork and stopping any communication
   with the server in the child process, or giving an error now.  I have
   chosen the latter. */
pid_t new_fork(void)
{
  region_t r;

  cap_t new_fs_server;
  int fd;
  struct cap_args result;
  
  if(plash_init() < 0) return -1;
  if(!fs_server) { __set_errno(ENOSYS); return -1; }

  r = region_make();
  cap_call(fs_server, r,
	   cap_args_make(mk_string(r, "Copy"), caps_empty, fds_empty),
	   &result);
  if(expect_cap1(result, &new_fs_server) < 0) {
    region_free(r);
    __set_errno(ENOSYS);
    return -1;
  }

  {
    cap_t *a = region_alloc(r, process_caps.size * sizeof(cap_t));
    seqf_t list = seqf_string(process_caps_names);
    seqf_t elt;
    int i = 0;
    while(parse_cap_list(list, &elt, &list)) {
      assert(i < process_caps.size);
      if(seqf_equal(elt, seqf_string("fs_op"))) {
	a[i] = inc_ref(new_fs_server);
      }
      else {
	a[i] = inc_ref(process_caps.caps[i]);
      }
      i++;
    }
    assert(i == process_caps.size);
    filesys_obj_free(new_fs_server);
    cap_call(conn_maker, r,
	     cap_args_make(cat2(r, mk_string(r, "Mkco"), mk_int(r, 0)),
			   cap_seq_make(a, process_caps.size),
			   fds_empty),
	     &result);
  }
  if(expect_fd1(result, &fd) < 0) {
    region_free(r);
    __set_errno(ENOSYS);
    return -1;
  }

  {
    pid_t pid;
    region_free(r);
    pid = fork();
    if(pid == 0) {
      int comm_sock_saved = comm_sock;
      /* This sets comm_sock to -1.  We save comm_sock and restore it. */
      plash_libc_reset_connection();
      comm_sock = comm_sock_saved;
      
      if(dup2(fd, comm_sock) < 0) {
	if(libc_debug) fprintf(stderr, "libc: fork(): dup2() failed\n");
	/* Fail quietly at this point. */
	unsetenv("PLASH_COMM_FD");
      }
      close(fd);
      /* This may not work in some cases, if a program empties
	 out the environment */
      /* However, it's not needed as we keep the indexes the same */
      /* setenv("PLASH_CAPS", "fs_op;conn_maker;fs_op_maker", 1); */
      return 0;
    }
    else if(pid < 0) {
      close(fd);
      return -1;
    }
    else {
      close(fd);
      return pid;
    }
  }

#if 0
  seqf_t reply;
  fds_t fds;
  if(req_and_reply_with_fds(r, mk_string(r, "Fork"), &reply, &fds) < 0) goto error;
  {
    seqf_t msg = reply;
    int fd;
    int ok = 1;
    m_str(&ok, &msg, "RFrk");
    m_end(&ok, &msg);
    m_fd(&ok, &fds, &fd);
    if(ok) {
      pid_t pid;
      region_free(r);
      pid = fork();
      if(pid == 0) {
#ifdef SIMPLE_SERVER
	comm_free(comm);
	comm = 0;
#else
	/* This is generally necessary.  Even though the socket will be
	   closed by being overwritten by dup below, that doesn't remove
	   the entry from the connection list, and we'd end up having
	   two entries with the same socket FD number!  Freeing fs_server
	   won't close the connection if there are return continuations
	   that the server failed to deallocate, in the case of server
	   bugs.  And of course it won't close other connections. */
	cap_close_all_connections();
	/* This needs to be done after clearing out the connection list.
	   Otherwise, dropping this reference could result in sending
	   a "drop" message which will mess up the parent process's
	   connection. */
	if(fs_server) filesys_obj_free(fs_server);
	if(conn_maker) filesys_obj_free(conn_maker);
	fs_server = 0;
	conn_maker = 0;
#endif
	/* Re-assign the forked FD to the FD slot used by the parent
	   process.  That saves us having to change the environment
	   variable PLASH_COMM_FD. */
	if(dup2(fd, comm_sock) < 0) {
	  /* Fail quietly at this point. */
	  comm_sock = -2;
	  unsetenv("PLASH_COMM_FD");
	}
	close(fd);
	return 0;
      }
      else if(pid < 0) {
	close(fd);
	return -1;
      }
      else {
	close(fd);
	return pid;
      }
    }
  }
  
  __set_errno(ENOSYS);
 error:
  region_free(r);
  return -1;
#endif
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
	     cap_args_make(mk_string(r, "Grtd"),
			   caps_empty, fds_empty),
	     &result);
    if(expect_cap1(result, &root_dir) < 0) {
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
	   cap_args_make(cat3(r, mk_string(r, "Exeo"),
			      mk_int(r, args),
			      argbuf_data(argbuf)),
			 argbuf_caps(argbuf),
			 argbuf_fds(argbuf)),
	   &result);
  {
    seqf_t msg = flatten_reuse(r, result.data);
    int rc;
    int ok = 1;
    m_str(&ok, &msg, "Okay");
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

/* EXPORT: plash_libc_kernel_execve */
int plash_libc_kernel_execve(const char *cmd_filename, char *argv[], char *envp[])
{
  return execve(cmd_filename, argv, envp);
}

/* EXPORT: new_execve => WEAK:execve __execve */
/* This execs an executable using a particular dynamic linker.
   Won't work for shell scripts (using "#!") or for setuid executables. */
/* We don't need to do anything special to hand off the connection to
   the server to the executable that's taking over the process.
   The socket should be in a consistent state: there should not be any
   bytes waiting on the socket to read, nor should we have buffered any
   unprocessed bytes from the socket, because the server is only supposed
   to send us replies. */
/* Based on sysdeps/unix/sysv/linux/execve.c */
/* The const qualifiers glibc uses for execve are somewhat stupid. */
extern void __pthread_kill_other_threads_np (void);
weak_extern (__pthread_kill_other_threads_np)
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

  if(plash_init() < 0) { __set_errno(ENOSYS); goto error; }
  if(!fs_server) {
    if(libc_debug) fprintf(stderr, "libc: execve: fs_server not defined\n");
    __set_errno(ENOSYS);
    goto error;
  }

  /* Unset the close-on-exec flag. */
  if(fcntl(comm_sock, F_SETFD, 0) < 0) { goto error; }
      
  cap_call(fs_server, r,
	   cap_args_d(cat5(r, mk_string(r, "Exec"),
			   mk_int(r, strlen(cmd_filename)),
			   mk_string(r, cmd_filename),
			   mk_int(r, args),
			   argbuf_data(argbuf))),
	   &result);
  {
    seqf_t msg = flatten_reuse(r, result.data);
    seqf_t cmd_filename2;
    int argc;
    int ok = 1;
    m_str(&ok, &msg, "RExe");
    m_lenblock(&ok, &msg, &cmd_filename2);
    m_int(&ok, &msg, &argc);
    if(ok && result.fds.count == 0 && result.caps.size == 0) {
      int i;
      char **argv2 = alloca((argc + 1) * sizeof(char *));
      for(i = 0; i < argc; i++) {
	seqf_t arg;
	m_lenblock(&ok, &msg, &arg);
	if(!ok) { __set_errno(EIO); goto error; }
	argv2[i] = region_strdup_seqf(r, arg);
      }
      argv2[argc] = 0;
      
      execve(region_strdup_seqf(r, cmd_filename2), argv2, envp);
      goto error;
    }
  }
  {
    seqf_t msg = flatten_reuse(r, result.data);
    int ok = 1;
    m_str(&ok, &msg, "RExo");
    m_end(&ok, &msg);
    if(ok && result.fds.count == 0 && result.caps.size == 1) {
      cap_t exec_obj = result.caps.caps[0];
      region_free(r);
      return exec_object(exec_obj, argc,
			 (const char **) argv,
			 (const char **) envp);
    }
  }
  {
    seqf_t msg = flatten_reuse(r, result.data);
    int err;
    int ok = 1;
    m_str(&ok, &msg, "Fail");
    m_int(&ok, &msg, &err);
    m_end(&ok, &msg);
    if(ok && result.caps.size == 0 && result.fds.count == 0) {
      __set_errno(err);
      goto error;
    }
  }

  if(libc_debug) fprintf(stderr, "libc: execve: bad response\n");
  __set_errno(ENOSYS);
 error:
  region_free(r);
  return -1;
}


#include "out-aliases_libc-fork-exec.h"
