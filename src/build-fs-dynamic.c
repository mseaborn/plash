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
#include "filesysobj.h"
#include "filesysobj-fab.h"
#include "filesysobj-readonly.h"
#include "serialise.h"
#include "build-fs.h"


#define MOD_DEBUG 0


/* Behaviour of "..":
   a path like "/home/mrs/../httpd" will create a directory "/home/mrs"
   but not put anything into it (it will be empty), while
   "/home/httpd" will be attached.
*/

/* Recording where grants come from: */
/* (is this useful if there is no revoke facility?) */

/* Node flags:  whether attaching a file using "path=EXPR" syntax or not */
/* Would be used for reviewing grants */
/* Should we record EXPR in this data structure? */

/* record whether attaching a ro or rw slot */

/* NB. when attaching a rw slot, it can be a symlink, file or dir */

/* what happens if we attach a real dir as rw, but attach stuff below it?
   does it still behave as a slot?  what if it's replaced by a non-dir
   or removed?
   * follow the same rules as recursive union dirs:
     if the two objects aren't both dirs, return the first one;
     in our case, the "attached below" objects come first, and the
     "attached at" directory (slot) comes second
*/

/* two kinds of object derived from a node:
     slot of node?:  node on its own
     node + directory:  not quite the same as union, because when node
       contains an attached object is replaces the corresponding part
       of the directory
*/

/* implementations:
    - given node, return slot.
    - given node, return file/dir/symlink obj.
*/

/* BUGS:
   command line: "=> foo" will *not* follow foo as a symlink.
   hence read/write/create is not a superset of read-only.
*/


/* There are two kinds of fabricated directory:
    - Purely fabricated (these have dir = 0)
    - Combined (with a fabricated, falling back to a real directory)
*/
DECLARE_VTABLE(comb_dir_vtable);
struct comb_dir {
  struct filesys_obj hdr;
  struct node *node; /* only the `children' field will be accessed through this pointer */
  struct filesys_obj *dir; /* may be null */
};

void comb_dir_free(struct filesys_obj *obj1)
{
  struct comb_dir *obj = (void *) obj1;
  filesys_obj_free((void *) obj->node);
  if(obj->dir) filesys_obj_free(obj->dir);
}

int comb_dir_stat(struct filesys_obj *obj, struct stat *st, int *err)
{
  struct comb_dir *dir = (void *) obj;
  st->st_dev = FAB_OBJ_STAT_DEVICE;
  st->st_ino = dir->node->inode;
  st->st_mode = S_IFDIR | 0777;
  st->st_nlink = 0; /* FIXME: this can be used to count the number of child directories */
  st->st_uid = 0;
  st->st_gid = 0;
  st->st_rdev = 0;
  st->st_size = 0;
  st->st_blksize = 1024;
  st->st_blocks = 0;
  st->st_atime = 0;
  st->st_mtime = 0;
  st->st_ctime = 0;
  return 0;
}

/* Takes owning references.  `dir' may be null. */
struct filesys_obj *make_comb_dir(struct node *node, struct filesys_obj *dir)
{
  struct comb_dir *n = amalloc(sizeof(struct comb_dir));
  n->hdr.refcount = 1;
  n->hdr.vtable = &comb_dir_vtable;
  n->node = node;
  n->dir = dir;
  return (struct filesys_obj *) n;
}

/* Takes `node' as a non-owning reference.  `dir' is non-owning, may be null. */
struct filesys_obj *object_of_node(struct node *node, struct filesys_obj *dir,
				   int ensure_fabricated)
{
  if(node->symlink_dest) {
    /* FIXME? copying the destination is unnecessary */
    struct fab_symlink *sym = amalloc(sizeof(struct fab_symlink));
    sym->hdr.refcount = 1;
    sym->hdr.vtable = &fab_symlink_vtable;
    sym->dest = dup_seqf(seqf_string(node->symlink_dest));
    sym->inode = node->inode;
    return (struct filesys_obj *) sym;
  }
  else if(node->attach_slot) {
    /* In this case, `dir' is ignored: it is overridden by any attached
       slot (whether empty or full). */
    /* FIXME: check if `obj' is not a dir; if so, pass null instead */
    struct filesys_obj *obj =
      node->attach_slot->vtable->slot_get(node->attach_slot);
    if(node->children || ensure_fabricated) {
      node->hdr.refcount++;
      return make_comb_dir(node, obj /* may be null */);
    }
    else return obj;
  }
  else {
    node->hdr.refcount++;
    if(dir) inc_ref(dir);
    return make_comb_dir(node, dir /* may be null */);
  }
}

struct filesys_obj *comb_dir_traverse(struct filesys_obj *obj1, const char *name)
{
  struct comb_dir *obj = (void *) obj1;

  struct node_list *list_entry =
    (void *) assoc((void *) obj->node->children, name);
  if(list_entry) {
    struct filesys_obj *r;
    struct filesys_obj *subdir = 0;
    if(MOD_DEBUG) fprintf(stderr, "traverse: \"%s\": fab slot\n", name);
    /* If this object is already a combined directory, continue with
       a combined directory where possible. */
    if(obj->dir && !list_entry->node->attach_slot) {
      /* FIXME: check if `subdir' is not a dir; if so, pass null instead */
      subdir = obj->dir->vtable->traverse(obj->dir, name);
    }
    r = object_of_node(list_entry->node, subdir, 0);
    if(subdir) filesys_obj_free(subdir);
    return r;
  }
  else if(obj->dir) {
    if(MOD_DEBUG) fprintf(stderr, "traverse: \"%s\": try real\n", name);
    return obj->dir->vtable->traverse(obj->dir, name);
  }
  else {
    if(MOD_DEBUG) fprintf(stderr, "traverse: \"%s\": no real\n", name);
    return 0;
  }
}

static int sort_seqf_compare(const void *p1, const void *p2)
{
  return seqf_compare(*(seqf_t *) p1, *(seqf_t *) p2);
}

int comb_dir_list(struct filesys_obj *obj1, region_t r, seqt_t *result, int *err)
{
  struct comb_dir *obj = (void *) obj1;
  int count1, count2;
  seqt_t got;
  seqf_t buf;
  seqf_t *array1, *array2;
  int i;

  region_t r2 = region_make();

  /* Get real directory listing. */
  if(obj->dir) {
    count1 = obj->dir->vtable->list(obj->dir, r2, &got, err);
    if(count1 < 0) { region_free(r2); return -1; }
    array1 = region_alloc(r2, count1 * sizeof(seqf_t));
    buf = flatten(r, got);
    for(i = 0; i < count1; i++) {
      int inode, type;
      seqf_t name;
      int ok = 1;
      m_int(&ok, &buf, &inode);
      m_int(&ok, &buf, &type);
      m_lenblock(&ok, &buf, &name);
      if(ok) {
	array1[i] = name;
      }
      else { region_free(r2); *err = EIO; return -1; }
    }
  }
  else {
    count1 = 0;
    array1 = 0;
  }

  /* Get fabricated directory listing. */
  {
    int length = 0;
    struct node_list *list = obj->node->children;
    while(list) { length++; list = list->next; }

    array2 = region_alloc(r2, length * sizeof(seqf_t));
    count2 = 0;
    list = obj->node->children;
    while(list) {
      /* Check whether entry is a full slot. */
      if(list->node->symlink_dest || list->node->children) {}
      else if(list->node->attach_slot) {
	struct filesys_obj *obj =
	  list->node->attach_slot->vtable->slot_get(list->node->attach_slot);
	if(!obj) goto empty;
	filesys_obj_free(obj);
      }
      else goto empty;

      /* Slot is full: include it in directory listing. */
      assert(count2 < length);
      array2[count2] = seqf_string(list->name);
      count2++;

    empty:
      list = list->next;
    }
  }

  /* Sort the sequences to make it easy to merge them. */
  qsort(array1, count1, sizeof(seqf_t), sort_seqf_compare);
  qsort(array2, count2, sizeof(seqf_t), sort_seqf_compare);

  /* Merge the two sequences. */
  {
    cbuf_t buf = cbuf_make(r, 100);
    int count = 0;
    int i1 = 0, i2 = 0;
    while(1) {
      int comp;
      if(i1 < count1 && i2 < count2) {
	comp = seqf_compare(array1[i1], array2[i2]);
      }
      else if(i1 < count1) comp = -1;
      else if(i2 < count2) comp = 1;
      else break;

      cbuf_put_int(buf, 0); /* d_ino */
      cbuf_put_int(buf, 0); /* d_type */
      if(comp < 0) {
	cbuf_put_int(buf, array1[i1].size);
	cbuf_put_seqf(buf, array1[i1]);
	i1++;
      }
      else if(comp > 0) {
	cbuf_put_int(buf, array2[i2].size);
	cbuf_put_seqf(buf, array2[i2]);
	i2++;
      }
      else {
	cbuf_put_int(buf, array1[i1].size);
	cbuf_put_seqf(buf, array1[i1]);
	i1++;
	i2++;
      }
      count++;
    }
    *result = seqt_of_cbuf(buf);
    region_free(r2);
    return count;
  }
}

#define N_SLOT 1
#define N_DIR 2

/* If there is an entry for `name' in the fabricated portion of this
   directory, returns N_SLOT and fills out `slot' (with a borrowed
   reference).
   Otherwise, if there is a valid directory in obj->dir to fall back to,
   returns N_DIR.
   Otherwise, it returns something else and fills out `err' with EACCES
   to indicate that this isn't a modifiable slot.
*/
int comb_dir_get_slot(struct comb_dir *obj, const char *name,
		      struct filesys_obj **slot, int *err)
{
  struct node_list *list_entry =
    (void *) assoc((void *) obj->node->children, name);
  if(list_entry) {
    struct node *node = list_entry->node;
    if(node->symlink_dest) {
      /* This is a purely fabricated symlink. */
      *err = EACCES; /* The slot is read-only */
      return 0;
    }
    else if(node->attach_slot && !node->children) {
      *slot = node->attach_slot;
      return N_SLOT;
    }
    else {
      /* This is a purely fabricated directory, or a combined (fabricated
	 plus real) directory. */
      *err = EACCES;
      return 0;
    }
  }
  else if(obj->dir) return N_DIR;
  else {
    *err = EACCES;
    return 0;
  }
}

/* Usually, the N_DIR branch of these methods (in which `dir' is used)
   won't return a successful result because directories with stuff
   attached below will be read-only.  One exception will be "/tmp" with
   stuff (such as ".X11-unix") attached below. */
int comb_dir_create_file(struct filesys_obj *obj1, const char *name,
			 int flags, int mode, int *err)
{
  struct comb_dir *obj = (void *) obj1;
  struct filesys_obj *slot;
  switch(comb_dir_get_slot(obj, name, &slot, err)) {
    case N_SLOT: return slot->vtable->slot_create_file(slot, flags, mode, err);
    case N_DIR: return obj->dir->vtable->create_file(obj->dir, name, flags, mode, err);
    default: return -1;
  }
}
int comb_dir_mkdir(struct filesys_obj *obj1, const char *name,
		   int mode, int *err)
{
  struct comb_dir *obj = (void *) obj1;
  struct filesys_obj *slot;
  switch(comb_dir_get_slot(obj, name, &slot, err)) {
    case N_SLOT: return slot->vtable->slot_mkdir(slot, mode, err);
    case N_DIR: return obj->dir->vtable->mkdir(obj->dir, name, mode, err);
    default: return -1;
  }
}
int comb_dir_symlink(struct filesys_obj *obj1, const char *name,
		     const char *oldpath, int *err)
{
  struct comb_dir *obj = (void *) obj1;
  struct filesys_obj *slot;
  switch(comb_dir_get_slot(obj, name, &slot, err)) {
    case N_SLOT: return slot->vtable->slot_symlink(slot, oldpath, err);
    case N_DIR: return obj->dir->vtable->symlink(obj->dir, name, oldpath, err);
    default: return -1;
  }
}
int comb_dir_unlink(struct filesys_obj *obj1, const char *name, int *err)
{
  struct comb_dir *obj = (void *) obj1;
  struct filesys_obj *slot;
  switch(comb_dir_get_slot(obj, name, &slot, err)) {
    case N_SLOT: return slot->vtable->slot_unlink(slot, err);
    case N_DIR: return obj->dir->vtable->unlink(obj->dir, name, err);
    default: return -1;
  }
}
int comb_dir_rmdir(struct filesys_obj *obj1, const char *name, int *err)
{
  struct comb_dir *obj = (void *) obj1;
  struct filesys_obj *slot;
  switch(comb_dir_get_slot(obj, name, &slot, err)) {
    case N_SLOT: return slot->vtable->slot_rmdir(slot, err);
    case N_DIR: return obj->dir->vtable->rmdir(obj->dir, name, err);
    default: return -1;
  }
}
int comb_dir_socket_bind(struct filesys_obj *obj1, const char *name,
			 int sock_fd, int *err)
{
  struct comb_dir *obj = (void *) obj1;
  struct filesys_obj *slot;
  switch(comb_dir_get_slot(obj, name, &slot, err)) {
    case N_SLOT: return slot->vtable->slot_socket_bind(slot, sock_fd, err);
    case N_DIR: return obj->dir->vtable->socket_bind(obj->dir, name, sock_fd, err);
    default: return -1;
  }
}


struct filesys_obj *fs_make_root(struct node *node)
{
  /* In some cases, we want to make sure that if the tree structure is
     modified, the change will always be reflected in the root
     directory that is returned here, even if, when this function is
     called, the root node has a directory attached to it with nothing
     attached below.  So we set the flag to true. */
  return object_of_node(node, 0 /* dir */, 1 /* ensure_fabricated */);
}

#include "out-vtable-build-fs-dynamic.h"
