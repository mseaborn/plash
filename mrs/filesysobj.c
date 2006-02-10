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
#include <unistd.h>
#include <fcntl.h>

#include "region.h"
#include "filesysobj.h"
#include "cap-utils.h"


int set_close_on_exec_flag(int fd, int value)
{
  int oldflags = fcntl(fd, F_GETFD, 0);
  if(oldflags < 0) return oldflags;
  if(value != 0) { oldflags |= FD_CLOEXEC; }
    else { oldflags &= ~FD_CLOEXEC; }
  return fcntl(fd, F_SETFD, oldflags);
}

struct file_desc fd_list =
  { .refcount = 0, .fd = -1, .prev = &fd_list, .next = &fd_list };

struct file_desc *make_fd(int fd)
{
  struct file_desc *desc = amalloc(sizeof(struct file_desc));
  desc->refcount = 1;
  desc->fd = fd;

  /* Link into global list. */
  desc->prev = &fd_list;
  desc->next = fd_list.next;
  fd_list.next->prev = desc;
  fd_list.next = desc;

  return desc;
}

void free_fd(struct file_desc *desc)
{
  assert(desc);
  assert(desc->refcount > 0);
  desc->refcount--;
  if(desc->refcount <= 0) {
    /* Unlink from list */
    desc->prev->next = desc->next;
    desc->next->prev = desc->prev;
    
    close(desc->fd);
    free(desc);
  }
}

/* This is used in a newly-forked child process, just before calling exec.
   So there's no need to unlink the FDs from the list. */
void close_our_fds()
{
  struct file_desc *node;
  for(node = fd_list.next; node != &fd_list; node = node->next) {
    close(node->fd);
  }
}


/* Abstract types */

void filesys_obj_free(struct filesys_obj *obj)
{
  assert(obj);
  assert(obj->refcount > 0);
  obj->refcount--;
  if(obj->refcount <= 0) {
    obj->vtable->free(obj);
    free(obj);
  }
}


void marshall_cap_call(struct filesys_obj *obj, region_t r,
		       struct cap_args args, struct cap_args *result)
{
  {
    seqf_t data = flatten_reuse(r, args.data);
    int ok = 1;
    m_str(&ok, &data, "Otyp");
    m_end(&ok, &data);
    if(ok && args.caps.size == 0 && args.fds.count == 0) {
      result->data =
	cat2(r, mk_string(r, "Okay"),
	     mk_int(r, obj->vtable->type(obj)));
      result->caps = caps_empty;
      result->fds = fds_empty;
      return;
    }
  }
  {
    seqf_t data = flatten_reuse(r, args.data);
    int ok = 1;
    m_str(&ok, &data, "Osta");
    m_end(&ok, &data);
    if(ok && args.caps.size == 0 && args.fds.count == 0) {
      int err;
      struct stat stat;
      result->caps = caps_empty;
      result->fds = fds_empty;
      if(obj->vtable->stat(obj, &stat, &err) < 0) {
	result->data = cat2(r, mk_string(r, "Fail"), mk_int(r, err));
	return;
      }
      result->data = cat4(r, mk_string(r, "Okay"),
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
      return;
    }
  }
  caps_free(args.caps);
  close_fds(args.fds);
  result->data = mk_string(r, "RMsg");
  result->caps = caps_empty;
  result->fds = fds_empty;
}

int marshall_type(struct filesys_obj *obj)
{
  region_t r = region_make();
  struct cap_args result;
  cap_call(obj, r,
	   cap_args_make(mk_string(r, "Otyp"),
			 caps_empty, fds_empty),
	   &result);
  {
    seqf_t msg = flatten_reuse(r, result.data);
    int t;
    int ok = 1;
    m_str(&ok, &msg, "Okay");
    m_int(&ok, &msg, &t);
    m_end(&ok, &msg);
    if(ok && result.caps.size == 0 && result.fds.count == 0) {
      region_free(r);
      return t;
    }
  }
  caps_free(result.caps);
  close_fds(result.fds);
  region_free(r);
  return 0;
}

int marshall_stat(struct filesys_obj *obj, struct stat *buf, int *err)
{
  region_t r = region_make();
  struct cap_args result;
  cap_call(obj, r,
	   cap_args_make(mk_string(r, "Osta"),
			 caps_empty, fds_empty),
	   &result);
  {
    seqf_t msg = flatten_reuse(r, result.data);
    int myst_dev, myst_ino, myst_mode, myst_nlink, myst_uid, myst_gid, myst_rdev,
      myst_size, myst_blksize, myst_blocks, myst_atime, myst_mtime, myst_ctime;
    int ok = 1;
    m_str(&ok, &msg, "Okay");
    m_int(&ok, &msg, &myst_dev);
    m_int(&ok, &msg, &myst_ino);
    m_int(&ok, &msg, &myst_mode);
    m_int(&ok, &msg, &myst_nlink);
    m_int(&ok, &msg, &myst_uid);
    m_int(&ok, &msg, &myst_gid);
    m_int(&ok, &msg, &myst_rdev);
    m_int(&ok, &msg, &myst_size);
    m_int(&ok, &msg, &myst_blksize);
    m_int(&ok, &msg, &myst_blocks);
    m_int(&ok, &msg, &myst_atime);
    m_int(&ok, &msg, &myst_mtime);
    m_int(&ok, &msg, &myst_ctime);
    m_end(&ok, &msg);
    if(ok && result.caps.size == 0 && result.fds.count == 0) {
      buf->st_dev = myst_dev;
      buf->st_ino = myst_ino;
      buf->st_mode = myst_mode;
      buf->st_nlink = myst_nlink;
      buf->st_uid = myst_uid;
      buf->st_gid = myst_gid;
      buf->st_rdev = myst_rdev;
      buf->st_size = myst_size;
      buf->st_blksize = myst_blksize;
      buf->st_blocks = myst_blocks;
      buf->st_atime = myst_atime;
      buf->st_mtime = myst_mtime;
      buf->st_ctime = myst_ctime;
      region_free(r);
      return 0;
    }
  }
  {
    seqf_t msg = flatten_reuse(r, result.data);
    int ok = 1;
    m_str(&ok, &msg, "Fail");
    m_int(&ok, &msg, err);
    m_end(&ok, &msg);
    if(ok) {
      region_free(r);
      return -1;
    }
  }
  caps_free(result.caps);
  close_fds(result.fds);
  region_free(r);
  *err = ENOSYS;
  return -1;
}

int objt_unknown(struct filesys_obj *obj) { return 0; }
int objt_file(struct filesys_obj *obj) { return OBJT_FILE; }
int objt_dir(struct filesys_obj *obj) { return OBJT_DIR; }
int objt_symlink(struct filesys_obj *obj) { return OBJT_SYMLINK; }

int dummy_stat(struct filesys_obj *obj, struct stat *buf, int *err)
{
  *err = ENOSYS;
  return -1;
}

int dummy_utimes(struct filesys_obj *obj, const struct timeval *atime,
		 const struct timeval *mtime, int *err)
{
  *err = ENOSYS;
  return -1;
}

int dummy_chmod(struct filesys_obj *obj, int mode, int *err)
{
  *err = ENOSYS;
  return -1;
}

struct filesys_obj *dummy_traverse(struct filesys_obj *obj, const char *leaf)
{
  return 0;
}

int dummy_list(struct filesys_obj *obj, region_t r, seqt_t *result, int *err)
{
  *err = ENOSYS;
  return -1;
}

int dummy_create_file(struct filesys_obj *obj, const char *leaf,
		      int flags, int mode, int *err)
{
  *err = ENOSYS;
  return -1;
}

int dummy_mkdir(struct filesys_obj *obj, const char *leaf, int mode, int *err)
{
  *err = ENOSYS;
  return -1;
}

int dummy_symlink(struct filesys_obj *obj, const char *leaf,
		  const char *oldpath, int *err)
{
  *err = ENOSYS;
  return -1;
}

int dummy_rename_or_link(struct filesys_obj *obj, const char *leaf,
			 struct filesys_obj *dest_dir, const char *dest_leaf,
			 int *err)
{
  *err = ENOSYS;
  return -1;
}

int dummy_unlink(struct filesys_obj *obj, const char *leaf, int *err)
{
  *err = ENOSYS;
  return -1;
}

int dummy_rmdir(struct filesys_obj *obj, const char *leaf, int *err)
{
  *err = ENOSYS;
  return -1;
}

int dummy_socket_bind(struct filesys_obj *obj, const char *leaf, int sock_fd, int *err)
{
  *err = ENOSYS;
  return -1;
}

int dummy_readlink(struct filesys_obj *obj, region_t r, seqf_t *result, int *err)
{
  *err = ENOSYS;
  return -1;
}

int dummy_open(struct filesys_obj *obj, int flags, int *err)
{
  *err = ENOSYS;
  return -1;
}

int dummy_socket_connect(struct filesys_obj *obj, int sock_fd, int *err)
{
  *err = ENOSYS;
  return -1;
}
