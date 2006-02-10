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
#include "serialise.h"
#include "filesysobj-fab.h"
#include "cap-protocol.h"


DECLARE_VTABLE(union_dir_vtable);

struct union_dir {
  struct filesys_obj hdr;
  struct filesys_obj *x, *y;
};

struct filesys_obj *make_union_dir(struct filesys_obj *x, struct filesys_obj *y)
{
  struct union_dir *dir =
    filesys_obj_make(sizeof(struct union_dir), &union_dir_vtable);
  dir->x = x;
  dir->y = y;
  return (struct filesys_obj *) dir;
}


void union_dir_free(struct filesys_obj *obj)
{
  struct union_dir *dir = (void *) obj;
  filesys_obj_free(dir->x);
  filesys_obj_free(dir->y);
}

#ifdef GC_DEBUG
void union_dir_mark(struct filesys_obj *obj)
{
  struct union_dir *dir = (void *) obj;
  filesys_obj_mark(dir->x);
  filesys_obj_mark(dir->y);
}
#endif

int union_dir_stat(struct filesys_obj *obj, struct stat *buf, int *err)
{
  struct union_dir *dir = (void *) obj;
  if(dir->x->vtable->stat(dir->x, buf, err) < 0) return -1;
  buf->st_nlink = 0; /* FIXME: this can be used to count the number of child directories */
  return 0;
}

struct filesys_obj *union_dir_traverse(struct filesys_obj *obj, const char *leaf)
{
  struct union_dir *dir = (void *) obj;
  int type1, type2;
  struct filesys_obj *child2;
  struct filesys_obj *child1 = dir->x->vtable->traverse(dir->x, leaf);
  if(!child1) return dir->y->vtable->traverse(dir->y, leaf);

  type1 = child1->vtable->type(child1);
  if(type1 != OBJT_DIR) return child1;

  child2 = dir->y->vtable->traverse(dir->y, leaf);
  if(!child2) return child1;

  type2 = child2->vtable->type(child2);
  if(type2 != OBJT_DIR) {
    filesys_obj_free(child2);
    return child1;
  }

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

  count1 = dir->x->vtable->list(dir->x, r2, &got, err);
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

  count2 = dir->y->vtable->list(dir->y, r2, &got, err);
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
