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

static int gen_slot_symlink(struct filesys_slot *obj, const char *oldpath, int *err)
{
  struct filesys_generic_slot *slot = (void *) obj;
  return slot->dir->vtable->symlink(slot->dir, slot->leaf, oldpath, err);
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

static int gen_slot_socket_bind(struct filesys_slot *obj, int sock_fd, int *err)
{
  struct filesys_generic_slot *slot = (void *) obj;
  return slot->dir->vtable->socket_bind(slot->dir, slot->leaf, sock_fd, err);
}

static struct filesys_slot_vtable gen_slot_vtable = {
  /* .free = */ gen_slot_free,
  /* .get = */ gen_slot_get,
  /* .slot_create_file = */ gen_slot_create_file,
  /* .slot_mkdir = */ gen_slot_mkdir,
  /* .slot_symlink = */ gen_slot_symlink,
  /* .slot_unlink = */ gen_slot_unlink,
  /* .slot_rmdir = */ gen_slot_rmdir,
  /* .slot_socket_bind = */ gen_slot_socket_bind,
  1
};

/* Takes an owning reference, and returns an owning reference. */
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

static int ro_slot_symlink(struct filesys_slot *slot, const char *oldpath, int *err)
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

static int ro_slot_socket_bind(struct filesys_slot *slot, int sock_fd, int *err)
{
  *err = EACCES;
  return -1;
}

static struct filesys_slot_vtable ro_slot_vtable = {
  /* .free = */ ro_slot_free,
  /* .get = */ ro_slot_get,
  /* .slot_create_file = */ ro_slot_create_file,
  /* .slot_mkdir = */ ro_slot_mkdir,
  /* .slot_symlink = */ ro_slot_symlink,
  /* .slot_unlink = */ ro_slot_unlink,
  /* .slot_rmdir = */ ro_slot_rmdir,
  /* .slot_socket_bind = */ ro_slot_socket_bind,
  1
};

/* Takes an owning reference, and returns an owning reference. */
struct filesys_slot *make_read_only_slot(struct filesys_obj *obj)
{
  struct filesys_read_only_slot *slot =
    amalloc(sizeof(struct filesys_read_only_slot));
  slot->hdr.refcount = 1;
  slot->hdr.vtable = &ro_slot_vtable;
  slot->obj = obj;
  return (void *) slot;
}
