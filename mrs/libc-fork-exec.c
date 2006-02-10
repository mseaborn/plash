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
#include "comms.h"
#include "libc-comms.h"
#include "cap-protocol.h"


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
  cap_seq_t caps;
  seqt_t reply;
  cap_seq_t reply_caps;
  fds_t reply_fds;

  int fd;
  
  if(plash_init() < 0) return -1;
  if(!fs_server || !conn_maker || !fs_op_maker) { __set_errno(ENOSYS); return -1; }

  r = region_make();
  fs_server->vtable->cap_call(fs_server, r,
			      mk_string(r, "Copy"), caps_empty, fds_empty,
			      &reply, &reply_caps, &reply_fds);
  {
    seqf_t msg = flatten_reuse(r, reply);
    int ok = 1;
    m_str(&ok, &msg, "Okay");
    m_end(&ok, &msg);
    if(ok && reply_caps.size == 1 && reply_fds.count == 0) {
      new_fs_server = reply_caps.caps[0];
    }
    else {
      caps_free(reply_caps);
      close_fds(reply_fds);
      region_free(r);
      __set_errno(ENOSYS);
      return -1;
    }
  }

  {
    int count = 3;
    cap_t *a = region_alloc(r, count * sizeof(cap_t));
    a[0] = new_fs_server;
    a[0]->refcount++;
    a[1] = conn_maker;
    a[1]->refcount++;
    a[2] = fs_op_maker;
    a[2]->refcount++;
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
    if(ok && reply_caps.size == 0 && reply_fds.count == 1) {
      fd = reply_fds.fds[0];
    }
    else {
      caps_free(reply_caps);
      close_fds(reply_fds);
      region_free(r);
      __set_errno(ENOSYS);
      return -1;
    }
  }

  {
    pid_t pid;
    region_free(r);
    pid = fork();
    if(pid == 0) {
      cap_close_all_connections();
      if(fs_server) filesys_obj_free(fs_server);
      if(conn_maker) filesys_obj_free(conn_maker);
      fs_server = 0;
      conn_maker = 0;
      if(dup2(fd, comm_sock) < 0) {
	/* Fail quietly at this point. */
	unsetenv("PLASH_COMM_FD");
      }
      close(fd);
      setenv("PLASH_CAPS", "fs_op;conn_maker;fs_op_maker", 1);
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
  seqf_t reply;
  fds_t fds;
  int exec_fd = -1;

  seqt_t got = seqt_empty;
  int i, argc;
  for(i = 0; argv[i]; i++) {
    got = cat3(r, got,
	       mk_int(r, strlen(argv[i])),
	       mk_string(r, argv[i]));
  }
  argc = i;

  /* Allocate an FD number which we will copy an FD into later. */
  /* NB. FD 0 might have been closed by the program, in which case this
     will fail. */
  exec_fd = dup(0);
  if(exec_fd < 0) goto error;
  
  if(req_and_reply_with_fds(r, cat6(r, mk_string(r, "Exec"),
				    mk_int(r, exec_fd),
				    mk_int(r, strlen(cmd_filename)),
				    mk_string(r, cmd_filename),
				    mk_int(r, argc),
				    got),
			    &reply, &fds) < 0) goto error;
  {
    seqf_t msg = reply;
    seqf_t cmd_filename2;
    int argc;
    int exec_fd2;
    int ok = 1;
    m_str(&ok, &msg, "RExe");
    m_lenblock(&ok, &msg, &cmd_filename2);
    m_int(&ok, &msg, &argc);
    m_fd(&ok, &fds, &exec_fd2);
    if(ok) {
      int i;
      char **argv2 = alloca((argc + 1) * sizeof(char *));
      for(i = 0; i < argc; i++) {
	seqf_t arg;
	m_lenblock(&ok, &msg, &arg);
	if(!ok) { __set_errno(EIO); goto error; }
	argv2[i] = region_strdup_seqf(r, arg);
      }
      argv2[argc] = 0;

      if(dup2(exec_fd2, exec_fd) < 0) goto error;
      close(exec_fd2);
      
      execve(region_strdup_seqf(r, cmd_filename2), argv2, envp);
      goto error;
    }
  }
  {
    seqf_t msg = reply;
    int err;
    int ok = 1;
    m_str(&ok, &msg, "Fail");
    m_int(&ok, &msg, &err);
    m_end(&ok, &msg);
    if(ok) {
      __set_errno(err);
      goto error;
    }
  }

  __set_errno(ENOSYS);
 error:
  region_free(r);
  if(exec_fd >= 0) close(exec_fd);
  return -1;

#if 0
  /* Call the kernel to check whether we can do "exec" using the kernel
     on the file. */
  /* This is necessary because if we do "exec" on ld-linux.so instead
     of the real executable file, it will always succeed, even if the
     executable file doesn't exist.  Some programs, such as gcc, rely
     on "exec" failing on files that don't exist, because they call
     exec repeatedly in order to search PATH. */
  if(access(file, X_OK) < 0) return -1;
  
  /* If this is a threaded application kill all other threads.  */
  if (__pthread_kill_other_threads_np)
    __pthread_kill_other_threads_np ();
#if __BOUNDED_POINTERS__
  #error "I removed the __BOUNDED_POINTERS__ case because it wasn't used."
#else
  {
    int extra_args = 3;
    char **argv2;
    int i;
    for(i = 0; argv[i]; i++) /* do nothing */;
    argv2 = alloca((i + extra_args + 1) * sizeof(char*));
    argv2[0] = argv[0];
    argv2[1] = "--library-path";
    argv2[2] = "/usr/local/mrs:/lib:/usr/lib:/usr/X11R6/lib";
    argv2[3] = (char *) file;
    for(i = 1; argv[i]; i++) argv2[i + extra_args] = argv[i];
    argv2[i + extra_args] = 0;
    return execve("/usr/local/mrs/ld-linux.so.2", argv2, envp);
  }
#endif
#endif
}


#include "out-aliases_libc-fork-exec.h"
