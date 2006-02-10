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
#include "server.h"
#include "parse-filename.h"
#include "filesysobj-fab.h"
#include "build-fs.h"


/* As a special case, the root node is checked to make sure that if
   `attach' is non-null, it contains a directory object. */
/* Need to handle "slot" objects, which handle files/dirs that don't
   exist yet and can be created. */
struct node {
  /* Symlink:  malloc'd block.  May be null.  If this is used, the
     other parameters are ignored. */
  char *symlink_dest;
  /* Slot object to attach read-write.  May be null.  If this is used,
     the read-only object and the children are ignored. */
  struct filesys_slot *attach_rw_obj;
  /* Object to attach read-only, in a read-only slot.  This object may
     be writable; it needs to be attached using a read-only proxy.
     May be null.  If this is used, children may be attached below in
     the tree; they could be writable. */
  struct filesys_obj *attach_ro_obj;
  struct node_list *children; /* May be empty (ie. null) */
};

struct node_list {
  char *name;
  struct node *node;
  struct node_list *next;
};


struct node *make_empty_node()
{
  struct node *n = amalloc(sizeof(struct node));
  n->symlink_dest = 0;
  n->attach_rw_obj = 0;
  n->attach_ro_obj = 0;
  n->children = 0;
  return n;
}

/* Move down the filesystem being constructed.  Create a new node
   if necessary. */
struct node *tree_traverse(struct node *node, const char *name)
{
  struct node_list *list_entry = (void *) assoc((void *) node->children, name);
  if(!list_entry) {
    list_entry = amalloc(sizeof(struct node_list));
    list_entry->name = strdup(name);
    list_entry->node = make_empty_node();
    /* Insert into the current node */
    list_entry->next = node->children;
    node->children = list_entry;
  }
  return list_entry->node;
}

void print_tree(int indent, struct node *node)
{
  int i;
  struct node_list *list;
  
  for(i = 0; i < indent; i++) putchar(' ');
  printf("obj: symlink=%s, attach=%s\n",
	 node->symlink_dest ? node->symlink_dest : "none",
	 node->attach_rw_obj ? "rw" : node->attach_ro_obj ? "ro" : "none");
  for(list = node->children; list; list = list->next) {
    for(i = 0; i < indent+2; i++) putchar(' ');
    printf("%s =>\n", list->name);
    print_tree(indent + 4, list->node);
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

struct dirnode_stack *cwd_populate(struct dirnode_stack *root,
				   struct dir_stack *cwd)
{
  if(cwd->parent) {
    struct dirnode_stack *parent = cwd_populate(root, cwd->parent);
    struct dirnode_stack *dirstack = amalloc(sizeof(struct dirnode_stack));
    dirstack->refcount = 1;
    dirstack->dir = cwd->dir;
    dirstack->dir->refcount++;
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

#define REACHED_DIR 1
#define REACHED_FILE 2
#define ATTACHED_OBJ 3
/* If "attach" is true:
     * This will attach the final object reached in the path into the
       filesystem tree.
     * This is used in the first call, and to resolve any symlinks in
       the "tail" position.  Other symlinks are in directory position.
     * "create" has a meaning.
     * Will either return ATTACHED or 0 and an error.
   Otherwise:
   If it reached a directory, returns REACHED_DIR, and fills out "result"
   with a dirstack.  This is used for following symlinks.
   If it reached a file, returns REACHED_FILE.
   If it got an error, returns 0. */
int resolve_populate_aux
  (region_t r,
   struct dirnode_stack *root, struct dirnode_stack *cwd,
   seqf_t filename, int symlink_limit,
   int attach, int create, int dir_only,
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

      /* Attach as a writable slot.  NB. This code will attach "foo" as
	 a writable slot but not do the same for "foo/bar/..".  It won't
	 check that an object is a directory when the pathname has a
	 trailing slash.  FIXME! */
      if(end && attach && create) {
	struct node *next_node = tree_traverse(dirstack->node, name1);
	dirstack->dir->refcount++;
	next_node->attach_rw_obj =
	  make_generic_slot(dirstack->dir, name1);
	dirnode_stack_free(dirstack);
	return ATTACHED_OBJ;
      }
      obj = dirstack->dir->vtable->traverse(dirstack->dir, name1);
      if(!obj) {
	free(name1);
	dirnode_stack_free(dirstack);
	*err = ENOENT;
	return 0; /* Error */
      }
      
      next_node = tree_traverse(dirstack->node, name1);
      
      if(obj->vtable->type == OBJT_DIR) {
	struct dirnode_stack *new_d = amalloc(sizeof(struct dirnode_stack));
	new_d->refcount = 1;
	new_d->dir = obj;
	new_d->node = next_node;
	new_d->parent = dirstack;
	new_d->name = name1;
	dirstack = new_d;
      }
      else if(obj->vtable->type == OBJT_SYMLINK) {
	seqf_t link_dest;
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
	   Furthermore, a client can't modify/delete the symlink.
	   Should give different behaviour if "create" is set. */
	next_node->symlink_dest = strdup_seqf(link_dest);
	
	{
	  int rc = resolve_populate_aux(r, root, dirstack,
					link_dest, symlink_limit-1,
					end ? attach : 0,
					end ? create : 0,
					trailing_slash || dir_only,
					result, err);
	  dirnode_stack_free(dirstack);
	  if(end) return rc;
	  else if(rc == REACHED_DIR) {
	    dirstack = *result;
	  }
	  else if(rc == REACHED_FILE) {
	    *err = ENOTDIR;
	    return 0; /* Error */
	  }
	  else if(rc == 0) return 0;
	  else { assert(0); *err = EIO; return 0; }
	}
      }
      else if(obj->vtable->type == OBJT_FILE) {
	free(name1);
	dirnode_stack_free(dirstack);
	if(end && !(trailing_slash || dir_only)) {
	  if(attach) {
	    next_node->attach_ro_obj = obj;
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
    }
  }

  /* Reached a directory */
  if(attach) {
    dirstack->dir->refcount++;
    dirstack->node->attach_ro_obj = dirstack->dir;
    dirnode_stack_free(dirstack);
    return ATTACHED_OBJ;
  }
  else {
    *result = dirstack;
    return REACHED_DIR;
  }
}

/* Returns -1 if there's an error, 0 otherwise. */
int resolve_populate
  (struct filesys_obj *root_obj, struct node *root_node,
   struct dir_stack *cwd_ds,
   seqf_t filename, int create, int *err)
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
      cwd = cwd_populate(root, cwd_ds);
      break;
    default: *err = ENOENT; return 0; /* Error: bad filename */
  }

  {
    region_t r = region_make();
    struct dirnode_stack *result_junk; /* Result not used or filled out */
    int rc = resolve_populate_aux(r, root, cwd, filename, SYMLINK_LIMIT,
				  1 /* attach */, create, 0 /* dir_only */,
				  &result_junk, err);
    region_free(r);
    dirnode_stack_free(root);
    dirnode_stack_free(cwd);
    if(rc == 0) return -1;
    if(rc == ATTACHED_OBJ) return 0;
    assert(0); *err = EIO; return -1;
  }
}


static int next_inode = 1;

struct filesys_slot *build_fs(struct node *node)
{
  if(node->symlink_dest) {
    struct fab_symlink *sym = amalloc(sizeof(struct fab_symlink));
    seqf_t dest = { node->symlink_dest, strlen(node->symlink_dest) };
    sym->hdr.refcount = 1;
    sym->hdr.vtable = &fab_symlink_vtable;
    sym->dest = dup_seqf(dest);
    sym->inode = next_inode++;
    return make_read_only_slot((struct filesys_obj *) sym);
  }
  else if(node->attach_rw_obj) return node->attach_rw_obj;
  else if(node->attach_ro_obj) return make_read_only_slot(node->attach_ro_obj);
  else {
    /* Construct directory */
    struct s_fab_dir *dir = amalloc(sizeof(struct fab_dir));
    struct slot_list *nlist = 0;
    struct node_list *list;
    for(list = node->children; list; list = list->next) {
      struct slot_list *n = amalloc(sizeof(struct obj_list));
      n->name = strdup(list->name);
      n->slot = build_fs(list->node);
      n->next = nlist;
      nlist = n;
    }
    dir->hdr.refcount = 1;
    dir->hdr.vtable = &s_fab_dir_vtable;
    dir->entries = nlist;
    dir->inode = next_inode++;
    return make_read_only_slot((struct filesys_obj *) dir);
  }
}
