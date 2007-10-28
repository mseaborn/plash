/* Copyright (C) 2004, 2005 Mark Seaborn

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
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>

#include "region.h"
#include "parse-filename.h"
#include "resolve-filename.h"
#include "comms.h"
#include "config.h"
#include "config-read.h"
#include "serialise.h"
#include "cap-protocol.h"
#include "cap-utils.h"
#include "fs-operations.h"
#include "filesysobj-union.h"
#include "marshal.h"
#include "marshal-pack.h"
#include "exec.h"


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

/* Returns FD, or -1 for an error.
   If this is returning a dummy FD (for directories), it will set
   `dummy_fd' to 1, fill out `r_obj', and return -1. */
int process_open_d(struct filesys_obj *root, struct dir_stack *cwd,
		   seqf_t pathname, int flags, int mode, int *err,
		   int *dummy_fd, cap_t *r_obj)
{
  region_t r = region_make();
  void *result;
  int rc =
    resolve_obj(r, root, cwd, pathname, SYMLINK_LIMIT,
		((flags & O_NOFOLLOW) || (flags & O_EXCL)) ? 1:0,
		(flags & O_CREAT) ? 1:0,
		&result, err);
  region_free(r);
  *dummy_fd = 0;
  if(rc == RESOLVED_DIR) {
    /* Opening directories with open() is not implemented yet. */
    struct dir_stack *ds = result;
    *r_obj = dir_stack_downcast(ds);
    *err = EISDIR;

    /* Warning disabled now that fchdir() works.  This warning is
       printed too many times, especially with XEmacs, so it gets
       annoying. */
    if(0) {
      /* Give a warning about this */
      fprintf(stderr, "plash: warning: using open() on a directory, `");
      fprint_d(stderr, pathname);
      fprintf(stderr, "', is not fully supported\n");
    }

    *dummy_fd = 1;
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
      switch(obj->vtable->fsobj_type(obj)) {
        case OBJT_FILE:
	  {
	    int fd = obj->vtable->open(obj, flags, err);
	    filesys_obj_free(obj);
	    return fd;
	  }
        case OBJT_SYMLINK:
	  filesys_obj_free(obj);
	  *err = ELOOP; /* Symlink found when O_NOFOLLOW used */
	  return -1;
        default:
	  filesys_obj_free(obj);
	  *err = ENOTDIR; /* is there a more appropriate errno? */
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

/* Returns FD, or -1 for an error. */
int process_open(struct filesys_obj *root, struct dir_stack *cwd,
		 seqf_t pathname, int flags, int mode, int *err)
{
  int dummy_fd;
  cap_t r_obj;
  int fd = process_open_d(root, cwd, pathname, flags, mode, err,
			  &dummy_fd, &r_obj);
  if(fd < 0 && dummy_fd) filesys_obj_free(r_obj);
  return fd;
}

/* mkdir() behaves like open() with O_EXCL: it won't follow a symbolic
   link and create the destination. */
int process_mkdir(struct filesys_obj *root, struct dir_stack *cwd,
		  seqf_t pathname, int mode, int *err)
{
  int r;
  struct resolved_slot *slot =
    resolve_empty_slot(root, cwd, pathname, SYMLINK_LIMIT, err);
  if(!slot) return -1;
  r = slot->dir->vtable->mkdir(slot->dir, slot->leaf, mode, err);
  free_resolved_slot(slot);
  return r;
}

int process_symlink(struct filesys_obj *root, struct dir_stack *cwd,
		    seqf_t newpath, seqf_t oldpath, int *err)
{
  char *oldpath1;
  int r;
  struct resolved_slot *slot =
    resolve_empty_slot(root, cwd, newpath, SYMLINK_LIMIT, err);
  if(!slot) return -1;
  oldpath1 = strdup_seqf(oldpath);
  r = slot->dir->vtable->symlink(slot->dir, slot->leaf, oldpath1, err);
  free(oldpath1);
  free_resolved_slot(slot);
  return r;
}

int process_rename(struct filesys_obj *root,
		   struct dir_stack *old_dir, seqf_t oldpath,
		   struct dir_stack *new_dir, seqf_t newpath, int *err)
{
  struct resolved_slot *slot_src, *slot_dest;
  int rc;
  slot_src = resolve_any_slot(root, old_dir, oldpath, SYMLINK_LIMIT, err);
  if(!slot_src) return -1;
  slot_dest = resolve_any_slot(root, new_dir, newpath, SYMLINK_LIMIT, err);
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

int process_link(struct filesys_obj *root,
		 struct dir_stack *old_dir, seqf_t oldpath,
		 struct dir_stack *new_dir, seqf_t newpath, int *err)
{
  struct resolved_slot *slot_src, *slot_dest;
  int rc;
  slot_src = resolve_any_slot(root, old_dir, oldpath, SYMLINK_LIMIT, err);
  if(!slot_src) return -1;
  slot_dest = resolve_any_slot(root, new_dir, newpath, SYMLINK_LIMIT, err);
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

int process_unlink(struct filesys_obj *root, struct dir_stack *cwd,
		   seqf_t pathname, int *err)
{
  int rc;
  struct resolved_slot *slot =
    resolve_any_slot(root, cwd, pathname, SYMLINK_LIMIT, err);
  if(!slot) return -1;
  rc = slot->dir->vtable->unlink(slot->dir, slot->leaf, err);
  free_resolved_slot(slot);
  return rc;
}

int process_rmdir(struct filesys_obj *root, struct dir_stack *cwd,
		  seqf_t pathname, int *err)
{
  int rc;
  struct resolved_slot *slot =
    resolve_any_slot(root, cwd, pathname, SYMLINK_LIMIT, err);
  if(!slot) return -1;
  rc = slot->dir->vtable->rmdir(slot->dir, slot->leaf, err);
  free_resolved_slot(slot);
  return rc;
}

int process_chmod(struct filesys_obj *root, struct dir_stack *cwd,
		  seqf_t pathname, int mode, int nofollow, int *err)
{
  int rc;
  /* chmod does follow symlinks.  Symlinks do not have permissions of
     their own. */
  struct filesys_obj *obj =
    resolve_obj_simple(root, cwd, pathname, SYMLINK_LIMIT,
		       nofollow, err);
  if(!obj) return -1;
  rc = obj->vtable->fsobj_chmod(obj, mode, err);
  filesys_obj_free(obj);
  return rc;
}

/* This is currently a no-op.  It checks whether the file exists.  If
   not, it returns an error.  If the file exists and no owner or group
   change was requested, it returns success.  Otherwise, it returns
   EPERM. */
int process_chown(struct filesys_obj *root, struct dir_stack *cwd,
		  seqf_t pathname, int owner_uid, int group_gid,
		  int nofollow, int *err)
{
  struct filesys_obj *obj =
    resolve_obj_simple(root, cwd, pathname, SYMLINK_LIMIT,
		       nofollow, err);
  if(!obj) return -1;
  filesys_obj_free(obj);
  
  if(owner_uid == -1 &&
     group_gid == -1) {
    return 0;
  }
  else {
    *err = EPERM;
    return -1;
  }
}

int process_utimes(struct filesys_obj *root, struct dir_stack *cwd,
		   seqf_t pathname, int nofollow,
		   const struct timeval *atime, const struct timeval *mtime,
		   int *err)
{
  int rc;
  struct filesys_obj *obj =
    resolve_obj_simple(root, cwd, pathname, SYMLINK_LIMIT,
		       nofollow, err);
  if(!obj) return -1;
  rc = obj->vtable->fsobj_utimes(obj, atime, mtime, err);
  filesys_obj_free(obj);
  return rc;
}

int process_readlink(region_t r,
		     struct filesys_obj *root, struct dir_stack *cwd,
		     seqf_t pathname, seqf_t *result_dest, int *err)
{
  void *result;
  region_t r2 = region_make();
  int rc = resolve_obj(r2, root, cwd, pathname, SYMLINK_LIMIT,
		       TRUE /* nofollow */, 0 /* create */, &result, err);
  region_free(r2);
  if(rc == RESOLVED_FILE_OR_SYMLINK) {
    struct filesys_obj *obj = result;
    if(obj->vtable->fsobj_type(obj) == OBJT_SYMLINK) {
      int x = obj->vtable->readlink(obj, r, result_dest, err);
      filesys_obj_free(obj);
      return x;
    }
    else {
      filesys_obj_free(obj);
      *err = EINVAL; /* not a symlink */
      return -1;
    }
  }
  else if(rc == RESOLVED_DIR) {
    dir_stack_free(result);
    *err = EINVAL; /* not a symlink */
    return -1;
  }
  else return -1; /* `err' already filled out */
}


static void m_dir_fd(int *ok, seqf_t *msg, cap_seq_t *cap_args,
		     struct dir_stack **dir_stack)
{
  int cap_not_null;
  m_int(ok, msg, &cap_not_null);
  if(*ok && cap_not_null) {
    if(cap_args->size > 0) {
      cap_t object = cap_args->caps[0];
      cap_args->caps++;
      cap_args->size--;

      *dir_stack = dir_stack_upcast(object);
      if(!*dir_stack)
	*ok = FALSE;
    }
    else {
      *ok = FALSE;
    }
  }
  else {
    *dir_stack = NULL;
  }
}

static seqt_t pack_stat_info(region_t r, struct stat *st)
{
  return
    cat3(r,
	 cat5(r, mk_int(r, st->st_dev),
	      mk_int(r, st->st_ino),
	      mk_int(r, st->st_mode),
	      mk_int(r, st->st_nlink),
	      mk_int(r, st->st_uid)),
	 cat5(r, mk_int(r, st->st_gid),
	      mk_int(r, st->st_rdev),
	      mk_int(r, st->st_size),
	      mk_int(r, st->st_blksize),
	      mk_int(r, st->st_blocks)),
	 cat3(r, mk_int(r, st->st_atime),
	      mk_int(r, st->st_mtime),
	      mk_int(r, st->st_ctime)));
}

struct log_info {
  const char *op_name; /* Name of operation */
  int read_only; /* Whether the operation attempted was read-only */
};

int handle_fs_op_message(region_t r, struct process *proc,
			 struct fs_op_object *obj,
			 seqf_t msg_orig, fds_t fds_orig, cap_seq_t cap_args,
			 seqt_t *reply, fds_t *reply_fds,
			 cap_seq_t *r_caps,
			 seqt_t *log_msg, seqt_t *log_reply,
			 struct log_info *log)
{
  seqf_t msg = msg_orig;
  int ok = 1;
  int method_id;
  m_int(&ok, &msg, &method_id);
  if(ok) switch(method_id) {
  case METHOD_FSOP_COPY:
  {
    m_end(&ok, &msg);
    if(ok) {
      cap_t new_server;
      cap_t new_log = NULL;
      log->op_name = "fsop_copy";

      if(obj->log) {
	new_log = obj->log->vtable->log_branch(obj->log, seqf_string(""));
      }
      
      inc_ref(proc->root);
      if(proc->cwd) proc->cwd->hdr.refcount++;
      new_server = make_fs_op_server(new_log, proc->root, proc->cwd);
      
      *r_caps = mk_caps1(r, new_server);
      *reply = mk_int(r, METHOD_R_CAP);
      *log_reply = mk_string(r, "ok");
      return 0;
    }
    break;
  }
  case METHOD_FSOP_GET_ROOT_DIR:
  {
    /* Return a reference to the root directory */
    m_end(&ok, &msg);
    if(ok) {
      log->op_name = "fsop_get_root_dir";
      
      *r_caps = mk_caps1(r, inc_ref(proc->root));
      *reply = mk_int(r, METHOD_R_CAP);
      *log_reply = mk_printf(r, "ok");
      return 0;
    }
    break;
  }
  case METHOD_FSOP_GET_DIR:
  {
    /* Look up a pathname to a directory and return a reference to it */
    if(ok) {
      seqf_t pathname = msg;
      struct dir_stack *ds;
      int err;

      log->op_name = "fsop_get_dir";
      *log_msg = mk_leaf(r, pathname);
      ds = resolve_dir(r, proc->root, proc->cwd, pathname, SYMLINK_LIMIT, &err);
      if(ds) {
	*r_caps = mk_caps1(r, inc_ref(ds->dir));
	dir_stack_free(ds);
	*reply = mk_int(r, METHOD_OKAY);
	*log_reply = mk_printf(r, "ok");
	return 0;
      }
      else {
	return err;
      }
    }
    break;
  }
  case METHOD_FSOP_GET_OBJ:
  {
    /* Look up a pathname to any object and return a reference to the object */
    /* Will follow symlinks */
    if(ok) {
      seqf_t pathname = msg;
      int err;
      struct filesys_obj *obj;

      log->op_name = "fsop_get_obj";
      *log_msg = mk_leaf(r, pathname);
      obj = resolve_obj_simple(proc->root, proc->cwd, pathname,
			       SYMLINK_LIMIT, FALSE /* nofollow */, &err);
      if(obj) {
	*r_caps = mk_caps1(r, obj);
	*reply = mk_int(r, METHOD_OKAY);
	*log_reply = mk_printf(r, "ok");
	return 0;
      }
      else {
	return err;
      }
    }
    break;
  }
  case METHOD_FSOP_EXEC:
  {
    seqf_t cmd_filename;
    bufref_t argv_ref;
    m_lenblock(&ok, &msg, &cmd_filename);
    m_int(&ok, &msg, &argv_ref);
    if(ok) {
      struct arg_m_buf argbuf = { msg, caps_empty, fds_empty };
      int argc;
      int extra_args;
      const char **argv, **argv2;
      seqf_t cmd_filename2;
      int i;
      int err;
      struct filesys_obj *obj;
      int fd;
      const char *executable_filename; /* of interpreter for #! scripts */

      log->op_name = "exec";
      *log_msg = mk_leaf(r, cmd_filename);

      /* Unpack arguments. */
      {
	const bufref_t *a;
	if(argm_array(&argbuf, argv_ref, &argc, &a)) goto exec_fail;
	/* We use argv[0] later on, so check that argc >= 1. */
	if(argc < 1) goto exec_fail;
	argv = region_alloc(r, argc * sizeof(seqf_t));
	for(i = 0; i < argc; i++) {
	  seqf_t arg;
	  if(argm_str(&argbuf, a[i], &arg)) goto exec_fail;
	  argv[i] = region_strdup_seqf(r, arg);
	}
      }

      /* Log the execve() argument list too. */
      for(i = 0; i < argc; i++) {
	*log_msg = cat2(r, *log_msg, mk_printf(r, ", \"%s\"", argv[i]));
      }

      obj = resolve_file(r, proc->root, proc->cwd, cmd_filename,
			 SYMLINK_LIMIT, FALSE /* nofollow */, &err);
      if(!obj) goto exec_fail;

      /* Is this an executable object?  If so, we return the object for
	 the client to invoke itself. */
      {
	struct cap_args result;
	cap_call(obj, r,
		 cap_args_make(mk_int(r, METHOD_EO_IS_EXECUTABLE), caps_empty, fds_empty),
		 &result);
	if(expect_ok(result) >= 0) {
	  *reply = mk_int(r, METHOD_R_FSOP_EXEC_OBJECT);
	  *r_caps = mk_caps1(r, obj);
	  *log_reply = mk_string(r, "ok: executable object");
	  return 0;
	}
      }

      fd = open_executable_file(obj, cmd_filename, &err);
      filesys_obj_free(obj);
      if(fd < 0) goto exec_fail;

      /* Handle "#!" scripts. */
      if(exec_for_scripts(r, proc->root, proc->cwd,
			  region_strdup_seqf(r, cmd_filename), fd, argc, argv,
			  NULL /* fd not returned */,
			  &executable_filename,
			  &argc, &argv, &err) < 0) goto exec_fail;

      extra_args = 1;
      argv2 = region_alloc(r, (argc + extra_args) * sizeof(seqf_t));
      assert(argc >= 1);
      argv2[0] = argv[0];
      cmd_filename2 = seqf_string(pl_get_ldso_path());
      argv2[1] = executable_filename;
      if(0) {
	argv2[1] = "--library-path";
	argv2[2] = PLASH_LD_LIBRARY_PATH;
	argv2[3] = executable_filename;
      }
      for(i = 1; i < argc; i++) { argv2[extra_args + i] = argv[i]; }

      {
	seqt_t got = seqt_empty;
	int i;
	for(i = 0; i < argc + extra_args; i++) {
	  got = cat3(r, got,
		     mk_int(r, strlen(argv2[i])),
		     mk_string(r, argv2[i]));
	}
	*reply = cat5(r, mk_int(r, METHOD_R_FSOP_EXEC),
		      mk_int(r, cmd_filename2.size),
		      mk_leaf(r, cmd_filename2),
		      mk_int(r, argc + extra_args),
		      got);
	*log_reply = mk_string(r, "ok");
	log->read_only = TRUE;
	return 0;
      }
    exec_fail:
      return err;
    }
    break;
  }
  case METHOD_FSOP_OPEN:
  {
    struct dir_stack *dir;
    int flags, mode;
    m_dir_fd(&ok, &msg, &cap_args, &dir);
    m_int(&ok, &msg, &flags);
    m_int(&ok, &msg, &mode);
    if(ok) {
      seqf_t pathname = msg;
      int fd, err = 0, dummy_fd;
      cap_t d_obj;

      log->op_name = "open";
      *log_msg =
	cat2(r, mk_leaf(r, pathname),
	     mk_printf(r, ", flags=0o%o, mode=0o%o", flags, mode));
      if((flags & O_ACCMODE) == O_RDONLY) {
	log->read_only = TRUE;
      }

      if(!dir)
	dir = proc->cwd;
      fd = process_open_d(proc->root, dir, pathname, flags, mode, &err,
			  &dummy_fd, &d_obj);
      if(fd >= 0) {
	*reply = mk_int(r, METHOD_R_FSOP_OPEN);
	*reply_fds = mk_fds1(r, fd);
	*log_reply = mk_string(r, "ok");
	return 0;
      }
      else if(dummy_fd) {
	/* Open a file descriptor to use as the dummy FD.
	   We use /dev/null.  Check that we're getting what we expect
	   by looking at the device type -- it's 1,3 under Linux. */
	struct stat stat;
	int fd = open("/dev/null", O_RDONLY);
	if(fd < 0) goto dummy_fail;
	if(fstat(fd, &stat) < 0 ||
	   !S_ISCHR(stat.st_mode) ||
	   stat.st_rdev != 0x103) {
	  close(fd);
	  goto dummy_fail;
	}
	
	*reply = mk_int(r, METHOD_R_FSOP_OPEN_DIR);
	*log_reply = mk_string(r, "got dir, use dummy FD");
	*reply_fds = mk_fds1(r, fd);
	*r_caps = mk_caps1(r, d_obj);
	return 0;
      dummy_fail:
	filesys_obj_free(d_obj);
	*reply = cat2(r, mk_int(r, METHOD_FAIL),
		      mk_int(r, EIO));
	*log_reply = mk_string(r, "fail: can't use /dev/null as dummy");
	return 0;
      }
      else {
	return err;
      }
    }
    break;
  }
  case METHOD_FSOP_STAT:
  {
    struct dir_stack *dir;
    int nofollow;
    m_dir_fd(&ok, &msg, &cap_args, &dir);
    m_int(&ok, &msg, &nofollow);
    if(ok) {
      seqf_t pathname = msg;
      struct filesys_obj *obj;
      int err;
      struct stat stat;

      log->op_name = nofollow ? "lstat" : "stat";
      *log_msg = mk_leaf(r, pathname);
      log->read_only = TRUE;

      if(!dir)
	dir = proc->cwd;
      obj = resolve_obj_simple(proc->root, dir, pathname,
			       SYMLINK_LIMIT, nofollow, &err);
      if(!obj) {
	return err;
      }
      if(obj->vtable->fsobj_stat(obj, &stat, &err) < 0) {
	filesys_obj_free(obj);
	return err;
      }
      filesys_obj_free(obj);
      *reply = cat2(r, mk_int(r, METHOD_R_FSOP_STAT), pack_stat_info(r, &stat));
      *log_reply = mk_string(r, "ok");
      return 0;
    }
    break;
  }
  case METHOD_FSOP_READLINK:
  {
    struct dir_stack *dir;
    m_dir_fd(&ok, &msg, &cap_args, &dir);
    if(ok) {
      seqf_t pathname = msg;
      seqf_t link_dest;
      int err;
      
      log->op_name = "readlink";
      *log_msg = mk_leaf(r, pathname);
      log->read_only = TRUE;

      if(!dir)
	dir = proc->cwd;
      if(process_readlink(r, proc->root, dir, pathname,
			  &link_dest, &err) < 0) {
	return err;
      }
      else {
	*log_reply = mk_string(r, "ok");
	*reply = cat2(r, mk_int(r, METHOD_R_FSOP_READLINK),
		      mk_leaf(r, link_dest));
	return 0;
      }
    }
    break;
  }
  case METHOD_FSOP_GETCWD:
  {
    m_end(&ok, &msg);
    if(ok) {
      log->op_name = "getcwd";
      log->read_only = TRUE;
      if(proc->cwd) {
	*log_reply = mk_string(r, "ok");
	*reply = cat2(r, mk_int(r, METHOD_R_FSOP_GETCWD),
		      string_of_cwd(r, proc->cwd));
	return 0;
      }
      else {
	return E_NO_CWD_DEFINED;
      }
    }
    break;
  }
  case METHOD_FSOP_DIRLIST:
  {
    if(ok) {
      seqf_t pathname = msg;
      int err = 0;
      struct dir_stack *ds;
      log->op_name = "dirlist";
      *log_msg = mk_leaf(r, pathname);
      log->read_only = TRUE;
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
	  *reply = cat2(r, mk_int(r, METHOD_R_FSOP_DIRLIST), result);
	  *log_reply = mk_string(r, "ok");
	  dir_stack_free(ds);
	  return 0;
	}
	else {
	  /* Failed in getting list */
	  return err;
	}
      }
      else {
	/* Failed in resolving path */
	return err;
      }
    }
    break;
  }
  case METHOD_FSOP_ACCESS:
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
    struct dir_stack *dir;
    int mode;
    m_dir_fd(&ok, &msg, &cap_args, &dir);
    m_int(&ok, &msg, &mode);
    if(ok) {
      seqf_t pathname = msg;
      struct filesys_obj *obj;
      int err;

      log->op_name = "access";
      *log_msg = mk_leaf(r, pathname);
      log->read_only = TRUE;

      if(!dir)
	dir = proc->cwd;
      obj = resolve_obj_simple(proc->root, dir, pathname, SYMLINK_LIMIT,
			       FALSE /*nofollow*/, &err);
      if(obj) {
	filesys_obj_free(obj);
	*reply = mk_int(r, METHOD_OKAY);
	*log_reply = mk_string(r, "ok");
	return 0;
      }
      else {
	return err;
      }
    }
    break;
  }
  case METHOD_FSOP_MKDIR:
  {
    /* mkdir() call */
    struct dir_stack *dir;
    int mode;
    m_dir_fd(&ok, &msg, &cap_args, &dir);
    m_int(&ok, &msg, &mode);
    if(ok) {
      seqf_t pathname = msg;
      int err = 0;
      
      log->op_name = "mkdir";
      *log_msg = mk_leaf(r, pathname);

      if(!dir)
	dir = proc->cwd;
      if(process_mkdir(proc->root, dir, pathname, mode, &err) < 0) {
	return err;
      }
      else {
	*reply = mk_int(r, METHOD_OKAY);
	*log_reply = mk_string(r, "ok");
	return 0;
      }
    }
    break;
  }
  case METHOD_FSOP_SYMLINK:
  {
    /* symlink() call */
    struct dir_stack *dir;
    seqf_t newpath;
    m_dir_fd(&ok, &msg, &cap_args, &dir);
    m_lenblock(&ok, &msg, &newpath);
    if(ok) {
      seqf_t oldpath = msg;
      int err;
      
      log->op_name = "symlink";
      *log_msg = cat3(r, mk_leaf(r, newpath),
		      mk_string(r, " to link to "), mk_leaf(r, oldpath));

      if(!dir)
	dir = proc->cwd;
      if(process_symlink(proc->root, dir, newpath, oldpath, &err) < 0) {
	return err;
      }
      else {
	*reply = mk_int(r, METHOD_OKAY);
	return 0;
      }
    }
    break;
  }
  case METHOD_FSOP_RENAME:
  {
    /* rename() call */
    struct dir_stack *new_dir;
    struct dir_stack *old_dir;
    seqf_t newpath;
    m_dir_fd(&ok, &msg, &cap_args, &new_dir);
    m_dir_fd(&ok, &msg, &cap_args, &old_dir);
    m_lenblock(&ok, &msg, &newpath);
    if(ok) {
      seqf_t oldpath = msg;
      int err;
      
      log->op_name = "rename";
      *log_msg = cat3(r, mk_leaf(r, oldpath),
		      mk_string(r, " to "), mk_leaf(r, newpath));

      if(!new_dir)
	new_dir = proc->cwd;
      if(!old_dir)
	old_dir = proc->cwd;
      if(process_rename(proc->root,
			old_dir, oldpath,
			new_dir, newpath, &err) < 0) {
	return err;
      }
      else {
	*reply = mk_int(r, METHOD_OKAY);
	*log_reply = mk_string(r, "ok");
	return 0;
      }
    }
    break;
  }
  case METHOD_FSOP_LINK:
  {
    /* link() call */
    struct dir_stack *new_dir;
    struct dir_stack *old_dir;
    seqf_t newpath;
    m_dir_fd(&ok, &msg, &cap_args, &new_dir);
    m_dir_fd(&ok, &msg, &cap_args, &old_dir);
    m_lenblock(&ok, &msg, &newpath);
    if(ok) {
      seqf_t oldpath = msg;
      int err;

      log->op_name = "link";
      *log_msg = cat4(r, mk_string(r, "create "), mk_leaf(r, newpath),
		      mk_string(r, " to link to "), mk_leaf(r, oldpath));

      if(!new_dir)
	new_dir = proc->cwd;
      if(!old_dir)
	old_dir = proc->cwd;
      if(process_link(proc->root,
		      old_dir, oldpath,
		      new_dir, newpath, &err) < 0) {
	return err;
      }
      else {
	*reply = mk_int(r, METHOD_OKAY);
	*log_reply = mk_string(r, "ok");
	return 0;
      }
    }
    break;
  }
  case METHOD_FSOP_CHMOD:
  {
    /* chmod() call.  An old version of this re-used the code for "open"
       and then fchmod'ed the FD.  The problem with that is that it
       would allow the client process to chmod any file, even those it's
       not supposed to have write access to!  I have now added a chmod
       method to file and directory objects. */
    struct dir_stack *dir;
    int nofollow;
    int mode;
    m_dir_fd(&ok, &msg, &cap_args, &dir);
    m_int(&ok, &msg, &nofollow);
    m_int(&ok, &msg, &mode);
    if(ok) {
      seqf_t pathname = msg;
      int err;

      log->op_name = nofollow ? "lchmod" : "chmod";
      *log_msg = mk_leaf(r, pathname);

      if(!dir)
	dir = proc->cwd;
      if(process_chmod(proc->root, dir, pathname, mode, nofollow, &err) < 0) {
	return err;
      }
      else {
	*reply = mk_int(r, METHOD_OKAY);
	*log_reply = mk_string(r, "ok");
	return 0;
      }
    }
    break;
  }
  case METHOD_FSOP_CHOWN:
  {
    /* chown()/lchown() calls.   */
    struct dir_stack *dir;
    int nofollow;
    int owner_uid, group_gid;
    m_dir_fd(&ok, &msg, &cap_args, &dir);
    m_int(&ok, &msg, &nofollow);
    m_int(&ok, &msg, &owner_uid);
    m_int(&ok, &msg, &group_gid);
    if(ok) {
      seqf_t pathname = msg;
      int err;

      log->op_name = nofollow ? "lchown" : "chown";
      *log_msg = mk_leaf(r, pathname);

      if(!dir)
	dir = proc->cwd;
      if(process_chown(proc->root, dir, pathname, owner_uid, group_gid,
		       nofollow, &err) < 0) {
	return err;
      }
      else {
	*reply = mk_int(r, METHOD_OKAY);
	*log_reply = mk_string(r, "ok");
	return 0;
      }
    }
    break;
  }
  case METHOD_FSOP_UTIME:
  {
    /* utime()/utimes()/lutimes() calls */
    struct dir_stack *dir;
    int nofollow;
    int atime_sec, atime_usec;
    int mtime_sec, mtime_usec;
    m_dir_fd(&ok, &msg, &cap_args, &dir);
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

      log->op_name = nofollow ? "lutime" : "utime";
      *log_msg = mk_leaf(r, pathname);

      if(!dir)
	dir = proc->cwd;
      if(process_utimes(proc->root, dir, pathname, nofollow,
			&atime, &mtime, &err) < 0) {
	return err;
      }
      else {
	*reply = mk_int(r, METHOD_OKAY);
	*log_reply = mk_string(r, "ok");
	return 0;
      }
    }
    break;
  }
  case METHOD_FSOP_UNLINK:
  {
    /* unlink() call */
    struct dir_stack *dir;
    m_dir_fd(&ok, &msg, &cap_args, &dir);
    if(ok) {
      seqf_t pathname = msg;
      int err;

      log->op_name = "unlink";
      *log_msg = mk_leaf(r, pathname);

      if(!dir)
	dir = proc->cwd;
      if(process_unlink(proc->root, dir, pathname, &err) < 0) {
	return err;
      }
      else {
	*reply = mk_int(r, METHOD_OKAY);
	*log_reply = mk_string(r, "ok");
	return 0;
      }
    }
    break;
  }
  case METHOD_FSOP_RMDIR:
  {
    /* rmdir() call */
    struct dir_stack *dir;
    m_dir_fd(&ok, &msg, &cap_args, &dir);
    if(ok) {
      seqf_t pathname = msg;
      int err;

      log->op_name = "rmdir";
      *log_msg = mk_leaf(r, pathname);

      if(!dir)
	dir = proc->cwd;
      if(process_rmdir(proc->root, dir, pathname, &err) < 0) {
	return err;
      }
      else {
	*reply = mk_int(r, METHOD_OKAY);
	*log_reply = mk_string(r, "ok");
	return 0;
      }
    }
    break;
  }
  case METHOD_FSOP_CONNECT:
  {
    /* connect() for Unix domain sockets */
    fds_t fds = fds_orig;
    int sock_fd;
    m_fd(&ok, &fds, &sock_fd);
    if(ok) {
      seqf_t pathname = msg;
      int err;
      struct filesys_obj *obj;

      log->op_name = "connect";
      *log_msg = mk_leaf(r, pathname);
      obj = resolve_file(r, proc->root, proc->cwd, pathname,
			 SYMLINK_LIMIT, FALSE /*nofollow*/, &err);
      if(obj) {
	if(obj->vtable->socket_connect(obj, sock_fd, &err) >= 0) {
	  filesys_obj_free(obj);
	  *log_reply = mk_string(r, "ok");
	  *reply = mk_int(r, METHOD_OKAY);
	  return 0;
	}
	filesys_obj_free(obj);
      }
      return err;
    }
    break;
  }
  case METHOD_FSOP_BIND:
  {
    /* bind() for Unix domain sockets */
    fds_t fds = fds_orig;
    int sock_fd;
    m_fd(&ok, &fds, &sock_fd);
    if(ok) {
      seqf_t pathname = msg;
      int err;
      struct resolved_slot *slot;

      log->op_name = "bind";
      *log_msg = mk_leaf(r, pathname);
      slot = resolve_empty_slot(proc->root, proc->cwd, pathname,
				SYMLINK_LIMIT, &err);
      if(slot) {
	if(slot->dir->vtable->socket_bind(slot->dir, slot->leaf, sock_fd,
					  &err) >= 0) {
	  free_resolved_slot(slot);
	  *log_reply = mk_string(r, "ok");
	  *reply = mk_int(r, METHOD_OKAY);
	  return 0;
	}
	free_resolved_slot(slot);
      }
      return err;
    }
    break;
  }
  case METHOD_FSOP_CHDIR:
  {
    /* chdir() call */
    if(ok) {
      seqf_t pathname = msg;
      int err = 0;
      int e;

      log->op_name = "chdir";
      *log_msg = mk_leaf(r, pathname);
      log->read_only = TRUE;
      e = process_chdir(proc, pathname, &err);
      if(e == 0) {
	*log_reply = mk_string(r, "ok");
	*reply = mk_int(r, METHOD_OKAY);
	return 0;
      }
      else {
	return err;
      }
    }
    break;
  }
  case METHOD_FSOP_FCHDIR:
  {
    /* fchdir() call.  Takes a dir_stack object (used to represent a
       directory FD in Plash) rather than a kernel-level file
       descriptor. */
    m_end(&ok, &msg);
    if(ok && cap_args.size == 1) {
      struct dir_stack *n = dir_stack_upcast(cap_args.caps[0]);

      log->op_name = "fchdir";
      log->read_only = TRUE;
      if(n) {
	if(proc->cwd) dir_stack_free(proc->cwd);
	n->hdr.refcount++;
	proc->cwd = n;
	*reply = mk_int(r, METHOD_OKAY);
	*log_reply = mk_string(r, "ok");
	return 0;
      }
      else {
	/* Fail: not a dir_stack object */
	return ENOTDIR;
      }
    }
    break;
  }
  case METHOD_FSOP_DIR_FSTAT:
  {
    /* fstat() on directory FDs only. */
    m_end(&ok, &msg);
    if(ok && cap_args.size == 1) {
      struct dir_stack *n = dir_stack_upcast(cap_args.caps[0]);

      log->op_name = "dir_fstat";
      log->read_only = TRUE;
      if(n) {
	int err;
	struct stat st;
	n->hdr.refcount++;
	if(n->dir->vtable->fsobj_stat(n->dir, &st, &err) < 0) {
	  *reply = cat2(r, mk_int(r, METHOD_FAIL),
			mk_int(r, ENOTDIR));
	  *log_reply = mk_string(r, "fail");
	}
	else {
	  *reply = cat2(r, mk_int(r, METHOD_OKAY), pack_stat_info(r, &st));
	  *log_reply = mk_string(r, "ok");
	}
	dir_stack_free(n);
	return 0;
      }
      else {
	/* Fail: not a dir_stack object */
	return ENOTDIR;
      }
    }
  }
  case METHOD_FSOP_LOG:
  {
    if(ok) {
      log->op_name = "log";
      *log_msg = mk_leaf(r, msg);
      log->read_only = TRUE;
      return 0;
    }
  }
  }

  // if(DO_LOG_SUMMARY(state)) fprintf(state->log, "Unknown message!\n");
  /* Send a Fail reply. */
  return ENOSYS;
}


DECLARE_VTABLE(fs_op_vtable);

/* Takes owning references. */
cap_t make_fs_op_server(struct filesys_obj *log,
			struct filesys_obj *root, struct dir_stack *cwd)
{
  struct fs_op_object *obj =
    filesys_obj_make(sizeof(struct fs_op_object), &fs_op_vtable);
  obj->p.root = root;
  obj->p.cwd = cwd;
  obj->log = log;
  return (cap_t) obj;
}

void fs_op_free(struct filesys_obj *obj1)
{
  struct fs_op_object *obj = (void *) obj1;
  
  filesys_obj_free(obj->p.root);
  if(obj->p.cwd) dir_stack_free(obj->p.cwd);

  if(obj->log) filesys_obj_free(obj->log);
}

#ifdef GC_DEBUG
void fs_op_mark(struct filesys_obj *obj1)
{
  struct fs_op_object *obj = (void *) obj1;
  filesys_obj_mark(obj->p.root);
  if(obj->p.cwd) filesys_obj_mark(dir_stack_downcast(obj->p.cwd));
}
#endif

void fs_op_call(struct filesys_obj *obj1, region_t r,
		struct cap_args args, struct cap_args *result)
{
  struct fs_op_object *obj = (void *) obj1;
  seqt_t log_msg = mk_string(r, "");
  seqt_t log_reply = mk_string(r, "?");
  struct log_info log_info;
  int err;

  log_info.read_only = FALSE;
  log_info.op_name = "???";
  
  result->data = seqt_empty;
  result->caps = caps_empty;
  result->fds = fds_empty;
  err = handle_fs_op_message(r, &obj->p, obj, flatten_reuse(r, args.data),
		       args.fds, args.caps,
		       &result->data, &result->fds, &result->caps,
		       &log_msg, &log_reply, &log_info);
  if(err) {
    result->data = cat2(r, mk_int(r, METHOD_FAIL),
			mk_int(r, err));
    log_reply = mk_printf(r, "fail: %s", strerror(err));
  }
  caps_free(args.caps);
  close_fds(args.fds);
  
  if(obj->log) {
    seqt_t msg = mk_printf(r, "[%c%c] %s%s%s: %s",
	    log_info.read_only ? 'r' : 'w',
	    err ? '!' : '.',
	    log_info.op_name,
	    log_msg.size > 0 ? ": " : "",
	    flatten_str(r, log_msg),
	    flatten_str(r, log_reply));
    obj->log->vtable->log_msg(obj->log, flatten(r, msg));
  }
}


DECLARE_VTABLE(fs_op_maker_vtable);
struct fs_op_maker {
  struct filesys_obj hdr;
  
  struct filesys_obj *log; /* May be NULL */
};

cap_t fs_op_maker_make(struct filesys_obj *log)
{
  struct fs_op_maker *obj =
    filesys_obj_make(sizeof(struct fs_op_maker), &fs_op_maker_vtable);
  obj->log = log;
  return (struct filesys_obj *) obj;
}

void fs_op_maker_free(struct filesys_obj *obj1)
{
  struct fs_op_maker *obj = (void *) obj1;
  if(obj->log) filesys_obj_free(obj->log);
}

void fs_op_maker_call(struct filesys_obj *obj1, region_t r,
		      struct cap_args args, struct cap_args *result)
{
  struct fs_op_maker *obj = (void *) obj1;

  cap_t root_dir;
  cap_t log;
  if(pl_unpack(r, args, METHOD_MAKE_FS_OP, "c", &root_dir)) {
    if(obj->log) inc_ref(obj->log);
    *result = pl_pack(r, METHOD_R_CAP, "c",
		      make_fs_op_server(obj->log, root_dir, NULL /* cwd */));
  }
  else if(pl_unpack(r, args, METHOD_MAKE_FS_OP_WITH_LOG, "cd",
		    &root_dir, &log)) {
    *result = pl_pack(r, METHOD_R_CAP, "c",
		      make_fs_op_server(log, root_dir, NULL /* cwd */));
  }
  else {
    pl_args_free(&args);
    *result = pl_pack(r, METHOD_FAIL_UNKNOWN_METHOD, "");
  }
}


DECLARE_VTABLE(conn_maker_vtable);
struct conn_maker {
  struct filesys_obj hdr;
};

cap_t conn_maker_make()
{
  return filesys_obj_make(sizeof(struct conn_maker), &conn_maker_vtable);
}

void conn_maker_free(struct filesys_obj *obj)
{
}

int conn_maker_make_conn(struct filesys_obj *obj1, region_t r,
			 cap_seq_t export, int import_count, cap_t **import)
{
  int socks[2];
  if(socketpair(AF_LOCAL, SOCK_STREAM, 0, socks) < 0) {
    caps_free(export);
    return -1;
  }
  set_close_on_exec_flag(socks[0], 1);
  set_close_on_exec_flag(socks[1], 1);
  *import = cap_make_connection(r, socks[1], export, import_count,
				"to-client");
  return socks[0];
}


DECLARE_VTABLE(union_dir_maker_vtable);
struct union_dir_maker {
  struct filesys_obj hdr;
};

cap_t union_dir_maker_make()
{
  return filesys_obj_make(sizeof(struct union_dir_maker), &union_dir_maker_vtable);
}

void union_dir_maker_call(struct filesys_obj *obj_unused, region_t r,
			  struct cap_args args, struct cap_args *result)
{
  cap_t obj1, obj2;
  if(pl_unpack(r, args, METHOD_MAKE_UNION_DIR, "cc", &obj1, &obj2)) {
    *result = pl_pack(r, METHOD_R_CAP, "c", make_union_dir(obj1, obj2));
  }
  else {
    pl_args_free(&args);
    *result = pl_pack(r, METHOD_FAIL_UNKNOWN_METHOD, "");
  }
}

#include "out-vtable-fs-operations.h"
