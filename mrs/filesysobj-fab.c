
#include <errno.h>

#include "region.h"
#include "filesysobj-fab.h"


struct list *assoc(struct list *list, const char *name)
{
  while(list) {
    if(!strcmp(name, list->name)) return list;
    list = list->next;
  }
  return 0; /* Not found */
}


int refuse_chmod(struct filesys_obj *obj, int mode, int *err)
{
  *err = EACCES;
  return -1;
}

void fab_dir_free(struct filesys_obj *obj1)
{
  struct fab_dir *obj = (void *) obj1;
  /* FIXME: not implemented */
}

/* Device number reported by stat.  FIXME: avoid clashes? */
#define STAT_DEVICE 100

int fab_dir_stat(struct filesys_obj *obj, struct stat *st)
{
  struct fab_dir *dir = (void *) obj;
  st->st_dev = STAT_DEVICE;
  st->st_ino = dir->inode;
  st->st_mode = S_IFDIR | 0777;
  st->st_nlink = 0; /* FIXME: this can be used to count the number of child directories */
  st->st_uid = 0;
  st->st_gid = 0;
  st->st_rdev = 0;
  st->st_size = 0;
  st->st_blksize = 1024;
  st->st_blocks = 0;
  st->st_atime = 0;
  st->st_mtime = 0;
  st->st_ctime = 0;
  return 0;
}

struct filesys_obj *fab_dir_traverse(struct filesys_obj *obj, const char *leaf)
{
  struct fab_dir *dir = (void *) obj;
  struct obj_list *l = (void *) assoc((void *) dir->entries, leaf);
  if(l) {
    l->obj->refcount++;
    return l->obj;
  }
  else return 0;
}

int fab_dir_list(struct filesys_obj *obj, region_t r, seqt_t *result, int *err)
{
  struct fab_dir *dir = (void *) obj;
  struct obj_list *l;
  seqt_t got = seqt_empty;
  for(l = dir->entries; l; l = l->next) {
    /* FIXME: wasteful of space */
    int len = strlen(l->name);
    char *str = region_alloc(r, len);
    memcpy(str, l->name, len);
    got = cat5(r, got,
	       mk_int(r, 0), /* inode: FIXME */
	       mk_int(r, 0), /* d_type: FIXME */
	       mk_int(r, len),
	       mk_leaf2(r, str, len));
  }
  *result = got;
  return 0;
}

int fab_dir_create_file(struct filesys_obj *obj, const char *leaf,
			int flags, int mode, int *err)
{
  *err = EACCES;
  return -1;
}
int fab_dir_mkdir(struct filesys_obj *obj, const char *leaf,
		  int mode, int *err)
{
  *err = EACCES;
  return -1;
}
int fab_dir_unlink(struct filesys_obj *obj, const char *leaf, int *err)
{
  *err = EACCES;
  return -1;
}
int fab_dir_rmdir(struct filesys_obj *obj, const char *leaf, int *err)
{
  *err = EACCES;
  return -1;
}

struct filesys_obj_vtable fab_dir_vtable = {
  /* .type = */ OBJT_DIR,
  /* .free = */ fab_dir_free,
  /* .stat = */ fab_dir_stat,
  /* .chmod = */ refuse_chmod,
  /* .open = */ dummy_open,
  /* .connect = */ dummy_connect,
  /* .traverse = */ fab_dir_traverse,
  /* .list = */ fab_dir_list,
  /* .create_file = */ fab_dir_create_file,
  /* .mkdir = */ fab_dir_mkdir,
  /* .unlink = */ fab_dir_unlink,
  /* .rmdir = */ fab_dir_rmdir,
  /* .readlink = */ dummy_readlink,
  1
};


void fab_symlink_free(struct filesys_obj *obj1)
{
  struct fab_symlink *obj = (void *) obj1;
  free((char *) obj->dest.data);
}

int fab_symlink_stat(struct filesys_obj *obj, struct stat *st)
{
  struct fab_symlink *sym = (void *) obj;
  st->st_dev = STAT_DEVICE;
  st->st_ino = sym->inode;
  st->st_mode = S_IFLNK | 0777;
  st->st_nlink = 1; /* Not necessarily accurate */
  st->st_uid = 0;
  st->st_gid = 0;
  st->st_rdev = 0;
  st->st_size = sym->dest.size;
  st->st_blksize = 1024;
  st->st_blocks = 0;
  st->st_atime = 0;
  st->st_mtime = 0;
  st->st_ctime = 0;
  return 0;
}

int fab_symlink_readlink(struct filesys_obj *obj, region_t r, seqf_t *result, int *err)
{
  struct fab_symlink *sym = (void *) obj;
  *result = region_dup_seqf(r, sym->dest);
  return 0;
}

struct filesys_obj_vtable fab_symlink_vtable = {
  /* .type = */ OBJT_SYMLINK,
  /* .free = */ fab_symlink_free,
  /* .stat = */ fab_symlink_stat,
  /* .chmod = */ dummy_chmod,
  /* .open = */ dummy_open,
  /* .connect = */ dummy_connect,
  /* .traverse = */ dummy_traverse,
  /* .list = */ dummy_list,
  /* .create_file = */ dummy_create_file,
  /* .mkdir = */ dummy_mkdir,
  /* .unlink = */ dummy_unlink,
  /* .rmdir = */ dummy_rmdir,
  /* .readlink = */ fab_symlink_readlink,
  1
};



void s_fab_dir_free(struct filesys_obj *obj1)
{
  struct s_fab_dir *obj = (void *) obj1;
  /* FIXME: not implemented */
}

int s_fab_dir_stat(struct filesys_obj *obj, struct stat *st)
{
  struct s_fab_dir *dir = (void *) obj;
  st->st_dev = STAT_DEVICE;
  st->st_ino = dir->inode;
  st->st_mode = S_IFDIR | 0777;
  st->st_nlink = 0; /* FIXME: this can be used to count the number of child directories */
  st->st_uid = 0;
  st->st_gid = 0;
  st->st_rdev = 0;
  st->st_size = 0;
  st->st_blksize = 1024;
  st->st_blocks = 0;
  st->st_atime = 0;
  st->st_mtime = 0;
  st->st_ctime = 0;
  return 0;
}

struct filesys_obj *s_fab_dir_traverse(struct filesys_obj *obj, const char *leaf)
{
  struct s_fab_dir *dir = (void *) obj;
  struct slot_list *l = (void *) assoc((void *) dir->entries, leaf);
  if(l) return l->slot->vtable->get(l->slot);
  else return 0;
}

int s_fab_dir_list(struct filesys_obj *obj, region_t r, seqt_t *result, int *err)
{
  struct s_fab_dir *dir = (void *) obj;
  struct slot_list *l;
  seqt_t got = seqt_empty;
  for(l = dir->entries; l; l = l->next) {
    struct filesys_obj *obj = l->slot->vtable->get(l->slot);
    if(obj) {
      /* FIXME: wasteful of space */
      int len = strlen(l->name);
      char *str = region_alloc(r, len);
      memcpy(str, l->name, len);
      got = cat5(r, got,
		 mk_int(r, 0), /* inode: FIXME */
		 mk_int(r, 0), /* d_type: FIXME */
		 mk_int(r, len),
		 mk_leaf2(r, str, len));
      filesys_obj_free(obj);
    }
  }
  *result = got;
  return 0;
}

int s_fab_dir_create_file(struct filesys_obj *obj, const char *leaf,
			  int flags, int mode, int *err)
{
  struct s_fab_dir *dir = (void *) obj;
  struct slot_list *l = (void *) assoc((void *) dir->entries, leaf);
  if(l) {
    return l->slot->vtable->create_file(l->slot, flags, mode, err);
  }
  else {
    *err = EACCES;
    return -1;
  }
}

int s_fab_dir_mkdir(struct filesys_obj *obj, const char *leaf,
		    int mode, int *err)
{
  struct s_fab_dir *dir = (void *) obj;
  struct slot_list *l = (void *) assoc((void *) dir->entries, leaf);
  if(l) {
    return l->slot->vtable->mkdir(l->slot, mode, err);
  }
  else {
    *err = EACCES;
    return -1;
  }
}

int s_fab_dir_unlink(struct filesys_obj *obj, const char *leaf, int *err)
{
  struct s_fab_dir *dir = (void *) obj;
  struct slot_list *l = (void *) assoc((void *) dir->entries, leaf);
  if(l) {
    return l->slot->vtable->unlink(l->slot, err);
  }
  else {
    *err = EACCES;
    return -1;
  }
}

int s_fab_dir_rmdir(struct filesys_obj *obj, const char *leaf, int *err)
{
  struct s_fab_dir *dir = (void *) obj;
  struct slot_list *l = (void *) assoc((void *) dir->entries, leaf);
  if(l) {
    return l->slot->vtable->rmdir(l->slot, err);
  }
  else {
    *err = EACCES;
    return -1;
  }
}

struct filesys_obj_vtable s_fab_dir_vtable = {
  /* .type = */ OBJT_DIR,
  /* .free = */ s_fab_dir_free,
  /* .stat = */ s_fab_dir_stat,
  /* .chmod = */ refuse_chmod,
  /* .open = */ dummy_open,
  /* .connect = */ dummy_connect,
  /* .traverse = */ s_fab_dir_traverse,
  /* .list = */ s_fab_dir_list,
  /* .create_file = */ s_fab_dir_create_file,
  /* .mkdir = */ s_fab_dir_mkdir,
  /* .unlink = */ s_fab_dir_unlink,
  /* .rmdir = */ s_fab_dir_rmdir,
  /* .readlink = */ dummy_readlink,
  1
};
