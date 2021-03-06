/* Copyright (C) 2005 Mark Seaborn

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

#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include "config.h"

#ifdef USE_GTK
#include <gtk/gtkwindow.h>
#include <gtk/gtk.h>
#include <glib.h>
#endif

#include "region.h"
#include "filesysobj.h"
#include "filesysobj-real.h"
#include "filesysobj-readonly.h"
#include "resolve-filename.h"
#include "build-fs.h"
#include "fs-operations.h"
#include "cap-protocol.h"
#include "cap-utils.h"
#include "marshal.h"
#include "marshal-pack.h"
#include "plash-libc.h"
#include "powerbox.h"
#include "exec.h"
#include "log.h"


#define NAME "pola-run"
#define NAME_MSG NAME ": "


typedef int bool;


int under_plash = FALSE;


struct dir_stack *copy_cwd(struct filesys_obj *root_dir)
{
  /* Set up the current directory */
  /* If this fails, the cwd is left undefined: you can't access anything
     through it.  This seems safer than just setting the cwd to the root
     directory if the shell is to be used in batch mode. */
  struct dir_stack *cwd = NULL;
  char *cwd_name = getcwd(0, 0);
  if(!cwd_name) {
    perror(NAME_MSG _("can't get current working directory"));
  }
  else {
    region_t r = region_make();
    int err;
    cwd = resolve_dir(r, root_dir, NULL /* cwd */, seqf_string(cwd_name),
		      SYMLINK_LIMIT, &err);
    if(!cwd) {
      fprintf(stderr, NAME_MSG _("can't set cwd to \"%s\": %s\n"),
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
  struct filesys_obj *root_dir; /* Caller's root directory */
  struct dir_stack *cwd; /* Current working directory in caller's namespace */
  fs_node_t root_node; /* Callee's root directory */
  struct arg_list *args;
  int args_count;
  const char *executable_filename;
  struct filesys_obj *log;
  int debug;
  const char *pet_name;
  int powerbox;
  int server_as_parent;
  int search_path; /* Whether to search PATH for executable name */
};

void init_state(struct state *state)
{
  state->root_dir = NULL;
  state->cwd = NULL;
  state->root_node = NULL;
  state->args = NULL;
  state->args_count = 0;
  state->executable_filename = NULL;
  state->log = NULL;
  state->debug = FALSE;
  state->pet_name = NULL;
  state->powerbox = FALSE;
  state->server_as_parent = FALSE;
  state->search_path = TRUE;
}

void usage(FILE *fp)
{
  fprintf(fp,
	  _("Plash version " PLASH_VERSION "\n"
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
	  "  [--log]         Print method calls client makes to file server\n"
	  "  [--log-file <file>]\n"
	  "  [--server-as-parent]  Server runs as the parent process, not the child\n"
	  "  [--pet-name <name>]\n"
	  "  [--powerbox]\n"
	  "  [--no-path-search]  Don't look up executable name in PATH\n"
	  "  [-e <command> <arg>...]\n"
	  ));
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
  region_t r = region_make();
  fprintf(stderr, NAME_MSG _("error: unrecognised flag, \"%s\"\n"),
	  region_strdup_seqf(r, flag));
  region_free(r);
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
	    fprintf(stderr, NAME_MSG _("-f expects 1 parameter\n"));
	    return 1;
	  }
	  filename = argv[i++];
	}
	
	if(fs_resolve_populate(state->root_dir, state->root_node, state->cwd,
			       seqf_string(filename),
			       flags.build_fs, &err) < 0) {
	  /* Warning */
	  fprintf(stderr, NAME_MSG _("error resolving filename \"%s\": %s\n"),
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
	  fprintf(stderr, NAME_MSG _("-t expects 2 parameters\n"));
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
	  fprintf(stderr, NAME_MSG _("error resolving filename \"%s\": %s\n"),
		  src_filename, strerror(err));
	}
	else {
	  /* Unless the "w" or "objrw" flag is given, attach a
	     read-only version of the object. */
	  if(!((flags.build_fs & FS_SLOT_RWC) ||
	       (flags.build_fs & FS_OBJECT_RW))) {
	    obj = make_read_only_proxy(obj);
	  }
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
	    fprintf(stderr, NAME_MSG _("-a expects 1 parameter\n"));
	    return 1;
	  }
	  str = argv[i++];
	}
	else {
	  fprintf(stderr, NAME_MSG _("unrecognised argument to -a\n"));
	  return 1;
	}
	add_to_arg_list(r, state, str);
	goto arg_handled;
      }

      case 'e': {
	for(; i < argc; i++) {
	  if(!state->executable_filename) {
	    /* If --prog has not been specified, the first argument is
	       taken as the program to run. */
	    state->executable_filename = argv[i];
	  }
	  else {
	    add_to_arg_list(r, state, argv[i]);
	  }
	}
	goto arg_handled;
      }

      case 'B': {
	int number_args = 8;
	char *args[] = { "-fl", "/usr",
			 "-fl", "/bin",
			 "-fl", "/lib",
			 "-fl,objrw", "/dev/null" };
	if(handle_arguments(r, state, 0, number_args, args))
	  return 1;
	/* Errors from granting /lib64 are ignored. */
	fs_resolve_populate(state->root_dir, state->root_node, state->cwd,
			    seqf_string("/lib64"), FS_FOLLOW_SYMLINKS, &err);
	goto arg_handled;
      }

      case 'h': usage(stdout); return 1;
    }

    if(!strcmp(arg, "--prog")) {
      if(i + 1 > argc) {
	fprintf(stderr, NAME_MSG _("--prog expects 1 parameter\n"));
	return 1;
      }
      if(state->executable_filename) {
	fprintf(stderr, NAME_MSG _("--prog should be used only once\n"));
	return 1;
      }
      state->executable_filename = argv[i++];
      goto arg_handled;
    }
    
    if(!strcmp(arg, "--cwd")) {
      struct dir_stack *old_cwd = state->cwd;
      const char *pathname;
      if(i + 1 > argc) {
	fprintf(stderr, NAME_MSG _("--cwd expects 1 parameter\n"));
	return 1;
      }
      pathname = argv[i++];
      state->cwd = resolve_dir(r, state->root_dir, state->cwd,
			       seqf_string(pathname),
			       SYMLINK_LIMIT, &err);
      dir_stack_free(old_cwd);
      if(!state->cwd) {
	/* Warning */
	fprintf(stderr, NAME_MSG _("can't set cwd to \"%s\": %s\n"),
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

    if(!strcmp(arg, "--env")) {
      char *env_arg;
      if(i + 1 > argc) {
	fprintf(stderr, NAME_MSG _("--env expects 1 parameter\n"));
	return 1;
      }
      env_arg = argv[i++];
      putenv(env_arg);
      goto arg_handled;
    }

    if(!strcmp(arg, "--fd")) {
      int fd;
      if(i + 1 > argc) {
	fprintf(stderr, NAME_MSG _("--fd expects 1 parameter\n"));
	return 1;
      }
      fd = atoi(argv[i++]);
      /* Not implemented. */
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
      if(state->log) { filesys_obj_free(state->log); }
      state->log = make_log_from_fd(STDERR_FILENO);
      goto arg_handled;
    }

    if(!strcmp(arg, "--log-file")) {
      char *filename;
      int log_fd;
      if(i + 1 > argc) {
	fprintf(stderr, NAME_MSG _("--log-file expects 1 parameter\n"));
	return 1;
      }
      filename = argv[i++];
      log_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
      if(log_fd < 0) {
	fprintf(stderr, NAME_MSG _("Error opening \"%s\": %s\n"),
		filename, strerror(errno));
	return 1;
      }
      if(state->log) { filesys_obj_free(state->log); }
      state->log = make_log_from_fd(log_fd);
      close(log_fd);
      goto arg_handled;
    }

    if(!strcmp(arg, "--debug")) {
      state->debug = TRUE;
      goto arg_handled;
    }

    if(!strcmp(arg, "--powerbox")) {
#ifdef USE_GTK
      state->powerbox = TRUE;
      goto arg_handled;
#else
      fprintf(stderr, NAME_MSG _("powerbox support not compiled in\n"));
      return 1;
#endif
    }

    if(!strcmp(arg, "--pet-name")) {
      if(i + 1 > argc) {
	fprintf(stderr, NAME_MSG _("--pet-name expects 1 parameter\n"));
	return 1;
      }
      state->pet_name = argv[i++];
      goto arg_handled;
    }

    if(!strcmp(arg, "--server-as-parent")) {
      state->server_as_parent = TRUE;
      goto arg_handled;
    }

    if(!strcmp(arg, "--no-path-search")) {
      state->search_path = FALSE;
      goto arg_handled;
    }

    if(!strcmp(arg, "--help")) { usage(stdout); return 1; }

  unknown:
    fprintf(stderr, NAME_MSG _("error: unrecognised argument: \"%s\"\n"), arg);
    return 1;

  arg_handled:;
  }
  return 0;
}

static int args_to_exec_elf_program
  (region_t r, struct state *state, const char *executable_filename,
   int argc, const char **argv,
   const char **cmd_out, int *argc_out, const char ***argv_out,
   int debug)
{
  const char *sandbox_prog;
  int extra_args = 7 + (debug ? 1:0);
  const char **argv2;
  int i, j;

  assert(argc >= 1);
  argv2 = region_alloc(r, (argc + extra_args + 1) * sizeof(char *));
  argv2[0] = argv[0];
  i = 1;
  if(under_plash)
    sandbox_prog = getenv("PLASH_P_SANDBOX_PROG");
  else
    sandbox_prog = getenv("PLASH_SANDBOX_PROG");
  if(!sandbox_prog) {
    if(under_plash) {
      *cmd_out = "/run-as-anonymous";
    }
    else {
      *cmd_out = PLASH_SETUID_BIN_INSTALL "/run-as-anonymous";
    }
    if(debug) argv2[i++] = "--debug";
    const char *ld_library_path = getenv("LD_LIBRARY_PATH");
    if(!under_plash) {
      if(ld_library_path)
	ld_library_path = flatten_str(r, mk_printf(r, "%s:%s", LIB_INSTALL,
						   ld_library_path));
      else
	ld_library_path = LIB_INSTALL;
    }
    if(ld_library_path) {
      argv2[i++] = "-s";
      argv2[i++] = flatten_str(r, mk_printf(r, "LD_LIBRARY_PATH=%s",
					    ld_library_path));
    }
    /* Make sure LD_PRELOAD is copied through.  It might have been set in
       our current environment using the --env option. */
    char *ld_preload = getenv("LD_PRELOAD");
    if(ld_preload) {
      argv2[i++] = "-s";
      argv2[i++] = flatten_str(r, cat2(r, mk_string(r, "LD_PRELOAD="),
				       mk_string(r, ld_preload)));
    }
    int ldso_fd = open(PLASH_LDSO, O_RDONLY);
    if(ldso_fd < 0) {
      perror("open: " PLASH_LDSO);
      return -1;
    }
    argv2[i++] = "/chainloader";
    argv2[i++] = flatten_str(r, mk_printf(r, "%i", ldso_fd));
  }
  else {
    *cmd_out = sandbox_prog;
  }
  
  argv2[i++] = executable_filename;
  assert(i <= extra_args + 1);
  for(j = 1; j < argc; j++) { argv2[i++] = argv[j]; }
  argv2[i] = NULL;
  *argc_out = i;
  *argv_out = argv2;
  return 0;
}


static void server_process(int argc, char **argv,
			   bool use_gtk)
{
#ifdef GC_DEBUG
  gc_init();
  cap_mark_exported_objects();
  gc_check();
#endif

#ifdef USE_GTK
  if(use_gtk) {
    /* NB. We have already processed arguments!  Gtk should not
       take anything from this arg list, we hope. */
    gtk_init(&argc, &argv);
    
    /* It seems that if the X11 connection is broken, gtk_main() calls
       exit() rather than returning.  That's not what we want. */
    while(cap_server_exporting()) {
      gtk_main_iteration();
    }
  }
#endif
  
  /* We'd like to fall back to a normal non-X11 server if the X11
     event loop exits.  This in turn will exit if we're not serving
     any references any more. */
  cap_run_server();
}


int main(int argc, char **argv)
{
  region_t r = region_make();
  cap_t fs_op;
  int err;
  
  struct state state;
  init_state(&state);

  /* Don't call gtk_init(): don't open an X11 connection at this stage,
     because we don't know if we'll need one and X11 might not be
     available to us. */
  /* I don't like that Gtk will take command line arguments.  I would
     like to pass an empty array here, but that causes Gtk to segfault. */
  // gtk_parse_args(&argc, &argv);

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
    if(!pl_unpack(r, result, METHOD_R_CAP, "c", &state.root_dir)) {
      fprintf(stderr, NAME_MSG _("get-root failed\n"));
      return 1;
    }
  }
  else {
    under_plash = FALSE;
    
    state.root_dir = initial_dir("/", &err);
    if(!state.root_dir) {
      fprintf(stderr, NAME_MSG _("can't open root directory: %s\n"), strerror(err));
      return 1;
    }
  }

  state.cwd = copy_cwd(state.root_dir);

  state.root_node = fs_make_empty_node();
  assert(state.root_node);

  /* Set up the PLASH_FAKE_{E,}[UG]ID environment variables so that the
     process sees a sensible UID.  This is done before processing the
     arguments so that it can be overridden with --env. */
  if(!under_plash) {
    char buf[20];
    snprintf(buf, sizeof(buf), "%i", getuid());
    setenv("PLASH_FAKE_UID", buf, 1);
    setenv("PLASH_FAKE_EUID", buf, 1);
    snprintf(buf, sizeof(buf), "%i", getgid());
    setenv("PLASH_FAKE_GID", buf, 1);
    setenv("PLASH_FAKE_EGID", buf, 1);
  }
  
  if(handle_arguments(r, &state, 1, argc, argv)) { return 1; }

  if(!state.executable_filename) {
    fprintf(stderr, NAME_MSG _("--prog argument missing, no executable specified\n"));
    return 1;
  }
  int executable_fd;
  const char *executable_filename2; /* of interpreter for #! scripts */
  const char **args_array, **args_array2;
  int args_count2;
  int socks[2];
  int pid;
  struct filesys_obj *obj;

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

  cap_t child_root;
  struct dir_stack *child_cwd = NULL;
  int cap_count;
  cap_t *caps;
  const char *cap_names;

  child_root = fs_make_root(state.root_node);
  assert(child_root);

  if(0) { fs_print_tree(0, state.root_node); }

  /* Set up cwd.  If this fails, the cwd is left undefined. */
  if(state.cwd) {
    seqf_t cwd_path = flatten(r, string_of_cwd(r, state.cwd));
    int err;
    child_cwd = resolve_dir(r, child_root, NULL /* cwd */, cwd_path,
			    SYMLINK_LIMIT, &err);
  }

  if(state.search_path) {
    /* Look up executable name in PATH. */
    if(resolve_executable_name(r, child_root, child_cwd,
			       state.executable_filename,
			       &state.executable_filename) < 0) {
      fprintf(stderr, NAME_MSG _("executable not found: %s\n"),
	      state.executable_filename);
      return 1;
    }
  }

  obj = resolve_file(r, child_root, child_cwd,
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

  /* Handle scripts using the `#!' syntax. */
  /* This resolves the interpreter name in the callee's namespace. */
  if(exec_for_scripts(r, child_root, child_cwd,
		      state.executable_filename, executable_fd,
		      state.args_count + 1, args_array,
		      NULL,
		      &executable_filename2,
		      &args_count2, &args_array2, &err) < 0) {
    fprintf(stderr, NAME_MSG _("bad interpreter: %s: %s\n"),
	    state.executable_filename, strerror(err));
    return 1;
  }

  // "fs_op;conn_maker;fs_op_maker;union_dir_maker"
  if(state.powerbox) {
    cap_names = "fs_op;fs_op_maker;conn_maker;powerbox_req_filename";
    cap_count = 4;
  }
  else {
    cap_names = "fs_op;fs_op_maker;conn_maker";
    cap_count = 3;
  }
  caps = region_alloc(r, cap_count * sizeof(cap_t));
  if(state.log) inc_ref(state.log);
  caps[0] = make_fs_op_server(state.log, child_root, child_cwd);
  caps[1] = fs_op_maker_make(state.log);
  caps[2] = conn_maker_make();
  state.log = NULL;
  if(state.powerbox) {
#ifdef USE_GTK
    caps[3] = powerbox_make(state.pet_name,
			    inc_ref(state.root_dir),
			    (struct node *) inc_ref((cap_t) state.root_node));
#else
    assert(0);
#endif
  }

  filesys_obj_free(state.root_dir);
  if(state.cwd) { dir_stack_free(state.cwd); }

  free_node(state.root_node);

  /* Do I want to do a double fork so that the server process is no
     longer a child of the client process?  The client process might
     not be expecting to handle SIGCHILD signals.  Or perhaps these
     aren't delivered after an exec() call. */
  pid = fork();
  if(pid < 0) {
    perror(NAME_MSG "fork");
    return 1; /* Error */
  }
  if(state.server_as_parent ? pid > 0 : pid == 0) {
    close(socks[0]);

    cap_make_connection(r, socks[1], cap_seq_make(caps, cap_count),
			0, "to-client");
    caps_free(cap_seq_make(caps, cap_count));
    region_free(r);

    server_process(argc, argv, state.powerbox /* use_gtk */);
    exit(0);
  }
  cap_close_all_connections();

  close(socks[1]);

  {
    char buf[20];
    snprintf(buf, sizeof(buf), "%i", socks[0]);
    if(setenv("PLASH_COMM_FD", buf, 1) < 0 ||
       setenv("PLASH_CAPS", cap_names, 1) < 0) {
      fprintf(stderr, NAME_MSG _("setenv failed\n"));
      return 1;
    }
  }

  const char *cmd;
  if(args_to_exec_elf_program(r, &state, executable_filename2,
			      args_count2, args_array2,
			      &cmd, &args_count2, &args_array2,
			      state.debug) < 0)
    return 1;
  if(under_plash) {
    __typeof__(plash_libc_kernel_execve) *kernel_execve =
      dlsym(RTLD_NEXT, "plash_libc_kernel_execve");
    assert(kernel_execve != NULL);
    kernel_execve(cmd, (char **) args_array2, environ);
  }
  else {
    execve(cmd, (char **) args_array2, environ);
  }
  fprintf(stderr, NAME_MSG "execve: %s: %s\n", cmd, strerror(errno));
  return 1;
}
