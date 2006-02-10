
#ifndef region_h
#define region_h

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


typedef struct region *region_t;
region_t region_make();
void region_free(region_t r);
void *region_alloc(region_t r, size_t size);
int region_allocated(region_t r);


static inline void *amalloc(size_t size)
{
  void *x = malloc(size);
  assert(x);
  return x;
}
// for debugging:
// static inline void free(void *x) {}


struct seq_flat {
  const char *data;
  int size;
};
typedef struct seq_flat seqf_t;

struct seq_fds {
  const int *fds;
  int count;
};
typedef struct seq_fds fds_t;


extern seqf_t seqf_empty;
extern fds_t fds_empty;

static inline seqf_t seqf_string(const char *s) {
  seqf_t data = { s, strlen(s) };
  return data;
}

static inline void m_end(int *ok, seqf_t *block)
{
  if(block->size > 0) *ok = 0;
}

static inline void m_int(int *ok, seqf_t *block, int *res)
{
  if(!*ok) return;
  if(block->size >= sizeof(int)) {
    *res = *(int *) block->data;
    block->data += sizeof(int);
    block->size -= sizeof(int);
  }
  else { *ok = 0; }
}

static inline void m_str(int *ok, seqf_t *block, const char *str)
{
  if(!*ok) return;
  while(*str) {
    if(block->size >= 1 && *block->data == *str) {
      block->data++;
      block->size--;
      str++;
    }
    else { *ok = 0; return; }
  }
}

static inline void m_block(int *ok, seqf_t *block, int size, seqf_t *result)
{
  if(!*ok) return;
  if(block->size >= size) {
    result->data = block->data;
    result->size = size;
    block->data += size;
    block->size -= size;
  }
  else { *ok = 0; }
}

static inline void m_lenblock(int *ok, seqf_t *block, seqf_t *result)
{
  int len;
  m_int(ok, block, &len);
  m_block(ok, block, len, result);
}

static inline void m_fd(int *ok, fds_t *fds, int *fd)
{
  if(!*ok) return;
  if(fds->count > 0) {
    *fd = fds->fds[0];
    fds->count--;
  }
  else { *ok = 0; }
}


typedef struct seq_tree seqt_t;
struct seq_tree {
  struct seq_tree_node *t;
  int size;
};

struct seq_tree_node {
  int subtree_count; /* This is negated */
  struct seq_tree_node *subtrees[0];
};
struct seq_tree_leaf {
  int size;
  const char *data;
};

extern struct seq_tree seqt_empty;

static inline seqt_t mk_leaf2(region_t r, const char *data, int size) {
  struct seq_tree t;
  struct seq_tree_leaf *n = region_alloc(r, sizeof(struct seq_tree_leaf));
  n->size = size;
  n->data = data;
  t.t = (void *) n;
  t.size = size;
  return t;
}
static inline seqt_t mk_leaf(region_t r, seqf_t block) {
  return mk_leaf2(r, block.data, block.size);
}
static inline seqt_t mk_string(region_t r, const char *str) {
  return mk_leaf(r, seqf_string(str));
}
seqt_t mk_repeat(region_t r, char c, int n);
static inline seqt_t mk_int(region_t r, int x) {
  /* NB. This involves two allocations.  We could improve on that by
     combining it with mk_leaf2. */
  int *p = region_alloc(r, sizeof(int));
  *p = x;
  return mk_leaf2(r, (void *) p, sizeof(int));
}
static inline seqt_t cat2(region_t r, seqt_t t1, seqt_t t2) {
  struct seq_tree t;
  struct seq_tree_node *n =
    region_alloc(r, sizeof(struct seq_tree_node) +
		    2 * sizeof(struct seq_tree_node *));
  n->subtree_count = -2;
  n->subtrees[0] = t1.t;
  n->subtrees[1] = t2.t;
  t.t = n;
  t.size = t1.size + t2.size;
  return t;
}
static inline seqt_t cat3(region_t r, seqt_t t1, seqt_t t2, seqt_t t3) {
  struct seq_tree t;
  struct seq_tree_node *n =
    region_alloc(r, sizeof(struct seq_tree_node) +
		    3 * sizeof(struct seq_tree_node *));
  n->subtree_count = -3;
  n->subtrees[0] = t1.t;
  n->subtrees[1] = t2.t;
  n->subtrees[2] = t3.t;
  t.t = n;
  t.size = t1.size + t2.size + t3.size;
  return t;
}
static inline seqt_t cat4(region_t r, seqt_t t1, seqt_t t2, seqt_t t3, seqt_t t4) {
  struct seq_tree t;
  struct seq_tree_node *n =
    region_alloc(r, sizeof(struct seq_tree_node) +
		    4 * sizeof(struct seq_tree_node *));
  n->subtree_count = -4;
  n->subtrees[0] = t1.t;
  n->subtrees[1] = t2.t;
  n->subtrees[2] = t3.t;
  n->subtrees[3] = t4.t;
  t.t = n;
  t.size = t1.size + t2.size + t3.size + t4.size;
  return t;
}
static inline seqt_t cat5(region_t r, seqt_t t1, seqt_t t2, seqt_t t3, seqt_t t4, seqt_t t5) {
  struct seq_tree t;
  struct seq_tree_node *n =
    region_alloc(r, sizeof(struct seq_tree_node) +
		    5 * sizeof(struct seq_tree_node *));
  n->subtree_count = -5;
  n->subtrees[0] = t1.t;
  n->subtrees[1] = t2.t;
  n->subtrees[2] = t3.t;
  n->subtrees[3] = t4.t;
  n->subtrees[4] = t5.t;
  t.t = n;
  t.size = t1.size + t2.size + t3.size + t4.size + t5.size;
  return t;
}
static inline seqt_t cat6(region_t r, seqt_t t1, seqt_t t2, seqt_t t3, seqt_t t4, seqt_t t5, seqt_t t6) {
  struct seq_tree t;
  struct seq_tree_node *n =
    region_alloc(r, sizeof(struct seq_tree_node) +
		    6 * sizeof(struct seq_tree_node *));
  n->subtree_count = -6;
  n->subtrees[0] = t1.t;
  n->subtrees[1] = t2.t;
  n->subtrees[2] = t3.t;
  n->subtrees[3] = t4.t;
  n->subtrees[4] = t5.t;
  n->subtrees[5] = t6.t;
  t.t = n;
  t.size = t1.size + t2.size + t3.size + t4.size + t5.size + t6.size;
  return t;
}
seqf_t flatten(region_t r, seqt_t seq);
seqf_t flatten0(region_t r, seqt_t seq); /* Null-terminates data */
void print_data(seqf_t b);
void fprint_data(FILE *file, seqf_t b);
void fprint_d(FILE *file, seqf_t b);

static inline char *strdup_seqf(seqf_t x)
{
  char *s = amalloc(x.size + 1);
  memcpy(s, x.data, x.size);
  s[x.size] = 0;
  return s;
}

static inline seqf_t dup_seqf(seqf_t x)
{
  seqf_t result;
  char *buf = amalloc(x.size);
  memcpy(buf, x.data, x.size);
  result.data = buf;
  result.size = x.size;
  return result;
}

static inline seqf_t region_dup_seqf(region_t r, seqf_t x)
{
  seqf_t result;
  char *buf = region_alloc(r, x.size);
  memcpy(buf, x.data, x.size);
  result.data = buf;
  result.size = x.size;
  return result;
}

static inline char *region_strdup_seqf(region_t r, seqf_t x)
{
  char *buf = region_alloc(r, x.size + 1);
  memcpy(buf, x.data, x.size);
  buf[x.size] = 0;
  return buf;
}

static inline fds_t mk_fds1(region_t r, int fd1)
{
  fds_t fds;
  int *fds_space = region_alloc(r, sizeof(int));
  fds_space[0] = fd1;
  fds.fds = fds_space;
  fds.count = 1;
  return fds;
}

void close_fds(fds_t fds);


#endif
