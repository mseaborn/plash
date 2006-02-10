/* Copyright (C) 2005 Mark Seaborn

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

/* Needed for environ */
#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include "region.h"
#include "filesysobj.h"
#include "filesysobj-real.h"
#include "resolve-filename.h"
#include "build-fs.h"
#include "fs-operations.h"
#include "cap-protocol.h"
#include "cap-utils.h"
#include "config.h"
#include "marshal.h"
#include "plash-libc.h"


#define NAME "pola-run"
#define NAME_MSG NAME ": "

#define FALSE 0
#define TRUE 1


int under_plash = FALSE;

__asm__(".weak plash_libc_kernel_execve");


void print_args(int argc, const char **argv)
{
  int i;
  printf("%i args:\n", argc);
  for(i = 0; i < argc; i++) {
    fprintf(stderr, "  arg %i: \"%s\"\n", i, argv[i]);
  }
}

void list_fds()
{
  int fd;
  for(fd = 0; fd < 1024; fd++) {
    int flags = fcntl(fd, F_GETFD);
    if(flags < 0 && errno != EBADF) { perror(NAME_MSG "fcntl"); }
    if(flags >= 0) {
      fprintf(stderr, "fd %i%s\n", fd,
	      flags & FD_CLOEXEC ? " (ok, close-on-exec)" : "");
    }
  }
}

#include <sys/wait.h>
/* This is used for determining whether it is the server or client
   that dies from a segfault. */
void monitor_this_process(const char *name)
{
  int status, pid2;
  int pid = fork();
  if(pid < 0) { perror("fork"); exit(1); }
  if(pid == 0) return; /* Returns to the child process. */
  /* Parent monitors the child: */
  pid2 = waitpid(pid, &status, 0);
  if(WIFEXITED(status)) {
    fprintf(stderr, "%s: exited with code %i\n", name, WEXITSTATUS(status));
  }
  else if(WIFSIGNALED(status)) {
    fprintf(stderr, "%s: exited with signal %i (%s)\n",
	    name, WTERMSIG(status), strsignal(WTERMSIG(status)));
  }
  else {
    fprintf(stderr, "%s: ??\n", name);
  }
  exit(0);
}


struct dir_stack *copy_cwd(struct filesys_obj *root_dir)
{
  /* Set up the current directory */
  /* If this fails, the cwd is left undefined: you can't access anything
     through it.  This seems safer than just setting the cwd to the root
     directory if the shell is to be used in batch mode. */
  struct dir_stack *cwd = NULL;
  char *cwd_name = getcwd(0, 0);
  if(!cwd_name) {
    perror(NAME_MSG "can't get current working directory");
  }
  else {
    region_t r = region_make();
    int err;
    cwd = resolve_dir(r, root_dir, NULL /* cwd */, seqf_string(cwd_name),
		      SYMLINK_LIMIT, &err);
    if(!cwd) {
      fprintf(stderr, NAME_MSG "can't set cwd to \"%s\": %s\n",
	      cwd_name, strerror(err));
    }
    free(cwd_name);
    region_free(r);
  }
  return cwd;
}


struct arg_list {
  const char *str;
  struct arg_list *prev;
};

struct state {
  struct filesys_obj *root_dir;
  struct dir_stack *cwd;
  fs_node_t root_node;
  struct arg_list *args;
  int args_count;
  const char *executable_filename;
  int log_summary;
};

void usage(FILE *fp)
{
  fprintf(fp,
	  "Plash version " PLASH_VERSION "\n"
	  "Usage: "
	  NAME "\n"
	  "  --prog <file>   Gives filename of executable to invoke\n"
	  "  [ -f[awls]... <path>\n"
	  "                  Grant access to given file or directory\n"
	  "  | -t[awls]... <path-dest> <path-src>\n"
	  "                  Attach file/dir <path-src> into namespace at <path-dest>\n"
	  "  | -a <string>   Add string to argument list\n"
	  "  | --cwd <dir>   Set the current working directory (cwd)\n"
	  "  | --no-cwd      Set the cwd to be undefined\n"
	  "  | --copy-cwd    Copy cwd from calling process\n"
	  "  ]...\n"
	  "  [-B]            Grant access to /usr, /bin and /lib\n"
	  "  [--x11]         Grant access to X11 Window System\n"
	  "  [--net]         Grant access to network config files\n"
	  "  [--log]         Print method calls client makes to file server\n");
}

struct flags {
  int a;
  int build_fs;
};

int handle_flag(seqf_t flag, struct flags *f)
{
  if(flag.size == 1) switch(*flag.data) {
    case 'a':
      f->a = 1;
      return 0;
    case 'l':
      f->build_fs |= FS_FOLLOW_SYMLINKS;
      return 0;
    case 's':
      return 0;
    case 'w':
      f->build_fs |= FS_SLOT_RWC;
      return 0;
  }
  if(seqf_equal(flag, seqf_string("objrw"))) {
    f->build_fs |= FS_OBJECT_RW;
    return 0;
  }
  fprintf(stderr, NAME_MSG "error: unrecognised flag, \"");
  fprint_d(stderr, flag);
  fprintf(stderr, "\"\n");
  return 1;
}

/* "arg" receives the text after "=", if such exists.
   "arg" may be NULL if this syntax is not allowed. */
int parse_flags(const char *str, struct flags *f, const char **arg)
{
  seqf_t flag;
  f->a = 0;
  f->build_fs = FS_READ_ONLY;
  if(arg) { *arg = NULL; }
  
  for(; *str; str++) {
    if(*str == '=' && arg) {
      *arg = str + 1;
      return 0;
    }
    if(*str == ',') {
      /* Handle multi-char flags, separated by commas. */
      const char *p = str + 1;
      while(1) {
	int i = 0;
	while(p[i] && p[i] != ',' && p[i] != '=') { i++; }
	flag.data = p;
	flag.size = i;
	if(handle_flag(flag, f)) { return 1; }
	if(!p[i]) { break; }
	if(p[i] == '=' && arg) {
	  *arg = p + i + 1;
	  return 0;
	}
	p += i + 1;
      }
      return 0;
    }

    /* Handle single-char flag. */
    flag.data = str;
    flag.size = 1;
    if(handle_flag(flag, f)) { return 1; }
  }
  return 0;
}

void add_to_arg_list(region_t r, struct state *state, const char *arg)
{
  struct arg_list *node = region_alloc(r, sizeof(struct arg_list));
  node->str = arg;
  node->prev = state->args;
  state->args = node;
  state->args_count++;
}

/* Returns non-zero if an error occurs in parsing the arguments.
   (This is a fatal error.) */
int handle_arguments(region_t r, struct state *state,
		     int i, int argc, char **argv)
{
  int err;
  while(i < argc) {
    const char *arg = argv[i++];
    if(arg[0] != '-') { goto unknown; }
    switch(arg[1]) {
      case 'f': {
	struct flags flags;
	const char *filename;
	if(parse_flags(arg + 2, &flags, &filename)) { return 1; }
	if(!filename) {
	  if(i + 1 > argc) {
	    fprintf(stderr, NAME_MSG "-f expects 1 parameter\n");
	    return 1;
	  }
	  filename = argv[i++];
	}
	
	if(fs_resolve_populate(state->root_dir, state->root_node, state->cwd,
			       seqf_string(filename),
			       flags.build_fs, &err) < 0) {
	  /* Warning */
	  fprintf(stderr, NAME_MSG "error resolving filename \"%s\": %s\n",
		  filename, strerror(err));
	}
	if(flags.a) { add_to_arg_list(r, state, filename); }
	goto arg_handled;
      }
      
      case 't': {
	struct filesys_obj *obj;
	struct flags flags;
	const char *dest_filename, *src_filename;
	if(i + 2 > argc) {
	  fprintf(stderr, NAME_MSG "-t expects 2 parameters\n");
	  return 1;
	}
	dest_filename = argv[i++];
	src_filename = argv[i++];
	if(parse_flags(arg + 2, &flags, NULL)) { return 1; }
	
	obj = resolve_obj_simple(state->root_dir, state->cwd,
				 seqf_string(src_filename),
				 (flags.build_fs & FS_FOLLOW_SYMLINKS ?
				  SYMLINK_LIMIT : 0),
				 FALSE /* nofollow */, &err);
	if(!obj) {
	  /* Warning */
	  fprintf(stderr, NAME_MSG "error resolving filename \"%s\": %s\n",
		  src_filename, strerror(err));
	}
	else {
	  if(fs_attach_at_pathname(state->root_node, state->cwd,
				   seqf_string(dest_filename), obj,
				   &err) < 0) {
	    /* Warning */
	    fprintf(stderr, NAME_MSG "%s\n", strerror(err));
	  }
	}
	if(flags.a) { add_to_arg_list(r, state, dest_filename); }
	goto arg_handled;
      }
      
      case 'a': {
	const char *str;
	if(arg[2] == '=') { str = arg + 3; }
	else if(!arg[2]) {
	  if(i + 1 > argc) {
	    fprintf(stderr, NAME_MSG "-a expects 1 parameter\n");
	    return 1;
	  }
	  str = argv[i++];
	}
	else {
	  fprintf(stderr, NAME_MSG "unrecognised argument to -a\n");
	  return 1;
	}
	add_to_arg_list(r, state, str);
	goto arg_handled;
      }

      case 'B': {
	char *args[] = { "-fl", "/usr",
			 "-fl", "/bin",
			 "-fl", "/lib",
			 "-fl,objrw", "/dev/null",
			 "-fl,objrw", "/dev/tty" };
	if(handle_arguments(r, state, 0, 10, args)) { return 1; }
	goto arg_handled;
      }

      case 'h': usage(stdout); return 1;
    }

    if(!strcmp(arg, "--prog")) {
      if(i + 1 > argc) {
	fprintf(stderr, NAME_MSG "--prog expects 1 parameter\n");
	return 1;
      }
      if(state->executable_filename) {
	fprintf(stderr, NAME_MSG "--prog should be used only once\n");
	return 1;
      }
      state->executable_filename = argv[i++];
      goto arg_handled;
    }
    
    if(!strcmp(arg, "--cwd")) {
      struct dir_stack *old_cwd = state->cwd;
      const char *pathname;
      if(i + 1 > argc) {
	fprintf(stderr, NAME_MSG "--cwd expects 1 parameter\n");
	return 1;
      }
      pathname = argv[i++];
      state->cwd = resolve_dir(r, state->root_dir, state->cwd,
			       seqf_string(pathname),
			       SYMLINK_LIMIT, &err);
      dir_stack_free(old_cwd);
      if(!state->cwd) {
	/* Warning */
	fprintf(stderr, NAME_MSG "can't set cwd to \"%s\": %s\n",
		pathname, strerror(err));
      }
      goto arg_handled;
    }

    if(!strcmp(arg, "--no-cwd")) {
      dir_stack_free(state->cwd);
      state->cwd = NULL;
      goto arg_handled;
    }

    if(!strcmp(arg, "--copy-cwd")) {
      dir_stack_free(state->cwd);
      state->cwd = copy_cwd(state->root_dir);
      goto arg_handled;
    }

    if(!strcmp(arg, "--x11")) {
      /* FIXME: change from "-flw" to "-fl,socket". */
      char *args[] = { "-flw", "/tmp/.X11-unix/",
		       "-fl", NULL };
      args[3] = getenv("XAUTHORITY");
      if(!args[3]) {
	fprintf(stderr, NAME_MSG "XAUTHORITY is not set\n");
      }
      else {
	if(handle_arguments(r, state, 0, 4, args)) { return 1; }
      }
      goto arg_handled;
    }

    if(!strcmp(arg, "--net")) {
      char *args[] = { "-fl", "/etc/resolv.conf",
		       "-fl", "/etc/hosts",
		       "-fl", "/etc/services" };
      if(handle_arguments(r, state, 0, 6, args)) { return 1; }
      goto arg_handled;
    }

    if(!strcmp(arg, "--log")) {
      state->log_summary = TRUE;
      goto arg_handled;
    }

    if(!strcmp(arg, "--help")) { usage(stdout); return 1; }

  unknown:
    fprintf(stderr, NAME_MSG "error: unrecognised argument: \"%s\"\n", arg);
    return 1;

  arg_handled:;
  }
  return 0;
}

static void args_to_exec_elf_program
  (region_t r, const char *executable_filename,
   int argc, const char **argv,
   const char **cmd_out, int *argc_out, const char ***argv_out)
{
  int debug = 0;
  int extra_args = 4 + (debug ? 1:0);
  const char **argv2;
  int i;

  assert(argc >= 1);
  argv2 = region_alloc(r, (argc + extra_args + 1) * sizeof(char *));
  argv2[0] = argv[0];
  if(under_plash) {
    *cmd_out = "/run-as-anonymous";
  }
  else {
    *cmd_out = PLASH_SETUID_BIN_INSTALL "/run-as-anonymous";
  }
  i = 1;
  if(debug) argv2[i++] = "--debug";
  argv2[i++] = "/special/ld-linux.so.2";
  argv2[i++] = "--library-path";
  argv2[i++] = PLASH_LD_LIBRARY_PATH;
  argv2[i++] = executable_filename;
  for(i = 1; i < argc; i++) { argv2[extra_args+i] = argv[i]; }
  argv2[extra_args + argc] = NULL;
  *argc_out = extra_args + argc;
  *argv_out = argv2;
}

static void args_to_exec_elf_program_from_fd
  (region_t r, int fd, int argc, const char **argv,
   const char **cmd_out, int *argc_out, const char ***argv_out)
{
  int debug = 0;
  int extra_args = 6 + (debug ? 1:0);
  int buf_size = 20;
  char *buf = region_alloc(r, buf_size);
  const char **argv2;
  int i;

  assert(argc >= 1);
  argv2 = region_alloc(r, (argc + extra_args + 1) * sizeof(char *));
  argv2[0] = argv[0];
  if(under_plash) {
    *cmd_out = "/run-as-anonymous";
  }
  else {
    *cmd_out = PLASH_SETUID_BIN_INSTALL "/run-as-anonymous";
  }
  i = 1;
  if(debug) argv2[i++] = "--debug";
  argv2[i++] = "/special/ld-linux.so.2";
  argv2[i++] = "--library-path";
  argv2[i++] = PLASH_LD_LIBRARY_PATH;
  argv2[i++] = "--fd";
  snprintf(buf, buf_size, "%i", fd);
  argv2[i++] = buf;
  for(i = 0; i < argc; i++) { argv2[extra_args+i] = argv[i]; }
  argv2[extra_args + argc] = NULL;
  *argc_out = extra_args + argc;
  *argv_out = argv2;
}


int main(int argc, char **argv)
{
  region_t r = region_make();
  cap_t fs_op;
  int err;
  
  struct state state;
  // state.root_dir = NULL;
  // state.cwd = NULL;
  // state.root_node = NULL;
  state.args = NULL;
  state.args_count = 0;
  state.executable_filename = NULL;
  state.log_summary = FALSE;

  if(getenv("PLASH_CAPS")) {
    struct cap_args result;
    if(get_process_caps("fs_op", &fs_op,
			// "conn_maker", ...,
			NULL) < 0) { return 1; }
    under_plash = TRUE;

    cap_call(fs_op, r,
	     cap_args_make(mk_int(r, METHOD_FSOP_GET_ROOT_DIR),
			   caps_empty, fds_empty),
	     &result);
    filesys_obj_free(fs_op);
    if(expect_cap1(result, &state.root_dir) < 0) {
      fprintf(stderr, NAME_MSG "get-root failed\n");
      return 1;
    }
  }
  else {
    under_plash = FALSE;
    
    state.root_dir = initial_dir("/", &err);
    if(!state.root_dir) {
      fprintf(stderr, NAME_MSG "can't open root directory: %s\n", strerror(err));
      return 1;
    }
  }

  state.cwd = copy_cwd(state.root_dir);

  state.root_node = fs_make_empty_node();
  assert(state.root_node);
  
  if(handle_arguments(r, &state, 1, argc, argv)) { return 1; }

  if(!state.executable_filename) {
    fprintf(stderr, NAME_MSG "--prog argument missing, no executable specified\n");
    return 1;
  }
  {
    int executable_fd;
    // int executable_fd2;
    const char *executable_filename2; /* of interpreter for #! scripts */
    const char **args_array, **args_array2;
    int args_count2;
    int socks[2];
    int pid;
    
    struct filesys_obj *obj;
    obj = resolve_file(r, state.root_dir, state.cwd,
		       seqf_string(state.executable_filename),
		       SYMLINK_LIMIT, 0 /* nofollow */, &err);
    if(!obj) {
      fprintf(stderr, NAME_MSG "open/exec: %s: %s\n",
	      state.executable_filename, strerror(err));
      return 1;
    }
    /* This function gives warnings about setuid/gid executables. */
    executable_fd = open_executable_file(obj, seqf_string(state.executable_filename), &err);
    filesys_obj_free(obj);
    if(executable_fd < 0) {
      fprintf(stderr, NAME_MSG "open/exec: %s: %s\n",
	      state.executable_filename, strerror(err));
      return 1;
    }
    
    {
      struct arg_list *l;
      int i;
      args_array = region_alloc(r, sizeof(char *) * (state.args_count + 2));
      for(i = state.args_count - 1, l = state.args;
	  l;
	  i--, l = l->prev) {
	assert(i >= 0);
	args_array[i+1] = l->str;
      }
      args_array[0] = state.executable_filename;
      args_array[state.args_count+1] = NULL;
    }
    
    if(socketpair(AF_LOCAL, SOCK_STREAM, 0, socks) < 0) {
      perror(NAME_MSG "socketpair");
      return 1; /* Error */
    }

    {
      cap_t child_root;
      struct dir_stack *child_cwd = NULL;
      struct server_shared *shared;
      int cap_count;
      cap_t *caps;
      const char *cap_names;

      child_root = fs_make_root(state.root_node);
      assert(child_root);
      free_node(state.root_node);

      // fs_print_tree(0, state.root_node);

      /* Set up cwd.  If this fails, the cwd is left undefined. */
      if(state.cwd) {
	seqf_t cwd_path = flatten(r, string_of_cwd(r, state.cwd));
	int err;
	child_cwd = resolve_dir(r, child_root, NULL /* cwd */, cwd_path,
				SYMLINK_LIMIT, &err);
      }
    
      /* Handle scripts using the `#!' syntax. */
      /* This resolves the interpreter name in the callee's namespace. */
      if(exec_for_scripts(r, child_root, child_cwd,
			  state.executable_filename, executable_fd,
			  state.args_count + 1, args_array,
			  // &executable_fd2,
			  NULL,
			  &executable_filename2,
			  &args_count2, &args_array2, &err) < 0) {
	fprintf(stderr, NAME_MSG "bad interpreter: %s: %s\n",
		state.executable_filename, strerror(err));
	return 1;
      }

      filesys_obj_free(state.root_dir);
      if(state.cwd) { dir_stack_free(state.cwd); }
      
      shared = amalloc(sizeof(struct server_shared));
      shared->refcount = 1;
      shared->next_id = 1;
      {
	int fd = dup(STDERR_FILENO);
	if(fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
	  perror("fcntl");
	  return -1;
	}
	shared->log = fdopen(fd, "w");
      }
      shared->log_summary = state.log_summary;
      shared->log_messages = 0;

      // "fs_op;conn_maker;fs_op_maker;union_dir_maker"
      cap_names = "fs_op;conn_maker";
      cap_count = 2;
      caps = region_alloc(r, cap_count * sizeof(cap_t));
      caps[0] = make_fs_op_server(shared, child_root, child_cwd);
      caps[1] = conn_maker_make();

      /* Do I want to do a double fork so that the server process is no
	 longer a child of the client process?  The client process might
	 not be expecting to handle SIGCHILD signals.  Or perhaps these
	 aren't delivered after an exec() call. */
      pid = fork();
      if(pid == 0) {
	// monitor_this_process("server");
	close(socks[0]);
	cap_make_connection(r, socks[1], cap_seq_make(caps, cap_count),
			    0, "to-client");
	caps_free(cap_seq_make(caps, cap_count));
	region_free(r);

#ifdef GC_DEBUG
	gc_init();
	cap_mark_exported_objects();
	gc_check();
#endif

	cap_run_server();
	exit(0);
      }
      if(pid < 0) {
	perror(NAME_MSG "fork");
	return 1; /* Error */
      }
      cap_close_all_connections();

      close(socks[1]);

      {
	char buf[20];
	snprintf(buf, sizeof(buf), "%i", socks[0]);
	if(setenv("PLASH_COMM_FD", buf, 1) < 0 ||
	   setenv("PLASH_CAPS", cap_names, 1) < 0) {
	  fprintf(stderr, NAME_MSG "setenv failed\n");
	  return 1;
	}
      }

      {
	const char *cmd;
	args_to_exec_elf_program(r, executable_filename2,
				 args_count2, args_array2,
				 &cmd, &args_count2, &args_array2);
#if 0
	/* We must clear the close-on-exec flag explicitly for the
	   executable FD. */
	{
	  int flags = fcntl(executable_fd2, F_GETFD);
	  if(flags < 0) { perror(NAME_MSG "fcntl"); return 1; }
	  if(fcntl(executable_fd2, F_SETFD, flags &~ FD_CLOEXEC) < 0) {
	    perror(NAME_MSG "fcntl");
	    return 1;
	  }
	}
#endif

	if(0) {
	  print_args(args_count2, args_array2);
	  // printf("exec_fd=%i, sock_fd=%i\n", socks[0], executable_fd2);
	  list_fds();
	  monitor_this_process("client");
	}
	if(0) {
	  /* Check for FDs that have been left open that shouldn't be. */
	  int fd;
	  int fail = 0;
	  for(fd = 0; fd < 1024; fd++) {
	    int flags = fcntl(fd, F_GETFD);
	    if(flags < 0 && errno != EBADF) { perror(NAME_MSG "fcntl"); }
	    if(flags >= 0 &&
	       !(flags & FD_CLOEXEC) &&
	       fd > 2 &&
	       fd != socks[0]
	       // && fd != executable_fd2
	       ) {
	      fprintf(stderr, "fd %i left open\n", fd);
	      fail = 1;
	    }
	  }
	  assert(!fail);
	}

	if(under_plash) {
	  assert(plash_libc_kernel_execve);
	  plash_libc_kernel_execve(cmd, (char **) args_array2, environ);
	}
	else {
	  execve(cmd, (char **) args_array2, environ);
	}
	perror(NAME_MSG "exec");
	return 1;
      }
    }
  }
}
