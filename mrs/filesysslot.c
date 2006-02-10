
#include <errno.h>

#include "filesysslot.h"


void filesys_slot_free(struct filesys_slot *slot)
{
  assert(slot);
  assert(slot->refcount > 0);
  slot->refcount--;
  if(slot->refcount <= 0) {
    slot->vtable->free(slot);
    free(slot);
  }
}


static void gen_slot_free(struct filesys_slot *obj)
{
  struct filesys_generic_slot *slot = (void *) obj;
  filesys_obj_free(slot->dir);
  free(slot->leaf);
}

static struct filesys_obj *gen_slot_get(struct filesys_slot *obj)
{
  struct filesys_generic_slot *slot = (void *) obj;
  return slot->dir->vtable->traverse(slot->dir, slot->leaf);
}

static int gen_slot_create_file(struct filesys_slot *obj, int flags, int mode, int *err)
{
  struct filesys_generic_slot *slot = (void *) obj;
  return slot->dir->vtable->create_file(slot->dir, slot->leaf, flags, mode, err);
}

static int gen_slot_mkdir(struct filesys_slot *obj, int mode, int *err)
{
  struct filesys_generic_slot *slot = (void *) obj;
  return slot->dir->vtable->mkdir(slot->dir, slot->leaf, mode, err);
}

static int gen_slot_unlink(struct filesys_slot *obj, int *err)
{
  struct filesys_generic_slot *slot = (void *) obj;
  return slot->dir->vtable->unlink(slot->dir, slot->leaf, err);
}

static int gen_slot_rmdir(struct filesys_slot *obj, int *err)
{
  struct filesys_generic_slot *slot = (void *) obj;
  return slot->dir->vtable->rmdir(slot->dir, slot->leaf, err);
}

static struct filesys_slot_vtable gen_slot_vtable = {
  /* .free = */ gen_slot_free,
  /* .get = */ gen_slot_get,
  /* .create_file = */ gen_slot_create_file,
  /* .mkdir = */ gen_slot_mkdir,
  /* .unlink = */ gen_slot_unlink,
  /* .rmdir = */ gen_slot_rmdir,
  1
};

struct filesys_slot *make_generic_slot(struct filesys_obj *dir, char *leaf)
{
  struct filesys_generic_slot *slot =
    amalloc(sizeof(struct filesys_generic_slot));
  slot->hdr.refcount = 1;
  slot->hdr.vtable = &gen_slot_vtable;
  slot->dir = dir;
  slot->leaf = leaf;
  return (void *) slot;
}



static void ro_slot_free(struct filesys_slot *obj)
{
  struct filesys_read_only_slot *slot = (void *) obj;
  filesys_obj_free(slot->obj);
}

static struct filesys_obj *ro_slot_get(struct filesys_slot *obj)
{
  struct filesys_read_only_slot *slot = (void *) obj;
  slot->obj->refcount++;
  return slot->obj;
}

static int ro_slot_create_file(struct filesys_slot *obj, int flags, int mode, int *err)
{
  *err = EACCES;
  return -1;
}

static int ro_slot_mkdir(struct filesys_slot *slot, int mode, int *err)
{
  *err = EACCES;
  return -1;
}

static int ro_slot_unlink(struct filesys_slot *slot, int *err)
{
  *err = EACCES;
  return -1;
}

static int ro_slot_rmdir(struct filesys_slot *slot, int *err)
{
  *err = EACCES;
  return -1;
}

static struct filesys_slot_vtable ro_slot_vtable = {
  /* .free = */ ro_slot_free,
  /* .get = */ ro_slot_get,
  /* .create_file = */ ro_slot_create_file,
  /* .mkdir = */ ro_slot_mkdir,
  /* .unlink = */ ro_slot_unlink,
  /* .rmdir = */ ro_slot_rmdir,
  1
};

struct filesys_slot *make_read_only_slot(struct filesys_obj *obj)
{
  struct filesys_read_only_slot *slot =
    amalloc(sizeof(struct filesys_read_only_slot));
  slot->hdr.refcount = 1;
  slot->hdr.vtable = &ro_slot_vtable;
  slot->obj = obj;
  return (void *) slot;
}
