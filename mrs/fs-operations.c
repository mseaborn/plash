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

/* Necessary to get O_DIRECTORY and O_NOFOLLOW */
/* #define __USE_GNU */
#define _GNU_SOURCE

#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>

#include "region.h"
#include "server.h"
#include "parse-filename.h"
#include "comms.h"
#include "config.h"
#include "cap-protocol.h"
#include "fs-operations.h"


int process_chdir(struct process *p, seqf_t pathname, int *err)
{
  region_t r = region_make();
  struct dir_stack *d = resolve_dir(r, p->root, p->cwd, pathname, SYMLINK_LIMIT, err);
  region_free(r);
  if(d) {
    struct dir_stack *old_cwd = p->cwd;
    p->cwd = d;
    if(old_cwd) dir_stack_free(old_cwd);
    return 0;
  }
  else {
    return -1;
  }
}

int process_open(struct filesys_obj *root, struct dir_stack *cwd,
		 seqf_t pathname, int flags, int mode, int *err)
{
  region_t r = region_make();
  void *result;
  int rc =
    resolve_obj(r, root, cwd, pathname, SYMLINK_LIMIT,
		((flags & O_NOFOLLOW) || (flags & O_EXCL)) ? 1:0,
		(flags & O_CREAT) ? 1:0,
		&result, err);
  region_free(r);
  if(rc == RESOLVED_DIR) {
    /* Opening directories with open() is not implemented yet. */
    struct dir_stack *ds = result;
    dir_stack_free(ds);
    *err = EISDIR;
    return -1;
  }
  else if(rc == RESOLVED_FILE_OR_SYMLINK) {
    struct filesys_obj *obj = result;
    if(flags & O_DIRECTORY) {
      filesys_obj_free(obj);
      *err = ENOTDIR;
      return -1;
    }
    else if(flags & O_EXCL) {
      /* File exists already; can't create */
      filesys_obj_free(obj);
      *err = EEXIST;
      return -1;
    }
    else {
      if(obj->vtable->type == OBJT_FILE) {
	int fd = obj->vtable->open(obj, flags, err);
	filesys_obj_free(obj);
	return fd;
      }
      else if(obj->vtable->type == OBJT_SYMLINK) {
	filesys_obj_free(obj);
	*err = ELOOP;
	return -1;
      }
    }
  }
  else if(rc == RESOLVED_EMPTY_SLOT) {
    struct resolved_slot *slot = result;
    if(flags & O_DIRECTORY) {
      free_resolved_slot(slot);
      *err = ENOENT;
      return -1;
    }
    else {
      int fd = slot->dir->vtable->create_file(slot->dir, slot->leaf, flags, mode, err);
      free_resolved_slot(slot);
      return fd;
    }
  }
  else if(rc <= 0) { return -1; }
  *err = EIO;
  return -1;
}

/* mkdir() behaves like open() with O_EXCL: it won't follow a symbolic
   link and create the destination. */
int process_mkdir(struct process *p, seqf_t pathname, int mode, int *err)
{
  int r;
  struct resolved_slot *slot =
    resolve_empty_slot(p->root, p->cwd, pathname, SYMLINK_LIMIT, err);
  if(!slot) return -1;
  r = slot->dir->vtable->mkdir(slot->dir, slot->leaf, mode, err);
  free_resolved_slot(slot);
  return r;
}

int process_symlink(struct process *p, seqf_t newpath, seqf_t oldpath, int *err)
{
  char *oldpath1;
  int r;
  struct resolved_slot *slot =
    resolve_empty_slot(p->root, p->cwd, newpath, SYMLINK_LIMIT, err);
  if(!slot) return -1;
  oldpath1 = strdup_seqf(oldpath);
  r = slot->dir->vtable->symlink(slot->dir, slot->leaf, oldpath1, err);
  free(oldpath1);
  free_resolved_slot(slot);
  return r;
}

int process_rename(struct process *p, seqf_t oldpath, seqf_t newpath, int *err)
{
  struct resolved_slot *slot_src, *slot_dest;
  int rc;
  slot_src = resolve_any_slot(p->root, p->cwd, oldpath, SYMLINK_LIMIT, err);
  if(!slot_src) return -1;
  slot_dest = resolve_any_slot(p->root, p->cwd, newpath, SYMLINK_LIMIT, err);
  if(!slot_dest) {
    free_resolved_slot(slot_src);
    return -1;
  }
  rc = slot_src->dir->vtable->rename(slot_src->dir, slot_src->leaf,
				     slot_dest->dir, slot_dest->leaf,
				     err);
  free_resolved_slot(slot_src);
  free_resolved_slot(slot_dest);
  return rc;
}

int process_link(struct process *p, seqf_t oldpath, seqf_t newpath, int *err)
{
  struct resolved_slot *slot_src, *slot_dest;
  int rc;
  slot_src = resolve_any_slot(p->root, p->cwd, oldpath, SYMLINK_LIMIT, err);
  if(!slot_src) return -1;
  slot_dest = resolve_any_slot(p->root, p->cwd, newpath, SYMLINK_LIMIT, err);
  if(!slot_dest) {
    free_resolved_slot(slot_src);
    return -1;
  }
  rc = slot_src->dir->vtable->link(slot_src->dir, slot_src->leaf,
				   slot_dest->dir, slot_dest->leaf,
				   err);
  free_resolved_slot(slot_src);
  free_resolved_slot(slot_dest);
  return rc;
}

int process_unlink(struct process *p, seqf_t pathname, int *err)
{
  int rc;
  struct resolved_slot *slot =
    resolve_any_slot(p->root, p->cwd, pathname, SYMLINK_LIMIT, err);
  if(!slot) return -1;
  rc = slot->dir->vtable->unlink(slot->dir, slot->leaf, err);
  free_resolved_slot(slot);
  return rc;
}

int process_rmdir(struct process *p, seqf_t pathname, int *err)
{
  int rc;
  struct resolved_slot *slot =
    resolve_any_slot(p->root, p->cwd, pathname, SYMLINK_LIMIT, err);
  if(!slot) return -1;
  rc = slot->dir->vtable->rmdir(slot->dir, slot->leaf, err);
  free_resolved_slot(slot);
  return rc;
}

int process_chmod(struct process *p, seqf_t pathname, int mode, int *err)
{
  int rc;
  /* chmod does follow symlinks.  Symlinks do not have permissions of
     their own. */
  struct filesys_obj *obj =
    resolve_obj_simple(p->root, p->cwd, pathname, SYMLINK_LIMIT,
		       0 /* nofollow */, err);
  if(!obj) return -1;
  rc = obj->vtable->chmod(obj, mode, err);
  filesys_obj_free(obj);
  return rc;
}

int process_utimes(struct process *p, seqf_t pathname, int nofollow,
		   const struct timeval *atime, const struct timeval *mtime,
		   int *err)
{
  int rc;
  struct filesys_obj *obj =
    resolve_obj_simple(p->root, p->cwd, pathname, SYMLINK_LIMIT,
		       nofollow, err);
  if(!obj) return -1;
  rc = obj->vtable->utimes(obj, atime, mtime, err);
  filesys_obj_free(obj);
  return rc;
}


/* Checks for executables that are scripts using the `#!' syntax. */
/* This is not done recursively.  If there's a script that says it should
   be executed using another script, that won't work.  This is the
   behaviour of Linux.  There didn't seem much point in generalising the
   mechanism. */
/* Takes an FD for an executable.  The initial executable is looked up
   in the user's namespace.  But pathnames specified in the `#!' line
   are looked up in the process's namespace. */
/* Takes ownership of the FD it's given. */
/* Returns -1 if there's an error. */
int exec_for_scripts
  (region_t r,
   struct filesys_obj *root, struct dir_stack *cwd,
   const char *cmd, int exec_fd, int argc, const char **argv,
   int *exec_fd_out, int *argc_out, const char ***argv_out,
   int *err)
{
  char buf[1024];
  int got = 0;
  
  while(got < sizeof(buf)) {
    int x = read(exec_fd, buf + got, sizeof(buf) - got);
    if(x < 0) { *err = errno; return -1; }
    if(x == 0) break;
    got += x;
  }

  /* No whitespace is allowed before the "#!" */
  if(got >= 2 && buf[0] == '#' && buf[1] == '!') {
    int icmd_start, icmd_end;
    int arg_start, arg_end;
    int i = 2;
    seqf_t icmd;
    region_t r2;
    struct filesys_obj *obj;
    int fd;

    close(exec_fd);
    
    while(i < got && buf[i] == ' ') i++; /* Skip spaces */
    if(i >= got) { *err = EINVAL; return -1; }
    icmd_start = i;
    while(i < got && buf[i] != ' ' && buf[i] != '\n') i++; /* Skip to space */
    if(i >= got) { *err = EINVAL; return -1; }
    icmd_end = i;
    while(i < got && buf[i] == ' ') i++; /* Skip spaces */
    if(i >= got) { *err = EINVAL; return -1; }
    arg_start = i;
    while(i < got && buf[i] != '\n') i++; /* Skip to end of line */
    if(i >= got) { *err = EINVAL; return -1; }
    arg_end = i;

    icmd.data = buf + icmd_start;
    icmd.size = icmd_end - icmd_start;
    r2 = region_make();
    obj = resolve_file(r2, root, cwd, icmd, SYMLINK_LIMIT,
		       0 /* nofollow */, err);
    region_free(r2);
    if(!obj) return -1; /* Error */
    fd = obj->vtable->open(obj, O_RDONLY, err);
    filesys_obj_free(obj);
    if(fd < 0) return -1; /* Error */

    if(arg_end > arg_start) {
      seqf_t arg = { buf + arg_start, arg_end - arg_start };
      int i;
      const char **argv2 = region_alloc(r, (argc + 2) * sizeof(char *));
      argv2[0] = region_strdup_seqf(r, icmd);
      argv2[1] = region_strdup_seqf(r, arg);
      argv2[2] = cmd;
      for(i = 1; i < argc; i++) argv2[i+2] = argv[i];
      *exec_fd_out = fd;
      *argc_out = argc + 2;
      *argv_out = argv2;
      return 0;
    }
    else {
      int i;
      const char **argv2 = region_alloc(r, (argc + 2) * sizeof(char *));
      argv2[0] = region_strdup_seqf(r, icmd);
      argv2[1] = cmd;
      for(i = 1; i < argc; i++) argv2[i+1] = argv[i];
      *exec_fd_out = fd;
      *argc_out = argc + 1;
      *argv_out = argv2;
      return 0;
    }
  }
  else {
    /* Assume an ELF executable. */
    *exec_fd_out = exec_fd;
    *argc_out = argc;
    *argv_out = argv;
    return 0;
  }
}

void handle_fs_op_message1(region_t r, struct process *proc,
			   struct fs_op_object *obj,
			   seqf_t msg_orig, fds_t fds_orig,
			   seqt_t *reply, fds_t *reply_fds,
			   seqt_t *log_msg, seqt_t *log_reply)
{
  {
    seqf_t msg = msg_orig;
    int ok = 1;
    m_str(&ok, &msg, "Fork");
    m_end(&ok, &msg);
    if(ok) {
      int socks[2];
      cap_t new_server;
      
      *log_msg = mk_string(r, "fork");

      if(socketpair(AF_LOCAL, SOCK_STREAM, 0, socks) < 0) {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, errno));
	*log_reply = mk_string(r, "fail");
	return;
      }
      set_close_on_exec_flag(socks[0], 1);
      set_close_on_exec_flag(socks[1], 1);
      obj->shared->refcount++;
      proc->root->refcount++;
      if(proc->cwd) proc->cwd->refcount++;
      new_server = make_fs_op_server(obj->shared, proc->root, proc->cwd);
      cap_make_connection(r, socks[1], mk_caps1(r, new_server), 0, "to-client");
      
      *reply = mk_string(r, "RFrk");
      *reply_fds = mk_fds1(r, socks[0]);
      *log_reply = mk_printf(r, "ok, created #%i",
			     ((struct fs_op_object *) new_server)->id);
      return;
    }
  }
  handle_fs_op_message(r, proc, msg_orig, fds_orig, reply, reply_fds,
		       log_msg, log_reply);
}

void handle_fs_op_message(region_t r, struct process *proc,
			  seqf_t msg_orig, fds_t fds_orig,
			  seqt_t *reply, fds_t *reply_fds,
			  seqt_t *log_msg, seqt_t *log_reply)
{
  {
    seqf_t msg = msg_orig;
    int ok = 1;
    int fd_number;
    seqf_t cmd_filename;
    int argc;
    m_str(&ok, &msg, "Exec");
    m_int(&ok, &msg, &fd_number);
    m_lenblock(&ok, &msg, &cmd_filename);
    m_int(&ok, &msg, &argc);
    if(ok && argc >= 1) {
      int extra_args = 5;
      int buf_size = 40;
      char *buf = region_alloc(r, buf_size);
      const char **argv, **argv2;
      seqf_t cmd_filename2;
      int i;
      int err;
      struct filesys_obj *obj;
      int fd;

      *log_msg = cat2(r, mk_string(r, "exec: "), mk_leaf(r, cmd_filename));
      
      argv = region_alloc(r, argc * sizeof(seqf_t));
      for(i = 0; i < argc; i++) {
	seqf_t arg;
	m_lenblock(&ok, &msg, &arg);
	argv[i] = region_strdup_seqf(r, arg);
	if(!ok) goto exec_error;
      }
      
      obj = resolve_file(r, proc->root, proc->cwd, cmd_filename,
			 SYMLINK_LIMIT, 0 /* nofollow */, &err);
      if(!obj) goto exec_fail;
      fd = obj->vtable->open(obj, O_RDONLY, &err);
      filesys_obj_free(obj);
      if(fd < 0) goto exec_fail;

      if(exec_for_scripts(r, proc->root, proc->cwd,
			  region_strdup_seqf(r, cmd_filename), fd, argc, argv,
			  &fd, &argc, &argv, &err) < 0) goto exec_fail;
      
      argv2 = region_alloc(r, (argc + extra_args) * sizeof(seqf_t));
      argv2[0] = argv[0];
      cmd_filename2 = seqf_string("/special/ld-linux.so.2");
      argv2[1] = "--library-path";
      argv2[2] = PLASH_LD_LIBRARY_PATH;
      argv2[3] = "--fd";
      snprintf(buf, buf_size, "%i", fd_number);
      argv2[4] = buf;
      for(i = 0; i < argc; i++) argv2[i + extra_args] = argv[i];

      {
	seqt_t got = seqt_empty;
	int i;
	for(i = 0; i < argc + extra_args; i++) {
	  got = cat3(r, got,
		     mk_int(r, strlen(argv2[i])),
		     mk_string(r, argv2[i]));
	}
	*reply = cat5(r, mk_string(r, "RExe"),
		      mk_int(r, cmd_filename2.size),
		      mk_leaf(r, cmd_filename2),
		      mk_int(r, argc + extra_args),
		      got);
	*reply_fds = mk_fds1(r, fd);
	*log_reply = mk_string(r, "ok");
	return;
      }
    exec_fail:
      *reply = cat2(r, mk_string(r, "Fail"),
		    mk_int(r, err));
      *log_reply = mk_string(r, "fail");
      return;
    }
  exec_error:
  }
  {
    seqf_t msg = msg_orig;
    int flags, mode;
    int ok = 1;
    m_str(&ok, &msg, "Open");
    m_int(&ok, &msg, &flags);
    m_int(&ok, &msg, &mode);
    if(ok) {
      seqf_t pathname = msg;
      int fd, err = 0;

      *log_msg =
	cat2(r, mk_printf(r, "open: flags=0o%o, mode=0o%o, ", flags, mode),
	     mk_leaf(r, pathname));
            
      fd = process_open(proc->root, proc->cwd, pathname, flags, mode, &err);
      if(fd >= 0) {
	*reply = mk_string(r, "ROpn");
	*reply_fds = mk_fds1(r, fd);
	*log_reply = mk_string(r, "ok");
      }
      else {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
	*log_reply = mk_string(r, "fail");
      }
      return;
    }
  }
  {
    seqf_t msg = msg_orig;
    int nofollow;
    int ok = 1;
    m_str(&ok, &msg, "Stat");
    m_int(&ok, &msg, &nofollow);
    if(ok) {
      seqf_t pathname = msg;
      struct filesys_obj *obj;
      int err;
      struct stat stat;

      *log_msg = cat3(r, mk_string(r, nofollow ? "lstat" : "stat"),
		      mk_string(r, ": "),
		      mk_leaf(r, pathname));

      obj = resolve_obj_simple(proc->root, proc->cwd, pathname,
			       SYMLINK_LIMIT, nofollow, &err);
      if(!obj) {
	*log_reply = mk_string(r, "fail");
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
	return;
      }
      if(obj->vtable->stat(obj, &stat, &err) < 0) {
	filesys_obj_free(obj);
	*log_reply = mk_string(r, "fail");
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
	return;
      }
      filesys_obj_free(obj);
      *reply = cat4(r, mk_string(r, "RSta"),
		    cat5(r, mk_int(r, stat.st_dev),
			 mk_int(r, stat.st_ino),
			 mk_int(r, stat.st_mode),
			 mk_int(r, stat.st_nlink),
			 mk_int(r, stat.st_uid)),
		    cat5(r, mk_int(r, stat.st_gid),
			 mk_int(r, stat.st_rdev),
			 mk_int(r, stat.st_size),
			 mk_int(r, stat.st_blksize),
			 mk_int(r, stat.st_blocks)),
		    cat3(r, mk_int(r, stat.st_atime),
			 mk_int(r, stat.st_mtime),
			 mk_int(r, stat.st_ctime)));
      *log_reply = mk_string(r, "ok");
      return;
    }
  }
  {
    seqf_t msg = msg_orig;
    int ok = 1;
    m_str(&ok, &msg, "Rdlk");
    if(ok) {
      seqf_t pathname = msg;
      void *result;
      int rc, err = 0;
      
      region_t r2 = region_make();
      *log_msg = cat2(r, mk_string(r, "readlink: "), mk_leaf(r, pathname));
      rc = resolve_obj(r2, proc->root, proc->cwd, pathname, SYMLINK_LIMIT,
		       1 /*nofollow*/, 0 /* create */, &result, &err);
      region_free(r2);
      if(rc == RESOLVED_FILE_OR_SYMLINK) {
	struct filesys_obj *obj = result;
	if(obj->vtable->type == OBJT_SYMLINK) {
	  seqf_t link_dest;
	  if(obj->vtable->readlink(obj, r, &link_dest, &err) >= 0) {
	    *log_reply = mk_string(r, "ok");
	    *reply = cat2(r, mk_string(r, "RRdl"),
			  mk_leaf(r, link_dest));
	  }
	  else {
	    *reply = cat2(r, mk_string(r, "Fail"),
			  mk_int(r, err));
	  }
	}
	else {
	  *reply = cat2(r, mk_string(r, "Fail"),
			mk_int(r, EINVAL)); /* not a symlink */
	}
	filesys_obj_free(obj);
      }
      else if(rc == RESOLVED_DIR) {
	dir_stack_free(result);
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, EINVAL)); /* not a symlink */
      }
      else {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
      }
      return;
    }
  }
  {
    seqf_t msg = msg_orig;
    int ok = 1;
    m_str(&ok, &msg, "Gcwd");
    m_end(&ok, &msg);
    if(ok) {
      *log_msg = mk_string(r, "getcwd");
      if(proc->cwd) {
	*log_reply = mk_string(r, "ok");
	*reply = cat2(r, mk_string(r, "RCwd"),
		      string_of_cwd(r, proc->cwd));
      }
      else {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, E_NO_CWD_DEFINED));
      }
      return;
    }
  }
  {
    seqf_t msg = msg_orig;
    int ok = 1;
    m_str(&ok, &msg, "Dlst");
    if(ok) {
      seqf_t pathname = msg;
      int err = 0;
      struct dir_stack *ds;
      *log_msg = mk_string(r, "dirlist");
      ds = resolve_dir(r, proc->root, proc->cwd, pathname, SYMLINK_LIMIT, &err);
      if(ds) {
	seqt_t result;
	if(ds->dir->vtable->list(ds->dir, r, &result, &err) >= 0) {
	  /* Could add "." and ".." here but is it really useful?
	     struct dir_stack *parent = ds->parent ? ds->parent : ds;
	    result = cat5(r, result,
			  mk_int(r, ent->d_ino),
			  mk_int(r, ent->d_type),
			  mk_int(r, len),
			  mk_leaf2(r, str, len)); */
	  *reply = cat2(r, mk_string(r, "RDls"), result);
	  dir_stack_free(ds);
	}
	else {
	  *reply = cat2(r, mk_string(r, "Fail"),
			mk_int(r, err));
	}
      }
      else {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
      }
      return;
    }
  }
  {
    /* access() call.  This isn't very useful for secure programming in
       Unix, because of the race condition (TOCTTOU problem).
       It's basically used in a similar way to stat, checking whether a
       file exists, but checking whether it is accessible rather than
       returning information. */
    /* access() is based on effective vs. real uids/gids, a concept we
       don't have in the file server.  We don't care about uids/gids at
       all! */
    /* This implementation is incomplete.  It doesn't check permissions.
       It returns successfully simply if the object exists. */
    seqf_t msg = msg_orig;
    int mode;
    int ok = 1;
    m_str(&ok, &msg, "Accs");
    m_int(&ok, &msg, &mode);
    if(ok) {
      seqf_t pathname = msg;
      struct filesys_obj *obj;
      int err;

      *log_msg = cat2(r, mk_string(r, "access: "), mk_leaf(r, pathname));
      obj = resolve_obj_simple(proc->root, proc->cwd, pathname, SYMLINK_LIMIT,
			       0 /*nofollow*/, &err);
      if(obj) {
	filesys_obj_free(obj);
	*reply = mk_string(r, "RAcc");
	*log_reply = mk_string(r, "ok");
      }
      else {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
	*log_reply = mk_string(r, "fail");
      }
      return;
    }
  }
  {
    /* mkdir() call */
    seqf_t msg = msg_orig;
    int mode;
    int ok = 1;
    m_str(&ok, &msg, "Mkdr");
    m_int(&ok, &msg, &mode);
    if(ok) {
      seqf_t pathname = msg;
      int err = 0;
      *log_msg = mk_string(r, "mkdir");
      if(process_mkdir(proc, pathname, mode, &err) < 0) {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
      }
      else {
	*reply = mk_string(r, "RMkd");
      }
      return;
    }
  }
  {
    /* symlink() call */
    seqf_t msg = msg_orig;
    seqf_t newpath;
    int ok = 1;
    m_str(&ok, &msg, "Syml");
    m_lenblock(&ok, &msg, &newpath);
    if(ok) {
      seqf_t oldpath = msg;
      int err;
      *log_msg = cat4(r, mk_string(r, "symlink: "), mk_leaf(r, newpath),
		      mk_string(r, " to link to "), mk_leaf(r, oldpath));
      if(process_symlink(proc, newpath, oldpath, &err) < 0) {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
      }
      else {
	*reply = mk_string(r, "RSym");
      }
      return;
    }
  }
  {
    /* rename() call */
    seqf_t msg = msg_orig;
    seqf_t newpath;
    int ok = 1;
    m_str(&ok, &msg, "Renm");
    m_lenblock(&ok, &msg, &newpath);
    if(ok) {
      seqf_t oldpath = msg;
      int err;
      *log_msg = cat4(r, mk_string(r, "rename: "), mk_leaf(r, oldpath),
		      mk_string(r, " to "), mk_leaf(r, newpath));
      if(process_rename(proc, oldpath, newpath, &err) < 0) {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
	*log_reply = mk_string(r, "fail");
	return;
      }
      else {
	*reply = mk_string(r, "RRnm");
	*log_reply = mk_string(r, "ok");
	return;
      }
    }
  }
  {
    /* link() call */
    seqf_t msg = msg_orig;
    seqf_t newpath;
    int ok = 1;
    m_str(&ok, &msg, "Link");
    m_lenblock(&ok, &msg, &newpath);
    if(ok) {
      seqf_t oldpath = msg;
      int err;
      *log_msg = cat4(r, mk_string(r, "hard link: create "), mk_leaf(r, newpath),
		      mk_string(r, " to link to "), mk_leaf(r, oldpath));
      if(process_link(proc, oldpath, newpath, &err) < 0) {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
	*log_reply = mk_string(r, "fail");
	return;
      }
      else {
	*reply = mk_string(r, "RLnk");
	*log_reply = mk_string(r, "ok");
	return;
      }
    }
  }
  {
    /* chmod() call.  An old version of this re-used the code for "open"
       and then fchmod'ed the FD.  The problem with that is that it
       would allow the client process to chmod any file, even those it's
       not supposed to have write access to!  I have now added a chmod
       method to file and directory objects. */
    seqf_t msg = msg_orig;
    int mode;
    int ok = 1;
    m_str(&ok, &msg, "Chmd");
    m_int(&ok, &msg, &mode);
    if(ok) {
      seqf_t pathname = msg;
      int err;
      *log_msg = mk_string(r, "chmod");
      if(process_chmod(proc, pathname, mode, &err) < 0) {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
      }
      else {
	*reply = mk_string(r, "RChm");
      }
      return;
    }
  }
  {
    /* utime()/utimes()/lutimes() calls */
    seqf_t msg = msg_orig;
    int nofollow;
    int atime_sec, atime_usec;
    int mtime_sec, mtime_usec;
    int ok = 1;
    m_str(&ok, &msg, "Utim");
    m_int(&ok, &msg, &nofollow);
    m_int(&ok, &msg, &atime_sec);
    m_int(&ok, &msg, &atime_usec);
    m_int(&ok, &msg, &mtime_sec);
    m_int(&ok, &msg, &mtime_usec);
    if(ok) {
      seqf_t pathname = msg;
      int err;
      struct timeval atime, mtime;
      atime.tv_sec = atime_sec;
      atime.tv_usec = atime_usec;
      mtime.tv_sec = mtime_sec;
      mtime.tv_usec = mtime_usec;
      *log_msg = mk_string(r, "utime");
      if(process_utimes(proc, pathname, nofollow, &atime, &mtime, &err) < 0) {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
      }
      else {
	*reply = mk_string(r, "RUtm");
      }
      return;
    }
  }
  {
    /* unlink() call */
    seqf_t msg = msg_orig;
    int ok = 1;
    m_str(&ok, &msg, "Unlk");
    if(ok) {
      seqf_t pathname = msg;
      int err;
      *log_msg = cat2(r, mk_string(r, "unlink: "), mk_leaf(r, pathname));
      if(process_unlink(proc, pathname, &err) < 0) {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
	*log_reply = mk_string(r, "fail");
      }
      else {
	*reply = mk_string(r, "RUnl");
	*log_reply = mk_string(r, "ok");
      }
      return;
    }
  }
  {
    /* rmdir() call */
    seqf_t msg = msg_orig;
    int ok = 1;
    m_str(&ok, &msg, "Rmdr");
    if(ok) {
      seqf_t pathname = msg;
      int err;
      *log_msg = cat2(r, mk_string(r, "rmdir: "), mk_leaf(r, pathname));
      if(process_rmdir(proc, pathname, &err) < 0) {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
	*log_reply = mk_string(r, "fail");
      }
      else {
	*reply = mk_string(r, "RRmd");
	*log_reply = mk_string(r, "ok");
      }
      return;
    }
  }
  {
    /* connect() for Unix domain sockets */
    seqf_t msg = msg_orig;
    fds_t fds = fds_orig;
    int ok = 1;
    int sock_fd;
    m_str(&ok, &msg, "Fcon");
    m_fd(&ok, &fds, &sock_fd);
    if(ok) {
      seqf_t pathname = msg;
      int err;
      struct filesys_obj *obj;
      *log_msg = cat2(r, mk_string(r, "connect: "), mk_leaf(r, pathname));
      obj = resolve_file(r, proc->root, proc->cwd, pathname,
			 SYMLINK_LIMIT, 0 /*nofollow*/, &err);
      if(obj) {
	if(obj->vtable->socket_connect(obj, sock_fd, &err) >= 0) {
	  filesys_obj_free(obj);
	  *log_reply = mk_string(r, "ok");
	  *reply = mk_string(r, "RFco");
	  return;
	}
	filesys_obj_free(obj);
      }
      *log_reply = mk_string(r, "fail");
      *reply = cat2(r, mk_string(r, "Fail"),
		    mk_int(r, err));
      return;
    }
  }
  {
    /* bind() for Unix domain sockets */
    seqf_t msg = msg_orig;
    fds_t fds = fds_orig;
    int ok = 1;
    int sock_fd;
    m_str(&ok, &msg, "Fbnd");
    m_fd(&ok, &fds, &sock_fd);
    if(ok) {
      seqf_t pathname = msg;
      int err;
      struct resolved_slot *slot;
      *log_msg = cat2(r, mk_string(r, "bind: "), mk_leaf(r, pathname));
      slot = resolve_empty_slot(proc->root, proc->cwd, pathname,
				SYMLINK_LIMIT, &err);
      if(slot) {
	if(slot->dir->vtable->socket_bind(slot->dir, slot->leaf, sock_fd,
					  &err) >= 0) {
	  free_resolved_slot(slot);
	  *log_reply = mk_string(r, "ok");
	  *reply = mk_string(r, "RFbd");
	  return;
	}
	free_resolved_slot(slot);
      }
      *log_reply = mk_string(r, "fail");
      *reply = cat2(r, mk_string(r, "Fail"),
		    mk_int(r, err));
      return;
    }
  }
  {
    seqf_t msg = msg_orig;
    int ok = 1;
    m_str(&ok, &msg, "Chdr");
    if(ok) {
      seqf_t pathname = msg;
      int err = 0;
      int e;
      *log_msg = mk_string(r, "chdir");
      e = process_chdir(proc, pathname, &err);
      if(e == 0) {
	*log_reply = mk_string(r, "ok");
	*reply = mk_string(r, "RSuc");
      }
      else {
	*log_reply = mk_string(r, "fail");
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
      }
      return;
    }
  }

  // if(DO_LOG_SUMMARY(state)) fprintf(state->log, "Unknown message!\n");
  /* Send a Fail reply. */
  *reply = cat2(r, mk_string(r, "Fail"),
		mk_int(r, ENOSYS));
}


extern struct filesys_obj_vtable fs_op_vtable;

/* Takes owning references. */
cap_t make_fs_op_server(struct server_shared *shared,
			struct filesys_obj *root, struct dir_stack *cwd)
{
  struct fs_op_object *obj = amalloc(sizeof(struct fs_op_object));
  obj->hdr.refcount = 1;
  obj->hdr.vtable = &fs_op_vtable;
  obj->p.root = root;
  obj->p.cwd = cwd;
  obj->shared = shared;
  obj->id = shared->next_id++;
  return (cap_t) obj;
}

void server_shared_free(struct server_shared *s)
{
  assert(s->refcount > 0);
  s->refcount--;
  if(s->refcount == 0) {
    if(s->log) fclose(s->log);
    free(s);
  }
}

void fs_op_free(struct filesys_obj *obj1)
{
  struct fs_op_object *obj = (void *) obj1;
  filesys_obj_free(obj->p.root);
  if(obj->p.cwd) dir_stack_free(obj->p.cwd);

  server_shared_free(obj->shared);
}

void fs_op_call(struct filesys_obj *obj1, region_t r,
		seqt_t data, cap_seq_t caps, fds_t fds,
		seqt_t *r_data, cap_seq_t *r_caps, fds_t *r_fds)
{
  struct fs_op_object *obj = (void *) obj1;
  seqt_t log_msg = mk_string(r, "?");
  seqt_t log_reply = mk_string(r, "?");
  
  if(obj->shared->log && obj->shared->log_messages) {
    fprintf(obj->shared->log, "\nmessage from process %i\n", obj->id);
    fprint_data(obj->shared->log, flatten_reuse(r, data));
  }
  *r_data = seqt_empty;
  *r_fds = fds_empty;
  caps_free(caps);
  handle_fs_op_message1(r, &obj->p, obj, flatten_reuse(r, data), fds,
			r_data, r_fds, &log_msg, &log_reply);
  close_fds(fds);
  *r_caps = caps_empty;
  
  if(obj->shared->log && obj->shared->log_messages) {
    fprintf(obj->shared->log, "reply with %i FDs and this data:\n", r_fds->count);
    fprint_data(obj->shared->log, flatten(r, *r_data));
  }
  if(obj->shared->log && obj->shared->log_summary) {
    fprintf(obj->shared->log, "#%i: ", obj->id);
    fprint_d(obj->shared->log, flatten(r, log_msg));
    fprintf(obj->shared->log, ": ");
    fprint_d(obj->shared->log, flatten(r, log_reply));
    fprintf(obj->shared->log, "\n");
  }
}

struct filesys_obj_vtable fs_op_vtable = {
  /* .type = */ 0,
  /* .free = */ fs_op_free,

  /* .cap_invoke = */ local_obj_invoke,
  /* .cap_call = */ fs_op_call,
  /* .single_use = */ 0,
  
  /* .stat = */ dummy_stat,
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
