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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
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
#include <sys/un.h>
#include <sys/statfs.h>

#include "region.h"
#include "comms.h"
#include "libc-comms.h"
#include "libc-fds.h"
#include "marshal.h"
#include "cap-protocol.h"
#include "cap-utils.h"


#define MOD_MSG "libc: "
#if 1
#define log_msg(msg) /* nothing */
#else
static void log_msg(const char *msg)
{
  write(2, msg, strlen(msg));
}
#endif

#if defined IN_LIBC && 0
#define log_fd(fd, msg) fprintf(stderr, "libc: fd %i: %s\n", (fd), (msg))
#else
#define log_fd(fd, msg)
#endif


struct libc_fd *g_fds = NULL; /* Array allocated with malloc() */
int g_fds_size = 0;

static void libc_fd_init(struct libc_fd *fd)
{
  fd->fd_dir_obj = NULL;
  fd->fd_socket_pathname = NULL;
}

/* Ensure that the g_fds array is at least `fd + 1' elements long,
   ie. at least big enough to contain the index `fd'. */
void fds_resize(int fd)
{
  if(fd >= g_fds_size) {
    int i;
    int new_size = fd + 4; /* Some margin to reduce resize frequency */
    struct libc_fd *new_array = amalloc(new_size * sizeof(struct libc_fd));
    memcpy(new_array, g_fds, g_fds_size * sizeof(struct libc_fd));
    for(i = g_fds_size; i < new_size; i++) { libc_fd_init(&new_array[i]); }
    free(g_fds);
    g_fds = new_array;
    g_fds_size = new_size;
  }
}

void fds_slot_clear(int fd)
{
  if(0 <= fd && fd < g_fds_size) {
    struct libc_fd info = g_fds[fd];
    libc_fd_init(&g_fds[fd]);

    if(info.fd_dir_obj) {
      filesys_obj_free(info.fd_dir_obj);
    }
    if(info.fd_socket_pathname) {
      free(info.fd_socket_pathname);
    }
  }
}

void fds_slot_clear_warn_if_used(int fd)
{
  if(0 <= fd && fd < g_fds_size) {
    struct libc_fd info = g_fds[fd];
    libc_fd_init(&g_fds[fd]);

    /* These cases shouldn't happen.  The kernel has returned a FD
       with a number that we think is already in use. */
    if(info.fd_dir_obj) {
      log_fd(fd, "fd_dir_obj field was in use");
      libc_log("Warning: fd_dir_obj field was in use");
      filesys_obj_free(info.fd_dir_obj);
    }
    if(info.fd_socket_pathname) {
      log_fd(fd, "fd_socket_pathname field was in use");
      libc_log("Warning: fd_socket_pathname field was in use");
      free(info.fd_socket_pathname);
    }
  }
}


/* Try to set the errno from the given message, otherwise set it to ENOSYS. */
void set_errno_from_reply(seqf_t msg)
{
  int ok = 1;
  int err;
  m_int_const(&ok, &msg, METHOD_FAIL);
  m_int(&ok, &msg, &err);
  m_end(&ok, &msg);
  __set_errno(ok ? err : ENOSYS);
}


export_weak_alias(new_open, __open);
export_weak_alias(new_open, open);
export(new_open, __libc_open);
export(new_open, __GI_open);
export(new_open, __GI___open);
export(new_open, __GI___libc_open);
export(new_open, open_not_cancel);
export(new_open, open_not_cancel_2);
export(new_open, __open_nocancel);
export(new_open, __libc_open_nocancel);

int new_open(const char *filename, int flags, ...)
{
  region_t r = region_make();
  int mode = 0;
  struct cap_args result;
  log_msg(MOD_MSG "open\n");
  plash_libc_lock();

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
  {
    if(plash_init() < 0) goto error;
    if(!fs_server) { __set_errno(ENOSYS); goto error; }
    cap_call(fs_server, r,
	     cap_args_make(cat4(r, mk_int(r, METHOD_FSOP_OPEN),
				mk_int(r, flags),
				mk_int(r, mode),
				mk_string(r, filename)),
			   caps_empty, fds_empty),
	     &result);
  }
  {
    seqf_t msg = flatten_reuse(r, result.data);
    int ok = 1;
    m_int_const(&ok, &msg, METHOD_R_FSOP_OPEN);
    m_end(&ok, &msg);
    if(ok && result.fds.count == 1 && result.caps.size == 0) {
      int fd = result.fds.fds[0];
      fds_slot_clear_warn_if_used(fd);
      plash_libc_unlock();
      region_free(r);
      return fd;
    }
  }
  {
    /* This handles the case in which a directory is opened.  The server
       passes us a dummy FD for /dev/null, which we return. */
    seqf_t msg = flatten_reuse(r, result.data);
    int ok = 1;
    m_int_const(&ok, &msg, METHOD_R_FSOP_OPEN_DIR);
    m_end(&ok, &msg);
    if(ok && result.fds.count == 1 && result.caps.size == 1) {
      int fd = result.fds.fds[0];

      /* Resize the file descriptor array. */
      assert(fd >= 0);
      fds_resize(fd);
      fds_slot_clear_warn_if_used(fd);
      log_fd(fd, "fill out fd_dir_obj");
      g_fds[fd].fd_dir_obj = result.caps.caps[0];

      plash_libc_unlock();
      region_free(r);
      return fd;
    }
  }
  caps_free(result.caps);
  close_fds(result.fds);
  set_errno_from_reply(flatten_reuse(r, result.data));
 error:
  plash_libc_unlock();
  region_free(r);
  return -1;
}


export_weak_alias(new_close, close);
export_weak_alias(new_close, __close);
export(new_close, __libc_close);
export(new_close, __GI_close);
export(new_close, __GI___close);
export(new_close, __GI___libc_close);
export(new_close, close_not_cancel);
export(new_close, close_not_cancel_no_status);
export(new_close, __close_nocancel);

int new_close(int fd)
{
  log_msg(MOD_MSG "close\n");
  log_fd(fd, "close");
  /* Make sure that comm_sock has been initialised */
  plash_init();

  if(fd == comm_sock) {
    /* Pretend that this file descriptor slot is empty.  As far as the
       application knows, it *is* empty.  The chances are that the
       application is just closing all file descriptor numbers in a
       range.  Giving an error is the correct response in that
       case. */
#if !defined(IN_RTLD)
    if(libc_debug) fprintf(stderr, "libc: close(): refused to clobber fd %i\n", fd);
#endif
    __set_errno(EBADF);
    return -1;
  }
  fds_slot_clear(fd);
  return close(fd);
}


export_weak_alias(new_dup2, dup2);
export(new_dup2, __dup2);
export(new_dup2, __GI_dup2);
export(new_dup2, __GI___dup2);

int new_dup2(int source_fd, int dest_fd)
{
  int rc;
  log_msg(MOD_MSG "dup2\n");
  log_fd(source_fd, "dup2 source");
  log_fd(dest_fd, "dup2 dest");
  /* Make sure that comm_sock has been initialised */
  plash_init();

  if(dest_fd == comm_sock) {
    /* Don't allow the socket file descriptor to be clobbered.  This
       will stop applications which allocate their own FD numbers from
       going any further, assuming they check the return value for an
       error.  This should be better than clobbering the socket and
       getting a failure later. */
#if !defined(IN_RTLD)
    if(libc_debug) fprintf(stderr, "libc: dup2(): refused to clobber fd %i\n", dest_fd);
#endif
    __set_errno(EINVAL);
    return -1;
  }
  
  rc = dup2(source_fd, dest_fd);
  if(rc >= 0) {
    /* Make sure our entry for the destination FD is removed. */
    fds_slot_clear(dest_fd);
    
    /* To do: copy the g_fds entry from source_fd to dest_fd. */
  }
  return rc;
}


export_weak_alias(new_fchdir, fchdir);
export(new_fchdir, __fchdir);
export(new_fchdir, __GI_fchdir);
export(new_fchdir, __GI___fchdir);

int new_fchdir(int fd)
{
  log_msg(MOD_MSG "fchdir\n");
  log_fd(fd, "fchdir");
  if(0 <= fd && fd < g_fds_size) {
    cap_t obj = g_fds[fd].fd_dir_obj;
    if(obj) {
      struct cap_args result;
      region_t r = region_make();
      if(plash_init() < 0) goto error;
      if(!fs_server) { __set_errno(ENOSYS); goto error; }
      cap_call(fs_server, r,
	       cap_args_dc(mk_int(r, METHOD_FSOP_FCHDIR),
			   mk_caps1(r, inc_ref(obj))),
	       &result);
      {
	seqf_t msg = flatten_reuse(r, result.data);
	int ok = 1;
	m_int_const(&ok, &msg, METHOD_OKAY);
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
  }
  /* This should really return EBADF if there is no file descriptor in
     the slot `fd', but we don't have a good way of checking this.
     Return ENOTDIR to say that the FD is not for a directory. */
  __set_errno(ENOTDIR);
  return -1;
}


export_weak_alias(new_open64, open64);
export_weak_alias(new_open64, __open64);
export(new_open64, __libc_open64);
export(new_open64, __GI___open64);

int new_open64(const char *filename, int flags, ...)
{
  int mode = 0;
  log_msg(MOD_MSG "open64\n");
  if(flags & O_CREAT) {
    va_list arg;
    va_start(arg, flags);
    mode = va_arg(arg, int);
    va_end(arg);
  }
  return new_open(filename, flags | O_LARGEFILE, mode);
}


export_weak_alias(new_creat, creat);
export(new_creat, __libc_creat);
export(new_creat, __GI_creat);
export(new_creat, __GI___libc_creat);

int new_creat(const char *filename, int mode)
{
  log_msg(MOD_MSG "creat\n");
  return new_open(filename, O_WRONLY | O_CREAT | O_TRUNC, mode);
}


export(new_creat64, creat64);

int new_creat64(const char *filename, int mode)
{
  log_msg(MOD_MSG "creat64\n");
  return new_open64(filename, O_WRONLY | O_CREAT | O_TRUNC, mode);
}


export_weak_alias(new_getcwd, getcwd);
export(new_getcwd, __getcwd);

/* glibc implements getwd in terms of __getcwd. */
/* What about get_current_dir_name()? */
/* Cases:
   getcwd(NULL, 0): returns a dynamically-allocated buffer, as large
     as necessary.
   getcwd(NULL, s): returns a dynamically-allocated buffer of size s,
     or returns an error if s is not large enough.
   getcwd(buf, s): fills out given buffer if it is large enough,
     otherwise returns an error.
*/
char *new_getcwd(char *buf, size_t size)
{
  region_t r = region_make();
  seqf_t reply;
  log_msg(MOD_MSG "getcwd\n");
  if(req_and_reply(r, mk_int(r, METHOD_FSOP_GETCWD), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_int_const(&ok, &msg, METHOD_R_FSOP_GETCWD);
    if(ok) {
      if(!buf) {
	if(size == 0) {
	  size = msg.size + 1;
	}
	else if(size < msg.size + 1) {
	  __set_errno(ERANGE);
	  goto error;
	}
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
  set_errno_from_reply(reply);
 error:
  region_free(r);
  return 0;
}


export_weak_alias(new_chdir, chdir);
export(new_chdir, __chdir);
export(new_chdir, __GI_chdir);
export(new_chdir, __GI___chdir);

int new_chdir(const char *pathname)
{
  region_t r = region_make();
  seqf_t reply;
  log_msg(MOD_MSG "chdir\n");

  if(!pathname) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat2(r, mk_int(r, METHOD_FSOP_CHDIR),
			   mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_int_const(&ok, &msg, METHOD_OKAY);
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


export_weak_alias(new_opendir, opendir);
export(new_opendir, __opendir);

DIR *new_opendir(const char *pathname)
{
  region_t r = region_make();
  seqf_t reply;
  log_msg(MOD_MSG "opendir\n");
  if(!pathname) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat2(r, mk_int(r, METHOD_FSOP_DIRLIST),
			   mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_int_const(&ok, &msg, METHOD_R_FSOP_DIRLIST);
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
  set_errno_from_reply(reply);
 error:
  region_free(r);
  return 0;
}


export(new_fdopendir, fdopendir);
export(new_fdopendir, __fdopendir);

/* Added in glibc 2.4 */
DIR *new_fdopendir(int fd)
{
  log_msg(MOD_MSG "fdopendir\n");
  __set_errno(ENOSYS);
  return NULL;
}


export_weak_alias(new_readdir, readdir);
export(new_readdir, __readdir);

/* This is a bit of a stupid interface.  It returns zero for the end of the
   directory *or* an error -- how is a caller supposed to tell the
   difference?  Set errno to zero before, I suppose. */
struct dirent *new_readdir(DIR *dir)
{
  seqf_t buf;
  log_msg(MOD_MSG "readdir\n");
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


export_weak_alias(new_readdir_r, readdir_r);
export(new_readdir_r, __readdir_r);

/* This is a stupid interface too.  It provides a fixed-length buffer for
   the leaf name, and no way to say how big the buffer is (we assume that
   it's fixed by "struct dirent").  It's somewhat ambiguous whether the
   buffer has to be used or not; whether we can return a different buffer
   in "result" (if we do, how long is the buffer to be valid for?). */
int new_readdir_r(DIR *dir, struct dirent *ent, struct dirent **result)
{
  seqf_t buf;
  log_msg(MOD_MSG "readdir_r\n");
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
      if(rec_size > sizeof(struct dirent)) {
	__set_errno(EOVERFLOW);
	return -1;
      }
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

export(new_readdir64, __readdir64);
export_versioned_symbol(libc, new_readdir64, readdir64, GLIBC_2_2);
#if SHLIB_COMPAT(libc, GLIBC_2_1, GLIBC_2_2)
export_compat_symbol(libc, new_readdir64, readdir64, GLIBC_2_1);
#endif
export(new_readdir64, __old_readdir64);

/* The GLIBC_2.1 version / __old_readdir64 is included although I don't
   know how it's supposed to be different. */
struct dirent64 *new_readdir64(DIR *dir)
{
  seqf_t buf;
  log_msg(MOD_MSG "readdir64\n");
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


export(new_readdir64_r, __readdir64_r);
export_versioned_symbol(libc, new_readdir64_r, readdir64_r, GLIBC_2_2);
#if SHLIB_COMPAT(libc, GLIBC_2_1, GLIBC_2_2)
export_compat_symbol(libc, new_readdir64_r, readdir64_r, GLIBC_2_1);
#endif
export(new_readdir64_r, __old_readdir64_r);

/* The GLIBC_2.1 version / __old_readdir64_r is included although I don't
   know how it's supposed to be different. */
/* This is a stupid interface too.  It provides a fixed-length buffer for
   the leaf name, and no way to say how big the buffer is (we assume that
   it's fixed by "struct dirent").  It's somewhat ambiguous whether the
   buffer has to be used or not; whether we can return a different buffer
   in "result" (if we do, how long is the buffer to be valid for?). */
int new_readdir64_r(DIR *dir, struct dirent64 *ent, struct dirent64 **result)
{
  seqf_t buf;
  log_msg(MOD_MSG "readdir64_r\n");
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
      int rec_size = offsetof(struct dirent64, d_name) + name.size + 1;
      if(rec_size > sizeof(struct dirent64)) {
	__set_errno(EOVERFLOW);
	return -1;
      }
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


export_weak_alias(new_closedir, closedir);
export(new_closedir, __closedir);

int new_closedir(DIR *dir)
{
  log_msg(MOD_MSG "closedir\n");
  if(!dir) { __set_errno(EBADF); return -1; }
  free(dir->buf);
  free(dir);
  return 0;
}


export(dirfd, dirfd);

int dirfd(DIR *dir)
{
  log_msg(MOD_MSG "dirfd\n");
  if(!dir) { __set_errno(EBADF); return -1; }
  __set_errno(ENOSYS);
  return -1;
}


export(rewinddir, rewinddir);

void rewinddir(DIR *dir)
{
  log_msg(MOD_MSG "rewinddir\n");
  if(!dir) return; /* No way to return an error */
  dir->offset = 0;
}


export(telldir, telldir);

off_t telldir(DIR *dir)
{
  log_msg(MOD_MSG "telldir\n");
  if(!dir) { __set_errno(EBADF); return -1; }
  return dir->offset;
}


export(seekdir, seekdir);

void seekdir(DIR *dir, off_t offset)
{
  log_msg(MOD_MSG "seekdir\n");
  if(!dir) return; /* We can't return an error */
  dir->offset = offset;
}


export(new_getdents, __getdents);

int new_getdents(int fd, struct dirent *buf, unsigned count)
{
  log_msg(MOD_MSG "getdents\n");
  __set_errno(ENOSYS);
  return -1;
}


export(new_getdents64, __getdents64);

int new_getdents64(int fd, struct dirent *buf, unsigned count)
{
  log_msg(MOD_MSG "getdents64\n");
  __set_errno(ENOSYS);
  return -1;
}


export(new_xmknod, __xmknod);
export(new_xmknod, __GI___xmknod);

int new_xmknod(int ver, const char *path, mode_t mode, dev_t dev)
{
  log_msg(MOD_MSG "xmknod\n");
  __set_errno(ENOSYS);
  return -1;
}


export_weak_alias(new_readlink, readlink);
export(new_readlink, __readlink);
export(new_readlink, __GI_readlink);
export(new_readlink, __GI___readlink);

int new_readlink(const char *pathname, char *buf, size_t buf_size)
{
  region_t r = region_make();
  seqf_t reply;
  log_msg(MOD_MSG "readlink\n");
  if(!pathname || !buf) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat2(r, mk_int(r, METHOD_FSOP_READLINK),
			   mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_int_const(&ok, &msg, METHOD_R_FSOP_READLINK);
    if(ok) {
      int count = msg.size;
      if(count > buf_size) count = buf_size;
      memcpy(buf, msg.data, count);
      region_free(r);
      return count;
    }
  }
  set_errno_from_reply(reply);
 error:
  region_free(r);
  return -1;
}


export_weak_alias(new_access, access);
export(new_access, __access);
export(new_access, __GI_access);
export(new_access, __GI___access);

int new_access(const char *pathname, unsigned int mode)
{
  region_t r = region_make();
  seqf_t reply;
  log_msg(MOD_MSG "access\n");
  if(!pathname) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat3(r, mk_int(r, METHOD_FSOP_ACCESS),
			   mk_int(r, mode),
			   mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_int_const(&ok, &msg, METHOD_OKAY);
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

/* nofollow=0 for chmod, nofollow=1 for lchmod. */
/* Rather than adding a new message to the protocol, and adding new
   methods to the objects in the server, I wanted to just use open and
   fchmod to implement chmod.
   Unfortunately, that doesn't work, because the client process is
   running under a user ID that doesn't own the file, and fchmod will
   not work in this case -- FDs don't behave like capabilities in this
   case. */
int my_chmod(int nofollow, const char *pathname, unsigned int mode)
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
  if(req_and_reply(r, cat4(r, mk_int(r, METHOD_FSOP_CHMOD),
			   mk_int(r, nofollow),
			   mk_int(r, mode),
			   mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_int_const(&ok, &msg, METHOD_OKAY);
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


export_weak_alias(new_chmod, chmod);
export(new_chmod, __chmod);
export(new_chmod, __GI_chmod);
export(new_chmod, __GI___chmod);

int new_chmod(const char *pathname, unsigned int mode)
{
  log_msg(MOD_MSG "chmod\n");
  return my_chmod(0 /* nofollow */, pathname, mode);
}


export(new_lchmod, lchmod);

int new_lchmod(const char *pathname, unsigned int mode)
{
  log_msg(MOD_MSG "lchmod\n");
  return my_chmod(1 /* nofollow */, pathname, mode);
}

/* nofollow=0 for chown, nofollow=1 for lchown. */
static int my_chown(int nofollow, const char *pathname,
		    unsigned int owner_uid,
		    unsigned int group_gid)
{
  region_t r = region_make();
  seqf_t reply;
  if(!pathname) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat5(r, mk_int(r, METHOD_FSOP_CHOWN),
			   mk_int(r, nofollow),
			   mk_int(r, owner_uid),
			   mk_int(r, group_gid),
			   mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_int_const(&ok, &msg, METHOD_OKAY);
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


export_versioned_symbol(libc, new_chown, chown, GLIBC_2_1);
#if SHLIB_COMPAT(libc, GLIBC_2_0, GLIBC_2_1)
export_compat_symbol(libc, new_chown, chown, GLIBC_2_0);
#endif
export(new_chown, __chown);
export(new_chown, __GI___chown);

int new_chown(const char *pathname, unsigned int owner, unsigned int group)
{
  log_msg(MOD_MSG "chown\n");
  return my_chown(0 /* nofollow */, pathname, owner, group);
}


export(new_lchown, lchown);
export(new_lchown, __lchown);

int new_lchown(const char *pathname, unsigned int owner, unsigned int group)
{
  log_msg(MOD_MSG "lchown\n");
  return my_chown(1 /* nofollow */, pathname, owner, group);
}


export(new_rename, rename);
export(new_rename, __GI_rename);

int new_rename(const char *oldpath, const char *newpath)
{
  region_t r = region_make();
  seqf_t reply;
  log_msg(MOD_MSG "rename\n");
  if(!oldpath || !newpath) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat4(r, mk_int(r, METHOD_FSOP_RENAME),
			   mk_int(r, strlen(newpath)),
			   mk_string(r, newpath),
			   mk_string(r, oldpath)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_int_const(&ok, &msg, METHOD_OKAY);
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


export_weak_alias(new_link, link);
export(new_link, __link);
export(new_link, __GI_link);
export(new_link, __GI___link);

int new_link(const char *oldpath, const char *newpath)
{
  region_t r = region_make();
  seqf_t reply;
  log_msg(MOD_MSG "link\n");
  if(!oldpath || !newpath) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat4(r, mk_int(r, METHOD_FSOP_LINK),
			   mk_int(r, strlen(newpath)),
			   mk_string(r, newpath),
			   mk_string(r, oldpath)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_int_const(&ok, &msg, METHOD_OKAY);
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


export_weak_alias(new_symlink, symlink);
export(new_symlink, __symlink);
export(new_symlink, __GI_symlink);
export(new_symlink, __GI___symlink);

int new_symlink(const char *oldpath, const char *newpath)
{
  region_t r = region_make();
  seqf_t reply;
  log_msg(MOD_MSG "symlink\n");
  if(!oldpath || !newpath) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat4(r, mk_int(r, METHOD_FSOP_SYMLINK),
			   mk_int(r, strlen(newpath)),
			   mk_string(r, newpath),
			   mk_string(r, oldpath)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_int_const(&ok, &msg, METHOD_OKAY);
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


export_weak_alias(new_mkdir, mkdir);
export(new_mkdir, __mkdir);
export(new_mkdir, __GI_mkdir);
export(new_mkdir, __GI___mkdir);

int new_mkdir(const char *pathname, unsigned int mode)
{
  region_t r = region_make();
  seqf_t reply;
  log_msg(MOD_MSG "mkdir\n");
  if(!pathname) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat3(r, mk_int(r, METHOD_FSOP_MKDIR),
			   mk_int(r, mode),
			   mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_int_const(&ok, &msg, METHOD_OKAY);
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


export(new_mkfifo, mkfifo);

int new_mkfifo(const char *pathname, unsigned int mode)
{
  log_msg(MOD_MSG "mkfifo\n");
  __set_errno(ENOSYS);
  return -1;
}


export_weak_alias(new_unlink, unlink);
export(new_unlink, __unlink);
export(new_unlink, __GI_unlink);
export(new_unlink, __GI___unlink);

int new_unlink(const char *pathname)
{
  region_t r = region_make();
  seqf_t reply;
  log_msg(MOD_MSG "unlink\n");
  if(!pathname) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat2(r, mk_int(r, METHOD_FSOP_UNLINK),
			   mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_int_const(&ok, &msg, METHOD_OKAY);
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


export_weak_alias(new_rmdir, rmdir);
export(new_rmdir, __rmdir);
export(new_rmdir, __GI_rmdir);
export(new_rmdir, __GI___rmdir);

int new_rmdir(const char *pathname)
{
  region_t r = region_make();
  seqf_t reply;
  log_msg(MOD_MSG "rmdir\n");
  if(!pathname) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r, cat2(r, mk_int(r, METHOD_FSOP_RMDIR),
			   mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_int_const(&ok, &msg, METHOD_OKAY);
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


export_weak_alias(new_statfs, statfs);
export(new_statfs, __statfs);
export(new_statfs, __GI_statfs);
export(new_statfs, __GI___statfs);

int new_statfs(const char *path, struct statfs *buf)
{
  char *v = getenv("PLASH_FAKE_STATFS_AVAIL");
  log_msg(MOD_MSG "statfs\n");
  if(v) {
    int free = my_atoi(v);
    memset(buf, 0, sizeof(struct statfs));
    buf->f_type = 0;
    buf->f_bsize = 1024;
    buf->f_blocks = free;
    buf->f_bfree = free;
    buf->f_bavail = free;
    buf->f_files = free;
    buf->f_ffree = free;
    buf->f_namelen = 1024;
    return 0;
  }
  else {
    __set_errno(ENOSYS);
    return -1;
  }
}


export(new_setxattr, setxattr);
export(new_setxattr, __GI_setxattr);

int new_setxattr(const char *path, const char *name,
		 const void *value, size_t size, int flags)
{
  __set_errno(ENOSYS);
  return -1;
}

export(new_getxattr, getxattr);
export(new_getxattr, __GI_getxattr);

ssize_t new_getxattr(const char *path, const char *name,
		     void *value, size_t size)
{
  __set_errno(ENOSYS);
  return -1;
}

export(new_listxattr, listxattr);
export(new_listxattr, __GI_listxattr);

ssize_t new_listxattr(const char *path, char *list, size_t size)
{
  __set_errno(ENOSYS);
  return -1;
}

export(new_removexattr, removexattr);
export(new_removexattr, __GI_removexattr);

int new_removexattr(const char *path, const char *name)
{
  __set_errno(ENOSYS);
  return -1;
}


export(new_lsetxattr, lsetxattr);
export(new_lsetxattr, __GI_lsetxattr);

int new_lsetxattr(const char *path, const char *name,
		  const void *value, size_t size, int flags)
{
  __set_errno(ENOSYS);
  return -1;
}

export(new_lgetxattr, lgetxattr);
export(new_lgetxattr, __GI_lgetxattr);

ssize_t new_lgetxattr(const char *path, const char *name,
		      void *value, size_t size)
{
  __set_errno(ENOSYS);
  return -1;
}

export(new_llistxattr, llistxattr);
export(new_llistxattr, __GI_llistxattr);

ssize_t new_llistxattr(const char *path, char *list, size_t size)
{
  __set_errno(ENOSYS);
  return -1;
}

export(new_lremovexattr, lremovexattr);
export(new_lremovexattr, __GI_lremovexattr);

int new_lremovexattr(const char *path, const char *name)
{
  __set_errno(ENOSYS);
  return -1;
}
