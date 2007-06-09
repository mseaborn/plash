/* Copyright (C) 2006 Mark Seaborn

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
#include "serialise.h"
#include "cap-protocol.h"
#include "filesysobj-readonly.h"
#include "filesysobj-union.h"


DECLARE_VTABLE(cow_dir_vtable);

/* Parent and children nodes are linked together.  The following
   operations use this:

   * cow_dir_traverse():
     Tries to look up the child to re-use if an allocated node exists.
     Otherwise, creates a new node and adds it to the list.
   * realize():
     Follows the parent links recursively to create directories in
     the writable layer.
   * cow_dir_free():
     Removes a node from its parent's list.

   A parent can only be freed when its children are freed.
   The parent has only weak references to its children, so the
   children can be freed before the parent is freed.
*/

struct cow_dir {
  struct filesys_obj hdr;

  struct filesys_obj *dir_write; /* May be NULL if parent != NULL */
  struct filesys_obj *dir_read; /* May *not* be NULL */

  struct cow_dir *children; /* Non-owning reference */

  struct cow_dir *parent; /* Owning reference; may be NULL */
  /* These are only used in parent is non-NULL.
     "next" links this node into a list that starts from the
     parent's "children" field. */
  char *name; /* malloc'd */
  struct cow_dir *next; /* Non-owning reference */
};


static int realize(struct cow_dir *dir, int *err);


struct filesys_obj *
make_cow_dir(struct filesys_obj *dir_write,
	     struct filesys_obj *dir_read)
{
  struct cow_dir *dir =
    filesys_obj_make(sizeof(struct cow_dir), &cow_dir_vtable);
  dir->dir_write = dir_write;
  dir->dir_read = dir_read;
  dir->parent = NULL;
  dir->name = NULL;
  dir->children = NULL;
  dir->next = NULL;
  return (struct filesys_obj *) dir;
}


void cow_dir_free(struct filesys_obj *obj)
{
  struct cow_dir *dir = (void *) obj;
  
  if(dir->dir_write) { filesys_obj_free(dir->dir_write); }
  filesys_obj_free(dir->dir_read);

  if(dir->parent) {
    /* Unlink from parent's list. */
    int found = 0;
    struct cow_dir **node;
    for(node = &dir->parent->children; *node; node = &(*node)->next) {
      if(*node == dir) {
	*node = dir->next;
	found = 1;
	break;
      }
    }
    assert(found);
    
    free(dir->name);

    filesys_obj_free(dir->parent);
  }
}

#ifdef GC_DEBUG
void cow_dir_mark(struct filesys_obj *obj)
{
  struct cow_dir *dir = (void *) obj;

  if(dir->dir_write) {
    filesys_obj_mark(dir->dir_write);
  }
  filesys_obj_mark(dir->dir_read);

  if(dir->parent) {
    filesys_obj_mark(dir->parent);
  }
}
#endif

int cow_dir_stat(struct filesys_obj *obj, struct stat *buf, int *err)
{
  struct cow_dir *dir = (void *) obj;

  /* Use the directory in the writable layer if it exists,
     otherwise use the directory in the readable layer. */
  struct filesys_obj *stat_dir =
    dir->dir_write ? dir->dir_write : dir->dir_read;

  if(stat_dir->vtable->fsobj_stat(stat_dir, buf, err) < 0) return -1;
  buf->st_nlink = 0; /* FIXME: this can be used to count the number of child directories */
  return 0;
}

int cow_dir_utimes(struct filesys_obj *obj, const struct timeval *atime,
		   const struct timeval *mtime, int *err)
{
  struct cow_dir *dir = (void *) obj;

  if(realize(dir, err) < 0) {
    return -1;
  }
  return dir->dir_write->vtable->fsobj_utimes(dir->dir_write, atime, mtime,
					      err);
}

int cow_dir_chmod(struct filesys_obj *obj, int mode, int *err)
{
  struct cow_dir *dir = (void *) obj;

  if(realize(dir, err) < 0) {
    return -1;
  }
  return dir->dir_write->vtable->fsobj_chmod(dir->dir_write, mode, err);
}

static struct filesys_obj *
make_subdir(struct cow_dir *parent, const char *name,
	    struct filesys_obj *dir_write,
	    struct filesys_obj *dir_read)
{
  struct cow_dir *subdir =
    filesys_obj_make(sizeof(struct cow_dir), &cow_dir_vtable);
  subdir->dir_write = dir_write;
  subdir->dir_read = dir_read;
  inc_ref((struct filesys_obj *) parent);
  subdir->parent = parent;
  subdir->name = strdup(name);
  assert(subdir->name);
  subdir->children = NULL;
  
  /* Link into list. */
  subdir->next = parent->children;
  parent->children = subdir;
  
  return (struct filesys_obj *) subdir;
}

/* write_dir is a non-owning reference.
   file is an owning reference. */
static struct filesys_obj *
make_cow_file(struct filesys_obj *write_dir, const char *name,
	      struct filesys_obj *file)
{
  /* Temporary */
  return make_read_only_proxy(file);
}

struct filesys_obj *cow_dir_traverse(struct filesys_obj *obj, const char *leaf)
{
  struct cow_dir *dir = (void *) obj;
  
  struct filesys_obj *child1;
  struct filesys_obj *child2;
  int type1;
  int type2;
  
  /* Look up child node first. */
  struct cow_dir *node;
  for(node = dir->children; node; node = node->next) {
    if(!strcmp(node->name, leaf)) {
      inc_ref((struct filesys_obj *) node);
      return (struct filesys_obj *) node;
    }
  }

  if(dir->dir_write) {
    child1 = dir->dir_write->vtable->traverse(dir->dir_write, leaf);
    if(child1) {
      type1 = child1->vtable->fsobj_type(child1);
    }
  }
  else {
    child1 = NULL;
  }
  
  child2 = dir->dir_read->vtable->traverse(dir->dir_read, leaf);
  if(child2) {
    type2 = child2->vtable->fsobj_type(child2);
  }

  /* If not present in write layer: */
  if(!child1) {
    /* If not present in either layer, not found. */
    if(!child2) {
      return NULL;
    }
    else if(type2 == OBJT_DIR) {
      /* Directory will be created on demand in the write layer. */
      return make_subdir(dir /* parent */, leaf, NULL /* child1 */, child2);
    }
    else {
      /* File was found in read layer. */
      return make_cow_file(dir->dir_write, leaf, child2);
    }
  }

  if(type1 == OBJT_DIR &&
     child2 &&
     type2 == OBJT_DIR) {
    return make_subdir(dir /* parent */, leaf, child1, child2);
  }

  /* Otherwise, child1 overrides child2. */
  if(child2) {
    filesys_obj_free(child2);
  }
  return child1;
}

int cow_dir_list(struct filesys_obj *obj, region_t r, seqt_t *result, int *err)
{
  struct cow_dir *dir = (void *) obj;

  if(dir->dir_write) {
    return merge_dir_lists(r, dir->dir_write, dir->dir_read, result, err);
  }
  else {
    /* No merging required:  return dir_read's list. */
    return dir->dir_read->vtable->list(dir->dir_read, r, result, err);
  }
}

/* Ensure that the directory has been created in the writable layer.
   Recursively traverses the parent links to create the necessary
   directories. */
/* Post-condition:  dir->dir_write is non-NULL. */
static int realize(struct cow_dir *dir, int *err)
{
  if(!dir->dir_write) {
    struct stat st;
    struct filesys_obj *p, *old_dir, *new_dir;
    
    assert(dir->parent);
    if(realize(dir->parent, err) < 0) {
      return -1;
    }

    /* Stat the directory in the read layer, just to find out its
       permissions mode, so that we can create the new directory with
       the same permissions. */
    if(dir->dir_read->vtable->fsobj_stat(dir->dir_read, &st, err) < 0) {
      return -1;
    }

    /* Create the new directory, and get a reference to it. */
    p = dir->parent->dir_write;
    if(p->vtable->mkdir(p, dir->name, st.st_mode, err) < 0) {
      if(*err != EEXIST) {
	return -1;
      }
    }
    new_dir = p->vtable->traverse(p, dir->name);
    if(!new_dir) {
      /* Shouldn't happen since we just created the subdirectory. */
      *err = ENOENT;
      return -1;
    }
    old_dir = dir->dir_write;
    dir->dir_write = new_dir;
    if(old_dir) {
      /* This should only happen if the function got re-entered. */
      filesys_obj_free(old_dir);
    }
  }
  return 0;
}

static int exists_in_read_layer(struct cow_dir *dir, const char *leaf)
{
  struct filesys_obj *obj =
    dir->dir_read->vtable->traverse(dir->dir_read, leaf);
  if(obj) {
    filesys_obj_free(obj);
    return 1;
  }
  else {
    return 0;
  }
}

static int creation_check(struct cow_dir *dir, const char *leaf, int *err)
{
  if(exists_in_read_layer(dir, leaf)) {
    *err = EEXIST;
    return -1;
  }
  if(realize(dir, err) < 0) {
    return -1;
  }
  return 0;
}

int cow_dir_mkdir(struct filesys_obj *obj, const char *leaf, int mode, int *err)
{
  struct cow_dir *dir = (void *) obj;

  if(creation_check(dir, leaf, err) < 0) {
    return -1;
  }
  return dir->dir_write->vtable->mkdir(dir->dir_write, leaf, mode, err);
}

int cow_dir_create_file(struct filesys_obj *obj, const char *leaf,
			int flags, int mode, int *err)
{
  struct cow_dir *dir = (void *) obj;
  
  if(creation_check(dir, leaf, err) < 0) {
    return -1;
  }
  return dir->dir_write->vtable->create_file(dir->dir_write, leaf,
					     flags, mode, err);
}

int cow_dir_symlink(struct filesys_obj *obj, const char *leaf,
		    const char *oldpath, int *err)
{
  struct cow_dir *dir = (void *) obj;
  
  if(creation_check(dir, leaf, err) < 0) {
    return -1;
  }
  return dir->dir_write->vtable->symlink(dir->dir_write, leaf, oldpath, err);
}

int cow_dir_unlink(struct filesys_obj *obj, const char *leaf, int *err)
{
  struct cow_dir *dir = (void *) obj;
  
  if(exists_in_read_layer(dir, leaf)) {
    /* Deleting files that are present in the read layer is not
       supported. */
    *err = EPERM;
    return -1;
  }
  if(!dir->dir_write) {
    *err = ENOENT;
    return -1;
  }
  return dir->dir_write->vtable->unlink(dir->dir_write, leaf, err);
}

int cow_dir_rmdir(struct filesys_obj *obj, const char *leaf, int *err)
{
  struct cow_dir *dir = (void *) obj;
  
  if(exists_in_read_layer(dir, leaf)) {
    /* Deleting files that are present in the read layer is not
       supported. */
    *err = EPERM;
    return -1;
  }
  if(!dir->dir_write) {
    *err = ENOENT;
    return -1;
  }
  return dir->dir_write->vtable->rmdir(dir->dir_write, leaf, err);
}

int cow_dir_rename(struct filesys_obj *obj, const char *src_leaf,
		   struct filesys_obj *dest_dir, const char *dest_leaf, int *err)
{
  struct cow_dir *dir = (void *) obj;

  /* Handle the same-directory case only. */
  if(dir == (void *) dest_dir) {
    /* Removing or overwriting files that exist in the read layer is
       not allowed. */
    if(exists_in_read_layer(dir, src_leaf) ||
       exists_in_read_layer(dir, dest_leaf)) {
      *err = EPERM;
      return -1;
    }
    if(!dir->dir_write) {
      *err = ENOENT;
      return -1;
    }
    return dir->dir_write->vtable->rename(dir->dir_write, src_leaf,
                                          dir->dir_write, dest_leaf, err);
  }
  else {
    *err = EXDEV;
    return -1;
  }
}

int cow_dir_link(struct filesys_obj *obj, const char *src_leaf,
		 struct filesys_obj *dest_dir, const char *dest_leaf, int *err)
{
  *err = EXDEV;
  return -1;
}

int cow_dir_socket_bind(struct filesys_obj *obj, const char *leaf,
			int sock_fd, int *err)
{
  struct cow_dir *dir = (void *) obj;

  if(creation_check(dir, leaf, err) < 0) {
    return -1;
  }
  return dir->dir_write->vtable->socket_bind(dir->dir_write, leaf, sock_fd, err);
}


#include "out-vtable-filesysobj-cow.h"
