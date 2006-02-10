
#ifndef server_h
#define server_h


#include "filesysobj.h"


struct dir_stack {
  int refcount;
  struct filesys_obj *dir; /* Only those with OBJT_DIR */
  /* parent may be null if this is the root.
     In that case, name is null too. */
  struct dir_stack *parent;
  char *name;
};

void dir_stack_free(struct dir_stack *st);

struct dir_stack *dir_stack_root(struct filesys_obj *dir);
seqt_t string_of_cwd(region_t r, struct dir_stack *dir);

struct dir_stack *resolve_dir
  (region_t r, struct filesys_obj *root, struct dir_stack *cwd,
   seqf_t filename, int symlink_limit, int *err);

struct filesys_obj *resolve_file
  (region_t r, struct filesys_obj *root, struct dir_stack *cwd,
   seqf_t filename, int symlink_limit, int nofollow, int *err);


struct process {
  int sock_fd;
  struct filesys_obj *root;
  /* cwd may be null: processes may have an undefined current directory. */
  struct dir_stack *cwd;
};
/* This is the error given when trying to access something through the
   cwd when no cwd is defined.
   One choice is EACCES ("Permission denied"). */
#define E_NO_CWD_DEFINED EACCES

struct filesys_obj *initial_dir(const char *pathname);
struct process *process_create(void);
void start_server(struct process *initial_proc);

extern int server_log_messages;
extern FILE *server_log;

#define SYMLINK_LIMIT 100


#endif
