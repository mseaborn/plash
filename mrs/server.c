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

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>
#include <dirent.h>

#include "region.h"
#include "server.h"
#include "parse-filename.h"
#include "comms.h"
#include "config.h"


#define MOD_DEBUG 0
#define MOD_LOG server_log_messages	/* logs input and output messages */
#define MOD_SUMMARY 1	/* logs one-line summary of each input message */
#define MOD_MSG "server: "
int server_log_messages = 0;
FILE *server_log = 0; /* need to set this before using this module */


/* Takes a non-owning reference.  Returns an owning reference. */
struct dir_stack *dir_stack_root(struct filesys_obj *dir)
{
  struct dir_stack *root = amalloc(sizeof(struct dir_stack));
  dir->refcount++;
  root->refcount = 1;
  root->dir = dir;
  root->parent = 0;
  root->name = 0;
  return root;
}

void dir_stack_free(struct dir_stack *st)
{
  /* Written using a loop instead of tail recursion
     (although tail recursion would have been clearer). */
  assert(st->refcount > 0);
  st->refcount--;
  while(st && st->refcount <= 0) {
    struct dir_stack *parent = st->parent;

    filesys_obj_free(st->dir);
    if(st->parent) {
      assert(st->parent->refcount > 0);
      st->parent->refcount--;
      free(st->name);
    }
    free(st);
    st = parent;
  }
}


/* Used by chdir.
   Returns 0 if there's an error,
   eg. there's a file in the path. */
/* root and cwd are not passed as owning references.
   The dir_stack returned is an owning reference. */
struct dir_stack *resolve_dir
  (region_t r, struct filesys_obj *root, struct dir_stack *cwd,
   seqf_t filename, int symlink_limit, int *err)
{
  struct dir_stack *dirstack;  
  int end;
  
  switch(filename_parse_start(filename, &end, &filename)) {
    case FILENAME_ROOT:
      if(!root) { *err = EACCES; return 0; }
      dirstack = dir_stack_root(root);
      break;
    case FILENAME_CWD:
      dirstack = cwd;
      if(!dirstack) { *err = E_NO_CWD_DEFINED; return 0; }
      dirstack->refcount++;
      break;
    default: *err = ENOENT; return 0; /* Error: bad filename */
  }

  while(!end) {
    seqf_t name;
    int trailing_slash;
    filename_parse_component(filename, &name, &end, &filename, &trailing_slash);
    if(filename_parent(name)) {
      if(dirstack->parent) {
	struct dir_stack *p = dirstack->parent;
	p->refcount++;
	dir_stack_free(dirstack);
	dirstack = p;
      }
      /* Do nothing if we're at the root directory */
    }
    else if(filename_samedir(name)) {
      /* Do nothing */
    }
    else {
      char *name1 = strdup_seqf(name);
      struct filesys_obj *obj = dirstack->dir->vtable->traverse(dirstack->dir, name1);
      if(!obj) {
	free(name1);
	dir_stack_free(dirstack);
	*err = ENOENT;
	return 0; /* Error */
      }
      else if(obj->vtable->type == OBJT_DIR) {
	struct dir_stack *new_d = amalloc(sizeof(struct dir_stack));
	new_d->refcount = 1;
	new_d->dir = obj;
	new_d->parent = dirstack;
	new_d->name = name1;
	dirstack = new_d;
      }
      else if(obj->vtable->type == OBJT_SYMLINK) {
	struct dir_stack *new_stack;
	seqf_t link_dest;
	free(name1);
	if(symlink_limit <= 0) {
	  filesys_obj_free(obj);
	  dir_stack_free(dirstack);
	  *err = ELOOP;
	  return 0; /* Error */
	}
	if(obj->vtable->readlink(obj, r, &link_dest, err) < 0) {
	  filesys_obj_free(obj);
	  dir_stack_free(dirstack);
	  return 0; /* Error */
	}
	filesys_obj_free(obj);
	new_stack = resolve_dir(r, root, dirstack, link_dest, symlink_limit-1, err);
	dir_stack_free(dirstack);
	if(!new_stack) return 0; /* Error */
	dirstack = new_stack;
      }
      /* For OBJT_FILE: */
      else {
	free(name1);
	dir_stack_free(dirstack);
	filesys_obj_free(obj);
	*err = ENOTDIR;
	return 0; /* Error */
      }
    }
  }
  return dirstack;
}

/* Currently used by open, for files only. */
struct filesys_obj *resolve_file
  (region_t r, struct filesys_obj *root, struct dir_stack *cwd,
   seqf_t filename, int symlink_limit, int nofollow, int *err)
{
  struct dir_stack *dirstack;  
  int end;
  
  switch(filename_parse_start(filename, &end, &filename)) {
    case FILENAME_ROOT:
      if(MOD_DEBUG) fprintf(server_log, MOD_MSG "resolve_file: relative to root\n");
      if(!root) { *err = EACCES; return 0; }
      dirstack = dir_stack_root(root);
      break;
    case FILENAME_CWD:
      if(MOD_DEBUG) fprintf(server_log, MOD_MSG "resolve_file: relative to cwd\n");
      dirstack = cwd;
      if(!dirstack) { *err = E_NO_CWD_DEFINED; return 0; }
      dirstack->refcount++;
      break;
    default: *err = ENOENT; return 0; /* Error: bad filename */
  }
  if(end) {
    dir_stack_free(dirstack);
    /* Error: is a directory */
    *err = EISDIR;
    return 0;
  }

  while(1) {
    seqf_t name;
    int trailing_slash;
    filename_parse_component(filename, &name, &end, &filename, &trailing_slash);
    if(end && trailing_slash) goto got_directory;
    if(filename_parent(name)) {
      if(end) goto got_directory;
      if(dirstack->parent) {
	struct dir_stack *p = dirstack->parent;
	p->refcount++;
	dir_stack_free(dirstack);
	dirstack = p;
      }
      /* Do nothing if we're at the root directory */
    }
    else if(filename_samedir(name)) {
      if(end) goto got_directory;
      /* Do nothing */
    }
    else {
      char *name1 = strdup_seqf(name);
      struct filesys_obj *obj = dirstack->dir->vtable->traverse(dirstack->dir, name1);
      if(MOD_DEBUG) fprintf(server_log, MOD_MSG "resolve_file: \"%s\": ", name1);
      if(!obj) {
	if(MOD_DEBUG) fprintf(server_log, "not found\n");
	free(name1);
	dir_stack_free(dirstack);
	*err = ENOENT;
	return 0;
      }
      else if(obj->vtable->type == OBJT_DIR) {
	if(MOD_DEBUG) fprintf(server_log, "dir\n");
	if(end) {
	  /* Error: wanted to open a file, not a directory */
	  free(name1);
	  filesys_obj_free(obj);
	  goto got_directory;
	}
	else {
	  struct dir_stack *new_d = amalloc(sizeof(struct dir_stack));
	  new_d->refcount = 1;
	  new_d->dir = obj;
	  new_d->parent = dirstack;
	  new_d->name = name1; /* Don't really need to save this */
	  dirstack = new_d;
	}
      }
      else if(obj->vtable->type == OBJT_SYMLINK) {
	seqf_t link_dest;
	if(MOD_DEBUG) fprintf(server_log, "symlink\n");
	free(name1);
	if(end && nofollow) {
	  /* Error: ended with symlink, but called with NOFOLLOW */
	  filesys_obj_free(obj);
	  dir_stack_free(dirstack);
	  *err = ELOOP;
	  return 0;
	}
	else {
	  if(symlink_limit <= 0) {
	    filesys_obj_free(obj);
	    dir_stack_free(dirstack);
	    *err = ELOOP;
	    return 0; /* Error */
	  }
	  if(obj->vtable->readlink(obj, r, &link_dest, err) < 0) {
	    filesys_obj_free(obj);
	    dir_stack_free(dirstack);
	    return 0; /* Error */
	  }
	  filesys_obj_free(obj);
	  if(end) {
	    struct filesys_obj *file =
	      resolve_file(r, root, dirstack, link_dest, symlink_limit-1, 0, err);
	    dir_stack_free(dirstack);
	    return file;
	  }
	  else {
	    struct dir_stack *new_stack =
	      resolve_dir(r, root, dirstack, link_dest, symlink_limit-1, err);
	    dir_stack_free(dirstack);
	    if(!new_stack) return 0; /* Error */
	    dirstack = new_stack;
	  }
	}
      }
      /* For OBJT_FILE: */
      else if(obj->vtable->type == OBJT_FILE) {
	if(MOD_DEBUG) fprintf(server_log, "file\n");
	free(name1);
	dir_stack_free(dirstack);
	if(end) {
	  return obj;
	}
	else {
	  /* Error */
	  filesys_obj_free(obj);
	  *err = ENOTDIR;
	  return 0;
	}
      }
      else {
	if(MOD_DEBUG) fprintf(server_log, "erm?\n");
	assert(0); *err = EIO; return 0;
      }
    }
  }
 got_directory:
  /* Error */
  dir_stack_free(dirstack);
  *err = EISDIR;
  return 0;
}

struct resolved_slot {
  struct filesys_obj *dir; /* ref owned by this struct */
  char *leaf; /* malloc'd, owned by this struct */
};
void free_resolved_slot(struct resolved_slot *slot)
{
  filesys_obj_free(slot->dir);
  free(slot->leaf);
  free(slot);
}

#define CREATE_ONLY 2
#define RESOLVED_FILE_OR_SYMLINK 1
#define RESOLVED_DIR 2
#define RESOLVED_EMPTY_SLOT 3
/* Used by stat (with nofollow=0) and lstat (with nofollow=1). */
/* If it resolves a file or a symlink, returns RESOLVED_FILE_OR_SYMLINK and fills
   out *result with a filesys_obj*.  It will only return a symlink if
   nofollow is true.
   If it resolves a directory, returns RESOLVED_DIR and fills out *result
   with a dir_stack*.
   If create is true and the last component of the path is not found
   (but the rest are), returns RESOLVED_EMPTY_SLOT and fills out *result
   with a resolved_slot*; the resolved_slot is malloc'd.
    * If create is CREATE_ONLY, never returns RESOLVED_FILE_OR_SYMLINK
      or RESOLVED_DIR, only RESOLVED_EMPTY_SLOT, even if the slot is not
      actually empty.  This is used for unlink and rmdir.
      Actually, it will return RESOLVED_DIR for the root directory.
*/
int resolve_obj(region_t r, struct filesys_obj *root, struct dir_stack *cwd,
		seqf_t filename, int symlink_limit,
		int nofollow, int create,
		void **result, int *err)
{
  struct dir_stack *dirstack;  
  int end;
  
  switch(filename_parse_start(filename, &end, &filename)) {
    case FILENAME_ROOT:
      if(MOD_DEBUG) fprintf(server_log, MOD_MSG "resolve_obj: relative to root\n");
      if(!root) { *err = EACCES; return 0; }
      dirstack = dir_stack_root(root);
      break;
    case FILENAME_CWD:
      if(MOD_DEBUG) fprintf(server_log, MOD_MSG "resolve_obj: relative to cwd\n");
      dirstack = cwd;
      if(!dirstack) { *err = E_NO_CWD_DEFINED; return 0; }
      dirstack->refcount++;
      break;
    default: *err = ENOENT; return 0; /* Error: bad filename */
  }

  while(!end) {
    seqf_t name;
    int trailing_slash;
    filename_parse_component(filename, &name, &end, &filename, &trailing_slash);
    if(filename_parent(name)) {
      if(dirstack->parent) {
	struct dir_stack *p = dirstack->parent;
	p->refcount++;
	dir_stack_free(dirstack);
	dirstack = p;
      }
      /* Do nothing if we're at the root directory */
    }
    else if(filename_samedir(name)) {
      /* Do nothing */
    }
    else {
      char *name1 = strdup_seqf(name);
      struct filesys_obj *obj;
      
      if(end && create == CREATE_ONLY) {
	struct resolved_slot *slot = amalloc(sizeof(struct resolved_slot));
	if(MOD_DEBUG) fprintf(server_log, "create_only option; return slot\n");
	slot->dir = dirstack->dir;
	slot->leaf = name1;
	dirstack->dir->refcount++;
	dir_stack_free(dirstack);
	*result = slot;
	return RESOLVED_EMPTY_SLOT;
      }
      obj = dirstack->dir->vtable->traverse(dirstack->dir, name1);
      if(MOD_DEBUG) fprintf(server_log, MOD_MSG "resolve_obj: \"%s\": ", name1);
      if(!obj) {
	if(end && create) {
	  struct resolved_slot *slot = amalloc(sizeof(struct resolved_slot));
	  if(MOD_DEBUG) fprintf(server_log, "not found; create flag set\n");
	  slot->dir = dirstack->dir;
	  slot->leaf = name1;
	  dirstack->dir->refcount++;
	  dir_stack_free(dirstack);
	  *result = slot;
	  return RESOLVED_EMPTY_SLOT;
	}
	else {
	  if(MOD_DEBUG) fprintf(server_log, "not found\n");
	  free(name1);
	  dir_stack_free(dirstack);
	  *err = ENOENT;
	  return 0;
	}
      }
      else if(obj->vtable->type == OBJT_DIR) {
	struct dir_stack *new_d = amalloc(sizeof(struct dir_stack));
	if(MOD_DEBUG) fprintf(server_log, "dir\n");
	new_d->refcount = 1;
	new_d->dir = obj;
	new_d->parent = dirstack;
	new_d->name = name1;
	dirstack = new_d;
      }
      else if(obj->vtable->type == OBJT_SYMLINK) {
	seqf_t link_dest;
	if(MOD_DEBUG) fprintf(server_log, "symlink\n");
	free(name1);
	/* Return the symlink itself if NOFOLLOW is set -- unless, that is,
	   the pathname has a trailing slash, in which case we follow the
	   link and check that it is a directory. */
	if(end && nofollow && !trailing_slash) {
	  dir_stack_free(dirstack);
	  *result = obj;
	  return RESOLVED_FILE_OR_SYMLINK;
	}
	else {
	  if(symlink_limit <= 0) {
	    filesys_obj_free(obj);
	    dir_stack_free(dirstack);
	    *err = ELOOP;
	    return 0; /* Error */
	  }
	  if(obj->vtable->readlink(obj, r, &link_dest, err) < 0) {
	    filesys_obj_free(obj);
	    dir_stack_free(dirstack);
	    return 0; /* Error */
	  }
	  filesys_obj_free(obj);
	  if(end) {
	    int rc = resolve_obj(r, root, dirstack, link_dest, symlink_limit-1, 0, create, result, err);
	    dir_stack_free(dirstack);
	    if(trailing_slash) {
	      if(rc == RESOLVED_DIR) return RESOLVED_DIR;
	      if(rc == RESOLVED_EMPTY_SLOT) return RESOLVED_EMPTY_SLOT; // FIXME: add trailing_slash field to resolved_slot
	      if(rc == RESOLVED_FILE_OR_SYMLINK) {
		filesys_obj_free(*result);
		*err = ENOTDIR;
		return 0; /* Error */
	      }
	      *err = EIO;
	      return 0; /* Error */
	    }
	    return rc;
	  }
	  else {
	    struct dir_stack *new_stack =
	      resolve_dir(r, root, dirstack, link_dest, symlink_limit-1, err);
	    dir_stack_free(dirstack);
	    if(!new_stack) return 0; /* Error */
	    dirstack = new_stack;
	  }
	}
      }
      else if(obj->vtable->type == OBJT_FILE) {
	if(MOD_DEBUG) fprintf(server_log, "file\n");
	free(name1);
	dir_stack_free(dirstack);
	if(end && !trailing_slash) {
	  *result = obj;
	  return RESOLVED_FILE_OR_SYMLINK;
	}
	else {
	  filesys_obj_free(obj);
	  *err = ENOTDIR;
	  return 0; /* Error */
	}
      }
      else {
	if(MOD_DEBUG) fprintf(server_log, "erm?\n");
	assert(0);
      }
    }
  }
  *result = dirstack;
  return RESOLVED_DIR;
}

struct filesys_obj *initial_dir(const char *pathname)
{
  struct stat stat;
  int fd;
  struct real_dir *new_obj;
  
  fd = open(pathname, O_RDONLY | O_DIRECTORY);
  if(fd < 0) { assert(0); }
  if(fstat(fd, &stat) < 0) { assert(0); }
  new_obj = amalloc(sizeof(struct real_dir));
  new_obj = amalloc(sizeof(struct real_dir));
  new_obj->hdr.refcount = 1;
  new_obj->hdr.vtable = &real_dir_vtable;
  new_obj->stat = stat;
  new_obj->fd = make_fd(fd);
  return (void *) new_obj;
}

struct process *process_create(void)
{
  struct process *p;
  struct filesys_obj *root = initial_dir("/");

  p = amalloc(sizeof(struct process));
  p->sock_fd = -1;
  p->root = root;
  p->cwd = dir_stack_root(root);

  return p;
}

struct process *process_copy(struct process *p)
{
  struct process *p2 = amalloc(sizeof(struct process));
  p2->sock_fd = -1;
  p2->root = p->root;
  if(p->root) p->root->refcount++;
  p2->cwd = p->cwd;
  if(p->cwd) p->cwd->refcount++;
  return p2;
}

void process_free(struct process *p)
{
  if(p->root) filesys_obj_free(p->root);
  if(p->cwd) dir_stack_free(p->cwd);
  free(p);
}

void process_check(struct process *p)
{
  if(p->root) assert(p->root->refcount > 0);
  if(p->cwd) assert(p->cwd->refcount > 0);
}

int process_chdir(struct process *p, seqf_t pathname, int *err)
{
  region_t r = region_make();
  struct dir_stack *d = resolve_dir(r, p->root, p->cwd, pathname, SYMLINK_LIMIT, err);
  region_free(r);
  if(d) {
    struct dir_stack *old_cwd = p->cwd;
    p->cwd = d;
    if(old_cwd) dir_stack_free(old_cwd);
    return 0;
  }
  else {
    return -1;
  }
}

int process_open(struct process *p, seqf_t pathname, int flags, int mode, int *err)
{
  region_t r = region_make();
  void *result;
  int rc =
    resolve_obj(r, p->root, p->cwd, pathname, SYMLINK_LIMIT,
		((flags & O_NOFOLLOW) || (flags & O_EXCL)) ? 1:0,
		(flags & O_CREAT) ? 1:0,
		&result, err);
  region_free(r);
  if(rc == RESOLVED_DIR) {
    /* Opening directories with open() is not implemented yet. */
    struct dir_stack *ds = result;
    dir_stack_free(ds);
    *err = EISDIR;
    return -1;
  }
  else if(rc == RESOLVED_FILE_OR_SYMLINK) {
    struct filesys_obj *obj = result;
    if(flags & O_DIRECTORY) {
      filesys_obj_free(obj);
      *err = ENOTDIR;
      return -1;
    }
    else if(flags & O_EXCL) {
      /* File exists already; can't create */
      filesys_obj_free(obj);
      *err = EEXIST;
      return -1;
    }
    else {
      if(obj->vtable->type == OBJT_FILE) {
	int fd = obj->vtable->open(obj, flags, err);
	filesys_obj_free(obj);
	return fd;
      }
      else if(obj->vtable->type == OBJT_SYMLINK) {
	filesys_obj_free(obj);
	*err = ELOOP;
	return -1;
      }
    }
  }
  else if(rc == RESOLVED_EMPTY_SLOT) {
    struct resolved_slot *slot = result;
    if(flags & O_DIRECTORY) {
      free_resolved_slot(slot);
      *err = ENOENT;
      return -1;
    }
    else {
      int fd = slot->dir->vtable->create_file(slot->dir, slot->leaf, flags, mode, err);
      free_resolved_slot(slot);
      return fd;
    }
  }
  else if(rc <= 0) { return -1; }
  *err = EIO;
  return -1;
}

/* mkdir() behaves like open() with O_EXCL: it won't follow a symbolic
   link and create the destination. */
int process_mkdir(struct process *p, seqf_t pathname, int mode, int *err)
{
  region_t r = region_make();
  void *result;
  int rc =
    resolve_obj(r, p->root, p->cwd, pathname, SYMLINK_LIMIT,
		1 /* nofollow */, 1 /* create */, &result, err);
  region_free(r);
  if(rc == RESOLVED_DIR) {
    dir_stack_free(result);
    *err = EEXIST;
    return -1;
  }
  else if(rc == RESOLVED_FILE_OR_SYMLINK) {
    filesys_obj_free(result);
    *err = EEXIST;
    return -1;
  }
  else if(rc == RESOLVED_EMPTY_SLOT) {
    struct resolved_slot *slot = result;
    int r = slot->dir->vtable->mkdir(slot->dir, slot->leaf, mode, err);
    free_resolved_slot(slot);
    return r;
  }
  else if(rc <= 0) { return -1; }
  *err = EIO;
  return -1;
}

int process_chmod(struct process *p, seqf_t pathname, int mode, int *err)
{
  region_t r = region_make();
  void *result;
  /* chmod does follow symlinks.  Symlinks do not have permissions of
     their own. */
  int rc = resolve_obj(r, p->root, p->cwd, pathname, SYMLINK_LIMIT,
		       0 /* nofollow */, 0 /* create */, &result, err);
  region_free(r);
  if(rc == RESOLVED_DIR) {
    struct dir_stack *ds = result;
    int r = ds->dir->vtable->chmod(ds->dir, mode, err);
    dir_stack_free(ds);
    return r;
  }
  else if(rc == RESOLVED_FILE_OR_SYMLINK) {
    struct filesys_obj *obj = result;
    int r = obj->vtable->chmod(obj, mode, err);
    filesys_obj_free(obj);
    return r;
  }
  else if(rc <= 0) { return -1; }
  *err = EIO;
  return -1;
}

int process_unlink(struct process *p, seqf_t pathname, int *err)
{
  region_t r = region_make();
  void *result;
  int rc = resolve_obj(r, p->root, p->cwd, pathname, SYMLINK_LIMIT,
		       1 /* nofollow */, CREATE_ONLY /* create */, &result, err);
  region_free(r);
  if(rc == RESOLVED_EMPTY_SLOT) {
    struct resolved_slot *slot = result;
    int r = slot->dir->vtable->unlink(slot->dir, slot->leaf, err);
    free_resolved_slot(slot);
    return r;
  }
  /* This case should only happen for the root directory: */
  else if(rc == RESOLVED_DIR) {
    dir_stack_free(result);
    *err = EACCES; /* EISDIR would also be appropriate */
    return -1;
  }
  else if(rc <= 0) { return -1; }
  *err = EIO;
  return -1;
}

int process_rmdir(struct process *p, seqf_t pathname, int *err)
{
  region_t r = region_make();
  void *result;
  int rc = resolve_obj(r, p->root, p->cwd, pathname, SYMLINK_LIMIT,
		       1 /* nofollow */, CREATE_ONLY /* create */, &result, err);
  region_free(r);
  if(rc == RESOLVED_EMPTY_SLOT) {
    struct resolved_slot *slot = result;
    int r = slot->dir->vtable->rmdir(slot->dir, slot->leaf, err);
    free_resolved_slot(slot);
    return r;
  }
  /* This case should only happen for the root directory: */
  else if(rc == RESOLVED_DIR) {
    dir_stack_free(result);
    *err = EACCES; /* EBUSY could also be appropriate */
    return -1;
  }
  else if(rc <= 0) { return -1; }
  *err = EIO;
  return -1;
}

seqt_t string_of_cwd(region_t r, struct dir_stack *dir)
{
  seqt_t slash = mk_string(r, "/");
  if(dir->parent) {
    seqt_t got = seqt_empty;
    do {
      if(MOD_DEBUG) fprintf(server_log, "path component: %s\n", dir->name);
      got = cat3(r, slash, mk_string(r, dir->name), got);
      dir = dir->parent;
    } while(dir->parent);
    return got;
  }
  else {
    return slash;
  }
}


struct process_list {
  int id; /* 0 for the list head */
  struct comm *comm;
  struct process *proc;
  struct process_list *prev, *next;
};

struct server_state {
  struct process_list list;
  int next_proc_id;

  /* Arguments to select(): */
  int max_fd;
  fd_set set;
};

/* Sets up the arguments to select().  Needs to be called every time the
   process list is changed. */
void init_fd_set(struct server_state *state)
{
  struct process_list *node;
  state->max_fd = 0;
  FD_ZERO(&state->set);
  for(node = state->list.next; node->id; node = node->next) {
    int fd = node->comm->sock;
    if(state->max_fd < fd+1) state->max_fd = fd+1;
    FD_SET(fd, &state->set);
  }
}

/* This should fill out "reply" ("reply_fds" defaults to being empty),
   and can allocate the message from region r. */
void process_handle_msg(region_t r, struct server_state *state,
			struct process *proc,
			seqf_t msg_orig, fds_t fds_orig,
			seqt_t *reply, fds_t *reply_fds)
{
  {
    seqf_t msg = msg_orig;
    int ok = 1;
    m_str(&ok, &msg, "Fork");
    m_end(&ok, &msg);
    if(ok) {
      int socks[2];
      struct process *proc2;
      struct process_list *node_new;
      int new_id = state->next_proc_id++;
      
      if(MOD_DEBUG) fprintf(server_log, MOD_MSG "forking new process, %i\n", new_id);
      if(socketpair(AF_LOCAL, SOCK_STREAM, 0, socks) < 0) {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, errno));
	return;
      }
      proc2 = process_copy(proc);
      proc2->sock_fd = socks[1];
      node_new = amalloc(sizeof(struct process_list));
      node_new->id = new_id;
      node_new->comm = comm_init(socks[1]);
      node_new->proc = proc2;
      node_new->prev = state->list.prev;
      node_new->next = &state->list;
      state->list.prev->next = node_new;
      state->list.prev = node_new;

      init_fd_set(state);
      
      *reply = mk_string(r, "RFrk");
      *reply_fds = mk_fds1(r, socks[0]);
      return;
    }
  }
  {
    seqf_t msg = msg_orig;
    int ok = 1;
    int fd_number;
    seqf_t cmd_filename;
    int argc;
    m_str(&ok, &msg, "Exec");
    m_int(&ok, &msg, &fd_number);
    m_lenblock(&ok, &msg, &cmd_filename);
    m_int(&ok, &msg, &argc);
    if(ok && argc >= 1) {
      int extra_args = 5;
      int buf_size = 40;
      char *buf = region_alloc(r, buf_size);
      seqf_t *argv;
      seqf_t *argv2;
      seqf_t cmd_filename2;
      int i;
      int err;
      struct filesys_obj *obj;
      
      argv = region_alloc(r, argc * sizeof(seqf_t));
      for(i = 0; i < argc; i++) {
	m_lenblock(&ok, &msg, &argv[i]);
	if(!ok) goto exec_error;
      }
      
      argv2 = region_alloc(r, (argc + extra_args) * sizeof(seqf_t));
      argv2[0] = argv[0];
      cmd_filename2 = seqf_string("/special/ld-linux.so.2");
      argv2[1] = seqf_string("--library-path");
      argv2[2] = seqf_string(LIB_INSTALL ":/lib:/usr/lib:/usr/X11R6/lib");
      argv2[3] = seqf_string("--fd");
      snprintf(buf, buf_size, "%i", fd_number);
      argv2[4] = seqf_string(buf);
      for(i = 0; i < argc; i++) argv2[i + extra_args] = argv[i];

      obj = resolve_file(r, proc->root, proc->cwd, cmd_filename,
			 SYMLINK_LIMIT, 0 /* nofollow */, &err);
      if(obj) {
	int fd = obj->vtable->open(obj, O_RDONLY, &err);
	filesys_obj_free(obj);
	if(fd >= 0) {
	  seqt_t got = seqt_empty;
	  int i;
	  for(i = 0; i < argc + extra_args; i++) {
	    got = cat3(r, got,
		       mk_int(r, argv2[i].size),
		       mk_leaf(r, argv2[i]));
	  }
	  *reply = cat5(r, mk_string(r, "RExe"),
			mk_int(r, cmd_filename2.size),
			mk_leaf(r, cmd_filename2),
			mk_int(r, argc + extra_args),
			got);
	  *reply_fds = mk_fds1(r, fd);
	  return;
	}
      }
      *reply = cat2(r, mk_string(r, "Fail"),
		    mk_int(r, err));
      return;
    }
  exec_error:
  }
  {
    seqf_t msg = msg_orig;
    int flags, mode;
    int ok = 1;
    m_str(&ok, &msg, "Open");
    m_int(&ok, &msg, &flags);
    m_int(&ok, &msg, &mode);
    if(ok) {
      seqf_t pathname = msg;
      int fd, err = 0;
            
      fd = process_open(proc, pathname, flags, mode, &err);
      if(fd >= 0) {
	*reply = mk_string(r, "ROpn");
	*reply_fds = mk_fds1(r, fd);

	if(MOD_SUMMARY) {
	  fprintf(server_log, "open: flags=0o%o, mode=0o%o, ", flags, mode);
	  fprint_d(server_log, pathname);
	  fprintf(server_log, ": ok\n");
	}
      }
      else {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));

	if(MOD_SUMMARY) {
	  fprintf(server_log, "open: flags=0o%o, mode=0o%o, ", flags, mode);
	  fprint_d(server_log, pathname);
	  fprintf(server_log, ": fail errno=%i\n", err);
	}
      }
      return;
    }
  }
  {
    seqf_t msg = msg_orig;
    int nofollow;
    int ok = 1;
    m_str(&ok, &msg, "Stat");
    m_int(&ok, &msg, &nofollow);
    if(ok) {
      seqf_t pathname = msg;
      void *result;
      int rc, err = 0;
      struct stat stat;
      int got = -1;
      
      region_t r2 = region_make();
      rc = resolve_obj(r2, proc->root, proc->cwd, pathname, SYMLINK_LIMIT, nofollow, 0 /* create */, &result, &err);
      region_free(r2);
      if(rc == RESOLVED_DIR) {
	struct dir_stack *ds = result;
	got = ds->dir->vtable->stat(ds->dir, &stat);
	dir_stack_free(ds);
      }
      else if(rc == RESOLVED_FILE_OR_SYMLINK) {
	struct filesys_obj *obj = result;
	got = obj->vtable->stat(obj, &stat);
	filesys_obj_free(obj);
      }
      if(got >= 0) {
	*reply = cat4(r, mk_string(r, "RSta"),
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

	if(MOD_SUMMARY) {
	  fprintf(server_log, "%s: ", nofollow ? "lstat" : "stat");
	  fprint_d(server_log, pathname);
	  fprintf(server_log, ": ok\n");
	}
      }
      else {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));

	if(MOD_SUMMARY) {
	  fprintf(server_log, "%s: ", nofollow ? "lstat" : "stat");
	  fprint_d(server_log, pathname);
	  fprintf(server_log, ": fail errno=%i\n", err);
	}
      }
      return;
    }
  }
  {
    seqf_t msg = msg_orig;
    int ok = 1;
    m_str(&ok, &msg, "Rdlk");
    if(ok) {
      seqf_t pathname = msg;
      void *result;
      int rc, err = 0;
      
      region_t r2 = region_make();
      rc = resolve_obj(r2, proc->root, proc->cwd, pathname, SYMLINK_LIMIT,
		       1 /*nofollow*/, 0 /* create */, &result, &err);
      region_free(r2);
      if(rc == RESOLVED_FILE_OR_SYMLINK) {
	struct filesys_obj *obj = result;
	if(obj->vtable->type == OBJT_SYMLINK) {
	  seqf_t link_dest;
	  if(obj->vtable->readlink(obj, r, &link_dest, &err) >= 0) {
	    *reply = cat2(r, mk_string(r, "RRdl"),
			  mk_leaf(r, link_dest));
	  }
	  else {
	    *reply = cat2(r, mk_string(r, "Fail"),
			  mk_int(r, err));
	  }
	}
	else {
	  *reply = cat2(r, mk_string(r, "Fail"),
			mk_int(r, EINVAL)); /* not a symlink */
	}
	filesys_obj_free(obj);
      }
      else if(rc == RESOLVED_DIR) {
	dir_stack_free(result);
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, EINVAL)); /* not a symlink */
      }
      else {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
      }
      return;
    }
  }
  {
    seqf_t msg = msg_orig;
    int ok = 1;
    m_str(&ok, &msg, "Chdr");
    if(ok) {
      seqf_t pathname = msg;
      int err = 0;
      int e = process_chdir(proc, pathname, &err);
      if(e == 0) {
	*reply = mk_string(r, "RSuc");
      }
      else {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
      }
      return;
    }
  }
  {
    seqf_t msg = msg_orig;
    int ok = 1;
    m_str(&ok, &msg, "Gcwd");
    m_end(&ok, &msg);
    if(ok) {
      if(proc->cwd) {
	*reply = cat2(r, mk_string(r, "RCwd"),
		      string_of_cwd(r, proc->cwd));
      }
      else {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, E_NO_CWD_DEFINED));
      }
      return;
    }
  }
  {
    seqf_t msg = msg_orig;
    int ok = 1;
    m_str(&ok, &msg, "Dlst");
    if(ok) {
      seqf_t pathname = msg;
      int err = 0;
      struct dir_stack *ds = resolve_dir(r, proc->root, proc->cwd, pathname, SYMLINK_LIMIT, &err);
      if(ds) {
	seqt_t result;
	if(ds->dir->vtable->list(ds->dir, r, &result, &err) >= 0) {
	  /* Could add "." and ".." here but is it really useful?
	     struct dir_stack *parent = ds->parent ? ds->parent : ds;
	    result = cat5(r, result,
			  mk_int(r, ent->d_ino),
			  mk_int(r, ent->d_type),
			  mk_int(r, len),
			  mk_leaf2(r, str, len)); */
	  *reply = cat2(r, mk_string(r, "RDls"), result);
	  dir_stack_free(ds);
	}
	else {
	  *reply = cat2(r, mk_string(r, "Fail"),
			mk_int(r, err));
	}
      }
      else {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
      }
      return;
    }
  }
  {
    /* access() call.  This isn't very useful for secure programming in
       Unix, because of the race condition (TOCTTOU problem).
       It's basically used in a similar way to stat, checking whether a
       file exists, but checking whether it is accessible rather than
       returning information. */
    /* access() is based on effective vs. real uids/gids, a concept we
       don't have in the file server.  We don't care about uids/gids at
       all! */
    /* This implementation is incomplete.  It doesn't check permissions.
       It returns successfully simply if the object exists. */
    seqf_t msg = msg_orig;
    int mode;
    int ok = 1;
    m_str(&ok, &msg, "Accs");
    m_int(&ok, &msg, &mode);
    if(ok) {
      seqf_t pathname = msg;
      void *result;
      int rc, err = 0;

      region_t r2 = region_make();
      rc = resolve_obj(r2, proc->root, proc->cwd, pathname, SYMLINK_LIMIT,
		       0 /*nofollow*/, 0 /* create */, &result, &err);
      region_free(r2);
      if(rc == RESOLVED_FILE_OR_SYMLINK) {
	struct filesys_obj *obj = result;
	filesys_obj_free(obj);
	*reply = mk_string(r, "RAcc");

	if(MOD_SUMMARY) {
	  fprintf(server_log, "access: ");
	  fprint_d(server_log, pathname);
	  fprintf(server_log, ": ok\n");
	}
      }
      else if(rc == RESOLVED_DIR) {
	struct dir_stack *ds = result;
	dir_stack_free(ds);
	*reply = mk_string(r, "RAcc");

	if(MOD_SUMMARY) {
	  fprintf(server_log, "access: ");
	  fprint_d(server_log, pathname);
	  fprintf(server_log, ": ok\n");
	}
      }
      else {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));

	if(MOD_SUMMARY) {
	  fprintf(server_log, "access: ");
	  fprint_d(server_log, pathname);
	  fprintf(server_log, ": fail errno=%i\n", err);
	}
      }
      return;
    }
  }
  {
    /* mkdir() call */
    seqf_t msg = msg_orig;
    int mode;
    int ok = 1;
    m_str(&ok, &msg, "Mkdr");
    m_int(&ok, &msg, &mode);
    if(ok) {
      seqf_t pathname = msg;
      int err = 0;
      if(process_mkdir(proc, pathname, mode, &err) < 0) {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
      }
      else {
	*reply = mk_string(r, "RMkd");
      }
      return;
    }
  }
  {
    /* chmod() call.  An old version of this re-used the code for "open"
       and then fchmod'ed the FD.  The problem with that is that it
       would allow the client process to chmod any file, even those it's
       not supposed to have write access to!  I have now added a chmod
       method to file and directory objects. */
    seqf_t msg = msg_orig;
    int mode;
    int ok = 1;
    m_str(&ok, &msg, "Chmd");
    m_int(&ok, &msg, &mode);
    if(ok) {
      seqf_t pathname = msg;
      int err;
      if(process_chmod(proc, pathname, mode, &err) < 0) {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
      }
      else {
	*reply = mk_string(r, "RChm");
      }
      return;
    }
  }
  {
    /* unlink() call */
    seqf_t msg = msg_orig;
    int ok = 1;
    m_str(&ok, &msg, "Unlk");
    if(ok) {
      seqf_t pathname = msg;
      int err;
      if(process_unlink(proc, pathname, &err) < 0) {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
      }
      else {
	*reply = mk_string(r, "RUnl");
      }
      return;
    }
  }
  {
    /* rmdir() call */
    seqf_t msg = msg_orig;
    int ok = 1;
    m_str(&ok, &msg, "Rmdr");
    if(ok) {
      seqf_t pathname = msg;
      int err;
      if(process_rmdir(proc, pathname, &err) < 0) {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, err));
      }
      else {
	*reply = mk_string(r, "RRmd");
      }
      return;
    }
  }
  {
    /* connect() for Unix domain sockets */
    seqf_t msg = msg_orig;
    fds_t fds = fds_orig;
    int ok = 1;
    int sock_fd;
    m_str(&ok, &msg, "Fcon");
    m_fd(&ok, &fds, &sock_fd);
    if(ok) {
      seqf_t pathname = msg;
      int err;
      struct filesys_obj *obj =
	resolve_file(r, proc->root, proc->cwd, pathname,
		     SYMLINK_LIMIT, 0 /*nofollow*/, &err);
      if(obj) {
	if(obj->vtable->connect(obj, sock_fd, &err) >= 0) {
	  *reply = mk_string(r, "RFco");
	  return;
	}
      }
      *reply = cat2(r, mk_string(r, "Fail"),
		    mk_int(r, err));
      return;
    }
  }

  fprintf(server_log, "Unknown message!\n");
  /* Send a Fail reply. */
  *reply = cat2(r, mk_string(r, "Fail"),
		mk_int(r, ENOSYS));
}


void start_server(struct process *initial_proc)
{
  struct server_state state;
  state.list.id = 0;
  state.list.prev = &state.list;
  state.list.next = &state.list;
  state.next_proc_id = 1;

  /* Insert the initial process into the list */
  {
    struct process_list *node_new = amalloc(sizeof(struct process_list));
    node_new->id = state.next_proc_id++;
    node_new->comm = comm_init(initial_proc->sock_fd);
    node_new->proc = initial_proc;
    node_new->prev = state.list.prev;
    node_new->next = &state.list;
    state.list.prev->next = node_new;
    state.list.prev = node_new;
  }
  init_fd_set(&state);

  while(state.list.next->id) { /* while the process list is non-empty */
    int result;
    /* struct timeval timeout; */
    /* timeout.tv_sec = 0;
       timeout.tv_usec = 0; */
    
    if(MOD_DEBUG) fprintf(server_log, MOD_MSG "calling select()\n");
    result = select(state.max_fd, &state.set, 0, 0, 0 /* &timeout */);
    if(result < 0) { perror("select"); }

    {
      struct process_list *node;
      for(node = state.list.next; node->id && result > 0; node = node->next) {
	if(FD_ISSET(node->comm->sock, &state.set)) {
	  int r;
	  seqf_t msg;
	  fds_t fds;
    
	  result--;
	  r = comm_read(node->comm);
	  if(r < 0 || r == COMM_END) {
	    struct process_list *node_next = node->next;
	    if(MOD_LOG) fprintf(server_log, "\nprocess %i error/end\n", node->id);

	    /* Close socket and remove process from list */
	    close(node->comm->sock);
	    comm_free(node->comm);
	    process_free(node->proc);
	    node->prev->next = node->next;
	    node->next->prev = node->prev;
	    free(node);
	    node = node_next;
	    init_fd_set(&state);
	  }
	  else while(1) {
	    r = comm_try_get(node->comm, &msg, &fds);
	    if(r == COMM_AVAIL) {
	      region_t r = region_make();
	      seqt_t reply = seqt_empty;
	      fds_t reply_fds = fds_empty;
	      if(MOD_LOG) {
		fprintf(server_log, "\nmessage from process %i\n", node->id);
		fprint_data(server_log, msg);
	      }
	      process_check(node->proc);
	      process_handle_msg(r, &state, node->proc, msg, fds, &reply, &reply_fds);
	      if(MOD_LOG) {
		fprintf(server_log, "reply with %i FDs and this data:\n", reply_fds.count);
		fprint_data(server_log, flatten(r, reply));
	      }
	      comm_send(r, node->proc->sock_fd, reply, reply_fds);
	      close_fds(fds);
	      close_fds(reply_fds);
	      region_free(r);
	    }
	    else break;
	  }
	}
      }
    }
  }
}
