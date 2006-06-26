/* Copyright (C) 2004, 2005 Mark Seaborn

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
#include "parse-filename.h"
#include "filesysobj-readonly.h"
#include "filesysobj-fab.h"
#include "filesysslot.h"
#include "build-fs.h"


#define MOD_DEBUG 0
#define MOD_MSG "build_fs: "
#define LOG stderr


int next_inode = 1;

struct node *fs_make_empty_node()
{
  struct node *n = filesys_obj_make(sizeof(struct node), &node_vtable);
  n->inode = next_inode++;
  n->symlink_dest = 0;
  n->attach_slot = 0;
  n->children = 0;
  return n;
}

fs_node_t fs_node_upcast(cap_t obj)
{
  return obj->vtable == &node_vtable ? (fs_node_t) obj : NULL;
}

void node_free(struct filesys_obj *obj)
{
  struct node *node = (void *) obj;
  struct node_list *l = node->children;
  while(l) {
    struct node_list *next = l->next;
    free(l->name);
    filesys_obj_free((cap_t) l->node);
    free(l);
    l = next;
  }

  if(node->symlink_dest) free(node->symlink_dest);
  if(node->attach_slot) filesys_obj_free(node->attach_slot);
}

#ifdef GC_DEBUG
void node_mark(struct filesys_obj *obj)
{
  struct node *node = (void *) obj;
  struct node_list *l;
  for(l = node->children; l; l = l->next) {
    filesys_obj_mark((cap_t) l->node);
  }
  if(node->attach_slot) filesys_obj_mark(node->attach_slot);
}
#endif

/* Move down the filesystem being constructed.  Create a new node
   if necessary. */
/* Returns borrowed reference. */
struct node *tree_traverse(struct node *node, const char *name)
{
  struct node_list *list_entry = (void *) assoc((void *) node->children, name);
  if(!list_entry) {
    list_entry = amalloc(sizeof(struct node_list));
    list_entry->name = strdup(name);
    list_entry->node = fs_make_empty_node();
    /* Insert into the current node */
    list_entry->next = node->children;
    node->children = list_entry;
  }
  return list_entry->node;
}

void fs_print_tree(int indent, struct node *node)
{
  int i;
  struct node_list *list;
  
  /* for(i = 0; i < indent; i++) putchar(' '); */
  printf("obj: symlink=%s, attach=%s\n",
	 node->symlink_dest ? node->symlink_dest : "none",
	 node->attach_slot ? "slot" : "none");
  for(list = node->children; list; list = list->next) {
    for(i = 0; i < indent+2; i++) putchar(' ');
    printf("\"%s\" => ", list->name);
    fs_print_tree(indent + 2, list->node);
  }
}


/* Like dir_stack, but also contains pointers into the filesystem tree
   being constructed. */
struct dirnode_stack {
  int refcount;
  struct filesys_obj *dir;
  struct node *node; /* this is not an owning reference */
  /* parent may be null, in which case, name is null too. */
  struct dirnode_stack *parent;
  char *name;
};

void dirnode_stack_free(struct dirnode_stack *st)
{
  /* Written using a loop instead of tail recursion
     (although tail recursion would have been clearer). */
  assert(st->refcount > 0);
  st->refcount--;
  while(st && st->refcount <= 0) {
    struct dirnode_stack *parent = st->parent;

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

/* Takes `obj' as an owning reference; it is a file or directory. */
int attach_ro_obj(struct node *node, cap_t obj)
{
  int replaced = 0;
  if(node->attach_slot) {
    filesys_obj_free(node->attach_slot);
    replaced = 1;
  }
  node->attach_slot = make_read_only_slot(make_read_only_proxy(obj));
  return replaced;
}

/* Takes `obj' as an owning reference; it is a slot. */
int attach_rw_slot(struct node *node, struct filesys_obj *obj)
{
  int replaced = 0;
  if(node->attach_slot) {
    filesys_obj_free(node->attach_slot);
    replaced = 1;
  }
  node->attach_slot = obj;
  return replaced;
}

/* Converts a dir_stack to a dirnode_stack, populating the filesystem
   tree as it goes. */
/* Arguments `root' and `cwd' are taken as non-owning references.
   Returns an owning reference. */
struct dirnode_stack *cwd_populate(struct dirnode_stack *root,
				   struct dir_stack *cwd)
{
  if(cwd->parent) {
    struct dirnode_stack *parent = cwd_populate(root, cwd->parent);
    struct dirnode_stack *dirstack = amalloc(sizeof(struct dirnode_stack));
    dirstack->refcount = 1;
    dirstack->dir = inc_ref(cwd->dir);
    dirstack->node = tree_traverse(parent->node, cwd->name);
    dirstack->parent = parent;
    dirstack->name = strdup(cwd->name);
    return dirstack;
  }
  else {
    root->refcount++;
    return root;
  }
}

/* Similar, but it doesn't create a dirnode_stack. */
struct node *cwd_populate2(struct node *root, struct dir_stack *cwd)
{
  if(cwd->parent) {
    return tree_traverse(cwd_populate2(root, cwd->parent), cwd->name);
  }
  else return root;
}

/* Attaches a given object at a given path in the filesystem structure. */
/* Takes an owning reference to obj. */
int fs_attach_at_pathname(struct node *root_node, struct dir_stack *cwd_ds,
			  seqf_t filename, cap_t obj, int *err)
{
  struct node *node;
  int end;
  switch(filename_parse_start(filename, &end, &filename)) {
    case FILENAME_ROOT:
      node = root_node;
      break;
    case FILENAME_CWD:
      if(!cwd_ds) { filesys_obj_free(obj); *err = E_NO_CWD_DEFINED; return -1; }
      node = cwd_populate2(root_node, cwd_ds);
      break;
    default: filesys_obj_free(obj); *err = ENOENT; return -1; /* Error: bad filename */
  }

  while(!end) {
    seqf_t name;
    int trailing_slash;
    filename_parse_component(filename, &name, &end, &filename, &trailing_slash);
    if(filename_parent(name)) {
      /* Easier not to support "..". */
      printf(_("plash: warning: \"..\" not supported in filesystem bindings\n"));
      filesys_obj_free(obj);
      *err = ENOENT;
      return -1;
    }
    else if(filename_samedir(name)) {
      /* Do nothing */
    }
    else {
      char *name1 = strdup_seqf(name);
      node = tree_traverse(node, name1);
      free(name1);
    }
    /* Doesn't check trailing_slash.  FIXME? */
  }
  if(attach_rw_slot(node, make_read_only_slot(obj))) {
    region_t r = region_make();
    printf(_("plash: warning: object bound to `%s' was replaced\n"),
	   region_strdup_seqf(r, filename));
    region_free(r);
  }
  return 0;
}

#define REACHED_DIR 1
#define REACHED_FILE 2
#define ATTACHED_OBJ 3
/* If "attach" is true:
     * This will attach the final object reached in the path into the
       filesystem tree.
     * This is used in the first call, and to resolve any symlinks in
       the "tail" position.  Other symlinks are in directory position.
     * The FS_SLOT_RWC/FS_OBJECT_RW flags have a meaning.
     * Will either return ATTACHED or 0 and an error.
   Otherwise:
   If it reached a directory, returns REACHED_DIR, and fills out "result"
   with a dirstack.  This is used for following symlinks.
   If it reached a file, returns REACHED_FILE.
   If it got an error, returns 0. */
/* The purpose of the `dir_only' argument is to ensure that the function
   will not attach a file object when the pathname ends with a slash,
   including when the last element is a symlink.  This doesn't apply
   when the flag FS_SLOT_RWC is given, however. */
int fs_resolve_populate_aux
  (region_t r,
   struct dirnode_stack *root, struct dirnode_stack *cwd,
   seqf_t filename, int symlink_limit,
   int attach, int flags, int dir_only,
   struct dirnode_stack **result, int *err)
{
  struct dirnode_stack *dirstack;  
  int end;
  
  switch(filename_parse_start(filename, &end, &filename)) {
    case FILENAME_ROOT:
      dirstack = root;
      break;
    case FILENAME_CWD:
      dirstack = cwd;
      break;
    default: *err = ENOENT; return 0; /* Error: bad filename */
  }
  dirstack->refcount++;

  while(!end) {
    seqf_t name;
    int trailing_slash;
    filename_parse_component(filename, &name, &end, &filename, &trailing_slash);
    if(filename_parent(name)) {
      if(dirstack->parent) {
	struct dirnode_stack *p = dirstack->parent;
	p->refcount++;
	dirnode_stack_free(dirstack);
	dirstack = p;
      }
      /* Do nothing if we're at the root directory */
    }
    else if(filename_samedir(name)) {
      /* Do nothing */
    }
    else {
      struct node *next_node;
      char *name1 = strdup_seqf(name);
      struct filesys_obj *obj;
      int obj_type;
      if(MOD_DEBUG) fprintf(LOG, MOD_MSG "fs_resolve_populate_aux: \"%s\": ", name1);

      /* Attach as a writable slot.  NB. This code will attach "foo" as
	 a writable slot but not do the same for "foo/bar/..".  It won't
	 check that an object is a directory when the pathname has a
	 trailing slash.  FIXME! */
      if(end && attach && (flags & FS_SLOT_RWC)) {
	struct node *next_node = tree_traverse(dirstack->node, name1);
	if(MOD_DEBUG) fprintf(LOG, "attach writable slot\n");
	attach_rw_slot(next_node,
		       make_generic_slot(inc_ref(dirstack->dir), name1));
	
	/* If following symlinks is enabled, don't stop here at attaching
	   the slot.  We need to look at what's in the slot.  Rather than
	   dropping through to the usual symlink case, the code is
	   duplicated here because it's slightly different:
	    * There is no error if the slot is empty.
	    * There is no error if the slot contains a dangling symlink,
	      and if it contains a file or dir, no further action is
	      taken.
	    * In this case, we don't attach a symlink into the tree
	      (as we're attaching the slot that contains it). */
	if((flags & FS_FOLLOW_SYMLINKS) && symlink_limit > 0) {
	  /* NB. Carry on using name1 -- should be live. */
	  obj = dirstack->dir->vtable->traverse(dirstack->dir, name1);
	  if(obj) {
	    obj_type = obj->vtable->type(obj);
	    if(obj_type == OBJT_SYMLINK) {
	      seqf_t link_dest;
	      if(obj->vtable->readlink(obj, r, &link_dest, err) >= 0) {
		/* Ignore any errors this produces.  Maybe they should be
		   given as warnings instead. */
		fs_resolve_populate_aux(r, root, dirstack, link_dest,
					symlink_limit-1, attach, flags,
					trailing_slash, result, err);
	      }
	    }
	    filesys_obj_free(obj);
	  }
	}
	
	dirnode_stack_free(dirstack);
	return ATTACHED_OBJ;
      }
      
      obj = dirstack->dir->vtable->traverse(dirstack->dir, name1);
      if(!obj) {
	if(MOD_DEBUG) fprintf(LOG, "not found\n");
	free(name1);
	dirnode_stack_free(dirstack);
	*err = ENOENT;
	return 0; /* Error */
      }
      
      next_node = tree_traverse(dirstack->node, name1);

      obj_type = obj->vtable->type(obj);
      if(obj_type == OBJT_DIR) {
	struct dirnode_stack *new_d = amalloc(sizeof(struct dirnode_stack));
	new_d->refcount = 1;
	new_d->dir = obj;
	new_d->node = next_node;
	new_d->parent = dirstack;
	new_d->name = name1;
	dirstack = new_d;
	if(MOD_DEBUG) fprintf(LOG, "dir\n");
	/* Drop through to loop */
      }
      else if(obj_type == OBJT_SYMLINK) {
	seqf_t link_dest;
	if(MOD_DEBUG) fprintf(LOG, "symlink\n");
	free(name1);
	if(symlink_limit <= 0) {
	  filesys_obj_free(obj);
	  dirnode_stack_free(dirstack);
	  *err = ELOOP;
	  return 0; /* Error */
	}
	if(obj->vtable->readlink(obj, r, &link_dest, err) < 0) {
	  filesys_obj_free(obj);
	  dirnode_stack_free(dirstack);
	  return 0; /* Error */
	}
	filesys_obj_free(obj);

	/* Attaches the symlink into the filesystem tree.  NB. This is a
	   copy.  If the underlying filesystem changes, this won't change.
	   Furthermore, a client can't modify/delete the symlink. */
	if(next_node->symlink_dest) free(next_node->symlink_dest);
	next_node->symlink_dest = strdup_seqf(link_dest);
	
	if(flags & FS_FOLLOW_SYMLINKS) {
	  int rc = fs_resolve_populate_aux(r, root, dirstack,
					   link_dest, symlink_limit-1,
					   end ? attach : 0,
					   flags,
					   (end && trailing_slash) || dir_only,
					   result, err);
	  dirnode_stack_free(dirstack);
	  if(end) return rc;
	  else if(rc == REACHED_DIR) {
	    dirstack = *result;
	    /* Drop through to loop */
	  }
	  else if(rc == REACHED_FILE) {
	    *err = ENOTDIR;
	    return 0; /* Error */
	  }
	  else if(rc == 0) return 0;
	  else { assert(0); *err = EIO; return 0; }
	}
	else {
	  dirnode_stack_free(dirstack);
	  if(end) return ATTACHED_OBJ;
	  else {
	    /* We want to say that we reached a symlink but we're not
	       following them.  This errno is vaguely appropriate:
	       open() gives it when using O_NOFOLLOW. */
	    *err = ELOOP;
	    return 0; /* Error */
	  }
	}
      }
      /* Treat remaining objects as files. */
      else /* if(obj_type == OBJT_FILE) */ {
	if(MOD_DEBUG) fprintf(LOG, "file\n");
	free(name1);
	dirnode_stack_free(dirstack);
	if(end && !(trailing_slash || dir_only)) {
	  if(attach) {
	    if(flags & FS_OBJECT_RW) {
	      attach_rw_slot(next_node, make_read_only_slot(obj));
	    }
	    else {
	      attach_ro_obj(next_node, obj);
	    }
	    return ATTACHED_OBJ;
	  }
	  else {
	    filesys_obj_free(obj);
	    return REACHED_FILE;
	  }
	}
	else {
	  filesys_obj_free(obj);
	  *err = ENOTDIR;
	  return 0; /* Error */
	}
      }
#if 0
      else {
	*err = ENOTDIR; /* anything more appropriate? */
	return 0; /* Error */
      }
#endif
    }
  }

  /* Reached a directory */
  if(attach) {
    /* This case is used for attaching the pathnames "." and "/".
       They can be attached as writable, but not as writable slots. */
    if(flags & (FS_SLOT_RWC | FS_OBJECT_RW)) {
      attach_rw_slot(dirstack->node, make_read_only_slot(inc_ref(dirstack->dir)));
    }
    else {
      /* FIXME: produce warning for overwriting? */
      attach_ro_obj(dirstack->node, inc_ref(dirstack->dir));
    }
    dirnode_stack_free(dirstack);
    return ATTACHED_OBJ;
  }
  else {
    *result = dirstack;
    return REACHED_DIR;
  }
}

/* Returns -1 if there's an error, 0 otherwise. */
int fs_resolve_populate
  (struct filesys_obj *root_obj, struct node *root_node,
   struct dir_stack *cwd_ds,
   seqf_t filename, int flags, int *err)
{
  struct dirnode_stack *root, *cwd;
  int end;
  root = amalloc(sizeof(struct dirnode_stack));
  root->refcount = 1;
  root->dir = root_obj;
  root->dir->refcount++;
  root->node = root_node;
  root->parent = 0;
  root->name = 0;
  switch(filename_parse_start(filename, &end, &filename)) {
    case FILENAME_ROOT:
      /* cwd shouldn't get used in this case, but initialise it
	 just in case. */
      cwd = root;
      cwd->refcount++;
      break;
    case FILENAME_CWD:
      if(!cwd_ds) { *err = E_NO_CWD_DEFINED; return -1; }
      cwd = cwd_populate(root, cwd_ds);
      break;
    default: *err = ENOENT; return -1; /* Error: bad filename */
  }

  {
    region_t r = region_make();
    struct dirnode_stack *result_junk; /* Result not used or filled out */
    int rc = fs_resolve_populate_aux(r, root, cwd, filename, SYMLINK_LIMIT,
				     1 /* attach */, flags, 0 /* dir_only */,
				     &result_junk, err);
    region_free(r);
    dirnode_stack_free(root);
    dirnode_stack_free(cwd);
    if(rc == 0) return -1;
    if(rc == ATTACHED_OBJ) return 0;
    assert(0); *err = EIO; return -1;
  }
}

#include "out-vtable-build-fs.h"
