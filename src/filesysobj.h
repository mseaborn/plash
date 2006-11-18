/* Copyright (C) 2004 Mark Seaborn

   This file is part of Plash.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
   USA.  */

#ifndef filesysobj_h
#define filesysobj_h

#include <sys/stat.h>
#include "region.h"


int set_close_on_exec_flag(int fd, int value);

/* These are kept in a global linked list so that all the FDs can be
   closed when we fork and exec an untrusted process. */
struct file_desc {
  int refcount;
  int fd;
  struct file_desc *prev, *next;
};

struct file_desc *make_fd(int fd);
void free_fd(struct file_desc *desc);
void close_our_fds(void);


struct filesys_obj;
typedef struct filesys_obj *cap_t;
struct cap_seq {
  const cap_t *caps;
  int size;
};
typedef struct cap_seq cap_seq_t;
static inline cap_seq_t cap_seq_make(const cap_t *caps, int size)
{
  cap_seq_t r = { caps, size };
  return r;
}


struct cap_args {
  seqt_t data;
  cap_seq_t caps;
  fds_t fds;
};
static inline struct cap_args cap_args_make
  (seqt_t data, cap_seq_t caps, fds_t fds)
{
  struct cap_args r = { data, caps, fds };
  return r;
}
/* Constructor for args with data portion only. */
static inline struct cap_args cap_args_d(seqt_t data)
{
  struct cap_args r = { data,
			{ NULL, 0 } /* caps_empty */,
			{ NULL, 0 } /* fds_empty */ };
  return r;
}
/* Constructor for args with data portion and caps portion only. */
static inline struct cap_args cap_args_dc(seqt_t data, cap_seq_t caps)
{
  struct cap_args r = { data, caps, { NULL, 0 } /* fds_empty */ };
  return r;
}


struct filesys_obj {
  int refcount;
  const struct filesys_obj_vtable *vtable;
#ifdef GC_DEBUG
  struct filesys_obj *prev, *next;
  int mark_count;
#endif
};
#define OBJT_FILE 1
#define OBJT_DIR 2
#define OBJT_SYMLINK 3
struct filesys_obj_vtable {
  void (*free)(struct filesys_obj *obj);

#ifdef GC_DEBUG
  /* This is for debugging only, for debugging refcount leaks. */
  void (*mark)(struct filesys_obj *obj);
#endif

  void (*cap_invoke)(struct filesys_obj *obj, struct cap_args args);
  void (*cap_call)(struct filesys_obj *obj, region_t r,
		   struct cap_args args, struct cap_args *result);
  int single_use; /* A hint to cap-protocol.c */

  /* Files, directories and symlinks: */
  int (*type)(struct filesys_obj *obj); /* returns an OBJT_* value */
  int (*stat)(struct filesys_obj *obj, struct stat *buf, int *err);
  int (*utimes)(struct filesys_obj *obj, const struct timeval *atime,
		const struct timeval *mtime, int *err);
  /* Files and directories only, not symlinks: */
  int (*chmod)(struct filesys_obj *obj, int mode, int *err);
  
  /* Files only: */
  int (*open)(struct filesys_obj *obj, int flags, int *err);
  int (*socket_connect)(struct filesys_obj *obj, int sock_fd, int *err);
  
  /* Directories only: */
  struct filesys_obj *(*traverse)(struct filesys_obj *obj, const char *leaf);
  /* `list' returns number of entries, or -1 for error */
  int (*list)(struct filesys_obj *obj, region_t r, seqt_t *result, int *err);
  int (*create_file)(struct filesys_obj *obj, const char *leaf,
		     int flags, int mode, int *err);
  int (*mkdir)(struct filesys_obj *obj, const char *leaf, int mode, int *err);
  int (*symlink)(struct filesys_obj *obj, const char *leaf,
		 const char *oldpath, int *err);
  int (*rename)(struct filesys_obj *obj, const char *leaf,
		struct filesys_obj *dest_dir, const char *dest_leaf, int *err);
  int (*link)(struct filesys_obj *obj, const char *leaf,
	      struct filesys_obj *dest_dir, const char *dest_leaf, int *err);
  int (*unlink)(struct filesys_obj *obj, const char *leaf, int *err);
  int (*rmdir)(struct filesys_obj *obj, const char *leaf, int *err);
  int (*socket_bind)(struct filesys_obj *obj, const char *leaf,
		     int sock_fd, int *err);
  
  /* Symlinks only: */
  int (*readlink)(struct filesys_obj *obj, region_t r, seqf_t *result, int *err);

  /* Slots only: */
  struct filesys_obj *(*slot_get)(struct filesys_obj *obj); /* Returns 0 if slot is currently empty */
  int (*slot_create_file)(struct filesys_obj *obj, int flags, int mode,
			  int *err);
  int (*slot_mkdir)(struct filesys_obj *slot, int mode, int *err);
  int (*slot_symlink)(struct filesys_obj *slot, const char *oldpath, int *err);
  int (*slot_unlink)(struct filesys_obj *slot, int *err);
  int (*slot_rmdir)(struct filesys_obj *slot, int *err);
  int (*slot_socket_bind)(struct filesys_obj *slot, int sock_fd, int *err);

  /* Returns FD for connection, or -1 for error. */
  /* `export' contains non-owning references. */
  int (*make_conn)(struct filesys_obj *obj, region_t r, cap_seq_t export,
		   int import_count, cap_t **import);
  /* Returns 0 for success, -1 for failure.
     sock_fd is an owning reference; the method takes ownership regardless
     of the return code. */
  int (*make_conn2)(struct filesys_obj *obj, region_t r,
		    int sock_fd, cap_seq_t export,
		    int import_count, cap_t **import);

  /* Output a message to the log stream. */
  void (*log_msg)(struct filesys_obj *obj, seqf_t msg);
  /* Fork the log stream.  Returns a new stream, or NULL. */
  struct filesys_obj *(*log_branch)(struct filesys_obj *obj, seqf_t msg);

  /* This is for debugging. */
  char *vtable_name;

  /* This is just here so that when I add fields, the compiler will give
     a warning when I forget to update all the vtables.  Unfortunately it
     doesn't give a warning when fields are missing. */
  int sentinel;
};

void filesys_obj_free(struct filesys_obj *obj);

/* Checks that reference is valid */
void filesys_obj_check(struct filesys_obj *obj);

#if !defined GC_DEBUG
static inline void *filesys_obj_make(int size, const struct filesys_obj_vtable *vtable)
{
  struct filesys_obj *obj = amalloc(size);
  obj->refcount = 1;
  obj->vtable = vtable;
  return obj;
}
#else
void *filesys_obj_make(int size, const struct filesys_obj_vtable *vtable);
#endif

#ifdef GC_DEBUG
void filesys_obj_mark(struct filesys_obj *obj);
void gc_init(void);
void gc_check(void);
#endif


void generic_free(struct filesys_obj *obj);

void marshal_cap_call(struct filesys_obj *obj, region_t r,
		      struct cap_args args, struct cap_args *result);
int marshal_type(struct filesys_obj *obj);
int marshal_stat(struct filesys_obj *obj, struct stat *buf, int *err);

int objt_unknown(struct filesys_obj *obj);
int objt_file(struct filesys_obj *obj);
int objt_dir(struct filesys_obj *obj);
int objt_symlink(struct filesys_obj *obj);

void dummy_cap_invoke(struct filesys_obj *obj, struct cap_args args);
int dummy_stat(struct filesys_obj *obj, struct stat *buf, int *err);
int dummy_utimes(struct filesys_obj *obj, const struct timeval *atime,
		 const struct timeval *mtime, int *err);
int dummy_chmod(struct filesys_obj *obj, int mode, int *err);
struct filesys_obj *dummy_traverse(struct filesys_obj *obj, const char *leaf);
int dummy_list(struct filesys_obj *obj, region_t r, seqt_t *result, int *err);
int dummy_create_file(struct filesys_obj *obj, const char *leaf,
		      int flags, int mode, int *err);
int dummy_mkdir(struct filesys_obj *obj, const char *leaf, int mode, int *err);
int dummy_symlink(struct filesys_obj *obj, const char *leaf,
		  const char *oldpath, int *err);
int dummy_rename_or_link(struct filesys_obj *obj, const char *leaf,
			 struct filesys_obj *dest_dir, const char *dest_leaf,
			 int *err);
int dummy_unlink(struct filesys_obj *obj, const char *leaf, int *err);
int dummy_rmdir(struct filesys_obj *obj, const char *leaf, int *err);
int dummy_socket_bind(struct filesys_obj *obj, const char *leaf, int sock_fd, int *err);
int dummy_readlink(struct filesys_obj *obj, region_t r, seqf_t *result, int *err);
int dummy_open(struct filesys_obj *obj, int flags, int *err);
int dummy_socket_connect(struct filesys_obj *obj, int sock_fd, int *err);
struct filesys_obj *dummy_slot_get(struct filesys_obj *obj);
int dummy_slot_create_file(struct filesys_obj *obj, int flags, int mode,
			   int *err);
int dummy_slot_mkdir(struct filesys_obj *slot, int mode, int *err);
int dummy_slot_symlink(struct filesys_obj *slot, const char *oldpath, int *err);
int dummy_slot_unlink(struct filesys_obj *slot, int *err);
int dummy_slot_rmdir(struct filesys_obj *slot, int *err);
int dummy_slot_socket_bind(struct filesys_obj *slot, int sock_fd, int *err);
int dummy_make_conn(struct filesys_obj *obj, region_t r, cap_seq_t export,
		    int import_count, cap_t **import);
int dummy_make_conn2(struct filesys_obj *obj, region_t r, int sock_fd,
		     cap_seq_t export, int import_count, cap_t **import);
void dummy_log_msg(struct filesys_obj *obj, seqf_t msg);
struct filesys_obj *dummy_log_branch(struct filesys_obj *obj, seqf_t msg);

int marshal_stat(struct filesys_obj *obj, struct stat *buf, int *err);
int marshal_utimes(struct filesys_obj *obj, const struct timeval *atime,
		 const struct timeval *mtime, int *err);
int marshal_chmod(struct filesys_obj *obj, int mode, int *err);
int marshal_open(struct filesys_obj *obj, int flags, int *err);
int marshal_socket_connect(struct filesys_obj *obj, int sock_fd, int *err);
struct filesys_obj *marshal_traverse(struct filesys_obj *obj, const char *leaf);
int marshal_list(struct filesys_obj *obj, region_t r, seqt_t *result, int *err);
int marshal_create_file(struct filesys_obj *obj, const char *leaf,
			int flags, int mode, int *err);
int marshal_mkdir(struct filesys_obj *obj, const char *leaf, int mode, int *err);
int marshal_symlink(struct filesys_obj *obj, const char *leaf,
		    const char *oldpath, int *err);
int marshal_rename(struct filesys_obj *obj, const char *leaf,
		   struct filesys_obj *dest_dir, const char *dest_leaf,
		   int *err);
int marshal_link(struct filesys_obj *obj, const char *leaf,
		 struct filesys_obj *dest_dir, const char *dest_leaf,
		 int *err);
int marshal_unlink(struct filesys_obj *obj, const char *leaf, int *err);
int marshal_rmdir(struct filesys_obj *obj, const char *leaf, int *err);
int marshal_socket_bind(struct filesys_obj *obj, const char *leaf, int sock_fd, int *err);
int marshal_readlink(struct filesys_obj *obj, region_t r, seqf_t *result, int *err);
int marshal_make_conn(struct filesys_obj *obj, region_t r, cap_seq_t export,
		      int import_count, cap_t **import);

int refuse_chmod(struct filesys_obj *obj, int mode, int *err);
int refuse_utimes(struct filesys_obj *obj, const struct timeval *atime,
		  const struct timeval *mtime, int *err);
int refuse_socket_connect(struct filesys_obj *obj, int sock_fd, int *err);
int refuse_create_file(struct filesys_obj *obj, const char *leaf,
		       int flags, int mode, int *err);
int refuse_mkdir(struct filesys_obj *obj, const char *leaf,
		 int mode, int *err);
int refuse_symlink(struct filesys_obj *obj, const char *leaf,
		   const char *oldpath, int *err);
int refuse_rename_or_link
  (struct filesys_obj *obj, const char *leaf,
   struct filesys_obj *dest_dir, const char *dest_leaf, int *err);
int refuse_unlink(struct filesys_obj *obj, const char *leaf, int *err);
int refuse_rmdir(struct filesys_obj *obj, const char *leaf, int *err);
int refuse_socket_bind(struct filesys_obj *obj, const char *leaf,
		       int sock_fd, int *err);


/* Capability sequences */

extern struct cap_seq caps_empty;

static inline cap_seq_t mk_caps1(region_t r, cap_t cap0)
{
  cap_seq_t caps;
  cap_t *caps_array = region_alloc(r, sizeof(cap_t));
  caps_array[0] = cap0;
  caps.caps = caps_array;
  caps.size = 1;
  return caps;
}

static inline cap_seq_t mk_caps2(region_t r, cap_t cap0, cap_t cap1)
{
  cap_seq_t caps;
  cap_t *caps_array = region_alloc(r, 2 * sizeof(cap_t));
  caps_array[0] = cap0;
  caps_array[1] = cap1;
  caps.caps = caps_array;
  caps.size = 2;
  return caps;
}

void caps_free(cap_seq_t c);
cap_seq_t cap_seq_append(region_t r, cap_seq_t seq1, cap_seq_t seq2);

static inline cap_seq_t cap_seq_dup(region_t r, cap_seq_t s)
{
  cap_seq_t result;
  cap_t *a = region_alloc(r, s.size * sizeof(cap_t));
  memcpy(a, s.caps, s.size * sizeof(cap_t));
  result.caps = a;
  result.size = s.size;
  return result;
}

void pl_args_free(const struct cap_args *args);


static inline cap_t inc_ref(cap_t x)
{
  x->refcount++;
  return x;
}


#define DECLARE_VTABLE(name) \
extern const struct filesys_obj_vtable name


#endif
