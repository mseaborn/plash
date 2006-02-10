

struct malloc_block {
  int check;
  struct malloc_block *next;
};
static struct malloc_block *blocks = 0;
void *check_malloc(int size)
{
  struct malloc_block *b;
  for(b = blocks; b; b = b->next) {
    assert(b->check = 0x12345678);
  }
  {
    struct malloc_block *x = malloc(sizeof(struct malloc_block) + size);
    x->check = 0x12345678;
    x->next = blocks;
    blocks = x;
    return (char *) x + sizeof(struct malloc_block);
  }
}
