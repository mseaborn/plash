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

/* To get "struct stat64" defined */
#define _LARGEFILE64_SOURCE

#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "libc-errno.h"
#include "libc-comms.h"
#include "libc-fds.h"
#include "kernel-fd-ops.h"
#include "cap-utils.h"
#include "marshal.h"
#include "marshal-pack.h"


#define log_msg(msg) /* nothing */
#define log_fd(fd, msg) /* nothing */


/* My codes: */
#define TYPE_STAT 1   /* struct stat */
#define TYPE_STAT64 2 /* struct stat64 */


int new_xstat(int vers, const char *pathname, struct stat *buf);
int new_xstat64(int vers, const char *pathname, struct stat64 *buf);
int new_lxstat(int vers, const char *pathname, struct stat *buf);
int new_lxstat64(int vers, const char *pathname, struct stat64 *buf);
int new_fxstat(int vers, int fd, struct stat *buf);
int new_fxstat64(int vers, int fd, struct stat64 *buf);
int new_fxstatat(int vers, int dir_fd, const char *filename,
		 struct stat *buf, int flags);
int new_fxstatat64(int vers, int dir_fd, const char *filename,
		   struct stat64 *buf, int flags);


static void m_stat_info(int *ok, seqf_t *msg, int type, void *buf)
{
  int myst_dev, myst_ino, myst_mode, myst_nlink, myst_uid, myst_gid, myst_rdev,
    myst_size, myst_blksize, myst_blocks, myst_atime, myst_mtime, myst_ctime;
  m_int(ok, msg, &myst_dev);
  m_int(ok, msg, &myst_ino);
  m_int(ok, msg, &myst_mode);
  m_int(ok, msg, &myst_nlink);
  m_int(ok, msg, &myst_uid);
  m_int(ok, msg, &myst_gid);
  m_int(ok, msg, &myst_rdev);
  m_int(ok, msg, &myst_size);
  m_int(ok, msg, &myst_blksize);
  m_int(ok, msg, &myst_blocks);
  m_int(ok, msg, &myst_atime);
  m_int(ok, msg, &myst_mtime);
  m_int(ok, msg, &myst_ctime);
  if(*ok) {
    /* The __pad*, __unused* and __st_ino fields are defined on i386
       but not x86-64.  The kernel_stat.h headers in glibc declare
       their presence via macros such as _HAVE_STAT___UNUSED4.
       However, it's difficult to get at that header, so we use
       memset() to clear those fields instead. */
    
    if(type == TYPE_STAT) {
      struct stat *stat = buf;
      memset(stat, 0, sizeof(struct stat));
      
      stat->st_dev = myst_dev;
      // stat->__pad1 = 0;
      stat->st_ino = myst_ino;
      stat->st_mode = myst_mode;
      stat->st_nlink = myst_nlink;
      stat->st_uid = myst_uid;
      stat->st_gid = myst_gid;
      stat->st_rdev = myst_rdev;
      // stat->__pad2 = 0;
      stat->st_size = myst_size;
      stat->st_blksize = myst_blksize;
      stat->st_blocks = myst_blocks;
      stat->st_atim.tv_sec = myst_atime;
      stat->st_atim.tv_nsec = 0; /* FIXME */
      stat->st_mtim.tv_sec = myst_mtime;
      stat->st_mtim.tv_nsec = 0; /* FIXME */
      stat->st_ctim.tv_sec = myst_ctime;
      stat->st_ctim.tv_nsec = 0; /* FIXME */
      // stat->__unused4 = 0;
      // stat->__unused5 = 0;
    }
    else if(type == TYPE_STAT64) {
      struct stat64 *stat = buf;
      memset(stat, 0, sizeof(struct stat64));
      
      stat->st_dev = myst_dev;
      // stat->__pad1 = 0;
      stat->__st_ino = myst_ino;
      stat->st_mode = myst_mode;
      stat->st_nlink = myst_nlink;
      stat->st_uid = myst_uid;
      stat->st_gid = myst_gid;
      stat->st_rdev = myst_rdev;
      // stat->__pad2 = 0;
      stat->st_size = myst_size;
      stat->st_blksize = myst_blksize;
      stat->st_blocks = myst_blocks;
      stat->st_atim.tv_sec = myst_atime;
      stat->st_atim.tv_nsec = 0; /* FIXME */
      stat->st_mtim.tv_sec = myst_mtime;
      stat->st_mtim.tv_nsec = 0; /* FIXME */
      stat->st_ctim.tv_sec = myst_ctime;
      stat->st_ctim.tv_nsec = 0; /* FIXME */
      stat->st_ino = myst_ino;
    }
    else {
      *ok = 0;
    }
  }
}

/* nofollow=0 for stat, nofollow=1 for lstat. */
int my_statat(int dir_fd, int nofollow, int type, const char *pathname,
	      void *buf)
{
  region_t r = region_make();
  cap_t fs_op_server;
  cap_t dir_obj;
  struct cap_args result;
  int rc = -1;
  plash_libc_lock();
  log_msg(MOD_MSG "stat\n");
  if(!pathname || !buf) {
    __set_errno(EINVAL);
    goto error;
  }
  if(libc_get_fs_op(&fs_op_server) < 0)
    goto error;
  if(fds_get_dir_obj(dir_fd, &dir_obj) < 0)
    goto error;
  cap_call(fs_op_server, r,
	   pl_pack(r, METHOD_FSOP_STAT, "diS", dir_obj, nofollow,
		   seqf_string(pathname)),
	   &result);
  seqf_t reply = flatten_reuse(r, result.data);
  pl_args_free(&result);
  {
    seqf_t msg = reply;
    int ok = 1;
    m_int_const(&ok, &msg, METHOD_R_FSOP_STAT);
    m_stat_info(&ok, &msg, type, buf);
    m_end(&ok, &msg);
    if(ok) {
      rc = 0;
      goto error;
    }
  }
  set_errno_from_reply(reply);
 error:
  plash_libc_unlock();
  region_free(r);
  return rc;
}

int my_fstat(int type, int fd, void *buf)
{
  log_msg(MOD_MSG "fstat\n");
  plash_libc_lock();

  if(0 <= fd && fd < g_fds_size && g_fds[fd].fd_dir_obj) {
    /* Handle directory FDs specially:  send a message. */
    cap_t dir_obj = g_fds[fd].fd_dir_obj;
    region_t r = region_make();
    cap_t fs_op_server;
    struct cap_args result;
    int rc = -1;
    log_fd(fd, "fstat on directory");
    if(!buf) {
      __set_errno(EINVAL);
      goto exit;
    }
    if(libc_get_fs_op(&fs_op_server) < 0)
      goto exit;
    cap_call(fs_op_server, r,
	     cap_args_dc(mk_int(r, METHOD_FSOP_DIR_FSTAT),
			 mk_caps1(r, inc_ref(dir_obj))),
	     &result);
    {
      seqf_t msg = flatten_reuse(r, result.data);
      int ok = 1;
      m_int_const(&ok, &msg, METHOD_OKAY);
      m_stat_info(&ok, &msg, type, buf);
      m_end(&ok, &msg);
      if(ok && result.fds.count == 0 && result.caps.size == 0) {
	rc = 0;
	goto exit;
      }
    }
    pl_args_free(&result);
    set_errno_from_reply(flatten_reuse(r, result.data));
  exit:
    plash_libc_unlock();
    region_free(r);
    return rc;
  }
  else {
    plash_libc_unlock();

    /* Use the normal fstat system call. */
    log_fd(fd, "normal fstat");
    if(type == TYPE_STAT) {
      return kernel_fxstat(_STAT_VER, fd, (struct stat *) buf);
    }
    else if(type == TYPE_STAT64) {
      return kernel_fxstat64(_STAT_VER, fd, (struct stat64 *) buf);
    }
    else {
      /* Don't recognise ABI version requested. */
      __set_errno(ENOSYS);
      return -1;
    }
  }
}


export(new_xstat, __xstat);
export(new_xstat, __GI___xstat);

int new_xstat(int vers, const char *pathname, struct stat *buf)
{
  int type;
  if(vers == _STAT_VER) type = TYPE_STAT;
  else {
#ifdef ENABLE_LOGGING
    plash_init();
    if(libc_debug)
      fprintf(stderr, "libc: __xstat version %i, `%s'\n", vers, pathname);
#endif
    __set_errno(ENOSYS); return -1;
  }
  return my_statat(AT_FDCWD, FALSE /* nofollow */, type, pathname, buf);
}


export(new_xstat64, ___xstat64);
export_versioned_symbol(libc, new_xstat64, __xstat64, GLIBC_2_2);
#if SHLIB_COMPAT(libc, GLIBC_2_1, GLIBC_2_2)
export_compat_symbol(libc, new_xstat64, __xstat64, GLIBC_2_1);
#endif
export(new_xstat64, __GI___xstat64);

int new_xstat64(int vers, const char *pathname, struct stat64 *buf)
{
  int type;
  if(vers == _STAT_VER) type = TYPE_STAT64;
  else {
#ifdef ENABLE_LOGGING
    plash_init();
    if(libc_debug)
      fprintf(stderr, "libc: __xstat64 version %i, `%s'\n", vers, pathname);
#endif
    __set_errno(ENOSYS); return -1;
  }
  return my_statat(AT_FDCWD, FALSE /* nofollow */, type, pathname, buf);
}


export(new_lxstat, __lxstat);
export(new_lxstat, __GI___lxstat);

int new_lxstat(int vers, const char *pathname, struct stat *buf)
{
  int type;
  if(vers == _STAT_VER) type = TYPE_STAT;
  else {
#ifdef ENABLE_LOGGING
    plash_init();
    if(libc_debug)
      fprintf(stderr, "libc: __lxstat version %i, `%s'\n", vers, pathname);
#endif
    __set_errno(ENOSYS); return -1;
  }
  return my_statat(AT_FDCWD, TRUE /* nofollow */, type, pathname, buf);
}


export(new_lxstat64, ___lxstat64);
export_versioned_symbol(libc, new_lxstat64, __lxstat64, GLIBC_2_2);
#if SHLIB_COMPAT(libc, GLIBC_2_1, GLIBC_2_2)
export_compat_symbol(libc, new_lxstat64, __lxstat64, GLIBC_2_1);
#endif
export(new_lxstat64, __GI___lxstat64);

int new_lxstat64(int vers, const char *pathname, struct stat64 *buf)
{
  int type;
  if(vers == _STAT_VER) type = TYPE_STAT64;
  else {
#ifdef ENABLE_LOGGING
    plash_init();
    if(libc_debug)
      fprintf(stderr, "libc: __lxstat64 version %i, `%s'\n", vers, pathname);
#endif
    __set_errno(ENOSYS); return -1;
  }
  return my_statat(AT_FDCWD, TRUE /* nofollow */, type, pathname, buf);
}


export(new_fxstat, __fxstat);
export(new_fxstat, __GI___fxstat);

int new_fxstat(int vers, int fd, struct stat *buf)
{
  int type;
  if(vers == _STAT_VER) type = TYPE_STAT;
  else {
#ifdef ENABLE_LOGGING
    plash_init();
    if(libc_debug)
      fprintf(stderr, "libc: __fxstat version %i, fd %i\n", vers, fd);
#endif
    __set_errno(ENOSYS); return -1;
  }
  return my_fstat(type, fd, buf);
}


export(new_fxstat64, ___fxstat64);
export_versioned_symbol(libc, new_fxstat64, __fxstat64, GLIBC_2_2);
#if SHLIB_COMPAT(libc, GLIBC_2_1, GLIBC_2_2)
export_compat_symbol(libc, new_fxstat64, __fxstat64, GLIBC_2_1);
#endif
export(new_fxstat64, __GI___fxstat64);

int new_fxstat64(int vers, int fd, struct stat64 *buf)
{
  int type;
  if(vers == _STAT_VER) type = TYPE_STAT64;
  else {
#ifdef ENABLE_LOGGING
    plash_init();
    if(libc_debug)
      fprintf(stderr, "libc: __fxstat64 version %i, fd %i\n", vers, fd);
#endif
    __set_errno(ENOSYS); return -1;
  }
  return my_fstat(type, fd, buf);
}


export(new_fxstatat, __fxstatat);
export(new_fxstatat, __GI___fxstatat);

int new_fxstatat(int vers, int dir_fd, const char *filename,
		 struct stat *buf, int flags)
{
  int nofollow = (flags & AT_SYMLINK_NOFOLLOW) ? TRUE : FALSE;
  int type;
  if((flags & ~AT_SYMLINK_NOFOLLOW) != 0) {
    __set_errno(EINVAL);
    return -1;
  }
  if(vers == _STAT_VER)
    type = TYPE_STAT;
  else {
    __set_errno(ENOSYS);
    return -1;
  }
  return my_statat(dir_fd, nofollow, type, filename, buf);
}


export(new_fxstatat64, __fxstatat64);
export(new_fxstatat64, __GI___fxstatat64);

int new_fxstatat64(int vers, int dir_fd, const char *filename,
		   struct stat64 *buf, int flags)
{
  int nofollow = (flags & AT_SYMLINK_NOFOLLOW) ? TRUE : FALSE;
  int type;
  if((flags & ~AT_SYMLINK_NOFOLLOW) != 0) {
    __set_errno(EINVAL);
    return -1;
  }
  if(vers == _STAT_VER)
    type = TYPE_STAT64;
  else {
    __set_errno(ENOSYS);
    return -1;
  }
  return my_statat(dir_fd, nofollow, type, filename, buf);
}
