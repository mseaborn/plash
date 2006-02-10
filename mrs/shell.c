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

/* Needed for strsignal() */
#define _GNU_SOURCE

#ifdef USE_GTK
#include <gtk/gtk.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <pwd.h>
#include <termios.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "region.h"
#include "server.h"
#include "config.h"
#include "shell-variants.h"
#include "shell.h"
#include "shell-fds.h"
#include "shell-wait.h"
#include "build-fs.h"
#include "shell-globbing.h"
#include "cap-protocol.h"
#include "cap-utils.h"
#include "fs-operations.h"
#include "log-proxy.h"
#include "reconnectable-obj.h"
#include "serialise.h"


int f_command(const char **err_pos, region_t r,
	      const char *pos_in1, const char *end,
	      const char **pos_out_p, void **ok_val_p);
int f_command_list(const char **err_pos, region_t r,
		   const char *pos_in1, const char *end,
		   const char **pos_out_p, void **ok_val_p);


const char *shell_pathname = 0;


#define ST_RUNNING 0
#define ST_STOPPED 1
#define ST_FINISHED 2
struct shell_process {
  int pid; /* Can be 0 for processes created through object invocations */
  int state;
  int wait_status; /* Filled out when stopped or finished */
  struct shell_process *next;
};

struct job {
  int id;
  struct shell_process *processes;
  int pgid;
  struct termios tmodes; /* Saved terminal state */
  int last_state;
  struct job *prev, *next;
};

struct env_list {
  char *name; /* malloc'd, owned */
  cap_t obj;
  struct env_list *next;
};

struct shell_state {
  struct filesys_obj *root;
  struct dir_stack *cwd;
  struct env_list *env;

  /* Make a connection to the central server */
  cap_t conn_maker;
  /* Make connections to the current process.  The object stays local
     to the server processes that are forked off, and it is used to make
     connections to these. */
  cap_t conn_maker_local;
  /* Not used: */
  cap_t union_dir_maker;
  cap_t fab_dir_maker;

  const char *prompt;

  int interactive;
  int fork_server_per_job;
  int next_job_id;
  struct job jobs;
  struct termios tmodes; /* Saved terminal state */

  int log_summary;
  int log_messages;
  int log_into_xterm;
  int print_fs_tree;
};

struct process_desc_fork {
  /* Set the new process's process group ID to pgid.
     If pgid is 0, use the process's pid as the pgid.
     If pgid is -1, don't set the process group ID. */
  int pgid;
  int foreground;
  int reset_signals;
};

/* Used for starting processes */
/* This is all allocated into a region, including FDs, which have
   finalisers added to the region. */
/* This struct is extended by `process_desc_sec' and `process_desc_nosec'. */
struct process_desc {
  /* This function is run inside the forked process. */
  void (*set_up_process)(struct process_desc *desc);
  const char *cmd;
  int argc; /* Not really needed */
  const char **argv; /* Array needs to include null terminator */
  fds_t fds;
};

struct process_spawn_list {
  /* This function spawns a process, either by fork-and-exec, or by an
     object invocation.  `p' will get modified later when the process
     exits or stops.  `p->pid' is also optionally filled out. */
  void (*spawn_f)(void *spawn_h, struct process_desc_fork *f,
		  struct shell_process *p);
  void *spawn_h;
  struct process_spawn_list *next;
};

/* A server description is a list of connections to open, giving objects
   to export on each connection. */
/* A connection may also import objects.  An array of pointers to cap_t
   is given: the imported objects are assigned to these locations.
   This allows the server to export objects that it doesn't provide itself.
   It is used to pass a return capability (located in the shell) to
   the client, connected through the server.
   The `import' array can be used to assign to `server_desc' elements
   later in the sequence. */
struct server_desc {
  int sock_fd;
  cap_seq_t export;
  cap_t **import;
  int import_count;
  struct server_desc *next;
};


/* This deletes data structures associated with the shell.  It is used
   when forking a server process which no longer needs the shell's data.
   Currently, all it deletes is the references in the environment, so
   that these are not imported by server processes (via the
   reconnectable_obj mechanism). */
void shell_finalise(struct shell_state *state)
{
  struct env_list *env;
  for(env = state->env; env; env = env->next) filesys_obj_free(env->obj);
}


cap_t eval_expr(struct shell_state *state, struct shell_expr *expr);

/* Returns 0 if no binding is found. */
/* Does not return an owning reference. */
cap_t env_lookup(struct env_list *env, const char *name)
{
  while(env) {
    if(!strcmp(env->name, name)) return env->obj;
    env = env->next;
  }
  return 0;
}

/* Will copy `name' if necessary.  `obj' is an owned reference. */
void env_bind(struct env_list **env_slot, const char *name, cap_t obj)
{
  struct env_list *env = *env_slot;
  while(env) {
    if(!strcmp(env->name, name)) {
      filesys_obj_free(env->obj);
      env->obj = obj;
      return;
    }
    env = env->next;
  }
  env = amalloc(sizeof(struct env_list));
  env->name = strdup(name);
  env->obj = obj;
  env->next = *env_slot;
  *env_slot = env;
}


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

struct flatten_params {
  struct shell_state *state;
  struct str_list **got_end;
  struct filesys_obj *root_dir;
  struct dir_stack *cwd; /* This may be null */
  /* This is zero if we're not constructing a filesystem.
     Used for the `!!' syntax. */
  struct node *tree;
  int fds_allowed;
  struct fd_array fds;
};

/* Returns 0 if ok, non-zero for an error. */
int flatten_args(region_t r, struct flatten_params *p,
		 int rw, int ambient, struct arg_list *args)
{
  if(m_arg_empty(args)) return 0;
  {
    struct arg_list *a1, *a2;
    if(m_arg_cat(args, &a1, &a2)) {
      return
	flatten_args(r, p, rw, ambient, a1) ||
	flatten_args(r, p, rw, ambient, a2);
    }
  }
  {
    struct arg_list *a;
    if(m_arg_read(args, &a)) {
      return flatten_args(r, p, 0, ambient, a);
    }
  }
  {
    struct arg_list *a;
    if(m_arg_write(args, &a)) {
      return flatten_args(r, p, 1, ambient, a);
    }
  }
  {
    struct arg_list *a;
    if(m_arg_ambient(args, &a)) {
      if(p->tree) return flatten_args(r, p, rw, 1, a);
      else {
	printf("plash: ambient args list ignored\n");
	return 0;
      }
    }
  }
  {
    struct char_cons *f;
    if(m_arg_filename(args, &f)) {
      seqf_t filename = tilde_expansion(r, flatten_charlist(r, f));
      int err;

      if(p->tree) {
	if(resolve_populate(p->root_dir, p->tree, p->cwd,
			    filename, rw /* create */, &err) < 0) {
	  printf("plash: error in resolving filename `");
	  fprint_d(stdout, filename);
	  printf("'\n");
	}
      }
      if(!ambient) {
	struct str_list *l = region_alloc(r, sizeof(struct str_list));
	l->str = (char *) filename.data;
	l->next = 0;
	*p->got_end = l;
	p->got_end = &l->next;
      }
      return 0;
    }
  }
  {
    struct glob_path *f;
    if(m_arg_glob_filename(args, &f)) {
      seqt_t filename1;
      if(filename_of_glob_path(r, f, &filename1) >= 0) {
	seqf_t filename = flatten0(r, filename1);
	int err;

	if(p->tree) {
	  if(resolve_populate(p->root_dir, p->tree, p->cwd,
			      filename, rw /* create */, &err) < 0) {
	    printf("plash: error in resolving filename `");
	    fprint_d(stdout, filename);
	    printf("'\n");
	  }
	}
	if(!ambient) {
	  struct str_list *l = region_alloc(r, sizeof(struct str_list));
	  l->str = (char *) filename.data;
	  l->next = 0;
	  *p->got_end = l;
	  p->got_end = &l->next;
	}
	return 0;
      }
      else {
	struct glob_params gp;
	struct str_list *args_got = 0, *l;
	int err;
      
	gp.got_end = &args_got;
	glob_resolve(r, p->root_dir, p->cwd, f, &gp);

	if(p->tree) {
	  for(l = args_got; l; l = l->next) {
	    if(resolve_populate(p->root_dir, p->tree, p->cwd,
				seqf_string(l->str), rw /* create */, &err) < 0) {
	      /* This error shouldn't happen unless the filesystem changes
		 underneath us. */
	      printf("plash: error in resolving globbed filename `%s'\n", l->str);
	    }
	  }
	}
	if(args_got) {
	  if(!ambient) {
	    /* This only works if the list to be added is non-empty. */
	    *p->got_end = args_got;
	    p->got_end = gp.got_end;
	  }
	}
	else {
	  printf("plash: glob pattern matched nothing\n");
	}
	return 0;
      }
    }
  }
  {
    struct char_cons *s;
    if(m_arg_string(args, &s)) {
      seqf_t string = flatten_charlist(r, s);
      if(!ambient) {
	struct str_list *l = region_alloc(r, sizeof(struct str_list));
	l->str = (char*) string.data;
	l->next = 0;
	*p->got_end = l;
	p->got_end = &l->next;
      }
      else {
	printf("warning: string argument \"%s\" in ambient arg list (ignored)\n", string.data);
      }
      return 0;
    }
  }
  {
    struct char_cons *fd;
    int type;
    struct redir_dest *dest;
    if(m_arg_redirection(args, &fd, &type, &dest)) {
      struct char_cons *dest_fd, *dest_file;
      int fd_no, fd_dest;
      
      if(!p->fds_allowed) {
	printf("fd redirection not allowed in this context\n");
	return 1; /* Error */
      }
      
      if(fd) {
	seqf_t string = flatten_charlist(r, fd);
	fd_no = atoi(string.data);
      }
      else {
	switch(type) {
	  case REDIR_IN: fd_no = 0; break;
	  case REDIR_OUT_TRUNC: fd_no = 1; break;
	  case REDIR_OUT_APPEND: fd_no = 1; break;
	  case REDIR_IN_OUT: fd_no = 0; /* Odd, but it's what bash does */ break;
	  default: assert(0); return 1;
	}
      }
      
      if(m_dest_fd(dest, &dest_fd)) {
	/* The redirection type (>, >> or <) is ignored in this case. */
	/* FIXME: check FD, as bash does, to see whether it's opened
	   for reading/writing, and it least warn if not. */
	seqf_t string = flatten_charlist(r, dest_fd);
	int i = atoi(string.data);
	if(i < p->fds.count && p->fds.fds[i] >= 0) {
	  fd_dest = p->fds.fds[i];
	}
	else {
	  printf("plash: file descriptor %i not open\n", i);
	  return 1;
	}
      }
      else if(m_dest_file(dest, &dest_file)) {
	seqf_t filename = flatten_charlist(r, dest_file);
	int flags, err;
	switch(type) {
	  case REDIR_IN: flags = O_RDONLY; break;
	  case REDIR_OUT_TRUNC: flags = O_WRONLY | O_CREAT | O_TRUNC; break;
	  case REDIR_OUT_APPEND: flags = O_WRONLY | O_CREAT | O_APPEND; break;
	  case REDIR_IN_OUT: flags = O_RDWR | O_CREAT; break;
	  default: assert(0); return 1;
	}
	fd_dest = process_open(p->root_dir, p->cwd, filename,
			       flags, 0666, &err);
	if(fd_dest < 0) {
	  printf("plash: couldn't open `");
	  fprint_d(stdout, filename);
	  printf("' for redirection\n");
	  return 1;
	}
	region_add_finaliser(r, finalise_close_fd, (void *) fd_dest);
      }
      else { assert(0); return 1; }

      array_set_fd(r, &p->fds, fd_no, fd_dest);
      return 0;
    }
  }
  {
    struct char_cons *pathname1;
    struct shell_expr *expr;
    if(m_arg_fs_binding(args, &pathname1, &expr)) {
      if(!p->tree) {
	printf("plash: cannot re-arrange filesystem when invoking commands with `!!'\n");
	return 1; /* Error */
      }
      else {
	int err;
	seqf_t pathname = tilde_expansion(r, flatten_charlist(r, pathname1));
	cap_t x = eval_expr(p->state, expr);
	if(!x) return 1; /* Error */
	if(attach_at_pathname(p->tree, p->cwd, pathname, x, &err) < 0) {
	  printf("plash: %s\n", strerror(err));
	  return 1; /* Error */
	}
	if(!ambient) {
	  struct str_list *l = region_alloc(r, sizeof(struct str_list));
	  l->str = (char*) pathname.data;
	  l->next = 0;
	  *p->got_end = l;
	  p->got_end = &l->next;
	}
	return 0;
      }
    }
  }
  assert(0);
  return 1;
}

extern char **environ;

#if 0
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
  cmd2 = BIN_INSTALL "/run-as-anonymous";
  argv2[1] = BIN_INSTALL "/ld-linux.so.2";
  argv2[2] = "--library-path";
  argv2[3] = PLASH_LD_LIBRARY_PATH;
  argv2[4] = cmd;
  for(i = 1; i < argc; i++) argv2[extra_args+i] = argv[i];
  argv2[extra_args + argc] = 0;

  execve(cmd2, (char **) argv2, environ);
}
#endif

void args_to_exec_elf_program_from_fd
  (region_t r, int fd, int argc, const char **argv,
   const char **cmd_out, int *argc_out, const char ***argv_out)
{
  int extra_args = 6;
  int buf_size = 20;
  char *buf = region_alloc(r, buf_size);
  const char **argv2;
  int i;

  argv2 = region_alloc(r, (argc + extra_args + 1) * sizeof(char *));
  argv2[0] = argv[0];
#if 0
  *cmd_out = "/usr/bin/strace";
  argv2[1] = JAIL_DIR "/special/ld-linux.so.2";
#else
 #ifdef USE_CHROOT
  #if 1
  *cmd_out = BIN_INSTALL "/run-as-anonymous";
  #else
  *cmd_out = BIN_INSTALL "/run-as-nobody+chroot";
  #endif
  argv2[1] = "/special/ld-linux.so.2";
 #else
  *cmd_out = BIN_INSTALL "/run-as-nobody";
  argv2[1] = JAIL_DIR "/special/ld-linux.so.2";
 #endif
#endif
  argv2[2] = "--library-path";
  argv2[3] = PLASH_LD_LIBRARY_PATH;
  argv2[4] = "--fd";
  snprintf(buf, buf_size, "%i", fd);
  argv2[5] = buf;
  for(i = 0; i < argc; i++) argv2[extra_args+i] = argv[i];
  argv2[extra_args + argc] = 0;
  *argc_out = extra_args + argc;
  *argv_out = argv2;
}

int job_state(struct job *job)
{
  struct shell_process *p;
  int all_finished = 1;
  for(p = job->processes; p; p = p->next) {
    if(p->state == ST_RUNNING) return ST_RUNNING;
    if(p->state != ST_FINISHED) all_finished = 0;
  }
  if(all_finished) return ST_FINISHED;
  else return ST_STOPPED;
}

void shell_handle_process_status(void *x, int status)
{
  struct shell_process *p = x;
  p->state = WIFSTOPPED(status) ? ST_STOPPED : ST_FINISHED;
  p->wait_status = status;
}

/* Get info for all child processes that have exited or stopped, but
   don't block. */
void poll_for_processes()
{
  while(1) {
    int status;
    int pid = waitpid(-1, &status, WUNTRACED | WNOHANG);
    if(pid < 0 && errno != ECHILD) perror("plash: waitpid");
    if(pid > 0) w_handle_process_status(pid, status);
    else break;
  }
}

void remove_job(struct job *job)
{
  struct shell_process *p = job->processes;
  while(p) {
    struct shell_process *next = p->next;
    free(p);
    p = next;
  }
  job->next->prev = job->prev;
  job->prev->next = job->next;
  free(job);
}

void report_job_errors(struct job *job)
{
  struct shell_process *p;
  int i = 1;
  for(p = job->processes; p; p = p->next) {
    if(WIFEXITED(p->wait_status)) {
      int rc = WEXITSTATUS(p->wait_status);
      if(rc) {
	printf("plash: job %i: process #%i (pid %i) exited with status %i\n",
	       job->id, i, p->pid, rc);
      }
    }
    else if(WIFSIGNALED(p->wait_status)) {
      int sig = WTERMSIG(p->wait_status);
      const char *desc = strsignal(sig);
      printf("plash: job %i: process #%i (pid %i) died with signal %i (%s)\n",
	     job->id, i, p->pid, sig, desc ? desc : "unknown signal");
    }
    i++;
  }
}

/* Used when starting a job in the foreground, or when returning a job
   to the foreground. */
void wait_for_job(struct shell_state *state, struct job *job)
{
  while(job->last_state == ST_RUNNING) {
    int status, pid;

    pid = waitpid(-1, &status, WUNTRACED | WNOHANG);
    if(pid < 0 && errno != ECHILD) perror("plash: waitpid");
    if(pid > 0) w_handle_process_status(pid, status);
    else {
      /* This assumes SIGCHLD has been enabled by w_setup().  If not, this
	 will probably hang. */
      cap_run_server_step();
    }
    
    job->last_state = job_state(job);
  }
  /* We now know that we can continue, because the job isn't running.
     But there might be more information we can get from waitpid() that
     indicates whether it is stopped or finished. */
  poll_for_processes();
  job->last_state = job_state(job);

  /* Grab control of the terminal. */
  if(tcsetpgrp(STDIN_FILENO, getpid()) < 0) { perror("plash: tcsetpgrp"); }
  /* Save the terminal state. */
  if(tcgetattr(STDIN_FILENO, &job->tmodes) < 0) { perror("plash: tcgetattr"); }
  /* Restore the terminal state. */
  if(tcsetattr(STDIN_FILENO, TCSADRAIN, &state->tmodes) < 0) {
    perror("plash: tcsetattr");
  }

  if(job->last_state == ST_STOPPED) {
    printf("plash: job %i stopped\n", job->id);
  }
  else if(job->last_state == ST_FINISHED) {
    /* Don't print any message when a foreground job finishes normally. */
    report_job_errors(job);
    remove_job(job);
  }
  else {
    printf("plash: unknown job state (job %i)\n", job->id);
  }
}

void report_jobs(struct shell_state *state)
{
  struct job *job;
  for(job = state->jobs.next; job->id; ) {
    int state = job_state(job);
    if(job->last_state != state) {
      job->last_state = state;
      if(state == ST_STOPPED) {
	printf("plash: job %i stopped\n", job->id);
      }
      else if(state == ST_FINISHED) {
	struct job *next = job->next;
	printf("plash: job %i finished\n", job->id);
	report_job_errors(job);
	remove_job(job);
	job = next;
	goto next;
      }
      else {
	printf("plash: unknown job state (job %i)\n", job->id);
      }
    }
    job = job->next;
  next:
  }
}

/* Called in a newly-forked process. */
void set_up_forked_process(struct process_desc_fork *f)
{
  int pgid = f->pgid;
  /* Although setpgid() does this translation, tcsetpgrp() doesn't: */
  if(pgid == 0) pgid = getpid();

  if(pgid > 0) {
    if(setpgid(0, pgid) < 0) { perror("plash/child: setpgid"); }
  }
  if(f->foreground) {
    if(tcsetpgrp(STDIN_FILENO, pgid) < 0) { perror("plash/child: tcsetpgrp"); }
  }
  if(f->reset_signals) {
    /* Set the handling for job control signals back to the default. */
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
  }
}

/* Called by the parent of a forked process. */
void parent_set_up_forked_process(struct process_desc_fork *f, int pid)
{
  int pgid = f->pgid;
  if(pgid == 0) pgid = pid;

  /* Since the client is changed to run under a different user ID,
     setpgid below can fail on it.  It's a race condition.  If it
     fails here, the client has already called setpgid itself, so
     that's okay. */
  if(pgid > 0) {
    if(setpgid(pid, pgid) < 0) { /* perror("plash: setpgid"); */ }
  }
  /* Give the terminal to the new job. */
  if(f->foreground) {
    if(tcsetpgrp(STDIN_FILENO, pgid) < 0) { perror("plash: tcsetpgrp"); }
  }
}

/* Spawns a process. */
/* Returns process ID of spawned process, or -1 if there was an error. */
int spawn_process(struct process_desc *proc, struct process_desc_fork *f)
{
  int attach_gdb_to_client = 0;
  int attach_strace_to_client = 0;

  int pid = fork();
  if(pid == 0) {
    cap_close_all_connections();

    /* Setting which process group owns the terminal needs to be done
       before setting up the client's FDs. */
    set_up_forked_process(f);

    proc->set_up_process(proc);

    /* Some FDs in the process are close-on-exec. */
    /* Some FDs are refcounted, and these are closed by close_our_fds().
       We don't mark them as close-on-exec, because doing so would
       involve lots of needless system calls. */
    /* Unneeded FDs must be closed for security and correctness reasons. */

    /* This comes first.  I assume that FDs that are passed to the
       new process are not among the refcounted FDs closed by this. */
    close_our_fds();

    if(install_fds(proc->fds) < 0) exit(1); /* Error already printed */

    if(attach_strace_to_client || attach_gdb_to_client) {
      if(kill(getpid(), SIGSTOP) < 0) { perror("kill(SIGSTOP)"); }
    }

    execve(proc->cmd, (char **) proc->argv, environ);
    perror("plash: exec");
    exit(1);
  }
  if(pid < 0) { perror("plash: fork"); return -1; }
  parent_set_up_forked_process(f, pid);
  return pid;
}

DECLARE_VTABLE(d_conn_maker_vtable);
struct d_conn_maker {
  struct filesys_obj hdr;
  region_t r;
  struct server_desc *conns;
};

void d_conn_maker_free(struct filesys_obj *obj1)
{
  struct d_conn_maker *obj = (void *) obj1;
  if(obj->conns) fprintf(stderr, "plash: some connections never handled\n");
  region_free(obj->r);
}

int d_make_conn(struct filesys_obj *obj1, region_t r, cap_seq_t export,
		int import_count, cap_t **import)
{
  struct d_conn_maker *obj = (void *) obj1;
  int socks[2];
  if(import_count > 0) return -1;
  if(socketpair(AF_LOCAL, SOCK_STREAM, 0, socks) < 0) {
    perror("plash: socketpair");
    return -1; /* Error */
  }
  set_close_on_exec_flag(socks[0], 1);
  set_close_on_exec_flag(socks[1], 1);
  {
    int i;
    struct server_desc *desc = region_alloc(obj->r, sizeof(struct server_desc));
    desc->sock_fd = socks[0];
    for(i = 0; i < export.size; i++) inc_ref(export.caps[i]);
    desc->export = cap_seq_dup(obj->r, export);
    desc->import_count = 0;
    desc->import = 0;
    desc->next = obj->conns;
    obj->conns = desc;
  }
  return socks[1];
}

int d_make_conn2(struct filesys_obj *obj1, region_t r, int sock_fd,
		 cap_seq_t export, int import_count, cap_t **import)
{
  struct d_conn_maker *obj = (void *) obj1;
  if(import_count > 0) return -1;
  set_close_on_exec_flag(sock_fd, 1);
  {
    int i;
    struct server_desc *desc = region_alloc(obj->r, sizeof(struct server_desc));
    desc->sock_fd = sock_fd;
    for(i = 0; i < export.size; i++) inc_ref(export.caps[i]);
    desc->export = cap_seq_dup(obj->r, export);
    desc->import_count = 0;
    desc->import = 0;
    desc->next = obj->conns;
    obj->conns = desc;
  }
  return 0;
}

/* Returns pid, or 0 if no process needed to be started, or -1 if an
   error occurred. */
int fork_server_process(region_t sock_r, struct shell_state *shell_state,
			struct server_desc *server_desc,
			struct process_desc_fork *f,
			cap_t conn_maker)
{
  int attach_gdb_to_server = 0;

  region_t r;
  int count;
  struct reconnectable_obj *n, **r_objs;
  cap_t *caps;
  int i;
  cap_t *imports;
  int sock_fd;

  int pid;
  
  /* Don't bother starting the server if it will have no processes to
     handle.  This is just an optimisation to avoid forking, because
     if there are no processes the server would immediately exit. */
  if(!server_desc) return 0;

  /* Handle reconnectable objects. */
  r = region_make();
  count = 0;
  n = reconnectable_list.next;
  while(!n->head) { count++; n = n->next; }
  if(count > 0) {
    /* We need to take a copy of the linked list, because it might change
       after calling cap_close_all_connections(), which might free some
       objects and remove them from the list. */
    r_objs = region_alloc(r, count * sizeof(cap_t));
    caps = region_alloc(r, count * sizeof(cap_t));
    for(n = reconnectable_list.next, i = 0; !n->head; n = n->next, i++) {
      n->hdr.refcount++;
      r_objs[i] = n;
      caps[i] = n->x;
    }
    sock_fd = conn_maker->vtable->make_conn
		(conn_maker, r, cap_seq_make(caps, count),
		 0 /* import_count */, &imports);
    if(sock_fd < 0) {
      printf("plash: can't create connection\n");
      return -1;
    }
  }
  else {
    /* stops the compiler complaining */
    r_objs = 0;
    sock_fd = -1;
  }
  
  pid = fork();
  if(pid == 0) {
    cap_close_all_connections();

    /* Reconnect the reconnectable objects. */
    if(count > 0) {
      imports = cap_make_connection(r, sock_fd, caps_empty, count,
				    "to-single-server");
      for(i = 0; i < count; i++) {
	filesys_obj_free(r_objs[i]->x);
	r_objs[i]->x = imports[i];
	filesys_obj_free((cap_t) r_objs[i]);
      }
    }
    region_free(r);
    shell_finalise(shell_state);
    
    set_up_forked_process(f);

    if(attach_gdb_to_server) {
      if(kill(getpid(), SIGSTOP) < 0) { perror("kill(SIGSTOP)"); }
    }

    /* This is used to close FDs that were opened to be passed to the
       other processes. */
    region_free(sock_r);

    {
      region_t r = region_make();
      struct server_desc *desc;
      for(desc = server_desc; desc; desc = desc->next) {
	int i;
	cap_t *import =
	  cap_make_connection(r, desc->sock_fd, desc->export,
			      desc->import_count, "to-client");
	caps_free(desc->export);

	for(i = 0; i < desc->import_count; i++) {
	  assert(!*desc->import[i]);
	  *desc->import[i] = import[i];
	}
      }
      region_free(r);
    }
    cap_run_server();
    exit(0);
  }
  if(pid < 0) { perror("plash: fork"); return -1; }
  parent_set_up_forked_process(f, pid);

  /* Tidy up for reconnectable objects: */
  if(count > 0) {
    close(sock_fd);
    for(i = 0; i < count; i++) {
      filesys_obj_free((cap_t) r_objs[i]);
    }
  }
  region_free(r);

  /* Also free the server state, which includes lots of FDs. */
  {
    struct server_desc *desc;
    for(desc = server_desc; desc; desc = desc->next) {
      /* This is no good because the sequence can contain nulls: */
      /* caps_free(desc->export); */
      int i;
      for(i = 0; i < desc->export.size; i++) {
	if(desc->export.caps[i]) filesys_obj_free(desc->export.caps[i]);
      }
      
      if(close(desc->sock_fd) < 0) perror("plash: close");
    }
  }

  /* The problem with this is that gdb hangs if you attach it to a
     stopped process.  It doesn't try to restart the process. */
  if(attach_gdb_to_server) { printf("server is pid %i\n", pid); }
#if 0
  if(attach_gdb_to_server && 0) {
    int pid_gdb = fork();
    if(pid_gdb == 0) {
      char buf[10];
      set_up_process_for_job(state, job, foreground);
      snprintf(buf, sizeof(buf), "%i", pid_server);
      // execl("/usr/bin/gdb", "/usr/bin/gdb", shell_pathname, buf, 0);
      execl("/usr/bin/strace", "/usr/bin/strace", "/usr/bin/gdb", shell_pathname, buf, 0);
      perror("exec");
      exit(1);
    }
    if(pid_gdb < 0) perror("plash: fork");
    
    {
      struct shell_process *p = amalloc(sizeof(struct shell_process));
      p->pid = pid_gdb;
      p->state = ST_RUNNING;
      p->next = job->processes;
      job->processes = p;
    }
    /* There is a race condition.  gdb should send this signal itself. */
    //if(kill(pid_server, SIGCONT) < 0) { perror("kill(SIGCONT)"); }
  }
#endif

  return pid;
}

void spawn_process_for_job(void *spawn_h, struct process_desc_fork *f,
			   struct shell_process *p)
{
  struct process_desc *proc = spawn_h;
  int pid = spawn_process(proc, f);
  p->pid = pid;
  w_add_process(pid, shell_handle_process_status, p);
}

/* This frees region r in the server process. */
struct job *spawn_job(region_t sock_r,
		      struct shell_state *state,
		      struct server_desc *server_desc,
		      struct process_spawn_list *procs,
		      int foreground)
{
  struct process_desc_fork f;
  struct job *job = amalloc(sizeof(struct job));
  job->id = state->next_job_id++;
  job->processes = 0;
  job->pgid = 0; /* The pgid will be the same as the pid of the first process. */
  job->prev = &state->jobs;
  job->next = state->jobs.next;
  job->last_state = ST_RUNNING;
  state->jobs.next->prev = job;
  state->jobs.next = job;

  if(state->interactive) {
    /* Save the terminal state (needed for background jobs). */
    if(tcgetattr(STDIN_FILENO, &job->tmodes) < 0) { perror("plash: tcgetattr"); }
    f.pgid = 0; /* Filled out with job->pgid later */
    f.foreground = foreground;
    f.reset_signals = 1;
  }
  else {
    f.pgid = -1;
    f.foreground = 0;
    f.reset_signals = 0;
  }

  /* Start the server process. */
  /* This comes first so that it will allocate a pgid which can be used
     by processes started through object invocations. */
  {
    int pid_server = fork_server_process(sock_r, state, server_desc, &f,
					 state->conn_maker);
    /* Add server process ID to list. */
    if(pid_server > 0) {
      struct shell_process *p = amalloc(sizeof(struct shell_process));
      p->pid = pid_server;
      p->state = ST_RUNNING;
      p->next = job->processes;
      job->processes = p;
      w_add_process(pid_server, shell_handle_process_status, p);

      if(job->pgid == 0) {
	job->pgid = pid_server;
	f.pgid = pid_server;
      }
    }
  }

  /* Start the client processes. */
  for(; procs; procs = procs->next) {
    struct shell_process *p = amalloc(sizeof(struct shell_process));
    p->pid = 0; /* may get overwritten by spawn_f below */
    p->state = ST_RUNNING;
    p->next = job->processes;
    job->processes = p;
    procs->spawn_f(procs->spawn_h, &f, p);

    if(job->pgid == 0 && p->pid > 0) {
      job->pgid = p->pid;
      f.pgid = p->pid;
    }
  }
  
  region_free(sock_r);

#if 0
  if(attach_gdb_to_client) {
    int pid_gdb = fork();
    if(pid_gdb == 0) {
      char buf[10];
      snprintf(buf, sizeof(buf), "%i", pid_client);
      execl("/usr/bin/gdb", "/usr/bin/gdb", proc->cmd, buf, 0);
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
#endif

  return job;
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

/* Look up executable name in PATH if it doesn't contain '/'. */
/* Assumes filename argument has null terminator. */
/* Returns -1 if executable was not found. */
int resolve_executable(region_t r, seqf_t filename, seqf_t *result)
{
  if(strchr(filename.data, '/')) {
    *result = filename;
    return 0;
  }
  else {
    /* Search PATH for the command */
    seqf_t path;
    const char *path1 = getenv("PATH");
    if(!path1) return -1;
    path = seqf_string(path1);
    while(1) {
      seqf_t dir, full_cmd;
      if(!parse_path(path, &dir, &path)) return -1;
      full_cmd =
	flatten0(r, cat3(r, mk_leaf(r, dir),
			 mk_string(r, "/"),
			 mk_leaf(r, filename)));
      /* FIXME: don't use access() here */
      if(access(full_cmd.data, X_OK) == 0) {
	*result = full_cmd;
	return 0;
      }
    }
  }
}

struct job_cons_args {
  struct server_shared *shared;
  cap_t fs_op_maker;
  // struct server_desc *conns;
  /* Used to make connections between the forked client and the server.
     The server may be forked off specially, or may be pre-existing.
     This is invoked by the shell: */
  cap_t conn_maker;
  /* This is to be passed to the client, not invoked by the shell: */
  cap_t conn_maker_for_client;

  struct process_spawn_list *procs;
};

struct proc_return {
  struct filesys_obj hdr;
  struct shell_process *p;
};
DECLARE_VTABLE(proc_return_vtable);
void proc_return_free(struct filesys_obj *obj1)
{
  /* This handles the case in which, say, exec-object dies without
     reporting its child process's status. */
  struct proc_return *obj = (void *) obj1;
  if(obj->p->state != ST_FINISHED) {
    obj->p->state = ST_FINISHED;
    /* This is Linux-specific, because <sys/wait.h> doesn't offer a way
       to construct wait statuses.  This should be an exit with return
       code 1.  See <bits/waitstatus.h>. */
    obj->p->wait_status = 0x100;
  }
}
void proc_return_invoke(struct filesys_obj *obj1, struct cap_args args)
{
  struct proc_return *obj = (void *) obj1;
  region_t r = region_make();
  {
    seqf_t msg = flatten_reuse(r, args.data);
    int status;
    int ok = 1;
    m_str(&ok, &msg, "Okay");
    m_int(&ok, &msg, &status);
    m_end(&ok, &msg);
    if(ok && args.caps.size == 0 && args.fds.count == 0) {
      shell_handle_process_status(obj->p, status);
      region_free(r);
      return;
    }
  }
  
  caps_free(args.caps);
  close_fds(args.fds);
  region_free(r);
}

struct inv_process {
  cap_t exec_obj;
  cap_t root_dir;
  struct str_list *args_got;
  struct fd_array fds;
};
void spawn_inv_process(void *spawn_h, struct process_desc_fork *f,
		       struct shell_process *p)
{
  struct inv_process *obj = spawn_h;

  region_t r = region_make();
  argmkbuf_t argbuf = argbuf_make(r);
  bufref_t argv_arg, env_arg, root_arg, cwd_arg, fds_arg, spawn_args;
  int got_cwd;
  
  cap_t return_cont;
  {
    struct proc_return *c = amalloc(sizeof(struct proc_return));
    c->hdr.refcount = 1;
    c->hdr.vtable = &proc_return_vtable;
    c->p = p;
    return_cont = (cap_t) c;
  }

  /* Copy the arguments from the list into an array. */
  {
    bufref_t *a;
    int arg_count, i;
    struct str_list *l;
    
    for(l = obj->args_got, arg_count = 0; l; l = l->next) arg_count++;
    argv_arg = argmk_array(argbuf, arg_count+1, &a);
    a[0] = argmk_str(argbuf, mk_string(r, "none")); /* FIXME: argv[0] should be omitted */
    for(l = obj->args_got, i = 0; l; l = l->next, i++) {
      a[i+1] = argmk_str(argbuf, mk_string(r, l->str));
    }
  }

  {
    bufref_t *a;
    env_arg = argmk_array(argbuf, 0, &a);
  }

  root_arg = argmk_cap(argbuf, inc_ref(obj->root_dir));
  got_cwd = 0;

  {
    bufref_t *a;
    int i, j, fd_count = 0;
    for(i = 0; i < obj->fds.count; i++) {
      if(obj->fds.fds[i] >= 0) fd_count++;
    }
    fds_arg = argmk_array(argbuf, fd_count, &a);
    for(i = 0, j = 0; j < fd_count; i++) {
      assert(i < obj->fds.count);
      if(obj->fds.fds[i] >= 0) {
	int fd = dup(obj->fds.fds[i]);
	if(fd < 0) {
	  perror("dup");
	  argbuf_free_refs(argbuf);
	  return; /* Error */
	}
	a[j] = argmk_pair(argbuf, argmk_int(argbuf, i),
			  argmk_fd(argbuf, fd));
	j++;
      }
    }
    assert(j == fd_count);
  }

  {
    int i;
    bufref_t *a;
    spawn_args = argmk_array(argbuf, 4 + (got_cwd ? 1:0)
			               + (f->pgid > 0 ? 1:0), &a);
    a[0] = argmk_pair(argbuf,
		      argmk_str(argbuf, mk_string(r, "Argv")),
		      argv_arg);
    a[1] = argmk_pair(argbuf,
		      argmk_str(argbuf, mk_string(r, "Env.")),
		      env_arg);
    a[2] = argmk_pair(argbuf,
		      argmk_str(argbuf, mk_string(r, "Fds.")),
		      fds_arg);
    a[3] = argmk_pair(argbuf,
		      argmk_str(argbuf, mk_string(r, "Root")),
		      root_arg);
    i = 4;
    if(got_cwd) {
      a[i++] = argmk_pair(argbuf,
			  argmk_str(argbuf, mk_string(r, "Cwd.")),
			  cwd_arg);
    }
    if(f->pgid > 0) {
      a[i++] = argmk_pair(argbuf,
			  argmk_str(argbuf, mk_string(r, "Pgid")),
			  argmk_int(argbuf, f->pgid));
    }
  }
  cap_invoke(obj->exec_obj,
	     cap_args_make(cat4(r, mk_string(r, "Call"),
				mk_string(r, "Exeo"),
				mk_int(r, spawn_args),
				argbuf_data(argbuf)),
			   cap_seq_append(r, mk_caps1(r, return_cont),
					  argbuf_caps(argbuf)),
			   argbuf_fds(argbuf)));
  region_free(r);
}

int command_invocation_object(region_t r, struct shell_state *state,
			      struct job_cons_args *job,
			      cap_t exec_obj, struct arg_list *args,
			      int fd_stdin, int fd_stdout)
{
  struct flatten_params p;
  struct str_list *args_got;
  struct filesys_obj *root_slot;
  struct filesys_obj *root, *root_ref2;

  /* Process the arguments. */
  p.state = state;
  args_got = 0;
  p.got_end = &args_got;
  p.root_dir = state->root;
  p.cwd = state->cwd;
  p.tree = make_empty_node();
  p.fds_allowed = 1;
  p.fds.count = 3;
  p.fds.fds = region_alloc(r, p.fds.count * sizeof(int));
  p.fds.fds[0] = fd_stdin;
  p.fds.fds[1] = fd_stdout;
  p.fds.fds[2] = STDERR_FILENO;
  if(flatten_args(r, &p, 0 /* rw */, 0 /* ambient */, args)) {
    free_node(p.tree);
    return -1;
  }

  if(state->print_fs_tree) print_tree(0, p.tree);

  /* Create the root directory. */
  root_slot = build_fs(p.tree);
  free_node(p.tree);
  root = root_slot->vtable->slot_get(root_slot);
  assert(root);
  filesys_slot_free(root_slot);

  /* Make a connection between the forked server and the central server.
     We need to get a reference to the root directory that does not go
     via the shell process.
     NB. This will result in multiple connections between the two
     server per job if the job contains multiple processes; the
     duplication is unnecessary.
     NB. The reconnectable_obj mechanism already makes a connection
     between the forked server and central server. */
  {
    cap_t *import, *import2;
    int rc;
    int sock_fd =
      state->conn_maker->vtable->make_conn(state->conn_maker, r,
				   caps_empty /* export */,
				   1 /* import_count */, &import);
    if(sock_fd < 0) {
      filesys_obj_free(root);
      return -1;
    }
    root_ref2 = import[0];
    rc = job->conn_maker->vtable->make_conn2(job->conn_maker, r, sock_fd,
				     mk_caps1(r, root), 0 /* import_count */,
				     &import2);
    filesys_obj_free(root);
    if(rc < 0) {
      filesys_obj_free(root_ref2);
      return -1;
    }
  }

  {
    struct inv_process *proc;
    struct process_spawn_list *proc_list;

    proc = region_alloc(r, sizeof(struct inv_process));
    proc->exec_obj = exec_obj;
    proc->root_dir = root_ref2;
    proc->args_got = args_got;
    proc->fds = p.fds;

    proc_list = region_alloc(r, sizeof(struct process_spawn_list));
    proc_list->spawn_f = spawn_inv_process;
    proc_list->spawn_h = proc;
    proc_list->next = job->procs;
    job->procs = proc_list;

    region_add_finaliser(r, filesys_obj_free, root_ref2);
  }
  return 0;
}


struct process_desc_sec {
  struct process_desc d;
  int comm_fd;
  const char *caps_names;
};
void set_up_sec_process(struct process_desc *desc1) {
  struct process_desc_sec *desc = (void *) desc1;
  char buf[10];

  snprintf(buf, sizeof(buf), "%i", getuid());
  setenv("PLASH_FAKE_UID", buf, 1);
  setenv("PLASH_FAKE_EUID", buf, 1);
  snprintf(buf, sizeof(buf), "%i", getgid());
  setenv("PLASH_FAKE_GID", buf, 1);
  setenv("PLASH_FAKE_EGID", buf, 1);

  snprintf(buf, sizeof(buf), "%i", desc->comm_fd);
  setenv("PLASH_COMM_FD", buf, 1);
  setenv("PLASH_CAPS", desc->caps_names, 1);

  /* Necessary for security when using run-as-nobody, otherwise the
     process can be left in a directory with read/write access which
     might not be reachable from the root directory. */
  /* Not necessary for run-as-nobody+chroot. */
  if(chdir("/") < 0) { perror("plash: chdir"); exit(1); }
}

/* Constructs a filesystem for the process being created for the command
   invocation.  Adds a process entry to the server state to handle this
   filesystem. */
/* When starting a process in order for it to return an object as a result,
   `return_cont' can be filled out; otherwise it can be null.
   `return_cont' is passed as an owning reference. */
int command_invocation_sec
  (region_t r, struct shell_state *state, struct job_cons_args *job,
   seqf_t cmd_filename, struct arg_list *args,
   int fd_stdin, int fd_stdout,
   cap_t return_cont)
{
  struct flatten_params p;
  struct str_list *args_got, *l;
  int i;
  int arg_count;
  const char **argv;
  int executable_fd;
  int argc2;
  const char **argv2;
  int executable_fd2;
  struct filesys_obj *root_slot;
  struct filesys_obj *root;
  struct dir_stack *cwd;
  int err;

  /* Process the arguments. */
  p.state = state;
  args_got = 0;
  p.got_end = &args_got;
  p.root_dir = state->root;
  p.cwd = state->cwd;
  p.tree = make_empty_node();
  p.fds_allowed = 1;
  p.fds.count = 3;
  p.fds.fds = region_alloc(r, p.fds.count * sizeof(int));
  p.fds.fds[0] = fd_stdin;
  p.fds.fds[1] = fd_stdout;
  p.fds.fds[2] = STDERR_FILENO;
  if(flatten_args(r, &p, 0 /* rw */, 0 /* ambient */, args)) {
    free_node(p.tree);
    if(return_cont) filesys_obj_free(return_cont);
    return -1;
  }

  /* FIXME: check for errors */
  resolve_populate(state->root, p.tree, p.cwd, seqf_string("/etc"), 0 /* create */, &err);
  resolve_populate(state->root, p.tree, p.cwd, seqf_string("/bin/"), 0 /* create */, &err);
  resolve_populate(state->root, p.tree, p.cwd, seqf_string("/lib"), 0 /* create */, &err);
  resolve_populate(state->root, p.tree, p.cwd, seqf_string("/usr"), 0 /* create */, &err);
  resolve_populate(state->root, p.tree, p.cwd, seqf_string("/dev/tty"), 1 /* create */, &err);
  resolve_populate(state->root, p.tree, p.cwd, seqf_string("/dev/null"), 1 /* create */, &err);

  /* Add the executable:  This is necessary for scripts (using the
     `#!' syntax).  It doesn't hurt for other executables. */
  resolve_populate(state->root, p.tree, p.cwd, cmd_filename, 0 /* create */, &err);

  /* Open the executable file. */
  {
    region_t r = region_make();
    int err;
    struct filesys_obj *obj;
    obj = resolve_file(r, state->root, state->cwd, cmd_filename,
		       SYMLINK_LIMIT, 0 /* nofollow */, &err);
    region_free(r);
    if(!obj) { errno = err; perror("plash: open/exec"); return -1; }
    executable_fd = obj->vtable->open(obj, O_RDONLY, &err);
    filesys_obj_free(obj);
    if(executable_fd < 0) { errno = err; perror("plash: open/exec"); return -1; }
  }

  if(state->print_fs_tree) print_tree(0, p.tree);

  /* Create the root directory. */
  root_slot = build_fs(p.tree);
  free_node(p.tree);
  root = root_slot->vtable->slot_get(root_slot);
  assert(root);
  filesys_obj_free(root_slot);

  /* Set the process's cwd. */
  if(state->cwd) {
    region_t r = region_make();
    seqf_t cwd_path = flatten(r, string_of_cwd(r, state->cwd));
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
    cwd = resolve_dir(r, root, 0 /* cwd */, cwd_path, SYMLINK_LIMIT, &err);
    if(state->print_fs_tree) {
      if(cwd) printf("plash: cwd set successfully\n");
      else printf("plash: cwd left undefined for this process\n");
    }
    region_free(r);
  }
  else cwd = 0;

  /* Copy the arguments from the list into an array. */
  for(l = args_got, arg_count = 0; l; l = l->next) arg_count++;
  argv = region_alloc(r, (arg_count+1) * sizeof(char*));
  argv[0] = cmd_filename.data;
  for(l = args_got, i = 0; l; l = l->next, i++) argv[i+1] = l->str;

  /* Handle scripts using the `#!' syntax. */
  if(exec_for_scripts(r, root, cwd,
		      cmd_filename.data, executable_fd, arg_count + 1, argv,
		      &executable_fd2, &argc2, &argv2, &err) < 0) {
    errno = err;
    perror("plash: bad interpreter");
    return -1;
  }
  region_add_finaliser(r, finalise_close_fd, (void *) executable_fd2);

  /* Construct the connection to the server. */
  {
    const char *caps_names;
    int sock_fd; /* client's connection to server */
    int cap_count = 5 + (return_cont ? 1:0);
    cap_t *imports; /* not used */
    cap_t *caps = region_alloc(r, cap_count * sizeof(cap_t));
    job->shared->refcount++;
    caps[0] = make_fs_op_server(job->shared, root, cwd);
    caps[1] = inc_ref(job->conn_maker_for_client);
    caps[2] = inc_ref(job->fs_op_maker);
    caps[3] = inc_ref(state->union_dir_maker);
    caps[4] = inc_ref(state->fab_dir_maker);
    if(return_cont) {
      // caps[5] = 0; /* Filled out by next desc */
      caps[5] = return_cont;
      caps_names = "fs_op;conn_maker;fs_op_maker;union_dir_maker;fab_dir_maker;return_cont";
    }
    else {
      caps_names = "fs_op;conn_maker;fs_op_maker;union_dir_maker;fab_dir_maker";
    }
    if(0) {
      int i;
      for(i = 0; i < cap_count; i++) {
	/* Don't proxy return_cont. */
	if(i != 5) caps[i] = make_log_proxy(caps[i]);
      }
    }
    sock_fd = job->conn_maker->vtable->make_conn(job->conn_maker, r,
				cap_seq_make(caps, cap_count),
				0 /* import_count */, &imports);
    caps_free(cap_seq_make(caps, cap_count));
    if(sock_fd < 0) {
      fprintf(stderr, "plash: can't make connection\n");
      return -1;
    }
    region_add_finaliser(r, finalise_close_fd, (void *) sock_fd);

#if 0
    /* Connect the client to the shell via the forked server. */
    if(return_cont) {
      int socks2[2];
      struct server_desc *desc2;
      if(socketpair(AF_LOCAL, SOCK_STREAM, 0, socks2) < 0) {
	perror("plash: socketpair");
	return -1; /* Error */
      }
      set_close_on_exec_flag(socks2[0], 1);
      set_close_on_exec_flag(socks2[1], 1);
      desc2 = region_alloc(r, sizeof(struct server_desc));
      desc2->sock_fd = socks2[0];
      desc2->export = caps_empty;
      desc2->import_count = 1;
      desc2->import = region_alloc(r, sizeof(cap_t*));
      desc2->import[0] = &caps[5];
      desc2->next = job->conns;
      job->conns = desc2;

      cap_make_connection(r, socks2[1], mk_caps1(r, return_cont), 0,
			  "to-forked-server");
    }
#endif
    
    {
      int proc_sock_fd, proc_exec_fd;
      struct process_desc_sec *proc;
      struct process_spawn_list *proc_list;
	
      proc_sock_fd = array_get_free_index(&p.fds);
      array_set_fd(r, &p.fds, proc_sock_fd, sock_fd);
      proc_exec_fd = array_get_free_index(&p.fds);
      array_set_fd(r, &p.fds, proc_exec_fd, executable_fd2);
	
      proc = region_alloc(r, sizeof(struct process_desc_sec));
      proc->d.fds.count = p.fds.count;
      proc->d.fds.fds = p.fds.fds;
      proc->comm_fd = proc_sock_fd;
      proc->caps_names = caps_names;
      args_to_exec_elf_program_from_fd
	(r, proc_exec_fd /* executable_fd */, argc2, argv2,
	 &proc->d.cmd, &proc->d.argc, &proc->d.argv);
      proc->d.set_up_process = set_up_sec_process;

      proc_list = region_alloc(r, sizeof(struct process_spawn_list));
      proc_list->spawn_f = spawn_process_for_job;
      proc_list->spawn_h = (struct process_desc *) proc;
      proc_list->next = job->procs;
      job->procs = proc_list;
      return 0;
    }
  }
}

struct process_desc_nosec {
  struct process_desc d;
  /* For processes started the conventional way, we need to set the cwd
     to the shell's logical cwd. */
  int cwd_fd;
};
void set_up_nosec_process(struct process_desc *desc1) {
  struct process_desc_nosec *desc = (void *) desc1;
  if(fchdir(desc->cwd_fd) < 0) {
    /* This should fail, because it might be dangerous if the command
       is run in the wrong directory, eg. "rm -rv ." */
    perror("plash: fchdir");
    exit(1);
  }
}

int command_invocation_nosec
  (region_t r, struct shell_state *state, struct job_cons_args *job,
   seqf_t cmd_filename, struct arg_list *args,
   int fd_stdin, int fd_stdout)
{
  struct flatten_params p;
  struct str_list *args_got, *l;
  int arg_count;
  const char **argv;
  int i;
  struct process_desc_nosec *proc;
  struct process_spawn_list *proc_list;

  /* Process the arguments. */
  p.state = state;
  args_got = 0;
  p.got_end = &args_got;
  p.root_dir = state->root;
  p.cwd = state->cwd;
  p.tree = 0;
  p.fds_allowed = 1;
  p.fds.count = 3;
  p.fds.fds = region_alloc(r, p.fds.count * sizeof(int));
  p.fds.fds[0] = fd_stdin;
  p.fds.fds[1] = fd_stdout;
  p.fds.fds[2] = STDERR_FILENO;
  if(flatten_args(r, &p, 0 /* rw */, 0 /* ambient */, args)) return -1;

  /* Copy the arguments from the list into an array. */
  for(l = args_got, arg_count = 0; l; l = l->next) arg_count++;
  argv = region_alloc(r, (arg_count+2) * sizeof(char*));
  argv[0] = cmd_filename.data;
  for(l = args_got, i = 0; l; l = l->next, i++) argv[i+1] = l->str;
  argv[arg_count+1] = 0;

  proc = region_alloc(r, sizeof(struct process_desc_nosec));
  proc->d.set_up_process = set_up_nosec_process;
  if(state->cwd && state->cwd->dir->vtable == &real_dir_vtable) {
    struct real_dir *dir = (void *) state->cwd->dir;
    if(dir->fd) {
      proc->cwd_fd = dir->fd->fd;
    }
    else {
      printf("plash: current directory does not have valid file descriptor\n");
      return -1;
    }
  }
  else {
    printf("plash: current directory is not real\n");
    return -1;
  }
  proc->d.cmd = cmd_filename.data;
  proc->d.argc = arg_count + 1;
  proc->d.argv = argv;
  proc->d.fds.fds = p.fds.fds;
  proc->d.fds.count = p.fds.count;

  proc_list = region_alloc(r, sizeof(struct process_spawn_list));
  proc_list->spawn_f = spawn_process_for_job;
  proc_list->spawn_h = (struct process_desc *) proc;
  proc_list->next = job->procs;
  job->procs = proc_list;
  return 0;
}

int command_invocation
  (region_t r, struct shell_state *state,
   struct job_cons_args *job, struct invocation *inv,
   int fd_stdin, int fd_stdout)
{
  int no_sec;
  struct char_cons *cmd_filename1;
  struct arg_list *args;

  if(m_invocation(inv, &no_sec, &cmd_filename1, &args)) {
    seqf_t cmd_filename2, cmd_filename3, cmd_filename;
    cap_t exec_obj;

    cmd_filename2 = flatten_charlist(r, cmd_filename1);
    exec_obj = env_lookup(state->env, cmd_filename2.data);
    if(exec_obj) {
      if(no_sec) {
	printf("plash: error: can't invoke a variable using `!!'\n");
	return -1;
      }
      else {
	return command_invocation_object(r, state, job, exec_obj, args,
					 fd_stdin, fd_stdout);
      }
    }

    cmd_filename3 = tilde_expansion(r, cmd_filename2);
    if(resolve_executable(r, cmd_filename3, &cmd_filename) < 0) {
      printf("plash: not found: %s\n", cmd_filename3.data);
      return -1;
    }

    if(no_sec) {
      return command_invocation_nosec(r, state, job, cmd_filename, args,
				      fd_stdin, fd_stdout);
    }
    else {
      return command_invocation_sec(r, state, job, cmd_filename, args,
				    fd_stdin, fd_stdout, 0 /* return_cont */);
    }
  }
  else {
    assert(0);
    return -1; /* Error */
  }
}

/* Returns -1 if there's an error. */
int pipeline_invocation
  (region_t r, struct shell_state *state,
   struct job_cons_args *job,
   struct pipeline *pipeline,
   int fd_stdin, int fd_stdout)
{
  struct invocation *inv;
  struct pipeline *rest;
  
  if(m_pipeline_inv(pipeline, &inv)) {
    return command_invocation(r, state, job, inv, fd_stdin, fd_stdout);
  }
  if(m_pipeline_cons(pipeline, &inv, &rest)) {
    int pipe_fd[2];
    if(pipe(pipe_fd) < 0) { perror("plash: pipe"); return 0; }
    set_close_on_exec_flag(pipe_fd[0], 1);
    set_close_on_exec_flag(pipe_fd[1], 1);

    region_add_finaliser(r, finalise_close_fd, (void *) pipe_fd[0]);
    region_add_finaliser(r, finalise_close_fd, (void *) pipe_fd[1]);

    if(command_invocation(r, state, job, inv, fd_stdin, pipe_fd[1]) < 0)
      return -1; /* Error */
    if(pipeline_invocation(r, state, job, rest, pipe_fd[0], fd_stdout) < 0)
      return -1; /* Error */
    return 0;
  }
  assert(0);
  return -1;
}


#ifdef USE_GTK
static int gtk_available = 0;

struct window_info {
  struct shell_state *state;
  GtkWidget *window;
  GtkWidget *o_log_summary;
  GtkWidget *o_log_messages;
  GtkWidget *o_log_into_xterm;
  GtkWidget *o_print_fs_tree;
};

void window_ok(GtkWidget *widget, gpointer data)
{
  struct window_info *w = (void *) data;
  w->state->log_summary = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->o_log_summary));
  w->state->log_messages = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->o_log_messages));
  w->state->log_into_xterm = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->o_log_into_xterm));
  w->state->print_fs_tree = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->o_print_fs_tree));
  gtk_widget_destroy(w->window);
}

gint window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
  return TRUE;
}

void window_destroy(GtkWidget *widget, gpointer data)
{
  gtk_main_quit();
}

void option_window(struct shell_state *state)
{
  struct window_info w;
  GtkWidget *widget;
  GtkWidget *vbox;
  GtkWidget *button;

  w.state = state;
  
  w.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(G_OBJECT(w.window), "delete_event",
		   G_CALLBACK(window_delete_event), NULL);
  g_signal_connect(G_OBJECT(w.window), "destroy",
		   G_CALLBACK(window_destroy), NULL);

  widget = gtk_check_button_new_with_label("Log calls, single line each");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), state->log_summary);
  gtk_widget_show(widget);
  w.o_log_summary = widget;

  widget = gtk_check_button_new_with_label("Log messages in/out");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), state->log_messages);
  gtk_widget_show(widget);
  w.o_log_messages = widget;

  widget = gtk_check_button_new_with_label("Send log data to new xterm");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), state->log_into_xterm);
  gtk_widget_show(widget);
  w.o_log_into_xterm = widget;

  widget = gtk_check_button_new_with_label("Print constructed filesystem tree");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), state->print_fs_tree);
  gtk_widget_show(widget);
  w.o_print_fs_tree = widget;
  
  button = gtk_button_new_with_label("Okay");
  gtk_widget_show(button);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(window_ok), &w);
  
  vbox = gtk_vbox_new(0, 5);
  gtk_box_pack_start(GTK_BOX(vbox), w.o_log_summary, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), w.o_log_messages, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), w.o_log_into_xterm, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), w.o_print_fs_tree, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);

  gtk_container_add(GTK_CONTAINER(w.window), vbox);
  gtk_widget_show(vbox);
  
  gtk_widget_show(w.window);
  gtk_main();
}
#endif

struct server_shared *make_server_shared(struct shell_state *state)
{
  struct server_shared *shared = amalloc(sizeof(struct server_shared));
  shared->refcount = 1;
  shared->next_id = 1;
  shared->log = 0;
  shared->log_summary = state->log_summary;
  shared->log_messages = state->log_messages;

  if(state->log_messages || state->log_summary) {
    if(state->log_into_xterm) {
      int pipe_fd[2];
      int pid;
      if(pipe(pipe_fd) < 0) {
	perror("plash: pipe");
      }
      set_close_on_exec_flag(pipe_fd[0], 1);
      set_close_on_exec_flag(pipe_fd[1], 1);
      pid = fork();
      if(pid == 0) {
	close_our_fds();
	dup2(pipe_fd[0], 3);
	execl("/usr/bin/X11/xterm",
	      "xterm", "-e", "sh", "-c", "less <&3", 0);
	perror("exec: xterm");
	exit(1);
      }
      if(pid < 0) perror("fork");
      close(pipe_fd[0]);
      shared->log = fdopen(pipe_fd[1], "w");
    }
    else {
      shared->log = fdopen(dup(STDERR_FILENO), "w");
    }
    assert(shared->log);
    setvbuf(shared->log, 0, _IONBF, 0);
  }

  return shared;
}

#include <signal.h>
void signal_handler(int sig)
{
  printf("sig %i\n", sig);
}

cap_t eval_expr(struct shell_state *state, struct shell_expr *expr)
{
  struct char_cons *var;
  
  struct char_cons *pathname1;

  struct char_cons *cmd_filename1;
  struct arg_list *args;

  if(m_expr_var(expr, &var)) {
    region_t r = region_make();
    seqf_t var2 = flatten_charlist(r, var);
    cap_t x = env_lookup(state->env, var2.data);
    if(x) inc_ref(x);
    else printf("plash: error: unbound variable, `%s'\n", var2.data);
    region_free(r);
    return x;
  }
  if(m_expr_filename(expr, &pathname1)) {
    region_t r = region_make();
    seqf_t pathname = tilde_expansion(r, flatten_charlist(r, pathname1));
    int err;
    cap_t x =
      resolve_obj_simple(state->root, state->cwd, pathname, SYMLINK_LIMIT,
			 0 /* nofollow */, &err);
    if(!x) {
      printf("plash: `");
      fprint_d(stdout, pathname);
      printf("': %s\n", strerror(err));
      region_free(r);
      return 0;
    }
    region_free(r);
    return x;
  }
  if(m_expr_mkfs(expr, &args)) {
    region_t r = region_make();
    struct flatten_params p;
    struct filesys_obj *root_slot, *root;
    
    p.state = state;
    p.got_end = 0; /* should not be used because ambient=1 */
    p.root_dir = state->root;
    p.cwd = 0;
    p.tree = make_empty_node();
    p.fds_allowed = 0;
    p.fds.fds = 0;
    p.fds.count = 0;
    if(flatten_args(r, &p, 0 /* rw */, 1 /* ambient */, args)) {
      /* Error already printed */
      free_node(p.tree);
      region_free(r);
      return 0;
    }

    root_slot = build_fs(p.tree);
    free_node(p.tree);
    root = root_slot->vtable->slot_get(root_slot);
    if(!root) printf("plash: error constructing directory\n");
    filesys_obj_free(root_slot);
    
    region_free(r);
    return root;
  }
  if(m_cap_cmd(expr, &cmd_filename1, &args)) {
    int rc;
    region_t r = region_make();
    region_t sock_r = region_make();
    seqf_t cmd_filename2, cmd_filename;
    struct job_cons_args job_cons;
    struct d_conn_maker *d_conn = 0;
    struct cap_args result;
    struct return_state ret_state;
    ret_state.r = r;
    ret_state.returned = 0;
    ret_state.result = &result;

    cmd_filename2 = tilde_expansion(r, flatten_charlist(r, cmd_filename1));
    if(resolve_executable(r, cmd_filename2, &cmd_filename) < 0) {
      printf("plash: not found: %s\n", cmd_filename2.data);
      return 0;
    }

    job_cons.shared = make_server_shared(state);
	
    job_cons.shared->refcount++;
    job_cons.fs_op_maker = fs_op_maker_make(job_cons.shared);

    if(state->fork_server_per_job) {
      d_conn = amalloc(sizeof(struct d_conn_maker));
      d_conn->hdr.refcount = 1;
      d_conn->hdr.vtable = &d_conn_maker_vtable;
      d_conn->r = region_make();
      d_conn->conns = 0;
      job_cons.conn_maker = (cap_t) d_conn;
      job_cons.conn_maker_for_client = inc_ref(state->conn_maker_local);
    }
    else {
      job_cons.conn_maker = inc_ref(state->conn_maker);
      job_cons.conn_maker_for_client = inc_ref(state->conn_maker);
    }
    job_cons.procs = 0;

    rc = command_invocation_sec(sock_r, state, &job_cons, cmd_filename, args,
				STDIN_FILENO, STDOUT_FILENO,
				make_reconnectable(make_return_cont(&ret_state)));
    server_shared_free(job_cons.shared);
    filesys_obj_free(job_cons.fs_op_maker);
    if(rc >= 0) {
      spawn_job(sock_r, state, d_conn ? d_conn->conns : 0, job_cons.procs,
		1 /* foreground */);
      if(d_conn) d_conn->conns = 0;
    }
    else {
      region_free(sock_r);
    }
    filesys_obj_free(job_cons.conn_maker);
    filesys_obj_free(job_cons.conn_maker_for_client);

    {
      struct sigaction act;
      act.sa_handler = signal_handler;
      act.sa_flags = 0;
      sigemptyset(&act.sa_mask);
      // sigaction(SIGCHLD, &act, 0);
    }

    while(!ret_state.returned) {
      if(!cap_run_server_step()) assert(0);
    }

    /* Grab control of the terminal. */
    if(tcsetpgrp(STDIN_FILENO, getpid()) < 0) { perror("plash: tcsetpgrp"); }
    /* Restore the terminal state. */
    if(tcsetattr(STDIN_FILENO, TCSADRAIN, &state->tmodes) < 0) {
      perror("plash: tcsetattr");
    }

    {
      cap_t c;
      if(expect_cap1(result, &c) < 0) {
	region_free(r);
	return 0;
      }
      region_free(r);
      return make_reconnectable(c);
    }
  }
  printf("plash: expr not handled\n");
  return 0;
}

void shell_command(region_t r, struct shell_state *state, struct command *command)
{
  {    
    struct char_cons *dir_filename1;
    if(m_chdir(command, &dir_filename1)) {
      seqf_t dir_filename =
	tilde_expansion(r, flatten_charlist(r, dir_filename1));
      int err;
      struct dir_stack *d =
	resolve_dir(r, state->root, state->cwd, dir_filename,
		    SYMLINK_LIMIT, &err);
      if(d) {
	if(state->cwd) dir_stack_free(state->cwd);
	state->cwd = d;
      }
      else {
	errno = err;
	perror("cd");
      }
      return;
    }
  }
  {
    struct pipeline *pipeline;
    int bg_flag;
    if(m_command(command, &pipeline, &bg_flag)) {

      struct invocation *inv;
      int no_sec;
      struct char_cons *cmd_filename1;
      struct arg_list *args;
      if(m_pipeline_inv(pipeline, &inv) &&
	 m_invocation(inv, &no_sec, &cmd_filename1, &args) &&
	 !no_sec &&
	 !strcmp("opts", flatten_charlist(r, cmd_filename1).data)) {
#ifdef USE_GTK
	if(gtk_available) {
	  printf("Opening options window... (close it to continue)\n");
	  option_window(state);
	}
	else printf("Gtk did not initialise successfully; no options window available\n");
#else
	printf("Gtk support not compiled in, so no options window available\n");
#endif
      }
      else {
	int rc;
	region_t sock_r = region_make();
	struct job_cons_args job_cons;
	struct d_conn_maker *d_conn = 0;

	job_cons.shared = make_server_shared(state);
	
	job_cons.shared->refcount++;
	job_cons.fs_op_maker = fs_op_maker_make(job_cons.shared);

	if(state->fork_server_per_job) {
	  d_conn = amalloc(sizeof(struct d_conn_maker));
	  d_conn->hdr.refcount = 1;
	  d_conn->hdr.vtable = &d_conn_maker_vtable;
	  d_conn->r = region_make();
	  d_conn->conns = 0;
	  job_cons.conn_maker = (cap_t) d_conn;
	  job_cons.conn_maker_for_client = inc_ref(state->conn_maker_local);
	}
	else {
	  job_cons.conn_maker = inc_ref(state->conn_maker);
	  job_cons.conn_maker_for_client = inc_ref(state->conn_maker);
	}
	job_cons.procs = 0;

	rc = pipeline_invocation(sock_r, state, &job_cons, pipeline,
				 STDIN_FILENO, STDOUT_FILENO);
	server_shared_free(job_cons.shared);
	filesys_obj_free(job_cons.fs_op_maker);
	if(rc >= 0) {
	  struct job *job;
	  /* Region sock_r is freed inside the forked server process
	     and before waiting for the child processes. */
	  job = spawn_job(sock_r, state,
			  d_conn ? d_conn->conns : 0,
			  job_cons.procs, !bg_flag);
	  if(d_conn) d_conn->conns = 0;
	  
	  if(bg_flag) {
	    if(state->interactive) {
	      printf("plash: job %i started\n", job->id);
	    }
	    /* Used to do remove_job(job) in non-interactive mode, but
	       that is no longer safe now that the wait handler has
	       references to struct shell_processes. */
	  }
	  else wait_for_job(state, job);
	}
	else {
	  region_free(sock_r);
	}
	filesys_obj_free(job_cons.conn_maker);
	filesys_obj_free(job_cons.conn_maker_for_client);
      }
      return;
    }
  }
  {
    struct char_cons *job_id_str;
    if(m_command_fg(command, &job_id_str)) {
      seqf_t arg = flatten_charlist(r, job_id_str);
      int job_id = atoi(arg.data);
      struct job *job;

      for(job = state->jobs.next; job->id; job = job->next) {
	if(job->id == job_id) {
	  if(job->last_state == ST_STOPPED) {
	    printf("plash: resuming job %i in foreground\n", job->id);
	  }
	  else {
	    printf("plash: putting job %i in foreground\n", job->id);
	  }

	  /* Restore the job's terminal state. */
	  if(tcsetattr(STDIN_FILENO, TCSADRAIN, &job->tmodes) < 0) {
	    perror("tcsetattr");
	  }
	  /* Put the job into the foreground. */
	  if(tcsetpgrp(STDIN_FILENO, job->pgid) < 0) { perror("tcsetpgrp"); }

	  if(job->last_state == ST_STOPPED) {
	    if(kill(-job->pgid, SIGCONT) < 0) {
	      perror("kill(SIGCONT)");
	    }
	  }
	  job->last_state = ST_RUNNING;

	  wait_for_job(state, job);
	  return;
	}
      }
      printf("plash: Unknown job ID: %i\n", job_id);
      return;
    }
  }
  {
    struct char_cons *job_id_str;
    if(m_command_bg(command, &job_id_str)) {
      seqf_t arg = flatten_charlist(r, job_id_str);
      int job_id = atoi(arg.data);
      struct job *job;

      for(job = state->jobs.next; job->id; job = job->next) {
	if(job->id == job_id) {
	  if(job->last_state == ST_RUNNING) {
	    printf("plash: job %i already in background\n", job->id);
	  }
	  if(job->last_state == ST_STOPPED) {
	    printf("plash: resuming job %i in background\n", job->id);
	    if(kill(-job->pgid, SIGCONT) < 0) {
	      perror("kill(SIGCONT)");
	    }
	    job->last_state = ST_RUNNING;
	  }
	  return;
	}
      }
      printf("plash: Unknown job ID: %i\n", job_id);
      return;
    }
  }
  {
    struct char_cons *var;
    struct shell_expr *expr;
    if(m_def_binding(command, &var, &expr)) {
      seqf_t var2 = flatten_charlist(r, var);
      cap_t c = eval_expr(state, expr);
      if(c) {
	env_bind(&state->env, var2.data, c);
	printf("plash: variable `%s' has been bound\n", var2.data);
      }
      else {
	printf("plash: did not bind variable `%s'\n", var2.data);
      }
      return;
    }
  }
  {
    struct char_cons *filename;
    if(m_command_source(command, &filename)) {
      seqf_t filename2 = flatten_charlist(r, filename);
      int fd;
      struct stat st;
      int size;
      char *data;
      int i;
      
      int err;
      cap_t obj = resolve_file(r, state->root, state->cwd, filename2,
			       SYMLINK_LIMIT, 0 /* nofollow */, &err);
      if(!obj) {
	printf("plash: error: can't resolve `%s': %s\n",
	       filename2.data, strerror(err));
	return;
      }
      fd = obj->vtable->open(obj, O_RDONLY, &err);
      filesys_obj_free(obj);
      if(fd < 0) {
	printf("plash: error: can't open `%s': %s\n",
	       filename2.data, strerror(err));
	return;
      }
      if(fstat(fd, &st) < 0) { err = errno; goto source_error; }
      size = st.st_size;
      data = region_alloc(r, size);
      i = 0;
      while(i < size) {
	int got = read(fd, data + i, size - i);
	if(got < 0) { err = errno; goto source_error; }
	if(got == 0) { err = EIO; goto source_error; }
	i += got;
      }
      close(fd);
      {
	const char *pos_out;
	void *val_out;
	const char *err_pos = data;
	if(f_command_list(&err_pos, r,
			  data, data + size,
			  &pos_out, &val_out)) {
	  if(pos_out == data + size) {
	    struct command_list *l = val_out;
	    while(l) {
	      struct command *cmd;
	      if(!m_commands_cons(l, &cmd, &l)) assert(0);
	      shell_command(r, state, cmd);
	    }
	  }
	  else {
	    printf("plash: file parse failed (reached index %i, consumed only %i chars)\n",
		   err_pos - data, pos_out - data);
	  }
	}
	else {
	  printf("plash: file parse failed (reached index %i)\n",
		 err_pos - data);
	}
      }
      return;
    source_error:
      printf("plash: error reading `%s': %s\n",
	     filename2.data, strerror(err));
      close(fd);
      return;
    }
  }
  printf("plash: command not handled\n");
}

void parse_and_run_shell_command(struct shell_state *state, seqf_t line)
{
  region_t r = region_make();
  const char *pos_out;
  void *val_out;
  const char *err_pos = line.data;
  if(f_command(&err_pos, r,
	       line.data, line.data + line.size,
	       &pos_out, &val_out)) {
    if(pos_out == line.data + line.size) {
      shell_command(r, state, val_out);
    }
    else {
      printf("plash: parse failed (consumed only %i chars)\n",
	     pos_out - line.data);
    }
  }
  else {
    printf("plash: parse failed (reached index %i)\n",
	   err_pos - line.data);
  }
  if(MOD_DEBUG) printf("allocated %i bytes\n", region_allocated(r));
  region_free(r);
}

/* Substitutions:
   %d for current working directory.
*/
void set_window_title(struct shell_state *state, seqf_t fmt)
{
  const char *term = getenv("TERM");
  if(term && !strcmp(term, "xterm")) {
    int i = 0;
    printf("\033]0;");
    while(i < fmt.size) {
      if(fmt.data[i] == '%' && i+1 < fmt.size && fmt.data[i+1] == 'd') {
	if(state->cwd) {
	  region_t r = region_make();
	  seqf_t cwd = flatten(r, string_of_cwd(r, state->cwd));
	  fprint_d(stdout, cwd);
	  region_free(r);
	}
	else {
	  fprintf(stdout, "undefined-cwd");
	}
	i += 2;
      }
      else { putchar(fmt.data[i]); i++; }
    }
    printf("\007");
  }
}

void interactive_setup(struct shell_state *state)
{
  state->interactive = isatty(STDIN_FILENO);
  if(state->interactive) {
    int shell_pgid;
      
    /* Loop until we are in the foreground. */
    while(1) {
      int shell_pgid = getpgrp();
      if(tcgetpgrp(STDIN_FILENO) == shell_pgid) break;
      if(kill(-shell_pgid, SIGTTIN) < 0) { perror("kill(SIGTTIN)"); }
    }
      
    /* Ignore interactive and job-control signals. */
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    /* signal(SIGCHLD, SIG_IGN); */

    /* Put ourselves in our own process group. */
    shell_pgid = getpid();
    if(setpgid(shell_pgid, shell_pgid) < 0) {
      /* This gives an error when the program is started in its own
	 terminal window.  Don't know why. */
      /* perror("plash: couldn't put the shell in its own process group"); */
    }
      
    /* Grab control of the terminal. */
    if(tcsetpgrp(STDIN_FILENO, shell_pgid) < 0) { perror("tcsetpgrp"); }

    /* Save terminal state. */
    if(tcgetattr(STDIN_FILENO, &state->tmodes) < 0) { perror("tcgetattr"); }
  }
}

void handle_gc_uid_locks_exit(void *x, int status)
{
  /* Doesn't need handling */
  /* printf("plash: gc-uid-locks done\n"); */
}

void handle_server_exit(void *x, int status)
{
  printf("plash: server process: ");
  print_wait_status(stdout, status);
  printf("\n");
}

int main(int argc, char *argv[])
{
  int single_server = 0;
  struct shell_state state;

  w_setup();

  state.env = 0;

  {
    int pid = fork();
    if(pid == 0) {
      /* Put it into its own process group, just in case user presses Ctrl-C. */
      setpgid(0, 0);
      execl(BIN_INSTALL "/gc-uid-locks", "gc-uid-locks", "--gc", 0);
      perror("plash/gc-uid-locks: exec");
      exit(1);
    }
    if(pid < 0) perror("plash/gc-uid-locks: fork");
    else w_add_process(pid, handle_gc_uid_locks_exit, 0);
  }
  
  state.conn_maker_local = conn_maker_make();

  if(single_server) {
    region_t r = region_make();
    int export_count = 4;
    int socks[2];
    int pid;
    cap_t *import;

    if(socketpair(AF_LOCAL, SOCK_STREAM, 0, socks) < 0) {
      perror("plash: socketpair");
      return 1; /* Error */
    }
    set_close_on_exec_flag(socks[0], 1);
    set_close_on_exec_flag(socks[1], 1);

    pid = fork();
    if(pid == 0) {
      int err;
      cap_t *caps;
      
      /* Put this process into its own process group, so that if the
	 user presses Ctrl-C while the shell is waiting for input and
	 is in the current process group, the server isn't killed. */
      setpgid(0, 0);
      
      caps = region_alloc(r, export_count * sizeof(cap_t));
      caps[0] = initial_dir("/", &err);
      if(!caps[0]) {
	fprintf(stderr, "plash: can't open root directory: %s\n",
		strerror(err));
	exit(1);
      }
      caps[1] = conn_maker_make();
      caps[2] = union_dir_maker_make();
      caps[3] = fab_dir_maker_make();
      {
	int i;
	for(i = 0; i < export_count; i++) {
	  caps[i] = make_log_proxy(caps[i]);
	}
      }
      close(socks[1]);
      cap_make_connection(r, socks[0], cap_seq_make(caps, export_count),
			  0, "to-shell");
      caps_free(cap_seq_make(caps, export_count));
      region_free(r);
      cap_run_server();
      exit(0);
    }
    if(pid < 0) {
      perror("plash: fork");
      return 1;
    }
    w_add_process(pid, handle_server_exit, 0);
    close(socks[0]);
    import = cap_make_connection(r, socks[1], caps_empty, export_count,
				 "to-server");
    state.root = import[0];
    state.conn_maker = import[1];
    state.union_dir_maker = import[2];
    state.fab_dir_maker = import[3];
    region_free(r);
  }
  else {
    int err;
    state.root = initial_dir("/", &err);
    if(!state.root) {
      fprintf(stderr, "plash: can't open root directory: %s\n", strerror(err));
      return 1;
    }
    state.conn_maker = inc_ref(state.conn_maker_local);
    state.union_dir_maker = union_dir_maker_make();
    state.fab_dir_maker = fab_dir_maker_make();
  }

  /* Start server used for mediating only. */
  {
    region_t r = region_make();
    int export_count = 1;
    int socks[2];
    int pid;
    cap_t *import;

    if(socketpair(AF_LOCAL, SOCK_STREAM, 0, socks) < 0) {
      perror("plash: socketpair");
      return 1; /* Error */
    }
    set_close_on_exec_flag(socks[0], 1);
    set_close_on_exec_flag(socks[1], 1);

    pid = fork();
    if(pid == 0) {
      cap_t *caps;
      
      /* Put this process into its own process group, so that if the
	 user presses Ctrl-C while the shell is waiting for input and
	 is in the current process group, the server isn't killed. */
      setpgid(0, 0);

      caps = region_alloc(r, export_count * sizeof(cap_t));
      caps[0] = conn_maker_make();
      close(socks[1]);
      cap_make_connection(r, socks[0], cap_seq_make(caps, export_count),
			  0, "single-server-to-shell");
      caps_free(cap_seq_make(caps, export_count));
      region_free(r);
      cap_run_server();
      exit(0);
    }
    if(pid < 0) {
      perror("plash: fork");
      return 1;
    }
    w_add_process(pid, handle_server_exit, 0);
    close(socks[0]);
    import = cap_make_connection(r, socks[1], caps_empty, export_count,
				 "shell-to-single-server");
    state.conn_maker = import[0];
    region_free(r);
  }

  state.cwd = 0;
  state.interactive = 0;
  state.fork_server_per_job = !single_server;
  state.next_job_id = 1;
  state.jobs.id = 0;
  state.jobs.next = &state.jobs;
  state.jobs.prev = &state.jobs;

  state.prompt = "plash$ ";

  state.log_summary = 0;
  state.log_messages = 0;
  state.log_into_xterm = 0;
  state.print_fs_tree = 0;

  assert(argc >= 1);
  shell_pathname = argv[0];

#ifdef USE_GTK
  if(gtk_init_check(&argc, &argv)) gtk_available = 1;
#endif
  
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
	fprintf(stderr, "plash: can't set current working directory: %s\n",
		strerror(err));
      }
      region_free(r);
      free(cwd_name);
    }
  }

  argc--;
  argv++;
  if(argc >= 2 && !strcmp(argv[0], "-p")) {
    state.prompt = argv[1];
    argc -= 2;
    argv += 2;
  }
  if(argc == 0) {
    interactive_setup(&state);
    
    /* Interactive mode */
    using_history();
    while(1) {
      char *line1;

      /* The filesysobj code will fchdir all over the place!  This gets
	 a bit confusing because readline uses the kernel-provided cwd to do
	 filename completion.  Set the kernel-provided cwd to our cwd.
	 Ultimately, the proper way to do this would be to change readline
	 to use our virtualised filesystem. */
      if(state.cwd && state.cwd->dir->vtable == &real_dir_vtable) {
	struct real_dir *dir = (void *) state.cwd->dir;
	if(dir->fd) {
	  if(fchdir(dir->fd->fd) < 0) perror("fchdir");
	}
      }
      set_window_title(&state, seqf_string("%d - plash"));
      line1 = readline(state.prompt);

      poll_for_processes();
      report_jobs(&state);
      
      if(!line1) {
	fprintf(stderr, "plash: end of user input\n");
	break;
      }
      if(*line1) {
	add_history(line1);

	parse_and_run_shell_command(&state, seqf_string(line1));
	
	poll_for_processes();
	report_jobs(&state);
      }
      /* As far as I can tell, add_history() doesn't take a copy of the line you give it. */
      /* free(line1); */
    }
    printf("\n");
  }
  else if(argc == 2 && !strcmp(argv[0], "-c")) {
    parse_and_run_shell_command(&state, seqf_string(argv[1]));
  }
  else {
    printf("Plash version " PLASH_VERSION "\n"
	   "Copyright 2004 Mark Seaborn\n"
	   "\n"
	   "Usage: shell [-c CMD] [-p PROMPT]\n"
	   "\n"
	   "Special commands in the shell:\n"
	   "  cd <directory pathname>  Change directory\n"
	   "  fg <job number>          Put job in foreground\n"
	   "  bg <job number>          Put job in background\n"
	   "  opts                     Displays an options window\n");
  }
  return 0;
}

#include "out-vtable-shell.h"
