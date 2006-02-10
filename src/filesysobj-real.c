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
#include "cap-protocol.h"


#define MOD_DEBUG 0
#define MOD_LOG 1
#define MOD_MSG "filesysobj: "

static FILE *server_log = 0; /* FIXME */


struct filesys_obj *initial_dir(const char *pathname, int *err)
{
  struct stat stat;
  int fd;
  struct real_dir *new_obj;
  
  fd = open(pathname, O_RDONLY | O_DIRECTORY);
  if(fd < 0) { *err = errno; return 0; }
  if(fstat(fd, &stat) < 0) { *err = errno; return 0; }
  new_obj = amalloc(sizeof(struct real_dir));
  new_obj = amalloc(sizeof(struct real_dir));
  new_obj->hdr.refcount = 1;
  new_obj->hdr.vtable = &real_dir_vtable;
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
  /* Couldn't open the directory; we don't have an FD for it. */
  if(!file->dir_fd) { *err = EIO; return -1; }
  if(fchdir(file->dir_fd->fd) < 0) { *err = errno; return -1; }
  if(chmod(file->leaf, mode) < 0) { *err = errno; return -1; }
  return 0;
}

int real_dir_chmod(struct filesys_obj *obj, int mode, int *err)
{
  struct real_dir *dir = (void *) obj;
  /* Couldn't open the directory; we don't have an FD for it. */
  if(!dir->fd) { *err = EIO; return -1; }
  if(fchmod(dir->fd->fd, mode) < 0) { *err = errno; return -1; }
  return 0;
}

/* NB. This is vulnerable to race conditions.  The utimes() syscall will
   follow symlinks, so someone could replace a file with a symlink.
   This is not a serious problem, as it won't grant access to files
   (except under contrived circumstances).
   This could be fixed by using lutimes(), but that syscall is not
   implemented. */
int real_file_utimes(struct filesys_obj *obj, const struct timeval *atime,
		     const struct timeval *mtime, int *err)
{
  struct real_file *file = (void *) obj;
  struct timeval times[2];
  /* Couldn't open the directory; we don't have an FD for it. */
  if(!file->dir_fd) { *err = EIO; return -1; }
  if(fchdir(file->dir_fd->fd) < 0) { *err = errno; return -1; }
  times[0] = *atime;
  times[1] = *mtime;
#ifdef HAVE_LUTIMES
  if(lutimes(file->leaf, times) < 0) {
    if(errno == ENOSYS) {
      if(utimes(file->leaf, times) < 0) { *err = errno; return -1; }
      return 0;
    }
    *err = errno;
    return -1;
  }
#else
  if(utimes(file->leaf, times) < 0) { *err = errno; return -1; }
  return 0;
#endif
  return 0;
}

int real_dir_utimes(struct filesys_obj *obj, const struct timeval *atime,
		    const struct timeval *mtime, int *err)
{
  struct real_dir *dir = (void *) obj;
  struct timeval times[2];
  /* Couldn't open the directory; we don't have an FD for it. */
  if(!dir->fd) { *err = EIO; return -1; }
  if(fchdir(dir->fd->fd) < 0) { *err = errno; return -1; }
  times[0] = *atime;
  times[1] = *mtime;
  if(utimes(".", times) < 0) { *err = errno; return -1; }
  return 0;
}

/* Won't work in practice, because lutimes() is not implemented. */
int real_symlink_utimes(struct filesys_obj *obj, const struct timeval *atime,
			const struct timeval *mtime, int *err)
{
#ifdef HAVE_LUTIMES
  struct real_symlink *symlink = (void *) obj;
  struct timeval times[2];
  /* Couldn't open the directory; we don't have an FD for it. */
  if(!symlink->dir_fd) { *err = EIO; return -1; }
  if(fchdir(symlink->dir_fd->fd) < 0) { *err = errno; return -1; }
  times[0] = atime;
  times[1] = mtime;
  if(lutimes(symlink->leaf, times) < 0) { *err = errno; return -1; }
  return 0;
#else
  *err = ENOSYS;
  return -1;
#endif
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
  if(leaf[0] == '.' && (!leaf[1] || (leaf[1] == '.' && !leaf[2]))) return 0;

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
  
  if(fchdir(dir->fd->fd) < 0) { return 0; }
  if(lstat(leaf, &stat) < 0) { return 0; }

  if(S_ISDIR(stat.st_mode)) {
    /* We open the directory presumptively, on the basis that the common
       case is to immediately look up something in it.  Opening it sooner
       rather than later reduces the possibility of it being replaced by
       a symlink between lstat'ing it and opening it.  It doesn't matter
       if the open fails.  We can open it because there's only one way
       to open directories -- read only.  The same is not true for files,
       and we can't open symlinks themselves. */
    int fd = open(leaf, O_RDONLY | O_NOFOLLOW | O_DIRECTORY);
    /* If O_NOFOLLOW doesn't work, we can fstat here. */
    struct real_dir *new_obj;
    if(fd < 0 && errno == ELOOP) {
      /* Dir changed to a symlink underneath us; could retry -- FIXME */
      return 0;
    }
    set_close_on_exec_flag(fd, 1);
    new_obj = amalloc(sizeof(struct real_dir));
    new_obj->hdr.refcount = 1;
    new_obj->hdr.vtable = &real_dir_vtable;
    new_obj->stat = stat;
    new_obj->fd = fd < 0 ? 0 : make_fd(fd);
    return (struct filesys_obj *) new_obj;
  }
  else if(S_ISLNK(stat.st_mode)) {
    /* Could do readlink presumptively, but it's not always called, so
       may as well save the syscall, which could involve reading a block
       from disc.  However, this means that readlink can fail. */
    struct real_symlink *new_obj = amalloc(sizeof(struct real_symlink));
    new_obj->hdr.refcount = 1;
    new_obj->hdr.vtable = &real_symlink_vtable;
    new_obj->stat = stat;
    new_obj->dir_fd = dir->fd;
    new_obj->leaf = strdup(leaf);
    dir->fd->refcount++;
    return (struct filesys_obj *) new_obj;
  }
  else {
    struct real_file *new_obj = amalloc(sizeof(struct real_file));
    new_obj->hdr.refcount = 1;
    new_obj->hdr.vtable = &real_file_vtable;
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
  
  if(fchdir(dir->fd->fd) < 0) { *err = errno; return -1; }
  dh = opendir(".");
  if(!dh) { *err = errno; return -1; }
  while(1) {
    ent = readdir64(dh);
    if(!ent) break;
    /* Don't list "." and ".." here.  These are added to the listing in
       another part of the code.  Since the directory may be reparented,
       we don't want to give the inode number here. */
    if(!(ent->d_name[0] == '.' &&
	 (!ent->d_name[1] || (ent->d_name[1] == '.' && !ent->d_name[2])))) {
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
  
  if(fchdir(dir->fd->fd) < 0) { *err = errno; return -1; }
  if(mkdir(leaf, mode) < 0) { *err = errno; return -1; }
  return 0;
}

int real_dir_symlink(struct filesys_obj *obj, const char *leaf,
		     const char *oldpath, int *err)
{
  struct real_dir *dir = (void *) obj;

  if(!leafname_ok(leaf)) { *err = ENOENT; return -1; }

  /* Couldn't open the directory; we don't have an FD for it. */
  if(!dir->fd) { *err = EIO; return -1; }
  
  if(fchdir(dir->fd->fd) < 0) { *err = errno; return -1; }
  if(symlink(oldpath, leaf) < 0) { *err = errno; return -1; }
  return 0;
}

int real_dir_rename(struct filesys_obj *obj, const char *leaf,
		    struct filesys_obj *dest_dir1, const char *dest_leaf,
		    int *err)
{
  struct real_dir *dir = (void *) obj;

  if(!leafname_ok(leaf) || !leafname_ok(dest_leaf)) { *err = ENOENT; return -1; }

  /* Handle the same-directory case only. */
  if(dir == (void *) dest_dir1) {
    /* Couldn't open the directory; we don't have an FD for it. */
    if(!dir->fd) { *err = EIO; return -1; }
    
    if(fchdir(dir->fd->fd) < 0) { *err = errno; return -1; }
    if(rename(leaf, dest_leaf) < 0) { *err = errno; return -1; }
    return 0;
  }
  else {
    *err = EXDEV;
    return -1;
  }
  /*
  if(dest_dir1->vtable == &real_dir_vtable) {
    struct real_dir *dest_dir = (void *) dest_dir1;
  }
  */
}

int real_dir_link(struct filesys_obj *obj, const char *leaf,
		  struct filesys_obj *dest_dir1, const char *dest_leaf,
		  int *err)
{
  struct real_dir *dir = (void *) obj;

  if(!leafname_ok(leaf) || !leafname_ok(dest_leaf)) { *err = ENOENT; return -1; }

  /* Handle the same-directory case only. */
  if(dir == (void *) dest_dir1) {
    /* Couldn't open the directory; we don't have an FD for it. */
    if(!dir->fd) { *err = EIO; return -1; }
    
    if(fchdir(dir->fd->fd) < 0) { *err = errno; return -1; }
    if(link(leaf, dest_leaf) < 0) { *err = errno; return -1; }
    return 0;
  }
  else {
    *err = EXDEV;
    return -1;
  }
}

int real_dir_unlink(struct filesys_obj *obj, const char *leaf, int *err)
{
  struct real_dir *dir = (void *) obj;

  if(!leafname_ok(leaf)) { *err = ENOENT; return -1; }

  /* Couldn't open the directory; we don't have an FD for it. */
  if(!dir->fd) { *err = EIO; return -1; }
  
  if(fchdir(dir->fd->fd) < 0) { *err = errno; return -1; }
  if(unlink(leaf) < 0) { *err = errno; return -1; }
  return 0;
}

int real_dir_rmdir(struct filesys_obj *obj, const char *leaf, int *err)
{
  struct real_dir *dir = (void *) obj;

  if(!leafname_ok(leaf)) { *err = ENOENT; return -1; }

  /* Couldn't open the directory; we don't have an FD for it. */
  if(!dir->fd) { *err = EIO; return -1; }
  
  if(fchdir(dir->fd->fd) < 0) { *err = errno; return -1; }
  if(rmdir(leaf) < 0) { *err = errno; return -1; }
  return 0;
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
  
  if(fchdir(dir->fd->fd) < 0) { *err = errno; return -1; }
  fd = open(leaf, flags | O_CREAT | O_EXCL | O_NOFOLLOW, mode);
  if(fd < 0) {
    *err = errno;
    return -1;
  }
  set_close_on_exec_flag(fd, 1);

  /* The file might have changed underneath us.  We must make sure that
     it didn't change to a directory.  We must never pass the process a
     FD for a directory, because it could use it to break out of a chroot
     jail.  Passing devices is okay, though. */
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
  if(fchdir(file->dir_fd->fd) < 0) { *err = errno; return -1; }
  fd = open(file->leaf, (flags | O_NOFOLLOW) & ~O_CREAT);
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
  struct sockaddr_un name;

  int len = strlen(file->leaf);
  if(len + 1 > sizeof(name.sun_path)) {
    *err = ENAMETOOLONG;
    return -1;
  }
  
  if(fchdir(file->dir_fd->fd) < 0) { *err = errno; return -1; }
  name.sun_family = AF_LOCAL;
  memcpy(name.sun_path, file->leaf, len);
  name.sun_path[len] = 0;
  if(connect(sock_fd, &name,
	     offsetof(struct sockaddr_un, sun_path) + len + 1) < 0) {
    *err = errno;
    return -1;
  }
  return 0;
}

int real_dir_socket_bind(struct filesys_obj *obj, const char *leaf,
			 int sock_fd, int *err)
{
  struct real_dir *dir = (void *) obj;
  struct sockaddr_un name;

  int len = strlen(leaf);
  if(len + 1 > sizeof(name.sun_path)) {
    *err = ENAMETOOLONG;
    return -1;
  }

  if(!leafname_ok(leaf)) { *err = ENOENT; return -1; }
  
  /* Couldn't open the directory; we don't have an FD for it. */
  if(!dir->fd) { *err = EIO; return -1; }
  
  if(fchdir(dir->fd->fd) < 0) { *err = errno; return -1; }
  name.sun_family = AF_LOCAL;
  memcpy(name.sun_path, leaf, len);
  name.sun_path[len] = 0;
  if(bind(sock_fd, &name,
	  offsetof(struct sockaddr_un, sun_path) + len + 1) < 0) {
    *err = errno;
    return -1;
  }
  return 0;
}

int real_symlink_readlink(struct filesys_obj *obj, region_t r, seqf_t *result, int *err)
{
  struct real_symlink *sym = (void *) obj;
  int got;
  char *link;
  char buf[10*1024];

  /* FIXME: check to see if we're already in dir_fd */
  if(fchdir(sym->dir_fd->fd) < 0) {
    if(MOD_DEBUG) fprintf(server_log, MOD_MSG "can't fchdir\n");
    *err = EIO;
    return -1;
  }
  got = readlink(sym->leaf, buf, sizeof(buf));
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


#if 0
struct filesys_obj_vtable real_dir_vtable = {
  /* .free = */ real_dir_free,
  /* .cap_invoke = */ local_obj_invoke,
  /* .cap_call = */ marshal_cap_call,
  /* .single_use = */ 0,
  /* .type = */ objt_dir,
  /* .stat = */ real_dir_stat,
  /* .utimes = */ real_dir_utimes,
  /* .chmod = */ real_dir_chmod,
  /* .open = */ dummy_open,
  /* .socket_connect = */ dummy_socket_connect,
  /* .traverse = */ real_dir_traverse,
  /* .list = */ real_dir_list,
  /* .create_file = */ real_dir_create_file,
  /* .mkdir = */ real_dir_mkdir,
  /* .symlink = */ real_dir_symlink,
  /* .rename = */ real_dir_rename,
  /* .link = */ real_dir_link,
  /* .unlink = */ real_dir_unlink,
  /* .rmdir = */ real_dir_rmdir,
  /* .socket_bind = */ real_dir_socket_bind,
  /* .readlink = */ dummy_readlink,
  1
};

struct filesys_obj_vtable real_file_vtable = {
  /* .free = */ real_file_free,
  /* .cap_invoke = */ local_obj_invoke,
  /* .cap_call = */ marshal_cap_call,
  /* .single_use = */ 0,
  /* .type = */ objt_file,
  /* .stat = */ real_file_stat,
  /* .utimes = */ real_file_utimes,
  /* .chmod = */ real_file_chmod,
  /* .open = */ real_file_open,
  /* .socket_connect = */ real_file_socket_connect,
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
  /* .readlink = */ dummy_readlink,
  1
};

struct filesys_obj_vtable real_symlink_vtable = {
  /* .free = */ real_symlink_free,
  /* .cap_invoke = */ local_obj_invoke,
  /* .cap_call = */ marshal_cap_call,
  /* .single_use = */ 0,
  /* .type = */ objt_symlink,
  /* .stat = */ real_symlink_stat,
  /* .utimes = */ real_symlink_utimes,
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
  /* .readlink = */ real_symlink_readlink,
  1
};
#endif

#include "out-vtable-filesysobj-real.h"
