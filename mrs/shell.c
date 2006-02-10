
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <pwd.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "region.h"
#include "server.h"
#include "config.h"
#include "shell-variants.h"
#include "shell.h"
#include "build-fs.h"

int f_command(region_t r, const char *pos_in1, const char *end,
	      const char **pos_out_p, void **ok_val_p);


#define USE_CHROOT 1
#define MOD_DEBUG 0

/* Includes null terminator in result, assuming input is null-terminated too. */
seqf_t tilde_expansion(region_t r, seqf_t pathname)
{
  if(pathname.size > 0 && pathname.data[0] == '~') {
    struct passwd *pwd;
    int i = 1;
    while(pathname.size > i && pathname.data[i] != '/') i++;
    if(i == 1) {
      /* Pathname of the form "~" or "~/..." */
      uid_t uid = getuid(); /* always succeeds */
      pwd = getpwuid(uid);
    }
    else {
      /* Pathname of the form "~USER" or "~USER/..." */
      seqf_t user_name = { pathname.data + 1, i - 1 };
      char *user_name1 = strdup_seqf(user_name);
      pwd = getpwnam(user_name1);
      free(user_name1);
    }
    
    if(pwd && pwd->pw_dir) {
      seqf_t rest = { pathname.data + i, pathname.size - i };
      return flatten0(r, cat2(r, mk_string(r, pwd->pw_dir),
			      mk_leaf(r, rest)));
    }
  }
  return pathname;
}

/* Gives a seqf_t as a result and *also* nulls-terminates the string. */
seqf_t flatten_charlist(region_t r, struct char_cons *list)
{
  int count;
  struct char_cons *l;
  char *buf;
  int i;
  seqf_t result;
  
  for(l = list, count = 0; l; l = l->next) count++;
  buf = region_alloc(r, count+1);
  for(l = list, i = 0; l; l = l->next, i++) buf[i] = l->c;
  buf[i] = 0;
  result.data = buf;
  result.size = count;
  return result;
}

struct str_list {
  struct str_list *prev;
  char *str;
};
struct flatten_params {
  struct str_list *got;
  struct filesys_obj *root_dir;
  struct node *tree;
  struct dir_stack *cwd;
};
void flatten_args(region_t r, struct flatten_params *p,
		  int rw, int ambient, struct arg_list *args)
{
  if(m_arg_empty(args)) return;
  {
    struct arg_list *a1, *a2;
    if(m_arg_cat(args, &a1, &a2)) {
      flatten_args(r, p, rw, ambient, a1);
      flatten_args(r, p, rw, ambient, a2);
      return;
    }
  }
  {
    struct arg_list *a;
    if(m_arg_read(args, &a)) {
      flatten_args(r, p, 0, ambient, a);
      return;
    }
  }
  {
    struct arg_list *a;
    if(m_arg_write(args, &a)) {
      flatten_args(r, p, 1, ambient, a);
      return;
    }
  }
  {
    struct arg_list *a;
    if(m_arg_ambient(args, &a)) {
      flatten_args(r, p, rw, 1, a);
      return;
    }
  }
  {
    struct char_cons *f;
    if(m_arg_filename(args, &f)) {
      seqf_t filename = tilde_expansion(r, flatten_charlist(r, f));
      int err;
      
      if(resolve_populate(p->root_dir, p->tree, p->cwd,
			  filename, rw /* create */, &err) < 0) {
	printf("plash: error in resolving filename `");
	fprint_d(stdout, filename);
	printf("'\n");
      }
      if(!ambient) {
	struct str_list *l = region_alloc(r, sizeof(struct str_list));
	l->str = (char*) filename.data;
	l->prev = p->got;
	p->got = l;
      }
      
      /*printf("filename: ");
      fprint_d(stdout, filename);
      printf("\n");*/
      return;
    }
  }
  {
    struct char_cons *s;
    if(m_arg_string(args, &s)) {
      seqf_t string = flatten_charlist(r, s);
      if(!ambient) {
	struct str_list *l = region_alloc(r, sizeof(struct str_list));
	l->str = (char*) string.data;
	l->prev = p->got;
	p->got = l;
      }
      else {
	printf("warning: string argument \"%s\" in ambient arg list (ignored)\n", string.data);
      }
      
      /*printf("string: ");
      fprint_d(stdout, string);
      printf("\n");*/
      return;
    }
  }
  printf("unknown!\n"); assert(0);
}

extern char **environ;

void print_wait_status(int status)
{
  if(WIFEXITED(status)) {
    printf("normal: %i", WEXITSTATUS(status));
  }
  else if(WIFSIGNALED(status)) {
    printf("uncaught signal: %i", WTERMSIG(status));
  }
  else printf("unknown");
}

#if 0
#define INTERPRETER_LIMIT 1

int do_exec(struct filesys_obj *root, const char *cmd, int argc, char **argv,
	    int interpreter_limit, int *err)
{
  region_t r = region_make();
  filesys_obj *obj =
    resolve_file(r, root, 0 /* cwd */, seqf_string(cmd), SYMLINK_LIMIT,
		 0 /* nofollow */, err);
  region_free(r);
  if(obj) {
    int fd = obj->vtable->open(obj, O_RDONLY, 0, &err);
    if(fd >= 0) {
      char buf[1024];
      int got = 0;
      filesys_obj_free(obj);
      
      while(got < sizeof(buf)) {
	int i;
	int x = read(fd, buf + got, sizeof(buf) - got);
	if(x < 0) { *err = errno; return -1; }
	if(x == 0) break;
	got += x;
      }

      /* No whitespace is allowed before the "#!" */
      if(got >= 2 && buf[0] == '#' && buf[1] == '!') {
	int icmd_start, icmd_end;
	int i = 2;

	if(interpreter_limit <= 0) { *err = ELOOP; return -1; }
	
	while(i < got && buf[i] == ' ') i++; /* Skip spaces */
	if(i >= got) { *err = EINVAL; return -1; }
	icmd_start = i;
	while(i < got && buf[i] != ' ' && buf[i] != '\n') i++;
	if(i >= got) { *err = EINVAL; return -1; }
	icmd_end = i;
	while(i < got && buf[i] == ' ') i++; /* Skip spaces */
	if(i >= got) { *err = EINVAL; return -1; }
	arg_start = i;
	while(i < got && buf[i] != ' ' && buf[i] != '\n') i++;
	if(i >= got) { *err = EINVAL; return -1; }
	arg_end = i;

	buf[icmd_end] = 0;
	buf[arg_end] = 0;
	
	if(arg_end > arg_start) {
	  int i;
	  const char *cmd2;
	  const char *argv2 = alloca((argc + 2) * sizeof(char *));
	  cmd2 = buf + icmd_start;
	  argv2[0] = buf + icmd_start;
	  argv2[1] = cmd;
	  argv2[2] = buf + arg_start;
	  for(i = 1; i < argc; i++) argv2[i+2] = argv[i];
	  return do_exec(root, cmd2, argc + 2, argv2, interpreter_limit-1, err);
	}
	else {
	  int i;
	  const char *cmd2;
	  const char *argv2 = alloca((argc + 1) * sizeof(char *));
	  cmd2 = buf + icmd_start;
	  argv2[0] = buf + icmd_start;
	  argv2[1] = cmd;
	  for(i = 1; i < argc; i++) argv2[i+1] = argv[i];
	  return do_exec(root, cmd2, argc + 1, argv2, interpreter_limit-1, err);
	}
      }
      else {
	/* Assume an ELF executable */
      }
    }
  }
}
#endif

void exec_elf_program_from_filename(const char *cmd, int argc, const char **argv)
{
  int extra_args = 4;
  const char *cmd2;
  const char **argv2;
  int i;

  /* Check whether we can "exec" the file first. */
  /* This simply gives a more intelligible error than when ld-linux
     finds it can't access the file. */
  if(access(cmd, X_OK) < 0) { perror("access/exec"); exit(1); }
      
  argv2 = alloca((argc + extra_args + 1) * sizeof(char *));
  argv2[0] = argv[0];
  cmd2 = BIN_INSTALL "/run-as-nobody";
  argv2[1] = BIN_INSTALL "/ld-linux.so.2";
  argv2[2] = "--library-path";
  argv2[3] = LIB_INSTALL ":/lib:/usr/lib:/usr/X11R6/lib";
  argv2[4] = cmd;
  for(i = 1; i < argc; i++) argv2[extra_args+i] = argv[i];
  argv2[extra_args + argc] = 0;

  execve(cmd2, (char **) argv2, environ);
}

void exec_elf_program_from_fd(int fd, const char *cmd, int argc, const char **argv)
{
  int extra_args = 6;
  char buf[20];
  const char *cmd2;
  const char **argv2;
  int i;

  argv2 = alloca((argc + extra_args + 1) * sizeof(char *));
  argv2[0] = argv[0];
#ifdef USE_CHROOT
  cmd2 = BIN_INSTALL "/run-as-nobody+chroot";
  argv2[1] = "/special/ld-linux.so.2";
#else
  cmd2 = BIN_INSTALL "/run-as-nobody";
  argv2[1] = BIN_INSTALL "/ld-linux.so.2";
#endif
  argv2[2] = "--library-path";
  argv2[3] = LIB_INSTALL ":/lib:/usr/lib:/usr/X11R6/lib";
  argv2[4] = "--fd";
  snprintf(buf, sizeof(buf), "%i", fd);
  argv2[5] = buf;
  for(i = 0; i < argc; i++) argv2[extra_args+i] = argv[i];
  argv2[extra_args + argc] = 0;

  execve(cmd2, (char **) argv2, environ);
}

/* cwd is a dir_stack in the user's namespace.  It is translated into
   one for the process's namespace.
   root is the process's namespace.
   exec_fd is an FD for the executable, whose file doesn't have to be
   included in the process's namespace.  It is closed (in the parent
   process) before the function returns. */
int spawn(struct filesys_obj *root, struct dir_stack *cwd,
	  int exec_fd, const char *cmd, int argc, const char **argv)
{
  int attach_gdb_to_server = 0;
  int attach_gdb_to_client = 0;
  int attach_strace_to_client = 0;
  int strace_client = 0;
  int pid_client, pid_server;
  int socks[2];
  
  if(socketpair(AF_LOCAL, SOCK_STREAM, 0, socks) < 0) {
    perror("socketpair");
    return -1;
  }

  pid_client = fork();
  if(pid_client == 0) {
    char buf[10];
    close(socks[0]);
    // set_cloexec_flag(socks[1], 0);
    snprintf(buf, sizeof(buf), "%i", socks[1]);
    setenv("COMM_FD", buf, 1);
    
    /* Necessary for security when using run-as-nobody, otherwise the
       process can be left in a directory with read/write access which
       might not be reachable from the root directory. */
    /* Not necessary for run-as-nobody+chroot. */
    if(chdir("/") < 0) { perror("chdir"); exit(1); }

    /* Close FDs for directories, etc.  Very important!  We don't use
       the close-on-exec flag for this because we need to keep those
       FDs open in the forked server process. */
    close_our_fds();

    if(attach_strace_to_client || attach_gdb_to_client) {
      if(kill(getpid(), SIGSTOP) < 0) { perror("kill/SIGSTOP"); }
    }

    exec_elf_program_from_fd(exec_fd, cmd, argc, argv);
    fprintf(stderr, "shell: ");
    perror("exec");
    exit(1);
  }
  if(pid_client < 0) { perror("fork"); return -1; }

  close(exec_fd);
  close(socks[1]);

  pid_server = fork();
  if(pid_server == 0) {
    struct process *p = amalloc(sizeof(struct process));
    p->sock_fd = socks[0];
    p->root = root;
    /* Set the process's cwd. */
    {
      region_t r = region_make();
      seqf_t cwd_path = flatten(r, string_of_cwd(r, cwd));
      int err;
      /* Failing is fine.  It just means that none of the arguments
	 to the process were relative to the cwd, so the cwd wasn't
	 included in the filesystem. */
      /* We could set the cwd to the root directory.  But that means
	 that if the user enters "ls", "find", "pwd", etc., rather
	 than "ls .", "find ." or "pwd + .", these programs will
	 operate on the root directory without warning.
	 Instead, processes are allowed to have an undefined
	 current directory, and will get an error if they try to
	 access anything through it. */
      p->cwd = resolve_dir(r, root, 0 /* cwd */, cwd_path, SYMLINK_LIMIT, &err);
      region_free(r);
    }

    if(attach_gdb_to_server) {
      if(kill(getpid(), SIGSTOP) < 0) { perror("kill/SIGSTOP"); }
    }
    
    server_log = fopen("/dev/null", "w");
    if(!server_log) server_log = fdopen(1, "w");
    start_server(p);
    exit(0);
  }
  if(pid_server < 0) { perror("fork"); return -1; }

  close(socks[0]);

  if(attach_gdb_to_server) {
    int pid_gdb = fork();
    if(pid_gdb == 0) {
      char buf[10];
      snprintf(buf, sizeof(buf), "%i", pid_server);
      execl("/usr/bin/gdb", "/usr/bin/gdb", "./mrs/shell", buf, 0);
      perror("exec");
      exit(1);
    }
    if(pid_gdb < 0) perror("fork");
  }
  if(attach_gdb_to_client) {
    int pid_gdb = fork();
    if(pid_gdb == 0) {
      char buf[10];
      snprintf(buf, sizeof(buf), "%i", pid_client);
      execl("/usr/bin/gdb", "/usr/bin/gdb", cmd, buf, 0);
      perror("exec");
      exit(1);
    }
    if(pid_gdb < 0) perror("fork");
  }

  /* Doesn't work because run-as-nobody is a setuid program.
     Instead, we could strace the process after run-as-nobody exec's
     the program.  We would have to run strace as nobody to do that. */
  if(attach_strace_to_client) {
    int pid_strace = fork();
    if(pid_strace == 0) {
      char buf[10];
      snprintf(buf, sizeof(buf), "%i", pid_client);
      execl("/usr/bin/strace", "/usr/bin/strace", "-p", buf, 0);
      perror("exec");
      exit(1);
    }
    if(pid_strace < 0) perror("fork");
  }

  if(MOD_DEBUG) printf("client=%i, server=%i\n", pid_client, pid_server);
  {
    int wait_for = 2;
    int pid;
    int status;
    int i;

    for(i = 0; i < wait_for; i++) {
      pid = wait(&status);
      if(WIFSIGNALED(status) && pid == pid_client) {
	printf("process died with signal %i\n", WTERMSIG(status));
      }
      if(WIFSIGNALED(status) && pid == pid_server) {
	printf("server died with signal %i\n", WTERMSIG(status));
      }
      if(MOD_DEBUG) {
	printf("pid %i finished: ", pid);
	print_wait_status(status);
	printf("\n");
      }
    }
  }
  return 0;
}

/* Parses PATH.  If it contains any elements, returns 1 and sets
   pathname to the first element, and rest to the rest. */
int parse_path(seqf_t path, seqf_t *pathname, seqf_t *rest)
{
  if(path.size > 0) {
    int i = 0;
    while(i < path.size) {
      if(path.data[i] == ':') {
	pathname->data = path.data;
	pathname->size = i;
	rest->data = path.data + i + 1;
	rest->size = path.size - i - 1;
	return 1;
      }
      i++;
    }
    pathname->data = path.data;
    pathname->size = i;
    rest->data = 0;
    rest->size = 0;
    return 1;
  }
  else return 0;
}

#if 0
struct filesys_obj *shell_test()
{
  struct filesys_obj *root_dir = initial_dir("/");
  struct dir_stack *cwd = dir_stack_root(root_dir);
  struct node *tree = make_empty_node();

  int err;
  resolve_populate(root_dir, tree, cwd, seqf_string("/home/mrs/Mail"), 0 /* create */, &err);
  resolve_populate(root_dir, tree, cwd, seqf_string("/etc"), 0 /* create */, &err);
  resolve_populate(root_dir, tree, cwd, seqf_string("/bin"), 0 /* create */, &err);
  resolve_populate(root_dir, tree, cwd, seqf_string("/usr"), 0 /* create */, &err);

  /*{
    region_t r = region_make();
    seqf_t str = seqf_string("ls foo bar 'jim'");
    const char *pos_out;
    void *val_out;
    printf("allocated %i bytes\n", region_allocated(r));
    if(f_command(r, str.data, str.data + str.size, &pos_out, &val_out) &&
       pos_out == str.data + str.size) {
      struct char_cons *f;
      struct arg_list *args;
      printf("parsed\n");
      if(m_command(val_out, &f, &args)) {
	flatten_args(0, args);
      }
    }
    else {
      printf("parse failed\n");
    }
    printf("allocated %i bytes\n", region_allocated(r));
    region_free(r);
    }*/

  print_tree(0, tree);
  return build_fs(tree);
}
#endif


struct shell_state {
  struct filesys_obj *root;
  struct dir_stack *cwd;
};

void shell_command(struct shell_state *state, seqf_t line)
{
  region_t r = region_make();
  const char *pos_out;
  void *val_out;
  if(f_command(r, line.data, line.data + line.size, &pos_out, &val_out) &&
     pos_out == line.data + line.size) {
    struct char_cons *cmd_filename1;
    struct char_cons *dir_filename1;
    struct arg_list *args;
    
    if(MOD_DEBUG) printf("parsed\n");
    if(m_chdir(val_out, &dir_filename1)) {
      seqf_t dir_filename =
	tilde_expansion(r, flatten_charlist(r, dir_filename1));
      int err;
      struct dir_stack *d =
	resolve_dir(r, state->root, state->cwd, dir_filename,
		    SYMLINK_LIMIT, &err);
      if(d) {
	dir_stack_free(state->cwd);
	state->cwd = d;
      }
      else {
	errno = err;
	perror("cd");
      }
    }
    else if(m_command(val_out, &cmd_filename1, &args)) {
      struct flatten_params p;
      struct str_list *l;
      int count;
      const char **argv;
      int i;
      struct filesys_slot *root_slot;
      struct filesys_obj *root;
      int exec_fd;
      int err;

      seqf_t cmd_filename = tilde_expansion(r, flatten_charlist(r, cmd_filename1));

      if(!strcmp(cmd_filename.data, "server-log-enable")) {
	server_log_messages = 1;
      }
      else if(!strcmp(cmd_filename.data, "server-log-disable")) {
	server_log_messages = 0;
      }
      else {

      /* Search PATH for the command */
      if(!strchr(cmd_filename.data, '/')) {
	const char *path1 = getenv("PATH");
	if(path1) {
	  seqf_t path = seqf_string(path1);
	  while(1) {
	    seqf_t dir;
	    if(parse_path(path, &dir, &path)) {
	      seqf_t full_cmd =
		flatten0(r, cat3(r, mk_leaf(r, dir),
				 mk_string(r, "/"),
				 mk_leaf(r, cmd_filename)));
	      if(access(full_cmd.data, X_OK) == 0) {
		cmd_filename = full_cmd;
		break;
	      }
	    }
	    else break;
	  }
	}
      }
      
      p.got = 0;
      p.root_dir = state->root;
      p.tree = make_empty_node();
      p.cwd = state->cwd;
      flatten_args(r, &p, 0 /* rw */, 0 /* ambient */, args);
      
      for(l = p.got, count = 0; l; l = l->prev) count++;
      argv = region_alloc(r, (count+1) * sizeof(char*));
      argv[0] = cmd_filename.data;
      for(l = p.got, i = count; l; l = l->prev) argv[--i + 1] = l->str;
      
      /* FIXME: check for errors */
      resolve_populate(state->root, p.tree, p.cwd, seqf_string("/etc"), 0 /* create */, &err);
      resolve_populate(state->root, p.tree, p.cwd, seqf_string("/bin/"), 0 /* create */, &err);
      resolve_populate(state->root, p.tree, p.cwd, seqf_string("/lib"), 0 /* create */, &err);
      resolve_populate(state->root, p.tree, p.cwd, seqf_string("/usr"), 0 /* create */, &err);
      resolve_populate(state->root, p.tree, p.cwd, seqf_string("/dev/tty"), 0 /* create */, &err);
      resolve_populate(state->root, p.tree, p.cwd, seqf_string("/dev/null"), 0 /* create */, &err);
      
      {
	region_t r = region_make();
	int err;
	struct filesys_obj *obj;
	obj = resolve_file(r, state->root, state->cwd, cmd_filename,
			   SYMLINK_LIMIT, 0 /* nofollow */, &err);
	region_free(r);
	if(!obj) { errno = err; perror("open"); goto error; }
	exec_fd = obj->vtable->open(obj, O_RDONLY, &err);
	filesys_obj_free(obj);
	if(exec_fd < 0) { errno = err; perror("open"); goto error; }
      }

      if(0) print_tree(0, p.tree);
      
      root_slot = build_fs(p.tree);
      root = root_slot->vtable->get(root_slot);
      assert(root);
      root->refcount++;
      filesys_slot_free(root_slot);
      spawn(root, state->cwd, exec_fd, cmd_filename.data, count+1, argv);
      filesys_obj_free(root);
      /* FIXME: deallocate tree */
      }
    }
    else assert(0);
  }
  else {
    printf("parse failed\n");
  }
  if(MOD_DEBUG) printf("allocated %i bytes\n", region_allocated(r));
 error:
  region_free(r);
}

int main(int argc, char *argv[])
{
  struct shell_state state;
  state.root = initial_dir("/");
  state.cwd = 0;
  /* Set up the current directory */
  /* If this fails, the cwd is left undefined: you can't access anything
     through it.  This seems safer than just setting the cwd to the root
     directory if the shell is to be used in batch mode. */
  {
    char *cwd_name = getcwd(0, 0);
    if(!cwd_name) {
      perror("can't get current working directory");
    }
    else {
      region_t r = region_make();
      int err;
      struct dir_stack *new_cwd =
	resolve_dir(r, state.root, 0 /* cwd */, seqf_string(cwd_name),
		    SYMLINK_LIMIT, &err);
      if(new_cwd) {
	state.cwd = new_cwd;
      }
      else {
	errno = err;
	perror("can't set current working directory");
      }
      region_free(r);
      free(cwd_name);
    }
  }

  if(argc <= 1) {
    /* Interactive mode */
    using_history();
    while(1) {
      char *line1;

      /* The filesysobj code will fchdir all over the place!  This gets
	 a bit confusing because readline uses the kernel-provided cwd to do
	 filename completion.  Set the kernel-provided cwd to our cwd.
	 Ultimately, the proper way to do this would be to change readline
	 to use our virtualised filesystem. */
      if(state.cwd->dir->vtable == &real_dir_vtable) {
	struct real_dir *dir = (void *) state.cwd->dir;
	if(dir->fd) {
	  if(fchdir(dir->fd->fd) < 0) perror("fchdir");
	}
      }
      line1 = readline("plash$ ");
      if(!line1) break;
      if(*line1) {
	add_history(line1);
	shell_command(&state, seqf_string(line1));
      }
      /* As far as I can tell, add_history() doesn't take a copy of the line you give it. */
      /* free(line1); */
    }
    printf("\n");
  }
  else if(argc == 3 && !strcmp(argv[1], "-c")) {
    shell_command(&state, seqf_string(argv[2]));
  }
  else {
    printf("Usage: shell [-c CMD]\n");
  }
  return 0;
}
