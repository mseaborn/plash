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

#include "libc-errno.h"
#include "libc-comms.h"
#include "libc-fds.h"
#include "cap-utils.h"
#include "marshal.h"


#define log_msg(msg) /* nothing */
#define log_fd(fd, msg) /* nothing */


/* From sysdeps/unix/sysv/linux/bits/stat.h */
#define _STAT_VER_KERNEL        1 // mrs: indicates `struct kernel_stat'
#define _STAT_VER_SVR4          2
#define _STAT_VER_LINUX         3 // mrs: indicates `struct stat'

/* My codes: */
#define TYPE_STAT 1   /* struct stat */
#define TYPE_STAT64 2 /* struct stat64 */


/* This is the ABI for glibc 2.2.5's "struct stat".  I arrived at it by
   looking at the preprocessed output for #include <sys/stat.h>, and
   expanding out any typedef'd types.  I have made all the members
   unsigned even though glibc declares some of them as signed; I don't
   think the sign is really needed. */
/* There are so many different definitions of "struct stat(64)", and
   not all of them are self-contained: some refer to types like __dev_t.
   So it is getting really difficult to work out what glibc's ABIs are.
   It's easy to work out for an ABI we can compile to, but I'm not sure
   about the older ABIs that glibc supports but that code compiled for
   glibc won't use.
   I'm most confused about the rationale and history behind all these
   different versions. */
/* glibc does the following in bits/stat.h:
    # define st_atime st_atim.tv_sec
    # define st_mtime st_mtim.tv_sec
    # define st_ctime st_ctim.tv_sec
   Truly horrible!  So I'll use the prefix `myst_` instead of `st_'
   to avoid clashing with glibc's macros. */
struct abi_stat {
  unsigned long long myst_dev;     /*  0: 8 bytes */
  unsigned int       __pad1;     /*  8: 4 bytes, but written as `short' in glibc */
  unsigned int	     myst_ino;     /* 12: 4 bytes */
  unsigned int	     myst_mode;    /* 16: 4 bytes */
  unsigned int	     myst_nlink;   /* 20: 4 bytes */
  unsigned int       myst_uid;     /* 24: 4 bytes */
  unsigned int       myst_gid;     /* 28: 4 bytes */
  unsigned long long myst_rdev;    /* 32: 8 bytes */
  unsigned int       __pad2;     /* 40: 4 bytes, but written as `short' in glibc */
  unsigned int       myst_size;    /* 44: 4 bytes */
  unsigned int       myst_blksize; /* 48: 4 bytes */
  unsigned int       myst_blocks;  /* 52: 4 bytes */
  unsigned int       myst_atime;   /* 56: 4 bytes */
  unsigned int       __unused1;  /* 60: 4 bytes */
  unsigned int       myst_mtime;   /* 64: 4 bytes */
  unsigned int       __unused2;  /* 68: 4 bytes */
  unsigned int       myst_ctime;   /* 72: 4 bytes */
  unsigned int       __unused3;  /* 76: 4 bytes */
  unsigned int       __unused4;  /* 80: 4 bytes */
  unsigned int       __unused5;  /* 84: 4 bytes */
                                 /* 88 */
};
/* ino field is duplicated: */
struct abi_stat64 {
  unsigned long long myst_dev;     /*  0: 8 bytes */
  unsigned int       __pad1;     /*  8: 4 bytes */
  unsigned int	     __myst_ino;   /* 12: 4 bytes */
  unsigned int	     myst_mode;    /* 16: 4 bytes */
  unsigned int	     myst_nlink;   /* 20: 4 bytes */
  unsigned int       myst_uid;     /* 24: 4 bytes */
  unsigned int       myst_gid;     /* 28: 4 bytes */
  unsigned long long myst_rdev;    /* 32: 8 bytes */
  unsigned int       __pad2;     /* 40: 4 bytes */
  unsigned long long myst_size;    /* 44: 8 bytes */
  unsigned int       myst_blksize; /* 52: 4 bytes */
  unsigned long long myst_blocks;  /* 56: 8 bytes */
  unsigned int       myst_atime;   /* 64: 4 bytes */
  unsigned int       __unused1;  /* 68: 4 bytes */
  unsigned int       myst_mtime;   /* 72: 4 bytes */
  unsigned int       __unused2;  /* 76: 4 bytes */
  unsigned int       myst_ctime;   /* 80: 4 bytes */
  unsigned int       __unused3;  /* 84: 4 bytes */
  unsigned long long myst_ino;     /* 88: 8 bytes */
                                 /* 96 */
};

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
    if(type == TYPE_STAT) {
      struct abi_stat *stat = buf;
      stat->myst_dev = myst_dev;
      stat->__pad1 = 0;
      stat->myst_ino = myst_ino;
      stat->myst_mode = myst_mode;
      stat->myst_nlink = myst_nlink;
      stat->myst_uid = myst_uid;
      stat->myst_gid = myst_gid;
      stat->myst_rdev = myst_rdev;
      stat->__pad2 = 0;
      stat->myst_size = myst_size;
      stat->myst_blksize = myst_blksize;
      stat->myst_blocks = myst_blocks;
      stat->myst_atime = myst_atime;
      stat->__unused1 = 0;
      stat->myst_mtime = myst_mtime;
      stat->__unused2 = 0;
      stat->myst_ctime = myst_ctime;
      stat->__unused3 = 0;
      stat->__unused4 = 0;
      stat->__unused5 = 0;
    }
    else if(type == TYPE_STAT64) {
      struct abi_stat64 *stat = buf;
      stat->myst_dev = myst_dev;
      stat->__pad1 = 0;
      stat->__myst_ino = myst_ino;
      stat->myst_mode = myst_mode;
      stat->myst_nlink = myst_nlink;
      stat->myst_uid = myst_uid;
      stat->myst_gid = myst_gid;
      stat->myst_rdev = myst_rdev;
      stat->__pad2 = 0;
      stat->myst_size = myst_size;
      stat->myst_blksize = myst_blksize;
      stat->myst_blocks = myst_blocks;
      stat->myst_atime = myst_atime;
      stat->__unused1 = 0;
      stat->myst_mtime = myst_mtime;
      stat->__unused2 = 0;
      stat->myst_ctime = myst_ctime;
      stat->__unused3 = 0;
      stat->myst_ino = myst_ino;
    }
    else {
      *ok = 0;
    }
  }
}

/* nofollow=0 for stat, nofollow=1 for lstat. */
int my_stat(int nofollow, int type, const char *pathname, void *buf)
{
  region_t r = region_make();
  seqf_t reply;
  log_msg(MOD_MSG "stat\n");
  if(!pathname || !buf) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat3(r, mk_int(r, METHOD_FSOP_STAT),
			   mk_int(r, nofollow),
			   mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_int_const(&ok, &msg, METHOD_R_FSOP_STAT);
    m_stat_info(&ok, &msg, type, buf);
    m_end(&ok, &msg);
    if(ok) {
      region_free(r);
      return 0;
    }
  }
  set_errno_from_reply(reply);
 error:
  region_free(r);
  return -1;
}

int my_fstat(int type, int fd, void *buf)
{
  struct stat st;
  log_msg(MOD_MSG "fstat\n");

  if(0 <= fd && fd < g_fds_size && g_fds[fd].fd_dir_obj) {
    /* Handle directory FDs specially:  send a message. */
    cap_t dir_obj = g_fds[fd].fd_dir_obj;
    region_t r = region_make();
    struct cap_args result;
    log_fd(fd, "fstat on directory");
    if(!buf) {
      __set_errno(EINVAL);
      goto error;
    }
    if(plash_init() < 0) { goto error; }
    if(!fs_server) { __set_errno(ENOSYS); goto error; }
    cap_call(fs_server, r,
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
	region_free(r);
	return 0;
      }
    }
    caps_free(result.caps);
    close_fds(result.fds);
    set_errno_from_reply(flatten_reuse(r, result.data));
  error:
    region_free(r);
    return -1;
  }
  else {
    /* Use the normal fstat system call. */
    log_fd(fd, "normal fstat");
    if(fstat(fd, &st) < 0) { return -1; }
    if(type == TYPE_STAT) {
      struct abi_stat *stat = buf;
      stat->myst_dev = st.st_dev;
      stat->__pad1 = 0;
      stat->myst_ino = st.st_ino;
      stat->myst_mode = st.st_mode;
      stat->myst_nlink = st.st_nlink;
      stat->myst_uid = st.st_uid;
      stat->myst_gid = st.st_gid;
      stat->myst_rdev = st.st_rdev;
      stat->__pad2 = 0;
      stat->myst_size = st.st_size;
      stat->myst_blksize = st.st_blksize;
      stat->myst_blocks = st.st_blocks;
      stat->myst_atime = st.st_atime;
      stat->__unused1 = 0;
      stat->myst_mtime = st.st_mtime;
      stat->__unused2 = 0;
      stat->myst_ctime = st.st_ctime;
      stat->__unused3 = 0;
      stat->__unused4 = 0;
      stat->__unused5 = 0;
      return 0;
    }
    else if(type == TYPE_STAT64) {
      struct abi_stat64 *stat = buf;
      stat->myst_dev = st.st_dev;
      stat->__pad1 = 0;
      stat->__myst_ino = st.st_ino;
      stat->myst_mode = st.st_mode;
      stat->myst_nlink = st.st_nlink;
      stat->myst_uid = st.st_uid;
      stat->myst_gid = st.st_gid;
      stat->myst_rdev = st.st_rdev;
      stat->__pad2 = 0;
      stat->myst_size = st.st_size;
      stat->myst_blksize = st.st_blksize;
      stat->myst_blocks = st.st_blocks;
      stat->myst_atime = st.st_atime;
      stat->__unused1 = 0;
      stat->myst_mtime = st.st_mtime;
      stat->__unused2 = 0;
      stat->myst_ctime = st.st_ctime;
      stat->__unused3 = 0;
      stat->myst_ino = st.st_ino;
      return 0;
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

int new_xstat(int vers, const char *pathname, void *buf)
{
  int type;
  if(vers == _STAT_VER_LINUX) type = TYPE_STAT;
  else {
    /* fprintf(stderr, "__xstat version %i, `%s'\n", vers, pathname); */
    __set_errno(ENOSYS); return -1;
  }
  return my_stat(0, type, pathname, buf);
}


export(new_xstat64, ___xstat64);
export_versioned_symbol(libc, new_xstat64, __xstat64, GLIBC_2_2);
#if SHLIB_COMPAT(libc, GLIBC_2_1, GLIBC_2_2)
export_compat_symbol(libc, new_xstat64, __xstat64, GLIBC_2_1);
#endif
export(new_xstat64, __GI___xstat64);

int new_xstat64(int vers, const char *pathname, void *buf)
{
  int type;
  if(vers == _STAT_VER_LINUX) type = TYPE_STAT64;
  else {
    /* fprintf(stderr, "__xstat64 version %i, `%s'\n", vers, pathname); */
    __set_errno(ENOSYS); return -1;
  }
  return my_stat(0, type, pathname, buf);
}


export(new_lxstat, __lxstat);
export(new_lxstat, __GI___lxstat);

int new_lxstat(int vers, const char *pathname, void *buf)
{
  int type;
  if(vers == _STAT_VER_LINUX) type = TYPE_STAT;
  else {
    /* fprintf(stderr, "__lxstat version %i, `%s'\n", vers, pathname); */
    __set_errno(ENOSYS); return -1;
  }
  return my_stat(1, type, pathname, buf);
}


export(new_lxstat64, ___lxstat64);
export_versioned_symbol(libc, new_lxstat64, __lxstat64, GLIBC_2_2);
#if SHLIB_COMPAT(libc, GLIBC_2_1, GLIBC_2_2)
export_compat_symbol(libc, new_lxstat64, __lxstat64, GLIBC_2_1);
#endif
export(new_lxstat64, __GI___lxstat64);

int new_lxstat64(int vers, const char *pathname, void *buf)
{
  int type;
  if(vers == _STAT_VER_LINUX) type = TYPE_STAT64;
  else {
    /* fprintf(stderr, "__lxstat64 version %i, `%s'\n", vers, pathname); */
    __set_errno(ENOSYS); return -1;
  }
  return my_stat(1, type, pathname, buf);
}


export(new_fxstat, __fxstat);
export(new_fxstat, __GI___fxstat);

int new_fxstat(int vers, int fd, void *buf)
{
  int type;
  if(vers == _STAT_VER_LINUX) type = TYPE_STAT;
  else {
    /* fprintf(stderr, "__fxstat version %i, fd %i\n", vers, fd); */
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

int new_fxstat64(int vers, int fd, void *buf)
{
  int type;
  if(vers == _STAT_VER_LINUX) type = TYPE_STAT64;
  else {
    /* fprintf(stderr, "__fxstat64 version %i, fd %i\n", vers, fd); */
    __set_errno(ENOSYS); return -1;
  }
  return my_fstat(type, fd, buf);
}


export(new_fxstatat, __fxstatat);
export(new_fxstatat, __GI___fxstatat);

/* Added in glibc 2.4 */
int new_fxstatat(int vers, int fd, const char *filename, void *buf, int flag)
{
  __set_errno(ENOSYS);
  return -1;
}


export(new_fxstatat64, __fxstatat64);
export(new_fxstatat64, __GI___fxstatat64);

/* Added in glibc 2.4 */
int new_fxstatat64(int vers, int fd, const char *filename, void *buf, int flag)
{
  __set_errno(ENOSYS);
  return -1;
}
