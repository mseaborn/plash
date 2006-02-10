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

/* Needed to get O_LARGEFILE */
#define _LARGEFILE64_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
/* #include <dirent.h> We have our own types */
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <utime.h>

#include "region.h"
#include "comms.h"
#include "libc-comms.h"


/* EXPORT: new_open => WEAK:open WEAK:__open __libc_open __GI_open __GI___open __GI___libc_open open_not_cancel open_not_cancel_2 */
int new_open(const char *filename, int flags, ...)
{
  region_t r = region_make();
  seqf_t reply;
  fds_t fds;
  int mode = 0;

  if(!filename) {
    __set_errno(EINVAL);
    goto error;
  }
  if(flags & O_CREAT) {
    va_list arg;
    va_start(arg, flags);
    mode = va_arg(arg, int);
    va_end(arg);
  }
  if(req_and_reply_with_fds
       (r, cat4(r, mk_string(r, "Open"),
		mk_int(r, flags),
		mk_int(r, mode),
		mk_string(r, filename)),
	&reply, &fds) < 0) goto error;
  {
    seqf_t msg = reply;
    int fd;
    int ok = 1;
    m_str(&ok, &msg, "ROpn");
    m_end(&ok, &msg);
    m_fd(&ok, &fds, &fd);
    if(ok) {
      region_free(r);
      return fd;
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
  return -1;
}

/* EXPORT: new_open64 => WEAK:open64 WEAK:__open64 __libc_open64 __GI___open64 */
int new_open64(const char *filename, int flags, ...)
{
  int mode = 0;
  if(flags & O_CREAT) {
    va_list arg;
    va_start(arg, flags);
    mode = va_arg(arg, int);
    va_end(arg);
  }
  return new_open(filename, flags | O_LARGEFILE, mode);
}

/* EXPORT: new_creat => WEAK:creat __libc_creat __GI_creat __GI___libc_creat */
int new_creat(const char *filename, int mode)
{
  return new_open(filename, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

/* EXPORT: new_creat64 => creat64 */
int new_creat64(const char *filename, int mode)
{
  return new_open64(filename, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

/* EXPORT: new_getcwd => WEAK:getcwd __getcwd */
/* glibc implements getwd in terms of __getcwd. */
/* What about get_current_dir_name()? */
char *new_getcwd(char *buf, size_t size)
{
  region_t r = region_make();
  seqf_t reply;
  if(req_and_reply(r, mk_string(r, "Gcwd"), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_str(&ok, &msg, "RCwd");
    if(ok) {
      if(!buf) {
	if(size < msg.size+1) size = msg.size+1;
	buf = malloc(size);
	if(!buf) { __set_errno(ENOMEM); goto error; }
      }
      else if(size < msg.size+1) {
	__set_errno(ERANGE);
	goto error;
      }
      memcpy(buf, msg.data, msg.size);
      buf[msg.size] = 0;
      region_free(r);
      return buf;
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
  return 0;
}

/* EXPORT: new_chdir => WEAK:chdir __chdir __GI_chdir __GI___chdir */
int new_chdir(const char *pathname)
{
  region_t r = region_make();
  seqf_t reply;

  if(!pathname) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat2(r, mk_string(r, "Chdr"),
			   mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_str(&ok, &msg, "RSuc");
    m_end(&ok, &msg);
    if(ok) {
      region_free(r);
      return 0;
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
  return -1;
}

/* EXPORT: new_fchdir => WEAK:fchdir __fchdir __GI_fchdir __GI___fchdir */
int new_fchdir(int fd)
{
  __set_errno(ENOSYS);
  return -1;
}

struct dirstream {
  void *buf;
  int buf_size;
  seqf_t data;
  int offset; /* Current offset */
};
typedef struct dirstream DIR;

/* d_ino: inode number (same as returned by stat)
   d_off: not really sure what this is for.  The man page for getdents
     says it's the offset (from the start of the buffer) to the next
     entry.  But it does not seem to be this when I have tested it.
     It would be the cumulative sum of d_reclen; but it's not.
     d_reclen is actually accurate.
   d_reclen: length of whole record in bytes, including padding.
   d_type: file type as returned by stat, or zero for unknown.
   d_name: the name, null terminated.  There may be padding after this. */
/* The getdents(2) man page on my system says:
       d_ino is an inode number.  d_off is the distance from  the
       start  of  the  directory to the start of the next dirent.
       d_reclen is the size of this entire dirent.  d_name  is  a
       null-terminated file name.
   However, the page http://www.delorie.com/gnu/docs/dirent/dirent.5.html
   says:
     The field d_off represents an offset of that directory entry in
     the actual file system directory.  ...  WARNING: The field d_off
     does not have a simple interpretation for some file system types
     and should not be used directly by applications.
   Also see
   http://www.ssc.com/pipermail/linux-list/2002-June/012755.html :
     Read the source.  fs/ext2/dir.c, for example.  The only fields
     that are not obvious are d_off and d_reclen.  The d_off field is
     the offset to the next struct dirent in the directory block.
     d_reclen is the actual size of the current struct dirent.
     Despite the apparent fixed size of the d_name[] array, a struct
     dirent is variable in length depending on the actual filename
     length.  Holes in directories can result from file deletions or
     renames because surviving entries are never moved.
*/
/* Based on sysdeps/unix/sysv/linux/bits/dirent.h */
/* Also see sysdeps/unix/sysv/linux/i386/olddirent.h, which mixes a
   64-bit offset with a 32-bit inode. */
struct dirent {
  long d_ino;
  off_t d_off;
  unsigned short int d_reclen;
  unsigned char d_type;
  char d_name[256];
};
struct dirent64 {
  /* uint64_t */ unsigned long long int d_ino;
  /* int64_t */ long long int d_off;
  unsigned short int d_reclen;
  unsigned char d_type;
  char d_name[256];
};

/* EXPORT: new_opendir => WEAK:opendir __opendir */
DIR *new_opendir(const char *pathname)
{
  region_t r = region_make();
  seqf_t reply;
  if(!pathname) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat2(r, mk_string(r, "Dlst"),
			   mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_str(&ok, &msg, "RDls");
    if(ok) {
      void *buf = amalloc(sizeof(struct dirstream) + msg.size);
      struct dirstream *dir = buf;
      char *data = (char *) buf + sizeof(struct dirstream);
      memcpy(data, msg.data, msg.size);
      dir->data.data = data;
      dir->data.size = msg.size;
      dir->offset = 0;
      dir->buf_size = 0;
      dir->buf = 0;
      region_free(r);
      return dir;
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
  return 0;
}

/* EXPORT: new_readdir => WEAK:readdir __readdir */
/* This is a bit of a stupid interface.  It returns zero for the end of the
   directory *or* an error -- how is a caller supposed to tell the
   difference?  Set errno to zero before, I suppose. */
struct dirent *new_readdir(DIR *dir)
{
  seqf_t buf;
  if(!dir) { __set_errno(EBADF); return 0; }
  buf.data = dir->data.data + dir->offset;
  buf.size = dir->data.size - dir->offset;
  if(buf.size == 0) return 0; /* end of directory */
  {
    int inode, type;
    seqf_t name;
    int ok = 1;
    m_int(&ok, &buf, &inode);
    m_int(&ok, &buf, &type);
    m_lenblock(&ok, &buf, &name);
    if(ok) {
      int rec_size = offsetof(struct dirent, d_name) + name.size + 1;
      struct dirent *ent;
      if(dir->buf_size < rec_size) {
	free(dir->buf);
	dir->buf = amalloc(rec_size + 50);
	dir->buf_size = rec_size;
      }
      ent = (void *) dir->buf;
      ent->d_ino = inode;
      ent->d_off = 0; /* shouldn't be used */
      ent->d_reclen = rec_size; /* shouldn't be used */
      ent->d_type = type;
      memcpy(ent->d_name, name.data, name.size);
      ent->d_name[name.size] = 0;
      dir->offset = dir->data.size - buf.size;
      return ent;
    }
    else {
      __set_errno(EIO); return 0;
    }
  }
}

/* EXPORT: new_readdir_r => WEAK:readdir_r __readdir_r */
int new_readdir_r(DIR *dir, struct dirent *ent, struct dirent **result)
{
  seqf_t buf;
  if(!dir) { __set_errno(EBADF); return -1; }
  buf.data = dir->data.data + dir->offset;
  buf.size = dir->data.size - dir->offset;
  if(buf.size == 0) { *result = 0; return 0; } /* end of directory */
  {
    int inode, type;
    seqf_t name;
    int ok = 1;
    m_int(&ok, &buf, &inode);
    m_int(&ok, &buf, &type);
    m_lenblock(&ok, &buf, &name);
    if(ok) {
      int rec_size = offsetof(struct dirent, d_name) + name.size + 1;
      ent->d_ino = inode;
      ent->d_off = 0; /* shouldn't be used */
      ent->d_reclen = rec_size; /* shouldn't be used */
      ent->d_type = type;
      memcpy(ent->d_name, name.data, name.size);
      ent->d_name[name.size] = 0;
      dir->offset = dir->data.size - buf.size;
      *result = ent;
      return 0;
    }
    else {
      __set_errno(EIO); return -1;
    }
  }
}

/* The GLIBC_2.1 version / __old_readdir64 is included although I don't
   know how it's supposed to be different. */
/* EXPORT: new_readdir64 => __readdir64 DEFVER:readdir64,GLIBC_2.2 VER:readdir64,GLIBC_2.1 __old_readdir64 */
struct dirent64 *new_readdir64(DIR *dir)
{
  seqf_t buf;
  if(!dir) { __set_errno(EBADF); return 0; }
  buf.data = dir->data.data + dir->offset;
  buf.size = dir->data.size - dir->offset;
  if(buf.size == 0) return 0; /* end of directory */
  {
    int inode, type;
    seqf_t name;
    int ok = 1;
    m_int(&ok, &buf, &inode);
    m_int(&ok, &buf, &type);
    m_lenblock(&ok, &buf, &name);
    if(ok) {
      int rec_size = offsetof(struct dirent64, d_name) + name.size + 1;
      struct dirent64 *ent;
      if(dir->buf_size < rec_size) {
	free(dir->buf);
	dir->buf = amalloc(rec_size + 50);
	dir->buf_size = rec_size;
      }
      ent = (void *) dir->buf;
      ent->d_ino = inode;
      ent->d_off = 0; /* shouldn't be used */
      ent->d_reclen = rec_size; /* shouldn't be used */
      ent->d_type = type;
      memcpy(ent->d_name, name.data, name.size);
      ent->d_name[name.size] = 0;
      dir->offset = dir->data.size - buf.size;
      return ent;
    }
    else {
      __set_errno(EIO); return 0;
    }
  }
}

/* EXPORT: new_closedir => WEAK:closedir __closedir */
int new_closedir(DIR *dir)
{
  if(!dir) { __set_errno(EBADF); return -1; }
  free(dir->buf);
  free(dir);
  return 0;
}

/* EXPORT: rewinddir */
void rewinddir(DIR *dir)
{
  if(!dir) return; /* No way to return an error */
  dir->offset = 0;
}

/* EXPORT: telldir */
off_t telldir(DIR *dir)
{
  if(!dir) { __set_errno(EBADF); return -1; }
  return dir->offset;
}

/* EXPORT: seekdir */
void seekdir(DIR *dir, off_t offset)
{
  if(!dir) return; /* We can't return an error */
  dir->offset = offset;
}

/* EXPORT: new_getdents => __getdents */
int new_getdents(int fd, struct dirent *buf, unsigned count)
{
  __set_errno(ENOSYS);
  return -1;
}
/* EXPORT: new_getdents64 => __getdents64 */
int new_getdents64(int fd, struct dirent *buf, unsigned count)
{
  __set_errno(ENOSYS);
  return -1;
}

/* EXPORT: new_xmknod => __xmknod __GI___xmknod */
int new_xmknod(int ver, const char *path, mode_t mode, dev_t dev)
{
  __set_errno(ENOSYS);
  return -1;
}


/* From sysdeps/unix/sysv/linux/bits/stat.h */
#define _STAT_VER_KERNEL        1 // mrs: indicates `struct kernel_stat'
#define _STAT_VER_SVR4          2
#define _STAT_VER_LINUX         3 // mrs: indicates `struct stat'

/* My codes: */
#define TYPE_STAT 1   /* struct stat */
#define TYPE_STAT64 2 /* struct stat64 */

/* From sysdeps/unix/sysv/linux/bits/stat.h */
#if 0
struct stat
  {
    // 1
    __dev_t st_dev;			/* Device.  */
    unsigned short int __pad1;
    // 2
#ifndef __USE_FILE_OFFSET64
    __ino_t st_ino;			/* File serial number.	*/
#else
    __ino_t __st_ino;			/* 32bit file serial number.	*/
#endif
    // 3
    __mode_t st_mode;			/* File mode.  */
    __nlink_t st_nlink;			/* Link count.  */
    // 4
    __uid_t st_uid;			/* User ID of the file's owner.	*/
    __gid_t st_gid;			/* Group ID of the file's group.*/
    // 5
    __dev_t st_rdev;			/* Device number, if device.  */
    unsigned short int __pad2;
#ifndef __USE_FILE_OFFSET64
    __off_t st_size;			/* Size of file, in bytes.  */
#else
    __off64_t st_size;			/* Size of file, in bytes.  */
#endif
    __blksize_t st_blksize;		/* Optimal block size for I/O.  */

#ifndef __USE_FILE_OFFSET64
    __blkcnt_t st_blocks;		/* Number 512-byte blocks allocated. */
#else
    __blkcnt64_t st_blocks;		/* Number 512-byte blocks allocated. */
#endif
    __time_t st_atime;			/* Time of last access.  */
    unsigned long int __unused1;
    __time_t st_mtime;			/* Time of last modification.  */
    unsigned long int __unused2;
    __time_t st_ctime;			/* Time of last status change.  */
    unsigned long int __unused3;
#ifndef __USE_FILE_OFFSET64
    unsigned long int __unused4;
    unsigned long int __unused5;
#else
    __ino64_t st_ino;			/* File serial number.	*/
#endif
  };
#endif

#if 0
#ifdef __USE_LARGEFILE64
struct stat64
  {
    __dev_t st_dev;			/* Device.  */
    unsigned int __pad1;

    __ino_t __st_ino;			/* 32bit file serial number.	*/
    __mode_t st_mode;			/* File mode.  */
    __nlink_t st_nlink;			/* Link count.  */
    __uid_t st_uid;			/* User ID of the file's owner.	*/
    __gid_t st_gid;			/* Group ID of the file's group.*/
    __dev_t st_rdev;			/* Device number, if device.  */
    unsigned int __pad2;
    __off64_t st_size;			/* Size of file, in bytes.  */
    __blksize_t st_blksize;		/* Optimal block size for I/O.  */

    __blkcnt64_t st_blocks;		/* Number 512-byte blocks allocated. */
    __time_t st_atime;			/* Time of last access.  */
    unsigned long int __unused1;
    __time_t st_mtime;			/* Time of last modification.  */
    unsigned long int __unused2;
    __time_t st_ctime;			/* Time of last status change.  */
    unsigned long int __unused3;
    __ino64_t st_ino;			/* File serial number.		*/
  };
#endif
#endif

#if 0
/* From sysdeps/unix/sysv/linux/kernel_stat.h */
struct kernel_stat
  {
    unsigned short int st_dev;
    unsigned short int __pad1;
    unsigned long int st_ino;
    unsigned short int st_mode;
    unsigned short int st_nlink;
    unsigned short int st_uid;
    unsigned short int st_gid;
    unsigned short int st_rdev;
    unsigned short int __pad2;
    unsigned long int st_size;
    unsigned long int st_blksize;
    unsigned long int st_blocks;
    unsigned long int st_atime;
    unsigned long int __unused1;
    unsigned long int st_mtime;
    unsigned long int __unused2;
    unsigned long int st_ctime;
    unsigned long int __unused3;
    unsigned long int __unused4;
    unsigned long int __unused5;
  };

/* From dietlibc's include/sys/stat.h */
struct x_stat {
	unsigned short	st_dev;
	unsigned short	__pad1;
	unsigned long	st_ino;
	unsigned short	st_mode;
	unsigned short	st_nlink;
	unsigned short	st_uid;
	unsigned short	st_gid;
	unsigned short	st_rdev;
	unsigned short	__pad2;
	unsigned long	st_size;
	unsigned long	st_blksize;
	unsigned long	st_blocks;
	  signed long	st_atime;
	unsigned long	__unused1;
	  signed long	st_mtime;
	unsigned long	__unused2;
	  signed long	st_ctime;
	unsigned long	__unused3;
	unsigned long	__unused4;
	unsigned long	__unused5;
};
#endif

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

/* nofollow=0 for stat, nofollow=1 for lstat. */
int my_stat(int nofollow, int type, const char *pathname, void *buf)
{
  region_t r = region_make();
  seqf_t reply;
  if(!pathname || !buf) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat3(r, mk_string(r, "Stat"),
			   mk_int(r, nofollow),
			   mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int myst_dev, myst_ino, myst_mode, myst_nlink, myst_uid, myst_gid, myst_rdev,
      myst_size, myst_blksize, myst_blocks, myst_atime, myst_mtime, myst_ctime;
    int ok = 1;
    m_str(&ok, &msg, "RSta");
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
    if(ok) {
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
	region_free(r);
	return 0;
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
	region_free(r);
	return 0;
      }
    }
  }
  {
    seqf_t msg = reply;
    int err;
    int ok = 1;
    m_str(&ok, &msg, "Fail");
    m_int(&ok, &msg, &err);
    if(ok) {
      __set_errno(err);
      goto error;
    }
  }

  __set_errno(ENOSYS);
 error:
  region_free(r);
  return -1;
}

int my_fstat(int type, int fd, void *buf)
{
  struct stat st;
  if(fstat(fd, &st) < 0) return -1;
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
  return -1;
}

/* EXPORT: new_xstat => __xstat __GI___xstat */
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
/* EXPORT: new_xstat64 => ___xstat64 DEFVER:__xstat64,GLIBC_2.2 VER:__xstat64,GLIBC_2.1 __GI___xstat64 */
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

/* EXPORT: new_lxstat => __lxstat __GI___lxstat */
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
/* EXPORT: new_lxstat64 => ___lxstat64 DEFVER:__lxstat64,GLIBC_2.2 VER:__lxstat64,GLIBC_2.1 __GI___lxstat64 */
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

/* EXPORT: new_fxstat => __fxstat __GI___fxstat */
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
/* EXPORT: new_fxstat64 => ___fxstat64 DEFVER:__fxstat64,GLIBC_2.2 VER:__fxstat64,GLIBC_2.1 __GI___fxstat64 */
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

/* EXPORT: new_readlink => WEAK:readlink __readlink __GI_readlink __GI___readlink */
int new_readlink(const char *pathname, char *buf, size_t buf_size)
{
  region_t r = region_make();
  seqf_t reply;
  if(!pathname || !buf) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat2(r, mk_string(r, "Rdlk"),
			   mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_str(&ok, &msg, "RRdl");
    if(ok) {
      int count = msg.size;
      if(count > buf_size) count = buf_size;
      memcpy(buf, msg.data, count);
      region_free(r);
      return count;
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
  return -1;
}

/* EXPORT: new_access => WEAK:access __access __GI_access __GI___access */
int new_access(const char *pathname, unsigned int mode)
{
  region_t r = region_make();
  seqf_t reply;
  if(!pathname) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat3(r, mk_string(r, "Accs"),
			   mk_int(r, mode),
			   mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_str(&ok, &msg, "RAcc");
    m_end(&ok, &msg);
    if(ok) {
      region_free(r);
      return 0;
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
  return -1;
}

/* EXPORT: new_chmod => WEAK:chmod __chmod __GI_chmod __GI___chmod */
/* Rather than adding a new message to the protocol, and adding new
   methods to the objects in the server, I wanted to just use open and
   fchmod to implement chmod.
   Unfortunately, that doesn't work, because the client process is
   running under a user ID that doesn't own the file, and fchmod will
   not work in this case -- FDs don't behave like capabilities in this
   case. */
int new_chmod(const char *pathname, unsigned int mode)
{
  /* Usually, this code would work:
  int fd, rc;
  fd = open(pathname, O_RDONLY);
  if(fd < 0) return -1;
  rc = fchmod(fd, mode);
  close(fd);
  return rc;
  */

  region_t r = region_make();
  seqf_t reply;
  if(!pathname) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat3(r, mk_string(r, "Chmd"),
			   mk_int(r, mode),
			   mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_str(&ok, &msg, "RChm");
    m_end(&ok, &msg);
    if(ok) {
      region_free(r);
      return 0;
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
  return -1;
}

/* EXPORT: new_lchmod => lchmod */
int new_lchmod(const char *pathname, unsigned int mode)
{
  __set_errno(ENOSYS);
  return -1;
}

/* EXPORT: new_chown => DEFVER:chown,GLIBC_2.1 VER:chown,GLIBC_2.0 __chown __GI___chown */
int new_chown(const char *pathname, unsigned int owner, unsigned int group)
{
  __set_errno(ENOSYS);
  return -1;
}

/* EXPORT: new_lchown => lchown __lchown */
int new_lchown(const char *pathname, unsigned int owner, unsigned int group)
{
  __set_errno(ENOSYS);
  return -1;
}

int my_utimes(int nofollow, const char *pathname,
	      struct timeval *atime, struct timeval *mtime)
{
  region_t r = region_make();
  seqf_t reply;
  if(!pathname || !atime || !mtime) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r,
		   cat4(r, mk_string(r, "Utim"),
			mk_int(r, nofollow),
			cat4(r,
			  mk_int(r, atime->tv_sec), mk_int(r, atime->tv_usec),
			  mk_int(r, mtime->tv_sec), mk_int(r, mtime->tv_usec)),
			mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_str(&ok, &msg, "RUtm");
    m_end(&ok, &msg);
    if(ok) {
      region_free(r);
      return 0;
    }
  }
  {
    seqf_t msg = reply;
    int err;
    int ok = 1;
    m_str(&ok, &msg, "Fail");
    m_int(&ok, &msg, &err);
    if(ok) {
      __set_errno(err);
      goto error;
    }
  }

  __set_errno(ENOSYS);
 error:
  region_free(r);
  return -1;
}

/* EXPORT: new_utime => WEAK:utime __utime __GI_utime */
int new_utime(const char *path, struct utimbuf *buf)
{
  if(buf) {
    struct timeval atime, mtime;
    atime.tv_sec = buf->actime;
    atime.tv_usec = 0;
    mtime.tv_sec = buf->modtime;
    mtime.tv_usec = 0;
    return my_utimes(0 /* nofollow */, path, &atime, &mtime);
  }
  else {
    struct timeval time;
    if(gettimeofday(&time, 0) < 0) return -1;
    return my_utimes(0 /* nofollow */, path, &time, &time);
  }
}

/* EXPORT: new_utimes => WEAK:utimes __utimes */
int new_utimes(const char *path, struct timeval times[2])
{
  if(times) {
    return my_utimes(0 /* nofollow */, path, &times[0], &times[1]);
  }
  else {
    struct timeval time;
    if(gettimeofday(&time, 0) < 0) return -1;
    return my_utimes(0 /* nofollow */, path, &time, &time);
  }
}

/* EXPORT: new_lutimes => WEAK:lutimes __lutimes */
int new_lutimes(const char *path, struct timeval times[2])
{
  if(times) {
    return my_utimes(1 /* nofollow */, path, &times[0], &times[1]);
  }
  else {
    struct timeval time;
    if(gettimeofday(&time, 0) < 0) return -1;
    return my_utimes(1 /* nofollow */, path, &time, &time);
  }
}

/* EXPORT: new_truncate => truncate __GI_truncate */
int new_truncate(const char *path, off_t length)
{
  int rc;
  int fd = new_open(path, O_WRONLY, 0);
  if(fd < 0) return -1;
  rc = ftruncate(fd, length);
  close(fd);
  return rc;
}

/* EXPORT: new_rename => rename __GI_rename */
int new_rename(const char *oldpath, const char *newpath)
{
  region_t r = region_make();
  seqf_t reply;
  if(!oldpath || !newpath) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat4(r, mk_string(r, "Renm"),
			   mk_int(r, strlen(newpath)),
			   mk_string(r, newpath),
			   mk_string(r, oldpath)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_str(&ok, &msg, "RRnm");
    m_end(&ok, &msg);
    if(ok) {
      region_free(r);
      return 0;
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
  return -1;
}

/* EXPORT: new_link => WEAK:link __link __GI_link __GI___link */
int new_link(const char *oldpath, const char *newpath)
{
  region_t r = region_make();
  seqf_t reply;
  if(!oldpath || !newpath) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat4(r, mk_string(r, "Link"),
			   mk_int(r, strlen(newpath)),
			   mk_string(r, newpath),
			   mk_string(r, oldpath)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_str(&ok, &msg, "RLnk");
    m_end(&ok, &msg);
    if(ok) {
      region_free(r);
      return 0;
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
  return -1;
}

/* EXPORT: new_symlink => WEAK:symlink __symlink __GI_symlink __GI___symlink */
int new_symlink(const char *oldpath, const char *newpath)
{
  region_t r = region_make();
  seqf_t reply;
  if(!oldpath || !newpath) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat4(r, mk_string(r, "Syml"),
			   mk_int(r, strlen(newpath)),
			   mk_string(r, newpath),
			   mk_string(r, oldpath)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_str(&ok, &msg, "RSym");
    m_end(&ok, &msg);
    if(ok) {
      region_free(r);
      return 0;
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
  return -1;
}

/* EXPORT: new_mkdir => WEAK:mkdir __mkdir __GI_mkdir __GI___mkdir */
int new_mkdir(const char *pathname, unsigned int mode)
{
  region_t r = region_make();
  seqf_t reply;
  if(!pathname) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat3(r, mk_string(r, "Mkdr"),
			   mk_int(r, mode),
			   mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_str(&ok, &msg, "RMkd");
    m_end(&ok, &msg);
    if(ok) {
      region_free(r);
      return 0;
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
  return -1;
}

/* EXPORT: new_mkfifo => mkfifo */
int new_mkfifo(const char *pathname, unsigned int mode)
{
  __set_errno(ENOSYS);
  return -1;
}

/* EXPORT: new_unlink => WEAK:unlink __unlink __GI_unlink __GI___unlink */
int new_unlink(const char *pathname)
{
  region_t r = region_make();
  seqf_t reply;
  if(!pathname) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat2(r, mk_string(r, "Unlk"),
			   mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_str(&ok, &msg, "RUnl");
    m_end(&ok, &msg);
    if(ok) {
      region_free(r);
      return 0;
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
  return -1;
}

/* EXPORT: new_rmdir => WEAK:rmdir __rmdir __GI_rmdir __GI___rmdir */
int new_rmdir(const char *pathname)
{
  region_t r = region_make();
  seqf_t reply;
  if(!pathname) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat2(r, mk_string(r, "Rmdr"),
			   mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_str(&ok, &msg, "RRmd");
    m_end(&ok, &msg);
    if(ok) {
      region_free(r);
      return 0;
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
  return -1;
}

/* EXPORT: new_statfs => WEAK:statfs __statfs __GI_statfs __GI___statfs */
int new_statfs(const char *path, void *buf) // struct statfs *buf
{
  __set_errno(ENOSYS);
  return -1;
}


/* EXPORT: new_setxattr => setxattr __GI_setxattr */
int new_setxattr(const char *path, const char *name,
		 const void *value, size_t size, int flags)
{
  __set_errno(ENOSYS);
  return -1;
}

/* EXPORT: new_getxattr => getxattr __GI_getxattr */
ssize_t new_getxattr(const char *path, const char *name,
		     void *value, size_t size)
{
  __set_errno(ENOSYS);
  return -1;
}

/* EXPORT: new_listxattr => listxattr __GI_listxattr */
ssize_t new_listxattr(const char *path, char *list, size_t size)
{
  __set_errno(ENOSYS);
  return -1;
}

/* EXPORT: new_removexattr => removexattr __GI_removexattr */
int new_removexattr(const char *path, const char *name)
{
  __set_errno(ENOSYS);
  return -1;
}


/* EXPORT: new_lsetxattr => lsetxattr __GI_lsetxattr */
int new_lsetxattr(const char *path, const char *name,
		  const void *value, size_t size, int flags)
{
  __set_errno(ENOSYS);
  return -1;
}

/* EXPORT: new_lgetxattr => lgetxattr __GI_lgetxattr */
ssize_t new_lgetxattr(const char *path, const char *name,
		      void *value, size_t size)
{
  __set_errno(ENOSYS);
  return -1;
}

/* EXPORT: new_llistxattr => llistxattr __GI_llistxattr */
ssize_t new_llistxattr(const char *path, char *list, size_t size)
{
  __set_errno(ENOSYS);
  return -1;
}

/* EXPORT: new_lremovexattr => lremovexattr __GI_lremovexattr */
int new_lremovexattr(const char *path, const char *name)
{
  __set_errno(ENOSYS);
  return -1;
}


#include "out-aliases_libc-misc.h"
