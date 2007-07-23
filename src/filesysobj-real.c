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

/* Necessary to get O_DIRECTORY and O_NOFOLLOW */
/* #define __USE_GNU */
#define _GNU_SOURCE

#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>

#include "region.h"
#include "serialise.h"
#include "filesysobj.h"
#include "filesysobj-real.h"
#include "cap-protocol.h"


#define MOD_DEBUG 0
#define MOD_LOG 1
#define MOD_MSG "filesysobj: "

static FILE *server_log = 0; /* FIXME */


static int
proc_connectat(int dir_fd, int sock_fd, const char *filename)
{
  struct sockaddr_un name;
  int path_len;

  name.sun_family = AF_LOCAL;
  path_len = snprintf(name.sun_path, sizeof(name.sun_path),
		      "/proc/self/fd/%i/%s", dir_fd, filename);
  if(path_len >= sizeof(name.sun_path) || path_len < 0) {
    errno = ENAMETOOLONG;
    return -1;
  }
  return connect(sock_fd, &name,
		 offsetof(struct sockaddr_un, sun_path) + path_len + 1);
}

static int
proc_bindat(int dir_fd, int sock_fd, const char *filename)
{
  struct sockaddr_un name;
  int path_len;

  name.sun_family = AF_LOCAL;
  path_len = snprintf(name.sun_path, sizeof(name.sun_path),
		      "/proc/self/fd/%i/%s", dir_fd, filename);
  if(path_len >= sizeof(name.sun_path) || path_len < 0) {
    errno = ENAMETOOLONG;
    return -1;
  }
  return bind(sock_fd, &name,
	      offsetof(struct sockaddr_un, sun_path) + path_len + 1);
}


struct filesys_obj *initial_dir(const char *pathname, int *err)
{
  struct stat stat;
  int fd;
  struct real_dir *new_obj;
  
  fd = open(pathname, O_RDONLY | O_DIRECTORY);
  if(fd < 0) { *err = errno; return 0; }
  set_close_on_exec_flag(fd, 1);
  if(fstat(fd, &stat) < 0) { *err = errno; return 0; }
  new_obj = filesys_obj_make(sizeof(struct real_dir), &real_dir_vtable);
  new_obj->stat = stat;
  new_obj->fd = make_fd(fd);
  return (void *) new_obj;
}


void real_dir_free(struct filesys_obj *obj1)
{
  struct real_dir *obj = (void *) obj1;
  if(obj->fd) free_fd(obj->fd);
}

void real_file_free(struct filesys_obj *obj1)
{
  struct real_file *obj = (void *) obj1;
  free_fd(obj->dir_fd);
  free(obj->leaf);
}

void real_symlink_free(struct filesys_obj *obj1)
{
  struct real_symlink *obj = (void *) obj1;
  free_fd(obj->dir_fd);
  free(obj->leaf);
}

int real_dir_stat(struct filesys_obj *obj1, struct stat *buf, int *err)
{
  struct real_dir *obj = (void *) obj1;
  *buf = obj->stat;
  return 0;
}

int real_file_stat(struct filesys_obj *obj1, struct stat *buf, int *err)
{
  struct real_file *obj = (void *) obj1;
  *buf = obj->stat;
  return 0;
}

int real_symlink_stat(struct filesys_obj *obj1, struct stat *buf, int *err)
{
  struct real_symlink *obj = (void *) obj1;
  *buf = obj->stat;
  return 0;
}

int real_file_chmod(struct filesys_obj *obj, int mode, int *err)
{
  struct real_file *file = (void *) obj;

  /* Do not allow the setuid or setgid bits to be set. */
  if((mode & S_ISUID) ||
     (mode & S_ISGID)) {
    *err = EPERM;
    return -1;
  }
  
  /* Couldn't open the directory; we don't have an FD for it. */
  if(!file->dir_fd) { *err = EIO; return -1; }

  if(1) {
    /* Note that this is vulnerable to symlink race conditions.
       Would like to pass AT_SYMLINK_NOFOLLOW as the flags argument but
       the kernel does not implement this yet. */
    if(fchmodat(file->dir_fd->fd, file->leaf, mode, 0) < 0) {
      *err = errno;
      return -1;
    }
    return 0;
  }
  else {
    int inode_fd;
    int rc;
    /* We can't use chmod() because it follows symlinks, which would
       make this code subject to symlink race conditions.  glibc exports
       an lchmod() function, but it isn't implemented (it always returns
       ENOSYS).  We do the equivalent of lchmod() by opening the file
       and doing fchmod(). */
    /* Problem:  open() fails if read permissions aren't set (even if the
       user owns the file).  So this call can't re-enable read
       permissions. */
    inode_fd = openat(file->dir_fd->fd, file->leaf, O_RDONLY | O_NOFOLLOW);
    if(inode_fd < 0) {
      *err = errno;
      return -1;
    }
    rc = fchmod(inode_fd, mode);
    if(rc < 0)
      *err = errno;
    close(inode_fd);
    return rc;
  }
}

int real_dir_chmod(struct filesys_obj *obj, int mode, int *err)
{
  struct real_dir *dir = (void *) obj;
  /* Couldn't open the directory; we don't have an FD for it. */
  if(!dir->fd) { *err = EIO; return -1; }
  if(fchmod(dir->fd->fd, mode) < 0) { *err = errno; return -1; }
  return 0;
}

int real_file_utimes(struct filesys_obj *obj, const struct timeval *atime,
		     const struct timeval *mtime, int *err)
{
  struct real_file *file = (void *) obj;
  struct timeval times[2];

  times[0] = *atime;
  times[1] = *mtime;

  /* Couldn't open the directory; we don't have an FD for it. */
  if(!file->dir_fd) { *err = EIO; return -1; }

  /* Note that glibc's futimes() call does utime() on /proc/self/fd/N.
     Linux does not have a utimes() syscall. */
  int inode_fd = openat(file->dir_fd->fd, file->leaf, O_RDONLY | O_NOFOLLOW);
  if(inode_fd < 0) {
    *err = errno;
    return -1;
  }
  int rc = futimes(inode_fd, times);
  if(rc < 0)
    *err = errno;
  close(inode_fd);
  return rc;
}

int real_dir_utimes(struct filesys_obj *obj, const struct timeval *atime,
		    const struct timeval *mtime, int *err)
{
  struct real_dir *dir = (void *) obj;
  struct timeval times[2];
  /* Couldn't open the directory; we don't have an FD for it. */
  if(!dir->fd) { *err = EIO; return -1; }
  times[0] = *atime;
  times[1] = *mtime;
  int rc = futimes(dir->fd->fd, times);
  if(rc < 0)
    *err = errno;
  return rc;
}

/* Won't work in practice, because lutimes() is not implemented. */
int real_symlink_utimes(struct filesys_obj *obj, const struct timeval *atime,
			const struct timeval *mtime, int *err)
{
  *err = ENOSYS;
  return -1;
}

static int special_leafname(const char *leaf)
{
  return (strcmp(leaf, ".") == 0 ||
	  strcmp(leaf, "..") == 0);
}

/* Check that leafnames don't contain '/', and aren't "." or "..".
   These are treated specially by the pathname resolver functions and
   will not get passed to the functions here when client programs refer
   to pathnames.  But when using an object-based interface, the client
   could pass these leafnames. */
int leafname_ok(const char *leaf)
{
  const char *c;
  
  /* Disallow the leaf names "." and "..".  In practice, these are
     treated specially by the pathname resolver functions, and will
     never get passed here. */
  if(special_leafname(leaf))
    return FALSE;

  for(c = leaf; *c; c++) {
    if(*c == '/') return 0;
  }
  return 1;
}

struct filesys_obj *real_dir_traverse(struct filesys_obj *obj, const char *leaf)
{
  struct real_dir *dir = (void *) obj;
  struct stat stat;

  if(!leafname_ok(leaf)) return 0;

  /* Couldn't open the directory; we don't have an FD for it. */
  if(!dir->fd) { return 0; }
  
  /* FIXME: errno not used */
  if(fstatat(dir->fd->fd, leaf, &stat, AT_SYMLINK_NOFOLLOW) < 0)
    return NULL;

  if(S_ISDIR(stat.st_mode)) {
    /* We open the directory presumptively, on the basis that the common
       case is to immediately look up something in it.  Opening it sooner
       rather than later reduces the possibility of it being replaced by
       a symlink between lstat'ing it and opening it.  It doesn't matter
       if the open fails.  We can open it because there's only one way
       to open directories -- read only.  The same is not true for files,
       and we can't open symlinks themselves. */
    int fd = openat(dir->fd->fd, leaf,
		    O_RDONLY | O_NOFOLLOW | O_DIRECTORY, 0);
    /* If O_NOFOLLOW doesn't work, we can fstat here. */
    struct real_dir *new_obj;
    if(fd < 0 && errno == ELOOP) {
      /* Dir changed to a symlink underneath us; could retry -- FIXME */
      return 0;
    }
    if(fd >= 0) { set_close_on_exec_flag(fd, 1); }
    new_obj = filesys_obj_make(sizeof(struct real_dir), &real_dir_vtable);
    new_obj->stat = stat;
    new_obj->fd = fd < 0 ? 0 : make_fd(fd);
    return (struct filesys_obj *) new_obj;
  }
  else if(S_ISLNK(stat.st_mode)) {
    /* Could do readlink presumptively, but it's not always called, so
       may as well save the syscall, which could involve reading a block
       from disc.  However, this means that readlink can fail. */
    struct real_symlink *new_obj =
      filesys_obj_make(sizeof(struct real_symlink), &real_symlink_vtable);
    new_obj->stat = stat;
    new_obj->dir_fd = dir->fd;
    new_obj->leaf = strdup(leaf);
    dir->fd->refcount++;
    return (struct filesys_obj *) new_obj;
  }
  else {
    struct real_file *new_obj =
      filesys_obj_make(sizeof(struct real_file), &real_file_vtable);
    new_obj->stat = stat;
    new_obj->dir_fd = dir->fd;
    new_obj->leaf = strdup(leaf);
    dir->fd->refcount++;
    return (struct filesys_obj *) new_obj;
  }
}

int real_dir_list(struct filesys_obj *obj, region_t r, seqt_t *result, int *err)
{
  struct real_dir *dir = (void *) obj;
  DIR *dh;
  /* dietlibc's struct dirent doesn't include d_type */
  struct dirent64 *ent;
  cbuf_t buf = cbuf_make(r, 100);
  int count = 0;

  /* Couldn't open the directory; we don't have an FD for it. */
  if(!dir->fd) { *err = EIO; return -1; }
  
  /* dup() is not good enough here: we need to get an FD with its
     position at the beginning of the directory. */
  int dir_fd = openat(dir->fd->fd, ".", O_RDONLY);
  if(dir_fd < 0) {
    *err = errno;
    return -1;
  }
  dh = fdopendir(dir_fd);
  if(!dh) {
    *err = errno;
    close(dir_fd);
    return -1;
  }

  while(1) {
    ent = readdir64(dh);
    if(!ent)
      break;
    /* Don't list "." and ".." here.  These are added to the listing in
       another part of the code.  Since the directory may be reparented,
       we don't want to give the inode number here. */
    if(!special_leafname(ent->d_name)) {
      seqf_t name = seqf_string(ent->d_name);
      cbuf_put_int(buf, ent->d_ino);
      cbuf_put_int(buf, ent->d_type);
      cbuf_put_int(buf, name.size);
      cbuf_put_seqf(buf, name);
      count++;
    }
  }
  closedir(dh);
  *result = seqt_of_cbuf(buf);
  return count;
}

int real_dir_mkdir(struct filesys_obj *obj, const char *leaf, int mode, int *err)
{
  struct real_dir *dir = (void *) obj;

  if(!leafname_ok(leaf)) { *err = ENOENT; return -1; }

  /* Couldn't open the directory; we don't have an FD for it. */
  if(!dir->fd) { *err = EIO; return -1; }
  
  int rc = mkdirat(dir->fd->fd, leaf, mode);
  if(rc < 0)
    *err = errno;
  return rc;
}

int real_dir_symlink(struct filesys_obj *obj, const char *leaf,
		     const char *oldpath, int *err)
{
  struct real_dir *dir = (void *) obj;

  if(!leafname_ok(leaf)) { *err = ENOENT; return -1; }

  /* Couldn't open the directory; we don't have an FD for it. */
  if(!dir->fd) { *err = EIO; return -1; }
  
  int rc = symlinkat(oldpath, dir->fd->fd, leaf);
  if(rc < 0)
    *err = errno;
  return rc;
}

int real_dir_rename(struct filesys_obj *obj, const char *leaf,
		    struct filesys_obj *dest_dir, const char *dest_leaf,
		    int *err)
{
  struct real_dir *dir = (void *) obj;

  if(!leafname_ok(leaf) || !leafname_ok(dest_leaf)) { *err = ENOENT; return -1; }

  if(dest_dir->vtable == &real_dir_vtable) {
    struct real_dir *real_dest_dir = (struct real_dir *) dest_dir;
    int rc = renameat(dir->fd->fd, leaf,
		      real_dest_dir->fd->fd, dest_leaf);
    if(rc < 0)
      *err = errno;
    return rc;
  }
  *err = EXDEV;
  return -1;
}

int real_dir_link(struct filesys_obj *obj, const char *leaf,
		  struct filesys_obj *dest_dir, const char *dest_leaf,
		  int *err)
{
  struct real_dir *dir = (void *) obj;

  if(!leafname_ok(leaf) || !leafname_ok(dest_leaf)) { *err = ENOENT; return -1; }

  if(dest_dir->vtable == &real_dir_vtable) {
    struct real_dir *real_dest_dir = (struct real_dir *) dest_dir;
    int rc = linkat(dir->fd->fd, leaf,
		    real_dest_dir->fd->fd, dest_leaf, 0);
    if(rc < 0)
      *err = errno;
    return rc;
  }
  *err = EXDEV;
  return -1;
}

int real_dir_unlink(struct filesys_obj *obj, const char *leaf, int *err)
{
  struct real_dir *dir = (void *) obj;

  if(!leafname_ok(leaf)) { *err = ENOENT; return -1; }

  /* Couldn't open the directory; we don't have an FD for it. */
  if(!dir->fd) { *err = EIO; return -1; }
  
  int rc = unlinkat(dir->fd->fd, leaf, 0);
  if(rc < 0)
    *err = errno;
  return rc;
}

int real_dir_rmdir(struct filesys_obj *obj, const char *leaf, int *err)
{
  struct real_dir *dir = (void *) obj;

  if(!leafname_ok(leaf)) { *err = ENOENT; return -1; }

  /* Couldn't open the directory; we don't have an FD for it. */
  if(!dir->fd) { *err = EIO; return -1; }
  
  int rc = unlinkat(dir->fd->fd, leaf, AT_REMOVEDIR);
  if(rc < 0)
    *err = errno;
  return rc;
}

/* Irrelevant flags:
   O_CREAT: assumed
   O_EXCL: used anyway
   O_NOFOLLOW: used anyway
   O_DIRECTORY: assumed to be unset
   O_NOCTTY, O_TRUNC, O_APPEND: only for existing files or devices
   Relevant:
   O_RDONLY vs. O_WRONLY vs. O_RDWR: although we don't expect O_RDONLY to be used much
   O_NONBLOCK/O_NDELAY
   O_SYNC/O_DSYNC/O_RSYNC
   O_LARGEFILE */
int real_dir_create_file(struct filesys_obj *obj, const char *leaf,
                         int flags, int mode, int *err)
{
  struct real_dir *dir = (void *) obj;
  int fd;
  struct stat st;

  if(!leafname_ok(leaf)) { *err = ENOENT; return -1; }
  
  /* Do not allow the setuid or setgid bits to be set. */
  if((mode & S_ISUID) ||
     (mode & S_ISGID)) {
    *err = EPERM;
    return -1;
  }

  if(flags &
     ~(O_ACCMODE | O_CREAT | O_EXCL | O_NOCTTY | O_TRUNC | O_APPEND |
       O_NONBLOCK | O_NDELAY | O_SYNC |
       O_DIRECTORY | O_NOFOLLOW | O_LARGEFILE)) {
    /* Unrecognised flags */
    if(MOD_DEBUG) fprintf(server_log, MOD_MSG "create_file: unrecognised flags: 0o%o\n", flags);
    *err = EINVAL;
    return -1;
  }

  /* Couldn't open the directory; we don't have an FD for it. */
  if(!dir->fd) { *err = EIO; return -1; }
  
  fd = openat(dir->fd->fd, leaf, flags | O_CREAT | O_EXCL | O_NOFOLLOW, mode);
  if(fd < 0) {
    *err = errno;
    return -1;
  }
  set_close_on_exec_flag(fd, 1);

  /* The file might have changed underneath us.  We must make sure that
     it didn't change to a directory.  We must never pass the process a
     FD for a directory, because it could use it to break out of a chroot
     jail.  Passing devices is okay, though. */
  /* This shouldn't happen, though, because we used O_EXCL. */
  /* We could also check that the inode number didn't change. */
  if(fstat(fd, &st) < 0) { close(fd); *err = errno; return -1; }
  if(S_ISDIR(st.st_mode)) {
    if(MOD_DEBUG) fprintf(server_log, MOD_MSG "create_file: turned into a directory\n");
    close(fd);
    *err = EIO;
    return -1;
  }
  return fd;
}

int real_file_open(struct filesys_obj *obj, int flags, int *err)
{
  struct real_file *file = (void *) obj;
  int fd;
  struct stat st;

  if(flags &
     ~(O_ACCMODE | O_CREAT | O_EXCL | O_NOCTTY | O_TRUNC | O_APPEND |
       O_NONBLOCK | O_NDELAY | O_SYNC |
       O_DIRECTORY | O_NOFOLLOW | O_LARGEFILE)) {
    /* Unrecognised flags */
    if(MOD_DEBUG) fprintf(server_log, MOD_MSG "open: unrecognised flags: 0o%o\n", flags);
    *err = EINVAL;
    return -1;
  }

  /* FIXME: check to see if we're already in dir_fd */
  fd = openat(file->dir_fd->fd, file->leaf, (flags | O_NOFOLLOW) & ~O_CREAT);
  if(fd < 0) {
    *err = errno;
    return -1;
  }
  set_close_on_exec_flag(fd, 1);
  
  /* If O_NOFOLLOW doesn't work, we can fstat here. */

  /* Check that the file wasn't replaced by a directory.  See note above. */
  if(fstat(fd, &st) < 0) { close(fd); *err = errno; return -1; }
  if(S_ISDIR(st.st_mode)) {
    if(MOD_DEBUG) fprintf(server_log, MOD_MSG "open: turned into a directory\n");
    close(fd);
    *err = EIO;
    return -1;
  }
  return fd;
}

int real_file_socket_connect(struct filesys_obj *obj, int sock_fd, int *err)
{
  struct real_file *file = (void *) obj;

  /* First, don't go through the hassle below if the object is not a
     Unix domain socket.  We don't really want to hard link ordinary
     files into /tmp. */
  if(!S_ISSOCK(file->stat.st_mode)) {
    *err = ECONNREFUSED;
    return -1;
  }

  /* Couldn't open the directory; we don't have an FD for it. */
  if(!file->dir_fd) {
    *err = EIO;
    return -1;
  }

  /* This code is vulnerable to symlink race conditions. */
  int rc = proc_connectat(file->dir_fd->fd, sock_fd, file->leaf);
  if(rc < 0)
    *err = errno;
  return rc;
}

int real_dir_socket_bind(struct filesys_obj *obj, const char *leaf,
			 int sock_fd, int *err)
{
  struct real_dir *dir = (void *) obj;

  if(!leafname_ok(leaf)) { *err = ENOENT; return -1; }
  
  /* Couldn't open the directory; we don't have an FD for it. */
  if(!dir->fd) { *err = EIO; return -1; }

  /* Note that bind() should not follow symlinks when creating the
     Unix domain socket, so there should be no symlink race condition. */
  int rc = proc_bindat(dir->fd->fd, sock_fd, leaf);
  if(rc < 0)
    *err = errno;
  return rc;
}

int real_symlink_readlink(struct filesys_obj *obj, region_t r, seqf_t *result, int *err)
{
  struct real_symlink *sym = (void *) obj;
  int got;
  char *link;
  char buf[10*1024];

  got = readlinkat(sym->dir_fd->fd, sym->leaf, buf, sizeof(buf));
  if(got < 0) {
    if(MOD_DEBUG) fprintf(server_log, MOD_MSG "can't readlink\n");
    if(errno == EINVAL || errno == ENOENT) {
      /* We get "Invalid argument" if the file was not a symlink
	 (or if the buffer size was not positive), which means the
	 file changed from a symlink underneath us. */
      *err = EIO;
      return -1;
    }
    *err = errno;
    return -1;
  }
  if(got == sizeof(buf)) {
    /* Link truncated (unless it was exactly the size of the buffer).
       Could retry. FIXME */
    if(MOD_DEBUG) fprintf(server_log, MOD_MSG "readlink truncated\n");
    *err = EIO;
    return -1;
  }
  link = region_alloc(r, got);
  memcpy(link, buf, got);
  result->data = link;
  result->size = got;
  if(MOD_DEBUG) {
    fprintf(server_log, MOD_MSG "readlink got ");
    fwrite(link, got, 1, server_log);
    fprintf(server_log, "\n");
  }
  return 0;
}


#include "out-vtable-filesysobj-real.h"
