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

#include <errno.h>

#include "region.h"
#include "filesysobj.h"
#include "parse-filename.h"
#include "resolve-filename.h"


#define MOD_DEBUG 0
#define MOD_MSG "server: "
static FILE *server_log = 0;


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

void free_resolved_slot(struct resolved_slot *slot)
{
  filesys_obj_free(slot->dir);
  free(slot->leaf);
  free(slot);
}

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

/* Returns a file or dir object (no dirstacks), or possibly a symlink
   object if nofollow is true. */
struct filesys_obj *resolve_obj_simple
  (struct filesys_obj *root, struct dir_stack *cwd,
   seqf_t pathname, int symlink_limit, int nofollow, int *err)
{
  region_t r = region_make();
  void *result;
  int rc = resolve_obj(r, root, cwd, pathname, symlink_limit, nofollow,
		       0 /* create */, &result, err);
  region_free(r);
  if(rc == RESOLVED_DIR) {
    struct dir_stack *ds = result;
    struct filesys_obj *dir = ds->dir;
    dir->refcount++;
    dir_stack_free(ds);
    return dir;
  }
  else if(rc == RESOLVED_FILE_OR_SYMLINK) {
    return result;
  }
  else if(rc <= 0) return 0;
  *err = EIO;
  return 0;
}

/* Used by mkdir, symlink and bind. */
struct resolved_slot *resolve_empty_slot
  (struct filesys_obj *root, struct dir_stack *cwd,
   seqf_t pathname, int symlink_limit, int *err)
{
  region_t r = region_make();
  void *result;
  int rc =
    resolve_obj(r, root, cwd, pathname, SYMLINK_LIMIT,
		1 /* nofollow */, 1 /* create */, &result, err);
  region_free(r);
  if(rc == RESOLVED_DIR) {
    dir_stack_free(result);
    *err = EEXIST;
    return 0;
  }
  else if(rc == RESOLVED_FILE_OR_SYMLINK) {
    filesys_obj_free(result);
    *err = EEXIST;
    return 0;
  }
  else if(rc == RESOLVED_EMPTY_SLOT) {
    return result;
  }
  else if(rc <= 0) { return 0; }
  *err = EIO;
  return 0;
}

/* Used by unlink, rmdir, rename and link. */
struct resolved_slot *resolve_any_slot
  (struct filesys_obj *root, struct dir_stack *cwd,
   seqf_t pathname, int symlink_limit, int *err)
{
  region_t r = region_make();
  void *result;
  int rc =
    resolve_obj(r, root, cwd, pathname, SYMLINK_LIMIT,
		1 /* nofollow */, CREATE_ONLY /* create */, &result, err);
  region_free(r);
  if(rc == RESOLVED_EMPTY_SLOT) {
    return result;
  }
  /* This case should only happen for the root directory: */
  else if(rc == RESOLVED_DIR) {
    dir_stack_free(result);
    *err = EACCES; /* EISDIR would also be appropriate */
    return 0;
  }
  else if(rc <= 0) { return 0; }
  *err = EIO;
  return 0;
}
