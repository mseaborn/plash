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

#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

#include "region.h"


struct process_list {
  int pid;
  void (*f)(void *x, int status);
  void *x;
  struct process_list *next;
};

static struct process_list *processes = 0;

void w_add_process(int pid, void (*f)(void *x, int status), void *x)
{
  struct process_list *node = amalloc(sizeof(struct process_list));
  node->pid = pid;
  node->f = f;
  node->x = x;
  node->next = processes;
  processes = node;
}

void print_wait_status(FILE *fp, int status)
{
  if(WIFEXITED(status)) {
    fprintf(fp, _("normal exit: %i"), WEXITSTATUS(status));
  }
  else if(WIFSIGNALED(status)) {
    const char *desc = strsignal(WTERMSIG(status));
    fprintf(fp, _("uncaught signal: %i (%s)"), WTERMSIG(status),
	    desc ? desc : _("unknown signal"));
  }
  else if(WIFSTOPPED(status)) {
    const char *desc = strsignal(WSTOPSIG(status));
    fprintf(fp, _("stopped with signal: %i (%s)"), WSTOPSIG(status),
	    desc ? desc : _("unknown signal"));
  }
  else fprintf(fp, _("unknown"));
}

void w_handle_process_status(int pid, int status)
{
  int remove = !WIFSTOPPED(status);
  struct process_list *n = processes, **prev = &processes;
  while(n) {
    if(n->pid == pid) {
      /* Remove the node from the list first.  This function should be
	 re-entrant. */
      if(remove) *prev = n->next;

      n->f(n->x, status);
      if(remove) free(n);
      return;
    }
    prev = &n->next;
    n = n->next;
  }
  
  fprintf(stderr, _("plash: unknown process, pid %i: "), pid);
  print_wait_status(stderr, status);
  fprintf(stderr, "\n");
}

/* This doesn't need to do anything.  We can call waitpid() after every
   select() call.  If this set a flag, that would be an optimisation to
   avoid calling waitpid(). */
static void sigchild_handler(int sig)
{
  assert(sig == SIGCHLD);
}

void w_setup()
{
  /* Really we only need to enable SIGCHLD during select() calls; these
     are the only calls that block.  We just need select() calls to be
     interrupted. */
  struct sigaction act;
  act.sa_handler = sigchild_handler;
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask);
  sigaction(SIGCHLD, &act, 0);
}
