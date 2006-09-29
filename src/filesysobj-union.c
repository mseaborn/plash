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

#include <errno.h>

#include "region.h"
#include "serialise.h"
#include "filesysobj-fab.h"
#include "cap-protocol.h"


DECLARE_VTABLE(union_dir_vtable);

struct union_dir {
  struct filesys_obj hdr;
  struct filesys_obj *dir1, *dir2;
};

struct filesys_obj *make_union_dir(struct filesys_obj *dir1,
				   struct filesys_obj *dir2)
{
  struct union_dir *dir =
    filesys_obj_make(sizeof(struct union_dir), &union_dir_vtable);
  dir->dir1 = dir1;
  dir->dir2 = dir2;
  return (struct filesys_obj *) dir;
}


void union_dir_free(struct filesys_obj *obj)
{
  struct union_dir *dir = (void *) obj;
  filesys_obj_free(dir->dir1);
  filesys_obj_free(dir->dir2);
}

#ifdef GC_DEBUG
void union_dir_mark(struct filesys_obj *obj)
{
  struct union_dir *dir = (void *) obj;
  filesys_obj_mark(dir->dir1);
  filesys_obj_mark(dir->dir2);
}
#endif

int union_dir_stat(struct filesys_obj *obj, struct stat *buf, int *err)
{
  struct union_dir *dir = (void *) obj;
  if(dir->dir1->vtable->stat(dir->dir1, buf, err) < 0) return -1;
  buf->st_nlink = 0; /* FIXME: this can be used to count the number of child directories */
  return 0;
}

struct filesys_obj *union_dir_traverse(struct filesys_obj *obj, const char *leaf)
{
  struct union_dir *dir = (void *) obj;
  struct filesys_obj *child1, *child2;
  int type1, type2;

  /* If the entry is not present in dir1, return entry from dir2. */
  child1 = dir->dir1->vtable->traverse(dir->dir1, leaf);
  if(!child1) return dir->dir2->vtable->traverse(dir->dir2, leaf);

  /* If dir1's entry is not a directory, it overrides any entry from dir2. */
  type1 = child1->vtable->type(child1);
  if(type1 != OBJT_DIR) return child1;

  /* If the entry is not present in dir2, return entry from dir1. */
  child2 = dir->dir2->vtable->traverse(dir->dir2, leaf);
  if(!child2) return child1;

  /* If dir2's entry is not a directory, dir1's entry overrides it. */
  type2 = child2->vtable->type(child2);
  if(type2 != OBJT_DIR) {
    filesys_obj_free(child2);
    return child1;
  }

  /* Otherwise, dir1 and dir2's entries are both directories, so we
     union them together.  Create a new union directory proxy
     object. */
  return make_union_dir(child1, child2);
}

static int sort_seqf_compare(const void *p1, const void *p2)
{
  return seqf_compare(*(seqf_t *) p1, *(seqf_t *) p2);
}

int union_dir_list(struct filesys_obj *obj, region_t r, seqt_t *result, int *err)
{
  struct union_dir *dir = (void *) obj;
  int count1, count2;
  seqt_t got;
  seqf_t buf;
  seqf_t *array1, *array2;
  int i;

  region_t r2 = region_make();

  count1 = dir->dir1->vtable->list(dir->dir1, r2, &got, err);
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

  count2 = dir->dir2->vtable->list(dir->dir2, r2, &got, err);
  if(count2 < 0) { region_free(r2); return -1; }
  array2 = region_alloc(r2, count2 * sizeof(seqf_t));
  buf = flatten(r, got);
  for(i = 0; i < count2; i++) {
    int inode, type;
    seqf_t name;
    int ok = 1;
    m_int(&ok, &buf, &inode);
    m_int(&ok, &buf, &type);
    m_lenblock(&ok, &buf, &name);
    if(ok) {
      array2[i] = name;
    }
    else { region_free(r2); *err = EIO; return -1; }
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


#include "out-vtable-filesysobj-union.h"
