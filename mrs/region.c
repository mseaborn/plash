
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
  return r;
}

void region_free(region_t r)
{
  struct region_page *b = &r->h; /* NB. This is equal to r */
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
  if(node->subtree_count <= 0) {
    int i;
    for(i = 0; i < -node->subtree_count; i++) {
      data = flatten_aux(data, node->subtrees[i]);
    }
    return data;
  }
  else {
    struct seq_tree_leaf *leaf = (void*) node;
    memcpy(data, leaf->data, leaf->size);
    return data + leaf->size;
  }
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

void close_fds(fds_t fds)
{
  int i;
  for(i = 0; i < fds.count; i++) close(fds.fds[i]);
}
