/* Copyright (C) 2004 Mark Seaborn

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

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "region.h"
#include "filesysobj.h"
#include "cap-utils.h"
#include "marshal.h"


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

#ifdef GC_DEBUG
static struct filesys_obj obj_list =
  { .refcount = 0, .vtable = NULL, .next = &obj_list, .prev = &obj_list };

void *filesys_obj_make(int size, const struct filesys_obj_vtable *vtable)
{
  struct filesys_obj *obj = amalloc(size);
  obj->refcount = 1;
  obj->vtable = vtable;

  /* Insert into list. */
  obj->prev = obj_list.prev;
  obj->next = &obj_list;
  obj_list.prev->next = obj;
  obj_list.prev = obj;
  return obj;
}
#endif

DECLARE_VTABLE(invalid_vtable);

void filesys_obj_free(struct filesys_obj *obj)
{
  filesys_obj_check(obj);
  obj->refcount--;
  if(obj->refcount <= 0) {
    obj->vtable->free(obj);

#ifdef GC_DEBUG
    /* Remove from list. */
    obj->prev->next = obj->next;
    obj->next->prev = obj->prev;
    obj->next = NULL;
    obj->prev = NULL;
#endif

    /* Ensures that using the pointer will fail but give a useful error. */
    obj->vtable = &invalid_vtable;

    free(obj);
  }
}

void filesys_obj_check(struct filesys_obj *obj)
{
  assert(obj);
  assert(obj->refcount > 0);
  assert(obj->vtable->sentinel == 1);
}

#ifdef GC_DEBUG

void filesys_obj_mark(struct filesys_obj *obj)
{
  if(obj->mark_count++ == 0) {
    if(obj->vtable->mark) { obj->vtable->mark(obj); }
  }
}

void gc_init(void)
{
  struct filesys_obj *n;
  for(n = obj_list.next; n != &obj_list; n = n->next) {
    n->mark_count = 0;
  }
}

void gc_check(void)
{
  struct filesys_obj *n;
  for(n = obj_list.next; n != &obj_list; n = n->next) {
    printf("obj: rc=%9i marks=%9i type=%s\n", n->refcount, n->mark_count, n->vtable->vtable_name);
  }
}

#endif

void caps_free(cap_seq_t c)
{
  int i;
  for(i = 0; i < c.size; i++) filesys_obj_free(c.caps[i]);
}

cap_seq_t cap_seq_append(region_t r, cap_seq_t seq1, cap_seq_t seq2)
{
  cap_seq_t seq;
  cap_t *array = region_alloc(r, (seq1.size + seq2.size) * sizeof(cap_t));
  memcpy(array, seq1.caps, seq1.size * sizeof(cap_t));
  memcpy(array + seq1.size, seq2.caps, seq2.size * sizeof(cap_t));
  seq.caps = array;
  seq.size = seq1.size + seq2.size;
  return seq;
}

void pl_args_free(const struct cap_args *args)
{
  caps_free(args->caps);
  close_fds(args->fds);
}

void generic_free(struct filesys_obj *obj)
{
}


void marshal_cap_call(struct filesys_obj *obj, region_t r,
		      struct cap_args args, struct cap_args *result)
{
  seqf_t data = flatten_reuse(r, args.data);
  int method_id;
  int ok = 1;
  m_int(&ok, &data, &method_id);
  if(ok) switch(method_id) {
  case METHOD_FSOBJ_TYPE:
  {
    if(unpack_fsobj_type(r, args) < 0) goto bad_msg;
    *result = pack_fsobj_type_result(r, obj->vtable->type(obj));
    return;
  }
  case METHOD_FSOBJ_STAT:
  {
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
      result->data = cat4(r, mk_int(r, METHOD_R_FSOBJ_STAT),
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
    goto bad_msg;
  }
  case METHOD_FSOBJ_UTIMES:
  {
    int atime_sec, atime_usec;
    int mtime_sec, mtime_usec;
    m_int(&ok, &data, &atime_sec);
    m_int(&ok, &data, &atime_usec);
    m_int(&ok, &data, &mtime_sec);
    m_int(&ok, &data, &mtime_usec);
    if(ok && args.caps.size == 0 && args.fds.count == 0) {
      int err;
      struct timeval atime, mtime;
      atime.tv_sec = atime_sec;
      atime.tv_usec = atime_usec;
      mtime.tv_sec = mtime_sec;
      mtime.tv_usec = mtime_usec;
      if(obj->vtable->utimes(obj, &atime, &mtime, &err) < 0) {
	*result = pack_fail(r, err);
	return;
      }
      result->caps = caps_empty;
      result->fds = fds_empty;
      result->data = mk_int(r, METHOD_OKAY);
      return;
    }
    goto bad_msg;
  }
  case METHOD_FSOBJ_CHMOD:
  {
    int mode;
    int err;
    if(unpack_fsobj_chmod(r, args, &mode) < 0) goto bad_msg;
    if(obj->vtable->chmod(obj, mode, &err) < 0) {
      *result = pack_fail(r, err);
      return;
    }
    *result = pack_fsobj_chmod_result(r);
    return;
  }
  case METHOD_FILE_OPEN:
  {
    int flags;
    int fd;
    int err;
    if(unpack_file_open(r, args, &flags) < 0) goto bad_msg;
    fd = obj->vtable->open(obj, flags, &err);
    if(fd < 0) {
      *result = pack_fail(r, err);
      return;
    }
    *result = pack_file_open_result(r, fd);
    return;
  }
  case METHOD_FILE_SOCKET_CONNECT:
  {
    int sock;
    int err;
    if(unpack_file_socket_connect(r, args, &sock) < 0) goto bad_msg;
    if(obj->vtable->socket_connect(obj, sock, &err) < 0) {
      *result = pack_fail(r, err);
      close(sock);
      return;
    }
    *result = pack_file_socket_connect_result(r);
    close(sock);
    return;
  }
  case METHOD_DIR_TRAVERSE:
  {
    seqf_t leaf;
    cap_t x;
    if(unpack_dir_traverse(r, args, &leaf) < 0) goto bad_msg;
    x = obj->vtable->traverse(obj, region_strdup_seqf(r, leaf));
    if(!x) {
      *result = pack_fail(r, ENOENT);
      return;
    }
    *result = pack_dir_traverse_result(r, x);
    return;
  }
  case METHOD_DIR_LIST:
  {
    int count;
    seqt_t data;
    int err;
    if(unpack_dir_list(r, args) < 0) goto bad_msg;
    count = obj->vtable->list(obj, r, &data, &err);
    if(count < 0) {
      *result = pack_fail(r, err);
      return;
    }
    *result = pack_dir_list_result(r, count, data);
    return;
  }
  case METHOD_DIR_CREATE_FILE:
  {
    int flags;
    int mode;
    seqf_t leaf;
    int fd;
    int err;
    if(unpack_dir_create_file(r, args, &flags, &mode, &leaf) < 0) goto bad_msg;
    fd = obj->vtable->create_file(obj, region_strdup_seqf(r, leaf),
				  flags, mode, &err);
    if(fd < 0) {
      *result = pack_fail(r, err);
      return;
    }
    *result = pack_dir_create_file_result(r, fd);
    return;
  }
  case METHOD_DIR_MKDIR:
  {
    int mode;
    seqf_t leaf;
    int err;
    if(unpack_dir_mkdir(r, args, &mode, &leaf) < 0) goto bad_msg;
    if(obj->vtable->mkdir(obj, region_strdup_seqf(r, leaf), mode, &err) < 0) {
      *result = pack_fail(r, err);
      return;
    }
    *result = pack_dir_mkdir_result(r);
    return;
  }
  case METHOD_DIR_SYMLINK:
  {
    seqf_t leaf;
    seqf_t dest_path;
    int err;
    if(unpack_dir_symlink(r, args, &leaf, &dest_path) < 0) goto bad_msg;
    if(obj->vtable->symlink(obj, region_strdup_seqf(r, leaf),
			    region_strdup_seqf(r, dest_path), &err) < 0) {
      *result = pack_fail(r, err);
      return;
    }
    *result = pack_dir_symlink_result(r);
    return;
  }
  case METHOD_DIR_RENAME:
  {
    seqf_t leaf;
    cap_t dest_dir;
    seqf_t dest_leaf;
    int err;
    if(unpack_dir_rename(r, args, &leaf, &dest_dir, &dest_leaf) < 0) goto bad_msg;
    if(obj->vtable->rename(obj, region_strdup_seqf(r, leaf),
			   dest_dir, region_strdup_seqf(r, dest_leaf), &err) < 0) {
      *result = pack_fail(r, err);
      filesys_obj_free(dest_dir);
      return;
    }
    *result = pack_dir_rename_result(r);
    filesys_obj_free(dest_dir);
    return;
  }
  case METHOD_DIR_LINK:
  {
    seqf_t leaf;
    cap_t dest_dir;
    seqf_t dest_leaf;
    int err;
    if(unpack_dir_link(r, args, &leaf, &dest_dir, &dest_leaf) < 0) goto bad_msg;
    if(obj->vtable->link(obj, region_strdup_seqf(r, leaf),
			 dest_dir, region_strdup_seqf(r, dest_leaf), &err) < 0) {
      *result = pack_fail(r, err);
      filesys_obj_free(dest_dir);
      return;
    }
    *result = pack_dir_link_result(r);
    filesys_obj_free(dest_dir);
    return;
  }
  case METHOD_DIR_UNLINK:
  {
    seqf_t leaf;
    int err;
    if(unpack_dir_unlink(r, args, &leaf) < 0) goto bad_msg;
    if(obj->vtable->unlink(obj, region_strdup_seqf(r, leaf), &err) < 0) {
      *result = pack_fail(r, err);
      return;
    }
    *result = pack_dir_unlink_result(r);
    return;
  }
  case METHOD_DIR_RMDIR:
  {
    seqf_t leaf;
    int err;
    if(unpack_dir_rmdir(r, args, &leaf) < 0) goto bad_msg;
    if(obj->vtable->rmdir(obj, region_strdup_seqf(r, leaf), &err) < 0) {
      *result = pack_fail(r, err);
      return;
    }
    *result = pack_dir_rmdir_result(r);
    return;
  }
  case METHOD_DIR_SOCKET_BIND:
  {
    seqf_t leaf;
    int sock;
    int err;
    if(unpack_dir_socket_bind(r, args, &leaf, &sock) < 0) goto bad_msg;
    if(obj->vtable->socket_bind(obj, region_strdup_seqf(r, leaf), sock, &err) < 0) {
      *result = pack_fail(r, err);
      close(sock);
      return;
    }
    *result = pack_dir_socket_bind_result(r);
    close(sock);
    return;
  }
  case METHOD_SYMLINK_READLINK:
  {
    seqf_t data;
    int err;
    if(unpack_symlink_readlink(r, args) < 0) goto bad_msg;
    if(obj->vtable->readlink(obj, r, &data, &err) < 0) {
      *result = pack_fail(r, err);
      return;
    }
    *result = pack_symlink_readlink_result(r, mk_leaf(r, data));
    return;
  }
  case METHOD_MAKE_CONN:
  {
    int import_count;
    m_int(&ok, &data, &import_count);
    m_end(&ok, &data);
    if(ok && args.fds.count == 0) {
      cap_t *import;
      int fd = obj->vtable->make_conn(obj, r, args.caps, import_count, &import);
      caps_free(args.caps);
      if(fd < 0) {
	*result = cap_args_make(mk_int(r, METHOD_FAIL),
				caps_empty, fds_empty);
      }
      else {
	*result = cap_args_make(mk_int(r, METHOD_R_MAKE_CONN),
				cap_seq_make(import, import_count),
				mk_fds1(r, fd));
      }
      return;
    }
    goto bad_msg;
  }
  default:
    caps_free(args.caps);
    close_fds(args.fds);
    result->data = mk_string(r, "RMth");
    result->caps = caps_empty;
    result->fds = fds_empty;
    return;
  }
bad_msg:
  caps_free(args.caps);
  close_fds(args.fds);
  result->data = mk_string(r, "RMsg");
  result->caps = caps_empty;
  result->fds = fds_empty;
}

int marshal_type(struct filesys_obj *obj)
{
  int type;
  region_t r = region_make();
  struct cap_args result;
  cap_call(obj, r, pack_fsobj_type(r), &result);
  if(unpack_fsobj_type_result(r, result, &type) < 0) {
    type = 0;
    caps_free(result.caps);
    close_fds(result.fds);
  }
  region_free(r);
  return type;
}

int marshal_stat(struct filesys_obj *obj, struct stat *buf, int *err)
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
    m_int_const(&ok, &msg, METHOD_R_FSOBJ_STAT);
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
    m_int_const(&ok, &msg, METHOD_FAIL);
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

int marshal_utimes(struct filesys_obj *obj, const struct timeval *atime,
		   const struct timeval *mtime, int *err)
{
  int rc;
  region_t r = region_make();
  struct cap_args result;
  cap_call(obj, r,
	   cap_args_make
	     (cat5(r, mk_int(r, METHOD_FSOBJ_UTIMES),
		   mk_int(r, atime->tv_sec), mk_int(r, atime->tv_usec),
		   mk_int(r, mtime->tv_sec), mk_int(r, mtime->tv_usec)),
	      caps_empty,
	      fds_empty),
	   &result);
  if(expect_ok(result) >= 0) { rc = 0; }
  else if(unpack_fail(r, result, err) >= 0) { rc = -1; }
  else {
    *err = ENOSYS;
    rc = -1;
    caps_free(result.caps);
    close_fds(result.fds);
  }
  region_free(r);
  return rc;
}

int marshal_chmod(struct filesys_obj *obj, int mode, int *err)
{
  int rc;
  region_t r = region_make();
  struct cap_args result;
  cap_call(obj, r, pack_fsobj_chmod(r, mode), &result);
  if(unpack_fsobj_chmod_result(r, result) >= 0) { rc = 0; }
  else if(unpack_fail(r, result, err) >= 0) { rc = -1; }
  else {
    *err = ENOSYS;
    rc = -1;
    caps_free(result.caps);
    close_fds(result.fds);
  }
  region_free(r);
  return rc;
}

int marshal_open(struct filesys_obj *obj, int flags, int *err)
{
  int fd;
  region_t r = region_make();
  struct cap_args result;
  cap_call(obj, r, pack_file_open(r, flags), &result);
  if(unpack_file_open_result(r, result, &fd) >= 0) {}
  else if(unpack_fail(r, result, err) >= 0) { fd = -1; }
  else {
    *err = ENOSYS;
    fd = -1;
    caps_free(result.caps);
    close_fds(result.fds);
  }
  region_free(r);
  return fd;
}

/* FIXME: change ownership & remove dup */
int marshal_socket_connect(struct filesys_obj *obj, int sock_fd, int *err)
{
  int rc;
  region_t r = region_make();
  struct cap_args result;
  cap_call(obj, r, pack_file_socket_connect(r, dup(sock_fd)), &result);
  if(unpack_file_socket_connect_result(r, result) >= 0) { rc = 0; }
  else if(unpack_fail(r, result, err) >= 0) { rc = -1; }
  else {
    *err = ENOSYS;
    rc = -1;
    caps_free(result.caps);
    close_fds(result.fds);
  }
  region_free(r);
  return rc;
}

struct filesys_obj *marshal_traverse(struct filesys_obj *obj, const char *leaf)
{
  cap_t x;
  region_t r = region_make();
  struct cap_args result;
  cap_call(obj, r, pack_dir_traverse(r, mk_string(r, leaf)), &result);
  if(unpack_dir_traverse_result(r, result, &x) < 0) {
    x = 0;
    caps_free(result.caps);
    close_fds(result.fds);
  }
  region_free(r);
  return x;
}

int marshal_list(struct filesys_obj *obj, region_t r, seqt_t *result_data, int *err)
{
  int count;
  seqf_t result_data1;
  struct cap_args result;
  cap_call(obj, r, pack_dir_list(r), &result);
  if(unpack_dir_list_result(r, result, &count, &result_data1) >= 0) {
    *result_data = mk_leaf(r, result_data1);
  }
  else if(unpack_fail(r, result, err) >= 0) { count = -1; }
  else {
    *err = ENOSYS;
    count = -1;
    caps_free(result.caps);
    close_fds(result.fds);
  }
  return count;
}

int marshal_create_file(struct filesys_obj *obj, const char *leaf,
		      int flags, int mode, int *err)
{
  int fd;
  region_t r = region_make();
  struct cap_args result;
  cap_call(obj, r, pack_dir_create_file(r, flags, mode,
					mk_string(r, leaf)), &result);
  if(unpack_dir_create_file_result(r, result, &fd) >= 0) {}
  else if(unpack_fail(r, result, err) >= 0) { fd = -1; }
  else {
    *err = ENOSYS;
    fd = -1;
    caps_free(result.caps);
    close_fds(result.fds);
  }
  region_free(r);
  return fd;
}

int marshal_mkdir(struct filesys_obj *obj, const char *leaf, int mode, int *err)
{
  int rc;
  region_t r = region_make();
  struct cap_args result;
  cap_call(obj, r, pack_dir_mkdir(r, mode, mk_string(r, leaf)), &result);
  if(unpack_dir_mkdir_result(r, result) >= 0) { rc = 0; }
  else if(unpack_fail(r, result, err) >= 0) { rc = -1; }
  else {
    *err = ENOSYS;
    rc = -1;
    caps_free(result.caps);
    close_fds(result.fds);
  }
  region_free(r);
  return rc;
}

int marshal_symlink(struct filesys_obj *obj, const char *leaf,
		    const char *oldpath, int *err)
{
  int rc;
  region_t r = region_make();
  struct cap_args result;
  cap_call(obj, r, pack_dir_symlink(r, mk_string(r, leaf),
				    mk_string(r, oldpath)), &result);
  if(unpack_dir_symlink_result(r, result) >= 0) { rc = 0; }
  else if(unpack_fail(r, result, err) >= 0) { rc = -1; }
  else {
    *err = ENOSYS;
    rc = -1;
    caps_free(result.caps);
    close_fds(result.fds);
  }
  region_free(r);
  return rc;
}

int marshal_rename(struct filesys_obj *obj, const char *leaf,
		   struct filesys_obj *dest_dir, const char *dest_leaf,
		   int *err)
{
  int rc;
  region_t r = region_make();
  struct cap_args result;
  cap_call(obj, r, pack_dir_rename(r, mk_string(r, leaf), inc_ref(dest_dir),
				   mk_string(r, dest_leaf)), &result);
  if(unpack_dir_rename_result(r, result) >= 0) { rc = 0; }
  else if(unpack_fail(r, result, err) >= 0) { rc = -1; }
  else {
    *err = ENOSYS;
    rc = -1;
    caps_free(result.caps);
    close_fds(result.fds);
  }
  region_free(r);
  return rc;
}

int marshal_link(struct filesys_obj *obj, const char *leaf,
		 struct filesys_obj *dest_dir, const char *dest_leaf,
		 int *err)
{
  int rc;
  region_t r = region_make();
  struct cap_args result;
  cap_call(obj, r, pack_dir_link(r, mk_string(r, leaf), inc_ref(dest_dir),
				 mk_string(r, dest_leaf)), &result);
  if(unpack_dir_link_result(r, result) >= 0) { rc = 0; }
  else if(unpack_fail(r, result, err) >= 0) { rc = -1; }
  else {
    *err = ENOSYS;
    rc = -1;
    caps_free(result.caps);
    close_fds(result.fds);
  }
  region_free(r);
  return rc;
}

int marshal_unlink(struct filesys_obj *obj, const char *leaf, int *err)
{
  int rc;
  region_t r = region_make();
  struct cap_args result;
  cap_call(obj, r, pack_dir_unlink(r, mk_string(r, leaf)), &result);
  if(unpack_dir_unlink_result(r, result) >= 0) { rc = 0; }
  else if(unpack_fail(r, result, err) >= 0) { rc = -1; }
  else {
    *err = ENOSYS;
    rc = -1;
    caps_free(result.caps);
    close_fds(result.fds);
  }
  region_free(r);
  return rc;
}

int marshal_rmdir(struct filesys_obj *obj, const char *leaf, int *err)
{
  int rc;
  region_t r = region_make();
  struct cap_args result;
  cap_call(obj, r, pack_dir_rmdir(r, mk_string(r, leaf)), &result);
  if(unpack_dir_rmdir_result(r, result) >= 0) { rc = 0; }
  else if(unpack_fail(r, result, err) >= 0) { rc = -1; }
  else {
    *err = ENOSYS;
    rc = -1;
    caps_free(result.caps);
    close_fds(result.fds);
  }
  region_free(r);
  return rc;
}

int marshal_socket_bind(struct filesys_obj *obj, const char *leaf, int sock_fd, int *err)
{
  int rc;
  region_t r = region_make();
  struct cap_args result;
  cap_call(obj, r, pack_dir_socket_bind(r, mk_string(r, leaf),
					dup(sock_fd)), &result);
  if(unpack_dir_socket_bind_result(r, result) >= 0) { rc = 0; }
  else if(unpack_fail(r, result, err) >= 0) { rc = -1; }
  else {
    *err = ENOSYS;
    rc = -1;
    caps_free(result.caps);
    close_fds(result.fds);
  }
  region_free(r);
  return rc;
}

int marshal_readlink(struct filesys_obj *obj, region_t r, seqf_t *dest, int *err)
{
  int rc;
  struct cap_args result;
  cap_call(obj, r, pack_symlink_readlink(r), &result);
  if(unpack_symlink_readlink_result(r, result, dest) >= 0) { rc = 0; }
  else if(unpack_fail(r, result, err) >= 0) { rc = -1; }
  else {
    *err = ENOSYS;
    rc = -1;
    caps_free(result.caps);
    close_fds(result.fds);
  }
  return rc;
}

int marshal_make_conn(struct filesys_obj *obj, region_t r, cap_seq_t export,
		      int import_count, cap_t **import)
{
  struct cap_args result;
  int i;
  for(i = 0; i < export.size; i++) inc_ref(export.caps[i]);
  cap_call(obj, r,
	   cap_args_make(cat2(r, mk_int(r, METHOD_MAKE_CONN),
			      mk_int(r, import_count)),
			 export,
			 fds_empty),
	   &result);
  {
    seqf_t msg = flatten_reuse(r, result.data);
    int ok = 1;
    m_int_const(&ok, &msg, METHOD_R_MAKE_CONN);
    m_end(&ok, &msg);
    if(ok && result.caps.size == import_count && result.fds.count == 1) {
      *import = (cap_t *) result.caps.caps;
      return result.fds.fds[0];
    }
  }
  caps_free(result.caps);
  close_fds(result.fds);
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

struct filesys_obj *dummy_slot_get(struct filesys_obj *obj)
{
  return 0;
}
int dummy_slot_create_file(struct filesys_obj *obj, int flags, int mode,
			   int *err)
{
  *err = ENOSYS;
  return -1;
}
int dummy_slot_mkdir(struct filesys_obj *slot, int mode, int *err)
{
  *err = ENOSYS;
  return -1;
}
int dummy_slot_symlink(struct filesys_obj *slot, const char *oldpath, int *err)
{
  *err = ENOSYS;
  return -1;
}
int dummy_slot_unlink(struct filesys_obj *slot, int *err)
{
  *err = ENOSYS;
  return -1;
}
int dummy_slot_rmdir(struct filesys_obj *slot, int *err)
{
  *err = ENOSYS;
  return -1;
}
int dummy_slot_socket_bind(struct filesys_obj *slot, int sock_fd, int *err)
{
  *err = ENOSYS;
  return -1;
}

int dummy_make_conn(struct filesys_obj *obj, region_t r, cap_seq_t export,
		    int import_count, cap_t **import)
{
  return -1;
}

int dummy_make_conn2(struct filesys_obj *obj, region_t r, int sock_fd,
		     cap_seq_t export, int import_count, cap_t **import)
{
  close(sock_fd);
  return -1;
}


void invalid_mark(struct filesys_obj *obj)
{
  assert(0);
}

void invalid_cap_invoke(struct filesys_obj *obj, struct cap_args args)
{
  assert(0);
}
void invalid_cap_call(struct filesys_obj *obj, region_t r,
		      struct cap_args args, struct cap_args *result)
{
  assert(0);
}

int invalid_objt(struct filesys_obj *obj) { assert(0); }

int invalid_stat(struct filesys_obj *obj, struct stat *buf, int *err)
{
  assert(0);
}

int invalid_utimes(struct filesys_obj *obj, const struct timeval *atime,
		   const struct timeval *mtime, int *err)
{
  assert(0);
  *err = ENOSYS;
  return -1;
}

int invalid_chmod(struct filesys_obj *obj, int mode, int *err)
{
  assert(0);
  *err = ENOSYS;
  return -1;
}

struct filesys_obj *invalid_traverse(struct filesys_obj *obj, const char *leaf)
{
  assert(0);
  return 0;
}

int invalid_list(struct filesys_obj *obj, region_t r, seqt_t *result, int *err)
{
  assert(0);
  *err = ENOSYS;
  return -1;
}

int invalid_create_file(struct filesys_obj *obj, const char *leaf,
			int flags, int mode, int *err)
{
  assert(0);
  *err = ENOSYS;
  return -1;
}

int invalid_mkdir(struct filesys_obj *obj, const char *leaf, int mode, int *err)
{
  assert(0);
  *err = ENOSYS;
  return -1;
}

int invalid_symlink(struct filesys_obj *obj, const char *leaf,
		    const char *oldpath, int *err)
{
  assert(0);
  *err = ENOSYS;
  return -1;
}

int invalid_rename_or_link(struct filesys_obj *obj, const char *leaf,
			   struct filesys_obj *dest_dir, const char *dest_leaf,
			   int *err)
{
  assert(0);
  *err = ENOSYS;
  return -1;
}

int invalid_unlink(struct filesys_obj *obj, const char *leaf, int *err)
{
  assert(0);
  *err = ENOSYS;
  return -1;
}

int invalid_rmdir(struct filesys_obj *obj, const char *leaf, int *err)
{
  assert(0);
  *err = ENOSYS;
  return -1;
}

int invalid_socket_bind(struct filesys_obj *obj, const char *leaf, int sock_fd, int *err)
{
  *err = ENOSYS;
  return -1;
}

int invalid_readlink(struct filesys_obj *obj, region_t r, seqf_t *result, int *err)
{
  assert(0);
  *err = ENOSYS;
  return -1;
}

int invalid_open(struct filesys_obj *obj, int flags, int *err)
{
  assert(0);
  *err = ENOSYS;
  return -1;
}

int invalid_socket_connect(struct filesys_obj *obj, int sock_fd, int *err)
{
  assert(0);
  *err = ENOSYS;
  return -1;
}

#include "out-vtable-filesysobj.h"
