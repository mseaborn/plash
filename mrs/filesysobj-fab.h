
#ifndef filesysobj_fab_h
#define filesysobj_fab_h

#include "filesysobj.h"
#include "filesysslot.h"


/* Abstract version that "assoc" operates on. */
struct list {
  char *name;
  void *val;
  struct list *next;
};

struct list *assoc(struct list *list, const char *name);


struct obj_list {
  char *name;
  struct filesys_obj *obj;
  struct obj_list *next;
};

struct fab_dir {
  struct filesys_obj hdr;
  struct obj_list *entries;
  int inode;
};

struct fab_symlink {
  struct filesys_obj hdr;
  seqf_t dest; /* malloc'd block, owned by this object */
  int inode;
};

extern struct filesys_obj_vtable fab_dir_vtable;
extern struct filesys_obj_vtable fab_symlink_vtable;


/* This is a version built from slots rather than filesys_objs */

struct slot_list {
  char *name;
  struct filesys_slot *slot;
  struct slot_list *next;
};

struct s_fab_dir {
  struct filesys_obj hdr;
  struct slot_list *entries;
  int inode;
};

extern struct filesys_obj_vtable s_fab_dir_vtable;
extern struct filesys_obj_vtable s_fab_symlink_vtable;


#endif
