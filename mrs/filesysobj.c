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
