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

#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include "region.h"


/* Region:  A storage area from which blocks can be allocated quickly.
   These blocks cannot be freed individually; the region is destroyed in
   one go.  The region is implemented as a linked list of malloc()'d
   `pages'.  Blocks are always allocated from the last page, and a new
   page will be malloc()'d if the last page does not have enough space.

   Possible future changes:  Different page sizes.  Don't free pages, but
   use them for future regions (as we're not picky about how big they are).
*/

#define STATS 1

struct finaliser_cons {
  void (*finalise)(void *obj);
  void *obj;
  struct finaliser_cons *next;
};

/* This is the header for each malloc()'d page.  The pages form a
   linked list. */
struct region_page {
#ifdef STATS
  int size;
#endif
  struct region_page *next;
};
/* This is the header for the first malloc()'d page, and contains
   information about the whole region. */
struct region {
  struct region_page h;
  struct region_page **last; /* Used to add new pages to the list */
  char *free; /* Pointer to next free byte */
  int avail; /* Bytes available in last page */
  struct finaliser_cons *finalisers;
};

region_t region_make()
{
  int size = 8 * 1024;
  struct region *r = malloc(sizeof(struct region) + size);
  if(!r) {
    /* fprintf(stderr, "Out of memory creating region\n");
       exit(EXIT_FAILURE); */
    assert(!"Out of memory creating region");
    _exit(EXIT_FAILURE);
  }
#ifdef STATS
  r->h.size = size;
#endif
  r->h.next = 0;
  r->last = &r->h.next;
  r->free = ((char *) r) + sizeof(struct region);
  r->avail = size;
  r->finalisers = 0;
  return r;
}

void region_free(region_t r)
{
  struct region_page *b;

  struct finaliser_cons *c;
  for(c = r->finalisers; c; c = c->next) c->finalise(c->obj);
  
  b = &r->h; /* NB. This is equal to r */
  while(b) {
    struct region_page *next = b->next;
    free(b);
    b = next;
  }
}

void *region_alloc(region_t r, size_t size)
{
  /* Word-align the size */
  int s = (size + 3) &~ 3;

  if(s <= r->avail) {
    char *x = r->free;
    r->free += s;
    r->avail -= s;
    return x;
  }
  else {
    int s2 = 8 * 1024;
    struct region_page *b;
    if(s >= s2) { s2 = s; }
    b = malloc(sizeof(struct region_page) + s2);
    if(!b) {
      /* fprintf(stderr, "Out of memory extending region (trying to malloc(%i))\n", sizeof(struct region_page) + s2);
         exit(EXIT_FAILURE); */
      assert(!"Out of memory extending region");
      _exit(EXIT_FAILURE);
    }
#ifdef STATS
    b->size = s2;
#endif
    b->next = 0;
    *r->last = b;
    r->last = &b->next;
    r->free = ((char *) b) + sizeof(struct region_page) + s;
    r->avail = s2 - s;
    return ((char *) b) + sizeof(struct region_page);
  }
}

void region_add_finaliser(region_t r, void (*f)(void *obj), void *obj)
{
  struct finaliser_cons *c = region_alloc(r, sizeof(struct finaliser_cons));
  c->finalise = f;
  c->obj = obj;
  c->next = r->finalisers;
  r->finalisers = c;
}

#ifdef STATS
/* Returns the number of bytes allocated from a region.
   This is an approximation: includes wasted bytes at the end of pages. */
int region_allocated(region_t r)
{
  int size = 0;
  struct region_page *page = &r->h;
  while(page->next) {
    size += page->size;
    page = page->next;
  }
  return size + page->size - r->avail;
}
#endif



seqf_t seqf_empty = { 0, 0 };
fds_t fds_empty = { 0, 0 };
static struct seq_tree_leaf seqt_empty_leaf = { 0, "" };
seqt_t seqt_empty = { (struct seq_tree_node *) &seqt_empty_leaf, 0 };

seqt_t mk_repeat(region_t r, char c, int n)
{
  seqf_t t;
  char *d = region_alloc(r, n);
  memset(d, c, n);
  t.data = d;
  t.size = n;
  return mk_leaf(r, t);
}

static char *flatten_aux(char *data, struct seq_tree_node *node)
{
  /* Remember, the subtree count is negated */
  if(node->subtree_count < 0) {
    int i;
    for(i = 0; i < -node->subtree_count; i++) {
      data = flatten_aux(data, node->subtrees[i]);
    }
    return data;
  }
  else {
    struct seq_tree_leaf *leaf = (void *) node;
    memcpy(data, leaf->data, leaf->size);
    return data + leaf->size;
  }
}

void flatten_into(char *out, seqt_t seq)
{
  flatten_aux(out, seq.t);
}

seqf_t flatten(region_t r, seqt_t seq)
{
  seqf_t flat;
  char *buf = region_alloc(r, seq.size);
  flat.data = buf;
  flat.size = seq.size;
  flatten_aux(buf, seq.t);
  return flat;
}

/* This null-terminates the data.  Sometimes this is useful.  If it's
   not, it only wastes a byte! */
seqf_t flatten0(region_t r, seqt_t seq)
{
  seqf_t flat;
  char *buf = region_alloc(r, seq.size + 1);
  flat.data = buf;
  flat.size = seq.size;
  flatten_aux(buf, seq.t);
  buf[seq.size] = 0;
  return flat;
}

seqf_t flatten_reuse(region_t r, seqt_t seq)
{
  if(seq.t->subtree_count < 0) {
    return flatten(r, seq);
  }
  else {
    struct seq_tree_leaf *leaf = (void *) seq.t;
    seqf_t flat = { leaf->data, leaf->size };
    return flat;
  }
}

int seqf_equal(seqf_t str1, seqf_t str2)
{
  if(str1.size == str2.size) {
#if 0
    return !memcmp(str1.data, str2.data, str1.size);
#else
    /* memcmp not available in ld.so, so use this: */
    int i;
    for(i = 0; i < str1.size; i++) {
      if(str1.data[i] != str2.data[i]) { return 0; }
    }
    return 1;
#endif
  }
  else return 0;
}

void close_fds(fds_t fds)
{
  int i;
  for(i = 0; i < fds.count; i++) close(fds.fds[i]);
}
