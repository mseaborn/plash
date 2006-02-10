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

#include "region.h"
#include "filesysobj-fab.h"
#include "cap-protocol.h"


struct list *assoc(struct list *list, const char *name)
{
  while(list) {
    if(!strcmp(name, list->name)) return list;
    list = list->next;
  }
  return 0; /* Not found */
}


int refuse_chmod(struct filesys_obj *obj, int mode, int *err)
{
  *err = EACCES;
  return -1;
}

int refuse_utimes(struct filesys_obj *obj, const struct timeval *atime,
		  const struct timeval *mtime, int *err)
{
  *err = EACCES;
  return -1;
}

int refuse_socket_connect(struct filesys_obj *obj, int sock_fd, int *err)
{
  *err = EACCES;
  return -1;
}

int refuse_create_file(struct filesys_obj *obj, const char *leaf,
		       int flags, int mode, int *err)
{
  *err = EACCES;
  return -1;
}
int refuse_mkdir(struct filesys_obj *obj, const char *leaf,
		 int mode, int *err)
{
  *err = EACCES;
  return -1;
}
int refuse_symlink(struct filesys_obj *obj, const char *leaf,
		   const char *oldpath, int *err)
{
  *err = EACCES;
  return -1;
}
int refuse_rename_or_link
  (struct filesys_obj *obj, const char *leaf,
   struct filesys_obj *dest_dir, const char *dest_leaf, int *err)
{
  *err = EACCES;
  return -1;
}
int refuse_unlink(struct filesys_obj *obj, const char *leaf, int *err)
{
  *err = EACCES;
  return -1;
}
int refuse_rmdir(struct filesys_obj *obj, const char *leaf, int *err)
{
  *err = EACCES;
  return -1;
}
int refuse_socket_bind(struct filesys_obj *obj, const char *leaf,
		       int sock_fd, int *err)
{
  *err = EACCES;
  return -1;
}

void fab_dir_free(struct filesys_obj *obj1)
{
  struct fab_dir *obj = (void *) obj1;
  struct obj_list *node = obj->entries;
  while(node) {
    struct obj_list *next = node->next;
    free(node->name);
    filesys_obj_free(node->obj);
    free(node);
    node = next;
  }
}

/* Device number reported by stat.  FIXME: avoid clashes? */
#define STAT_DEVICE 100

int fab_dir_stat(struct filesys_obj *obj, struct stat *st, int *err)
{
  struct fab_dir *dir = (void *) obj;
  st->st_dev = STAT_DEVICE;
  st->st_ino = dir->inode;
  st->st_mode = S_IFDIR | 0777;
  st->st_nlink = 0; /* FIXME: this can be used to count the number of child directories */
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

struct filesys_obj *fab_dir_traverse(struct filesys_obj *obj, const char *leaf)
{
  struct fab_dir *dir = (void *) obj;
  struct obj_list *l = (void *) assoc((void *) dir->entries, leaf);
  if(l) {
    l->obj->refcount++;
    return l->obj;
  }
  else return 0;
}

int fab_dir_list(struct filesys_obj *obj, region_t r, seqt_t *result, int *err)
{
  struct fab_dir *dir = (void *) obj;
  struct obj_list *l;
  seqt_t got = seqt_empty;
  int count = 0;
  for(l = dir->entries; l; l = l->next) {
    /* FIXME: wasteful of space */
    int len = strlen(l->name);
    char *str = region_alloc(r, len);
    memcpy(str, l->name, len);
    got = cat5(r, got,
	       mk_int(r, 0), /* inode: FIXME */
	       mk_int(r, 0), /* d_type: FIXME */
	       mk_int(r, len),
	       mk_leaf2(r, str, len));
    count++;
  }
  *result = got;
  return count;
}

struct filesys_obj_vtable fab_dir_vtable = {
  /* .free = */ fab_dir_free,
  /* .cap_invoke = */ local_obj_invoke,
  /* .cap_call = */ marshall_cap_call,
  /* .single_use = */ 0,
  /* .type = */ objt_dir,
  /* .stat = */ fab_dir_stat,
  /* .utimes = */ refuse_utimes,
  /* .chmod = */ refuse_chmod,
  /* .open = */ dummy_open,
  /* .socket_connect = */ dummy_socket_connect,
  /* .traverse = */ fab_dir_traverse,
  /* .list = */ fab_dir_list,
  /* .create_file = */ refuse_create_file,
  /* .mkdir = */ refuse_mkdir,
  /* .symlink = */ refuse_symlink,
  /* .rename = */ refuse_rename_or_link,
  /* .link = */ refuse_rename_or_link,
  /* .unlink = */ refuse_unlink,
  /* .rmdir = */ refuse_rmdir,
  /* .socket_bind = */ refuse_socket_bind,
  /* .readlink = */ dummy_readlink,
  1
};


void fab_symlink_free(struct filesys_obj *obj1)
{
  struct fab_symlink *obj = (void *) obj1;
  free((char *) obj->dest.data);
}

int fab_symlink_stat(struct filesys_obj *obj, struct stat *st, int *err)
{
  struct fab_symlink *sym = (void *) obj;
  st->st_dev = STAT_DEVICE;
  st->st_ino = sym->inode;
  st->st_mode = S_IFLNK | 0777;
  st->st_nlink = 1; /* Not necessarily accurate */
  st->st_uid = 0;
  st->st_gid = 0;
  st->st_rdev = 0;
  st->st_size = sym->dest.size;
  st->st_blksize = 1024;
  st->st_blocks = 0;
  st->st_atime = 0;
  st->st_mtime = 0;
  st->st_ctime = 0;
  return 0;
}

int fab_symlink_readlink(struct filesys_obj *obj, region_t r, seqf_t *result, int *err)
{
  struct fab_symlink *sym = (void *) obj;
  *result = region_dup_seqf(r, sym->dest);
  return 0;
}

struct filesys_obj_vtable fab_symlink_vtable = {
  /* .free = */ fab_symlink_free,
  /* .cap_invoke = */ local_obj_invoke,
  /* .cap_call = */ marshall_cap_call,
  /* .single_use = */ 0,
  /* .type = */ objt_symlink,
  /* .stat = */ fab_symlink_stat,
  /* .utimes = */ refuse_utimes,
  /* .chmod = */ dummy_chmod,
  /* .open = */ dummy_open,
  /* .socket_connect = */ dummy_socket_connect,
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
  /* .readlink = */ fab_symlink_readlink,
  1
};



void s_fab_dir_free(struct filesys_obj *obj1)
{
  struct s_fab_dir *obj = (void *) obj1;
  struct slot_list *node = obj->entries;
  while(node) {
    struct slot_list *next = node->next;
    free(node->name);
    filesys_slot_free(node->slot);
    free(node);
    node = next;
  }
}

int s_fab_dir_stat(struct filesys_obj *obj, struct stat *st, int *err)
{
  struct s_fab_dir *dir = (void *) obj;
  st->st_dev = STAT_DEVICE;
  st->st_ino = dir->inode;
  st->st_mode = S_IFDIR | 0777;
  st->st_nlink = 0; /* FIXME: this can be used to count the number of child directories */
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

struct filesys_obj *s_fab_dir_traverse(struct filesys_obj *obj, const char *leaf)
{
  struct s_fab_dir *dir = (void *) obj;
  struct slot_list *l = (void *) assoc((void *) dir->entries, leaf);
  if(l) return l->slot->vtable->get(l->slot);
  else return 0;
}

int s_fab_dir_list(struct filesys_obj *obj, region_t r, seqt_t *result, int *err)
{
  struct s_fab_dir *dir = (void *) obj;
  struct slot_list *l;
  seqt_t got = seqt_empty;
  int count = 0;
  for(l = dir->entries; l; l = l->next) {
    struct filesys_obj *obj = l->slot->vtable->get(l->slot);
    if(obj) {
      /* FIXME: wasteful of space */
      int len = strlen(l->name);
      char *str = region_alloc(r, len);
      memcpy(str, l->name, len);
      got = cat5(r, got,
		 mk_int(r, 0), /* inode: FIXME */
		 mk_int(r, 0), /* d_type: FIXME */
		 mk_int(r, len),
		 mk_leaf2(r, str, len));
      filesys_obj_free(obj);
    }
    count++;
  }
  *result = got;
  return count;
}

int s_fab_dir_create_file(struct filesys_obj *obj, const char *leaf,
			  int flags, int mode, int *err)
{
  struct s_fab_dir *dir = (void *) obj;
  struct slot_list *l = (void *) assoc((void *) dir->entries, leaf);
  if(l) {
    return l->slot->vtable->slot_create_file(l->slot, flags, mode, err);
  }
  else {
    *err = EACCES;
    return -1;
  }
}

int s_fab_dir_mkdir(struct filesys_obj *obj, const char *leaf,
		    int mode, int *err)
{
  struct s_fab_dir *dir = (void *) obj;
  struct slot_list *l = (void *) assoc((void *) dir->entries, leaf);
  if(l) {
    return l->slot->vtable->slot_mkdir(l->slot, mode, err);
  }
  else {
    *err = EACCES;
    return -1;
  }
}

int s_fab_dir_symlink(struct filesys_obj *obj, const char *leaf,
		      const char *oldpath, int *err)
{
  struct s_fab_dir *dir = (void *) obj;
  struct slot_list *l = (void *) assoc((void *) dir->entries, leaf);
  if(l) {
    return l->slot->vtable->slot_symlink(l->slot, oldpath, err);
  }
  else {
    *err = EACCES;
    return -1;
  }
}

int s_fab_dir_unlink(struct filesys_obj *obj, const char *leaf, int *err)
{
  struct s_fab_dir *dir = (void *) obj;
  struct slot_list *l = (void *) assoc((void *) dir->entries, leaf);
  if(l) {
    return l->slot->vtable->slot_unlink(l->slot, err);
  }
  else {
    *err = EACCES;
    return -1;
  }
}

int s_fab_dir_rmdir(struct filesys_obj *obj, const char *leaf, int *err)
{
  struct s_fab_dir *dir = (void *) obj;
  struct slot_list *l = (void *) assoc((void *) dir->entries, leaf);
  if(l) {
    return l->slot->vtable->slot_rmdir(l->slot, err);
  }
  else {
    *err = EACCES;
    return -1;
  }
}

int s_fab_dir_socket_bind(struct filesys_obj *obj, const char *leaf,
			  int sock_fd, int *err)
{
  struct s_fab_dir *dir = (void *) obj;
  struct slot_list *l = (void *) assoc((void *) dir->entries, leaf);
  if(l) {
    return l->slot->vtable->slot_socket_bind(l->slot, sock_fd, err);
  }
  else {
    *err = EACCES;
    return -1;
  }
}

struct filesys_obj_vtable s_fab_dir_vtable = {
  /* .free = */ s_fab_dir_free,
  /* .cap_invoke = */ local_obj_invoke,
  /* .cap_call = */ marshall_cap_call,
  /* .single_use = */ 0,
  /* .type = */ objt_dir,
  /* .stat = */ s_fab_dir_stat,
  /* .utimes = */ refuse_utimes,
  /* .chmod = */ refuse_chmod,
  /* .open = */ dummy_open,
  /* .socket_connect = */ dummy_socket_connect,
  /* .traverse = */ s_fab_dir_traverse,
  /* .list = */ s_fab_dir_list,
  /* .create_file = */ s_fab_dir_create_file,
  /* .mkdir = */ s_fab_dir_mkdir,
  /* .symlink = */ s_fab_dir_symlink,
  /* .rename = */ refuse_rename_or_link,
  /* .link = */ refuse_rename_or_link,
  /* .unlink = */ s_fab_dir_unlink,
  /* .rmdir = */ s_fab_dir_rmdir,
  /* .socket_bind = */ s_fab_dir_socket_bind,
  /* .readlink = */ dummy_readlink,
  1
};
