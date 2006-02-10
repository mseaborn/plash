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

#ifndef filesysobj_h
#define filesysobj_h

#include <sys/stat.h>
#include "region.h"


int set_close_on_exec_flag(int fd, int value);

/* These are kept in a global linked list so that all the FDs can be
   closed when we fork and exec an untrusted process. */
struct file_desc {
  int refcount;
  int fd;
  struct file_desc *prev, *next;
};

struct file_desc *make_fd(int fd);
void free_fd(struct file_desc *desc);
void close_our_fds(void);


struct filesys_obj {
  int refcount;
  struct filesys_obj_vtable *vtable;
};
#define OBJT_FILE 1
#define OBJT_DIR 2
#define OBJT_SYMLINK 3
struct filesys_obj_vtable {
  int type;
  void (*free)(struct filesys_obj *obj);
  int (*stat)(struct filesys_obj *obj, struct stat *buf);
  int (*utimes)(struct filesys_obj *obj, const struct timeval *atime,
		const struct timeval *mtime, int *err);
  /* Files and directories only, not symlinks: */
  int (*chmod)(struct filesys_obj *obj, int mode, int *err);
  
  /* Files only: */
  int (*open)(struct filesys_obj *obj, int flags, int *err);
  int (*connect)(struct filesys_obj *obj, int sock_fd, int *err);
  
  /* Directories only: */
  struct filesys_obj *(*traverse)(struct filesys_obj *obj, const char *leaf);
  int (*list)(struct filesys_obj *obj, region_t r, seqt_t *result, int *err);
  int (*create_file)(struct filesys_obj *obj, const char *leaf,
		     int flags, int mode, int *err);
  int (*mkdir)(struct filesys_obj *obj, const char *leaf, int mode, int *err);
  int (*symlink)(struct filesys_obj *obj, const char *leaf,
		 const char *oldpath, int *err);
  int (*rename)(struct filesys_obj *obj, const char *leaf,
		struct filesys_obj *dest_dir, const char *dest_leaf, int *err);
  int (*link)(struct filesys_obj *obj, const char *leaf,
	      struct filesys_obj *dest_dir, const char *dest_leaf, int *err);
  int (*unlink)(struct filesys_obj *obj, const char *leaf, int *err);
  int (*rmdir)(struct filesys_obj *obj, const char *leaf, int *err);
  int (*socket_bind)(struct filesys_obj *obj, const char *leaf,
		     int sock_fd, int *err);
  
  /* Symlinks only: */
  int (*readlink)(struct filesys_obj *obj, region_t r, seqf_t *result, int *err);

  /* This is just here so that when I add fields, the compiler will give
     a warning when I forget to update all the vtables.  Unfortunately it
     doesn't give a warning when fields are missing. */
  int sentinel;
};

/* Concrete types */

struct real_dir {
  struct filesys_obj hdr;
  struct stat stat;
  struct file_desc *fd; /* May be 0 if we failed to open the directory */
};

struct real_file {
  struct filesys_obj hdr;
  struct stat stat;
  struct file_desc *dir_fd;
  char *leaf;
};

struct real_symlink {
  struct filesys_obj hdr;
  struct stat stat; /* Result of lstat on symlink */
  struct file_desc *dir_fd;
  char *leaf;
};

extern struct filesys_obj_vtable real_dir_vtable;
extern struct filesys_obj_vtable real_file_vtable;
extern struct filesys_obj_vtable real_symlink_vtable;

void filesys_obj_free(struct filesys_obj *obj);

int dummy_utimes(struct filesys_obj *obj, const struct timeval *atime,
		 const struct timeval *mtime, int *err);
int dummy_chmod(struct filesys_obj *obj, int mode, int *err);
struct filesys_obj *dummy_traverse(struct filesys_obj *obj, const char *leaf);
int dummy_list(struct filesys_obj *obj, region_t r, seqt_t *result, int *err);
int dummy_create_file(struct filesys_obj *obj, const char *leaf,
		      int flags, int mode, int *err);
int dummy_mkdir(struct filesys_obj *obj, const char *leaf, int mode, int *err);
int dummy_symlink(struct filesys_obj *obj, const char *leaf,
		  const char *oldpath, int *err);
int dummy_rename_or_link(struct filesys_obj *obj, const char *leaf,
			 struct filesys_obj *dest_dir, const char *dest_leaf,
			 int *err);
int dummy_unlink(struct filesys_obj *obj, const char *leaf, int *err);
int dummy_rmdir(struct filesys_obj *obj, const char *leaf, int *err);
int dummy_socket_bind(struct filesys_obj *obj, const char *leaf, int sock_fd, int *err);
int dummy_readlink(struct filesys_obj *obj, region_t r, seqf_t *result, int *err);
int dummy_open(struct filesys_obj *obj, int flags, int *err);
int dummy_connect(struct filesys_obj *obj, int sock_fd, int *err);


#endif
