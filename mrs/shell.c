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
#include "build-fs.h"
#include "shell-globbing.h"
#include "cap-protocol.h"
#include "fs-operations.h"


int f_command(region_t r, const char *pos_in1, const char *end,
	      const char **pos_out_p, void **ok_val_p);


void finalise_close_fd(void *obj)
{
  int fd = (int) obj;
  if(close(fd) < 0) { perror("plash: close"); }
}


const char *shell_pathname = 0;


#define ST_RUNNING 0
#define ST_STOPPED 1
#define ST_FINISHED 2
struct shell_process {
  int pid;
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

struct shell_state {
  struct filesys_obj *root;
  struct dir_stack *cwd;

  int interactive;
  int next_job_id;
  struct job jobs;
  struct termios tmodes; /* Saved terminal state */

  int log_summary;
  int log_messages;
  int log_into_xterm;
  int print_fs_tree;
};

/* Used for starting processes */
/* This is all allocated into a region, including FDs, which have
   finalisers added to the region. */
struct process_desc {
  /* This function is run inside the forked process. */
  void (*set_up_process)(struct process_desc *desc);
  const char *cmd;
  int argc; /* Not really needed */
  const char **argv; /* Array needs to include null terminator */
  fds_t fds;
};
struct process_desc_sec {
  struct process_desc d;
  int comm_fd;
};
struct process_desc_nosec {
  struct process_desc d;
  /* For processes started the conventional way, we need to set the cwd
     to the shell's logical cwd. */
  int cwd_fd;
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
  setenv("COMM_FD", buf, 1);

  /* Necessary for security when using run-as-nobody, otherwise the
     process can be left in a directory with read/write access which
     might not be reachable from the root directory. */
  /* Not necessary for run-as-nobody+chroot. */
  if(chdir("/") < 0) { perror("plash: chdir"); exit(1); }
}
void set_up_nosec_process(struct process_desc *desc1) {
  struct process_desc_nosec *desc = (void *) desc1;
  if(fchdir(desc->cwd_fd) < 0) {
    /* This should fail, because it might be dangerous if the command
       is run in the wrong directory, eg. "rm -rv ." */
    perror("plash: fchdir");
    exit(1);
  }
}

struct process_desc_list {
  struct process_desc *proc;
  struct process_desc_list *next;
};

#ifndef SIMPLE_SERVER
/* A server description is a list of connections to open, giving objects
   to export on each connection. */
struct server_desc {
  int sock_fd;
  cap_seq_t export;
  struct server_desc *next;
};
#endif


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

/* FD array.  An entry of -1 means that the FD is not open. */
/* This is allocated in a region.  Resizing doesn't reallocate the old
   array. */
/* Like struct seq_fds, but intended to be writable. */
struct fd_array {
  int *fds;
  int count;
};

void array_set_fd(region_t r, struct fd_array *array, int j, int x) {
  /* Enlarge file descriptor array if necessary. */
  if(j >= array->count) {
    int i;
    int new_size = j + 1 + 10;
    int *fds_new = region_alloc(r, new_size * sizeof(int));
    memcpy(fds_new, array->fds, array->count * sizeof(int));
    for(i = array->count; i < new_size; i++) fds_new[i] = -1;
    array->fds = fds_new;
    array->count = new_size;
  }
  array->fds[j] = x;
}
int array_get_free_index(struct fd_array *array) {
  int i;
  for(i = 0; i < array->count; i++) {
    if(array->fds[i] < 0) return i;
  }
  return array->count;
}

struct flatten_params {
  struct str_list **got_end;
  struct filesys_obj *root_dir;
  struct dir_stack *cwd;
  /* This is zero if we're not constructing a filesystem.
     Used for the `!!' syntax. */
  struct node *tree;
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
  assert(0);
  return 1;
}

extern char **environ;

void print_wait_status(int status)
{
  if(WIFEXITED(status)) {
    printf("normal exit: %i", WEXITSTATUS(status));
  }
  else if(WIFSIGNALED(status)) {
    printf("uncaught signal: %i", WTERMSIG(status));
  }
  else if(WIFSTOPPED(status)) {
    printf("stopped with signal: %i", WSTOPSIG(status));
  }
  else printf("unknown");
}

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

int wait_for_processes(struct shell_state *state, int check)
{
  struct job *job;
  int status;
  int pid = waitpid(-1, &status, WUNTRACED | (check ? WNOHANG : 0));
  /* printf("pid %i: ", pid); print_wait_status(status); printf("\n"); */
  if(pid < 0) return -1;
  if(pid == 0) return 0;
    
  for(job = state->jobs.next; job->id; job = job->next) {
    struct shell_process *p;
    for(p = job->processes; p; p = p->next) {
      if(p->pid == pid) {
	p->state = WIFSTOPPED(status) ? ST_STOPPED : ST_FINISHED;
	p->wait_status = status;
	return 1;
      }
    }
  }
  printf("plash: unknown process %i exited\n", pid);
  return 1;
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

/* Used when starting a job in the foreground, or when returning a job
   to the foreground. */
void wait_for_job(struct shell_state *state, struct job *job)
{
  while(job->last_state == ST_RUNNING) {
    if(wait_for_processes(state, 0 /* check */) < 0) { perror("plash: wait"); break; }
    job->last_state = job_state(job);
  }
  /* We now know that we can continue, because the job isn't running.
     But there might be more information we can get from waitpid() that
     indicates whether it is stopped or finished. */
  while(wait_for_processes(state, 1 /* check */) > 0) /* nothing */;
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
    // printf("plash: job %i finished\n", job->id);
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

void set_up_process_for_job(struct shell_state *state, struct job *job, int foreground)
{
  if(state->interactive) {
    int pgid = job->pgid;
    if(!pgid) pgid = getpid();
    /* Set this process's process group ID to pgid (= pid_client). */
    if(setpgid(0, pgid) < 0) { perror("plash/child: setpgid"); }
    if(foreground) {
      if(tcsetpgrp(STDIN_FILENO, pgid) < 0) { perror("plash/child: tcsetpgrp"); }
    }

    /* Set the handling for job control signals back to the default. */
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
  }
}

/* cwd is a dir_stack in the user's namespace.  It is translated into
   one for the process's namespace.
   root is the process's namespace.
   exec_fd is an FD for the executable, whose file doesn't have to be
   included in the process's namespace.  It is closed (in the parent
   process) before the function returns. */
/*int spawn(struct filesys_obj *root, struct dir_stack *cwd,
	  int exec_fd, const char *cmd, int argc, const char **argv,
	  struct shell_state *state, int foreground)*/
/* This frees region r in the server process. */
int spawn_job(region_t sock_r,
	      struct shell_state *state,
#ifdef SIMPLE_SERVER
	      struct server_state *server_state,
#else
	      struct server_desc *server_desc,
#endif
	      struct process_desc_list *procs,
	      int foreground)
{
  int attach_gdb_to_server = 0;
  int attach_gdb_to_client = 0;
  int attach_strace_to_client = 0;
  
  struct job *job = amalloc(sizeof(struct job));
  job->id = state->next_job_id++;
  job->processes = 0;
  job->pgid = 0; /* The pgid will be the same as the pid of the first process. */
  job->prev = &state->jobs;
  job->next = state->jobs.next;
  job->last_state = ST_RUNNING;
  state->jobs.next->prev = job;
  state->jobs.next = job;

  /* Start the client processes. */
  for(; procs; procs = procs->next) {
    struct process_desc *proc = procs->proc;
    
    int pid_client = fork();
    if(pid_client == 0) {
      int i, max_fd;

      /* Setting which process group owns the terminal needs to be done
	 before setting up the client's FDs. */
      set_up_process_for_job(state, job, foreground);

      proc->set_up_process(proc);

      /* Some FDs in the process are close-on-exec. */
      /* Some FDs are refcounted, and these are closed by close_our_fds().
	 We don't mark them as close-on-exec, because doing so would
	 involve lots of needless system calls. */
      /* Unneeded FDs must be closed for security and correctness reasons. */

      /* This comes first.  I assume that FDs that are passed to the
	 new process are not among the refcounted FDs closed by this. */
      close_our_fds();
      /* Copy FDs into place. */
      /* First the FDs are copied to a position where they won't interfere
	 with the destination or source locations. */
      max_fd = proc->fds.count;
      for(i = 0; i < proc->fds.count; i++) {
	if(max_fd < proc->fds.fds[i] + 1) max_fd = proc->fds.fds[i] + 1;
      }
      for(i = 0; i < proc->fds.count; i++) {
	if(proc->fds.fds[i] >= 0) {
	  /* printf("dup2(%i, %i)\n", proc->fds.fds[i], max_fd + i); */
	  if(dup2(proc->fds.fds[i], max_fd + i) < 0) {
	    perror("plash/client: dup2 (first)");
	    exit(1);
	  }
	  if(set_close_on_exec_flag(max_fd + i, 1) < 0) perror("plash: cloexec");
	}
      }
      /* Now they are copied back to their real position. */
      for(i = 0; i < proc->fds.count; i++) {
	if(proc->fds.fds[i] >= 0) {
	  /* printf("dup2(%i, %i)\n", max_fd + i, i); */
	  if(dup2(max_fd + i, i) < 0) {
	    perror("plash/client: dup2 (second)");
	    exit(1);
	  }
	}
      }

      if(attach_strace_to_client || attach_gdb_to_client) {
	if(kill(getpid(), SIGSTOP) < 0) { perror("kill(SIGSTOP)"); }
      }

      execve(proc->cmd, (char **) proc->argv, environ);
      perror("plash: exec");
      exit(1);
    }
    if(pid_client < 0) { perror("plash: fork"); return -1; }

    if(job->pgid == 0) job->pgid = pid_client;

    /* Add process ID to list of processes in job. */
    {
      struct shell_process *p = amalloc(sizeof(struct shell_process));
      p->pid = pid_client;
      p->state = ST_RUNNING;
      p->next = job->processes;
      job->processes = p;
    }
  }

#ifndef SIMPLE_SERVER
  /* Start the server process. */
  /* Don't bother starting the server if it will have no processes to
     handle.  This is just an optimisation to avoid forking, because
     if there are no processes the server would immediately exit. */
  if(server_desc) {
    int pid_server = fork();
    if(pid_server == 0) {
      set_up_process_for_job(state, job, foreground);

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
	  cap_make_connection(r, desc->sock_fd, desc->export, 0, "to-client");
	}
	region_free(r);
      }
      cap_run_server();
      exit(0);
    }
    if(pid_server < 0) { perror("plash: fork"); return -1; }

    /* Add server process ID to list. */
    {
      struct shell_process *p = amalloc(sizeof(struct shell_process));
      p->pid = pid_server;
      p->state = ST_RUNNING;
      p->next = job->processes;
      job->processes = p;
    }

    /* The problem with this is that gdb hangs if you attach it to a
       stopped process.  It doesn't try to restart the process. */
    if(attach_gdb_to_server) { printf("server is pid %i\n", pid_server); }
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
  }
#else
  /* Start the server process. */
  /* Don't bother starting the server if it will have no processes to
     handle.  This is just an optimisation to avoid forking, because
     if there are no processes the server would immediately exit. */
  if(server_state->list.next->id) {
    int pid_server = fork();
    if(pid_server == 0) {
      set_up_process_for_job(state, job, foreground);

      if(attach_gdb_to_server) {
	if(kill(getpid(), SIGSTOP) < 0) { perror("kill(SIGSTOP)"); }
      }

      /* This is used to close FDs that were opened to be passed to the
	 other processes. */
      region_free(sock_r);

      // server_log = fopen("/dev/null", "w");
      // if(!server_log) server_log = fdopen(STDERR_FILENO, "w");
      run_server(server_state);
      exit(0);
    }
    if(pid_server < 0) { perror("plash: fork"); return -1; }

    /* Add server process ID to list. */
    {
      struct shell_process *p = amalloc(sizeof(struct shell_process));
      p->pid = pid_server;
      p->state = ST_RUNNING;
      p->next = job->processes;
      job->processes = p;
    }

    if(attach_gdb_to_server) {
      int pid_gdb = fork();
      if(pid_gdb == 0) {
	char buf[10];
	snprintf(buf, sizeof(buf), "%i", pid_server);
	execl("/usr/bin/gdb", "/usr/bin/gdb", shell_pathname, buf, 0);
	perror("exec");
	exit(1);
      }
      if(pid_gdb < 0) perror("plash: fork");
    }
  }
#endif

  region_free(sock_r);
#ifndef SIMPLE_SERVER
  /* Also free the server state, which includes lots of FDs. */
  {
    struct server_desc *desc;
    for(desc = server_desc; desc; desc = desc->next) {
      caps_free(desc->export);
      if(close(desc->sock_fd) < 0) perror("plash: close");
    }
  }
#else
  /* Also free the server state, which includes lots of FDs. */
  while(server_state->list.next->id) remove_process(server_state->list.next);
#endif

  /* Since the client is changed to run as nobody, setpgid below can
     fail on it.  It's a race condition.  If it fails here, the client
     has already called setpgid itself, so that's okay.  FIXME. */
  if(state->interactive) {
    struct shell_process *p;
    for(p = job->processes; p; p = p->next) {
      if(setpgid(p->pid, job->pgid) < 0) { /* perror("plash: setpgid"); */ }
    }
    /* Save the terminal state (needed for background jobs). */
    if(tcgetattr(STDIN_FILENO, &job->tmodes) < 0) { perror("plash: tcgetattr"); }
    /* Give the terminal to the new job. */
    if(foreground) {
      if(tcsetpgrp(STDIN_FILENO, job->pgid) < 0) { perror("plash: tcsetpgrp"); }
    }
  }

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

  if(foreground) wait_for_job(state, job);
  else {
    if(state->interactive)
      printf("plash: job %i started\n", job->id);
    else
      remove_job(job);
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


/* Constructs a filesystem for the process being created for the command
   invocation.  Adds a process entry to the server state to handle this
   filesystem. */
struct process_desc *command_invocation
  (region_t r, struct shell_state *state,
   struct server_shared *shared,
#ifdef SIMPLE_SERVER
   struct server_state *server_state,
#else
   struct server_desc **server_desc,
#endif
   struct invocation *inv,
   int fd_stdin, int fd_stdout)
{
  int no_sec;
  struct char_cons *cmd_filename1;
  struct arg_list *args;

  seqf_t cmd_filename2, cmd_filename;

  if(!m_invocation(inv, &no_sec, &cmd_filename1, &args)) {
    assert(0);
    return 0; /* Error */
  }

  cmd_filename2 = tilde_expansion(r, flatten_charlist(r, cmd_filename1));
  if(resolve_executable(r, cmd_filename2, &cmd_filename) < 0) {
    printf("plash: not found: %s\n", cmd_filename2.data);
    return 0;
  }

  if(no_sec) {
    struct flatten_params p;
    struct str_list *args_got, *l;
    int arg_count;
    const char **argv;
    int i;
    struct process_desc_nosec *proc;
    
    /* Process the arguments. */
    args_got = 0;
    p.got_end = &args_got;
    p.root_dir = state->root;
    p.cwd = state->cwd;
    p.tree = 0;
    p.fds.count = 3;
    p.fds.fds = region_alloc(r, p.fds.count * sizeof(int));
    p.fds.fds[0] = fd_stdin;
    p.fds.fds[1] = fd_stdout;
    p.fds.fds[2] = STDERR_FILENO;
    if(flatten_args(r, &p, 0 /* rw */, 0 /* ambient */, args)) return 0;

    /* Copy the arguments from the list into an array. */
    for(l = args_got, arg_count = 0; l; l = l->next) arg_count++;
    argv = region_alloc(r, (arg_count+2) * sizeof(char*));
    argv[0] = cmd_filename.data;
    for(l = args_got, i = 0; l; l = l->next, i++) argv[i+1] = l->str;
    argv[arg_count+1] = 0;
    
    proc = region_alloc(r, sizeof(struct process_desc_nosec));
    proc->d.set_up_process = set_up_nosec_process;
    if(state->cwd->dir->vtable == &real_dir_vtable) {
      struct real_dir *dir = (void *) state->cwd->dir;
      if(dir->fd) {
	proc->cwd_fd = dir->fd->fd;
      }
      else {
	printf("plash: current directory does not have valid file descriptor\n");
	return 0;
      }
    }
    else {
      printf("plash: current directory is not real\n");
      return 0;
    }
    proc->d.cmd = cmd_filename.data;
    proc->d.argc = arg_count + 1;
    proc->d.argv = argv;
    {
      /*int count = 3;
      int *fds = region_alloc(r, count * sizeof(int));
      proc->d.fds.count = count;
      proc->d.fds.fds = fds;
      fds[0] = fd_stdin;
      fds[1] = fd_stdout;
      fds[2] = STDERR_FILENO;*/
      proc->d.fds.fds = p.fds.fds;
      proc->d.fds.count = p.fds.count;
    }
    return (struct process_desc *) proc;
  }
  else {
    struct flatten_params p;
    struct str_list *args_got, *l;
    int i;
    int arg_count;
    const char **argv;
    int executable_fd;
    int argc2;
    const char **argv2;
    int executable_fd2;
    struct filesys_slot *root_slot;
    struct filesys_obj *root;
    struct dir_stack *cwd;
    int err;

    /* Process the arguments. */
    args_got = 0;
    p.got_end = &args_got;
    p.root_dir = state->root;
    p.cwd = state->cwd;
    p.tree = make_empty_node();
    p.fds.count = 3;
    p.fds.fds = region_alloc(r, p.fds.count * sizeof(int));
    p.fds.fds[0] = fd_stdin;
    p.fds.fds[1] = fd_stdout;
    p.fds.fds[2] = STDERR_FILENO;
    if(flatten_args(r, &p, 0 /* rw */, 0 /* ambient */, args)) return 0;
      
    /* FIXME: check for errors */
    resolve_populate(state->root, p.tree, p.cwd, seqf_string("/etc"), 0 /* create */, &err);
    resolve_populate(state->root, p.tree, p.cwd, seqf_string("/bin/"), 0 /* create */, &err);
    resolve_populate(state->root, p.tree, p.cwd, seqf_string("/lib"), 0 /* create */, &err);
    resolve_populate(state->root, p.tree, p.cwd, seqf_string("/usr"), 0 /* create */, &err);
    resolve_populate(state->root, p.tree, p.cwd, seqf_string("/dev/tty"), 0 /* create */, &err);
    resolve_populate(state->root, p.tree, p.cwd, seqf_string("/dev/null"), 0 /* create */, &err);

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
      if(!obj) { errno = err; perror("plash: open/exec"); return 0; }
      executable_fd = obj->vtable->open(obj, O_RDONLY, &err);
      filesys_obj_free(obj);
      if(executable_fd < 0) { errno = err; perror("plash: open/exec"); return 0; }
    }

    if(state->print_fs_tree) print_tree(0, p.tree);

    /* Create the root directory. */
    root_slot = build_fs(p.tree);
    free_node(p.tree);
    root = root_slot->vtable->get(root_slot);
    assert(root);
    filesys_slot_free(root_slot);
    
    /* Set the process's cwd. */
    {
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
      return 0;
    }
    region_add_finaliser(r, finalise_close_fd, (void *) executable_fd2);

    /* Construct the connection to the server. */
    {
      /* socks[0] goes to server, socks[1] goes to client. */
      int socks[2];
      struct process *server_proc;
  
      if(socketpair(AF_LOCAL, SOCK_STREAM, 0, socks) < 0) {
	perror("plash: socketpair");
	return 0; /* Error */
      }
      set_close_on_exec_flag(socks[0], 1);
      set_close_on_exec_flag(socks[1], 1);
      region_add_finaliser(r, finalise_close_fd, (void *) socks[1]);

#ifdef SIMPLE_SERVER
      server_proc = amalloc(sizeof(struct process));
      server_proc->sock_fd = socks[0];
      server_proc->root = root;
      server_proc->cwd = cwd;
      add_process(server_state, server_proc);
#else
      {
	struct server_desc *desc = region_alloc(r, sizeof(struct server_desc));
	shared->refcount++;
	desc->sock_fd = socks[0];
	desc->export = mk_caps1(r, make_fs_op_server(shared, root, cwd));
	desc->next = *server_desc;
	*server_desc = desc;
      }
#endif
    
      {
	int proc_sock_fd, proc_exec_fd;
	struct process_desc_sec *proc;
	
	proc_sock_fd = array_get_free_index(&p.fds);
	array_set_fd(r, &p.fds, proc_sock_fd, socks[1]);
	proc_exec_fd = array_get_free_index(&p.fds);
	array_set_fd(r, &p.fds, proc_exec_fd, executable_fd2);
	
	proc = region_alloc(r, sizeof(struct process_desc_sec));
	/*
	int count = 5;
	int *fds = region_alloc(r, count * sizeof(int));
	proc->fds.count = count;
	proc->fds.fds = fds;
	fds[0] = fd_stdin;
	fds[1] = fd_stdout;
	fds[2] = STDERR_FILENO;
	fds[3] = socks[1];
	fds[4] = executable_fd2;
	*/
	proc->d.fds.count = p.fds.count;
	proc->d.fds.fds = p.fds.fds;
	proc->comm_fd = proc_sock_fd;
	args_to_exec_elf_program_from_fd
	  (r, proc_exec_fd /* executable_fd */, argc2, argv2,
	   &proc->d.cmd, &proc->d.argc, &proc->d.argv);
	proc->d.set_up_process = set_up_sec_process;
	return (struct process_desc *) proc;
      }
    }
  }
  /* FIXME: deallocate tree */
}

/* Returns 0 if there's an error. */
struct process_desc_list *pipeline_invocation
  (region_t r, struct shell_state *state,
   struct server_shared *shared,
#ifdef SIMPLE_SERVER
   struct server_state *server_state,
#else
   struct server_desc **server_state, /* rename this! */
#endif
   struct pipeline *pipeline,
   int fd_stdin, int fd_stdout)
{
  struct invocation *inv;
  struct pipeline *rest;
  
  if(m_pipeline_inv(pipeline, &inv)) {
    struct process_desc *proc =
      command_invocation(r, state, shared, server_state, inv, fd_stdin, fd_stdout);
    struct process_desc_list *list =
      region_alloc(r, sizeof(struct process_desc_list));
    if(!proc) return 0; /* Error */
    list->proc = proc;
    list->next = 0;
    return list;
  }
  if(m_pipeline_cons(pipeline, &inv, &rest)) {
    struct process_desc *proc;
    struct process_desc_list *list1, *list2;
    int pipe_fd[2];
    if(pipe(pipe_fd) < 0) { perror("plash: pipe"); return 0; }
    set_close_on_exec_flag(pipe_fd[0], 1);
    set_close_on_exec_flag(pipe_fd[1], 1);

    region_add_finaliser(r, finalise_close_fd, (void *) pipe_fd[0]);
    region_add_finaliser(r, finalise_close_fd, (void *) pipe_fd[1]);

    proc = command_invocation(r, state, shared, server_state, inv, fd_stdin, pipe_fd[1]);
    if(!proc) return 0; /* Error */
    list1 = pipeline_invocation(r, state, shared, server_state, rest, pipe_fd[0], fd_stdout);
    if(!list1) return 0; /* Error */
    list2 = region_alloc(r, sizeof(struct process_desc_list));
    list2->proc = proc;
    list2->next = list1;
    return list2;
  }
  assert(0);
  return 0;
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

void shell_command(struct shell_state *state, seqf_t line)
{
  region_t r = region_make();
  const char *pos_out;
  void *val_out;
  if(f_command(r, line.data, line.data + line.size, &pos_out, &val_out) &&
     pos_out == line.data + line.size) {
    struct char_cons *dir_filename1;

    struct pipeline *pipeline;

    int bg_flag;

    struct char_cons *job_id_str;
    
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
    else if(m_command(val_out, &pipeline, &bg_flag)) {
      region_t sock_r = region_make();

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
	region_free(sock_r);
      }
      else {
#ifdef SIMPLE_SERVER
	struct server_state server_state;
#else
	struct server_desc *server_state = 0; /* rename! */
#endif
	struct process_desc_list *procs;
	struct server_shared *shared = amalloc(sizeof(struct server_shared));
	shared->refcount = 1;
	shared->next_id = 1;
	shared->log = 0;
	shared->log_summary = state->log_summary;
	shared->log_messages = state->log_messages;

#ifdef SIMPLE_SERVER
	init_server_state(&server_state);
#endif
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
#ifdef SIMPLE_SERVER
	    region_add_finaliser(r, finalise_close_fd, (void *) pipe_fd[1]);
	    server_state.log = fdopen(pipe_fd[1], "w");
#else
	    shared->log = fdopen(pipe_fd[1], "w");
#endif
	  }
	  else {
#ifdef SIMPLE_SERVER
	    server_state.log = fdopen(STDERR_FILENO, "w");
#else
	    shared->log = fdopen(dup(STDERR_FILENO), "w");
#endif
	  }
#ifdef SIMPLE_SERVER
	  assert(server_state.log);
	  setvbuf(server_state.log, 0, _IONBF, 0);
#endif
	}
#ifdef SIMPLE_SERVER
	server_state.log_summary = state->log_summary;
	server_state.log_messages = state->log_messages;
#endif
	
	procs = pipeline_invocation(sock_r, state, shared, &server_state, pipeline,
				    STDIN_FILENO, STDOUT_FILENO);
	server_shared_free(shared);
	if(procs) {
	  /* Region sock_r is freed inside the forked server process
	     and before waiting for the child processes. */
	  spawn_job(sock_r, state,
#ifdef SIMPLE_SERVER
		    &server_state,
#else
		    server_state,
#endif
		    procs, !bg_flag);
	}
	else {
	  region_free(sock_r);
	}
      }
    }
    else if(m_command_fg(val_out, &job_id_str)) {
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
	  goto done1;
	}
      }
      printf("plash: Unknown job ID: %i\n", job_id);
    done1:
    }
    else if(m_command_bg(val_out, &job_id_str)) {
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
	  goto done2;
	}
      }
      printf("plash: Unknown job ID: %i\n", job_id);
    done2:
    }
    else {
      printf("plash: command not handled\n");
    }
  }
  else {
    printf("plash: parse failed\n");
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
	region_t r = region_make();
	seqf_t cwd = flatten(r, string_of_cwd(r, state->cwd));
	fprint_d(stdout, cwd);
	i += 2;
      }
      else { putchar(fmt.data[i]); i++; }
    }
    printf("\007");
  }
}

int main(int argc, char *argv[])
{
  int err;
  struct shell_state state;
  state.root = initial_dir("/", &err);
  if(!state.root) {
    fprintf(stderr, "plash: can't open root directory\n");
    return 1;
  }
  state.cwd = 0;
  state.interactive = 0;
  state.next_job_id = 1;
  state.jobs.id = 0;
  state.jobs.next = &state.jobs;
  state.jobs.prev = &state.jobs;

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
	errno = err;
	perror("can't set current working directory");
      }
      region_free(r);
      free(cwd_name);
    }
  }

  if(argc <= 1) {
    state.interactive = isatty(STDIN_FILENO);
    if(state.interactive) {
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
      if(tcgetattr(STDIN_FILENO, &state.tmodes) < 0) { perror("tcgetattr"); }
    }
    
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
      set_window_title(&state, seqf_string("%d - plash"));
      line1 = readline("plash$ ");
      
      while(wait_for_processes(&state, 1 /* check */) > 0) /* nothing */;
      report_jobs(&state);
      
      if(!line1) break;
      if(*line1) {
	add_history(line1);
	
	shell_command(&state, seqf_string(line1));
	
	while(wait_for_processes(&state, 1 /* check */) > 0) /* nothing */;
	report_jobs(&state);
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
    printf("Plash version " PLASH_VERSION "\n"
	   "Copyright 2004 Mark Seaborn\n"
	   "\n"
	   "Usage: shell [-c CMD]\n"
	   "\n"
	   "Special commands in the shell:\n"
	   "  cd <directory pathname>  Change directory\n"
	   "  fg <job number>          Put job in foreground\n"
	   "  bg <job number>          Put job in background\n"
	   "  opts                     Displays an options window\n");
  }
  return 0;
}
