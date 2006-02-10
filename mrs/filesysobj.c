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

#include "region.h"
#include "server.h"
#include "parse-filename.h"
#include "comms.h"


#define MOD_DEBUG 0
#define MOD_LOG 1
#define MOD_MSG "filesysobj: "

static FILE *server_log = 0; /* FIXME */


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

int real_dir_stat(struct filesys_obj *obj1, struct stat *buf)
{
  struct real_dir *obj = (void *) obj1;
  *buf = obj->stat;
  return 0;
}

int real_file_stat(struct filesys_obj *obj1, struct stat *buf)
{
  struct real_file *obj = (void *) obj1;
  *buf = obj->stat;
  return 0;
}

int real_symlink_stat(struct filesys_obj *obj1, struct stat *buf)
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

/* FIXME: check that leafnames don't contain '/' in any of the "real" functions */

struct filesys_obj *real_dir_traverse(struct filesys_obj *obj, const char *leaf)
{
  struct real_dir *dir = (void *) obj;
  struct stat stat;

  /* Disallow the leaf names "." and "..".  In practice, these are
     treated specially by the pathname resolver functions, and will
     never get passed here. */
  if(leaf[0] == '.' && (!leaf[1] || (leaf[1] == '.' && !leaf[2]))) return 0;

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
  seqt_t got = seqt_empty;

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
      /* FIXME: this is extremely wasteful of space: */
      int len = strlen(ent->d_name);
      char *str = region_alloc(r, len);
      memcpy(str, ent->d_name, len);
      got = cat5(r, got,
		 mk_int(r, ent->d_ino),
		 mk_int(r, ent->d_type),
		 mk_int(r, len),
		 mk_leaf2(r, str, len));
    }
  }
  closedir(dh);
  *result = got;
  return 0;
}

int real_dir_mkdir(struct filesys_obj *obj, const char *leaf, int mode, int *err)
{
  struct real_dir *dir = (void *) obj;

  /* Couldn't open the directory; we don't have an FD for it. */
  if(!dir->fd) { *err = EIO; return -1; }
  
  if(fchdir(dir->fd->fd) < 0) { *err = errno; return -1; }
  if(mkdir(leaf, mode) < 0) { *err = errno; return -1; }
  return 0;
}

int real_dir_unlink(struct filesys_obj *obj, const char *leaf, int *err)
{
  struct real_dir *dir = (void *) obj;

  /* Couldn't open the directory; we don't have an FD for it. */
  if(!dir->fd) { *err = EIO; return -1; }
  
  if(fchdir(dir->fd->fd) < 0) { *err = errno; return -1; }
  if(unlink(leaf) < 0) { *err = errno; return -1; }
  return 0;
}

int real_dir_rmdir(struct filesys_obj *obj, const char *leaf, int *err)
{
  struct real_dir *dir = (void *) obj;

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

int real_file_connect(struct filesys_obj *obj, int sock_fd, int *err)
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
	     offsetof(struct sockaddr_un, sun_path) + len + 1) < 0)
    { *err = errno; return -1; }
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

int dummy_connect(struct filesys_obj *obj, int sock_fd, int *err)
{
  *err = ENOSYS;
  return -1;
}


struct filesys_obj_vtable real_dir_vtable = {
  /* .type = */ OBJT_DIR,
  /* .free = */ real_dir_free,
  /* .stat = */ real_dir_stat,
  /* .chmod = */ real_dir_chmod,
  /* .open = */ dummy_open,
  /* .connect = */ dummy_connect,
  /* .traverse = */ real_dir_traverse,
  /* .list = */ real_dir_list,
  /* .create_file = */ real_dir_create_file,
  /* .mkdir = */ real_dir_mkdir,
  /* .unlink = */ real_dir_unlink,
  /* .rmdir = */ real_dir_rmdir,
  /* .readlink = */ dummy_readlink,
  1
};

struct filesys_obj_vtable real_file_vtable = {
  /* .type = */ OBJT_FILE,
  /* .free = */ real_file_free,
  /* .stat = */ real_file_stat,
  /* .chmod = */ real_file_chmod,
  /* .open = */ real_file_open,
  /* .connect = */ real_file_connect,
  /* .traverse = */ dummy_traverse,
  /* .list = */ dummy_list,
  /* .create_file = */ dummy_create_file,
  /* .mkdir = */ dummy_mkdir,
  /* .unlink = */ dummy_unlink,
  /* .rmdir = */ dummy_rmdir,
  /* .readlink = */ dummy_readlink,
  1
};

struct filesys_obj_vtable real_symlink_vtable = {
  /* .type = */ OBJT_SYMLINK,
  /* .free = */ real_symlink_free,
  /* .stat = */ real_symlink_stat,
  /* .chmod = */ dummy_chmod,
  /* .open = */ dummy_open,
  /* .connect = */ dummy_connect,
  /* .traverse = */ dummy_traverse,
  /* .list = */ dummy_list,
  /* .create_file = */ dummy_create_file,
  /* .mkdir = */ dummy_mkdir,
  /* .unlink = */ dummy_unlink,
  /* .rmdir = */ dummy_rmdir,
  /* .readlink = */ real_symlink_readlink,
  1
};
