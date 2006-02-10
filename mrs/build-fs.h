
#ifndef build_fs_h
#define build_fs_h

#include "filesysobj.h"
#include "filesysslot.h"

struct node;

struct node *make_empty_node();
void print_tree(int indent, struct node *node);
int resolve_populate
  (struct filesys_obj *root_obj, struct node *root_node,
   struct dir_stack *cwd_ds,
   seqf_t filename, int create, int *err);
struct filesys_slot *build_fs(struct node *node);

#endif
