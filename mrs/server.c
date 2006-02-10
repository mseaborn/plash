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

/* Necessary to get O_DIRECTORY and O_NOFOLLOW */
/* #define __USE_GNU */
#define _GNU_SOURCE

#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>
#include <dirent.h>

#include "region.h"
#include "server.h"
#include "parse-filename.h"
#include "comms.h"
#include "config.h"
#include "fs-operations.h"


#define MOD_DEBUG 0
#define MOD_MSG "server: "
/* Logs whole messages and the replies */
#define DO_LOG_MESSAGES(state) ((state)->log && (state)->log_messages)
/* Logs one-line summary of each input message */
#define DO_LOG_SUMMARY(state) ((state)->log && (state)->log_summary)
FILE *server_log = 0;


struct process *process_copy(struct process *p)
{
  struct process *p2 = amalloc(sizeof(struct process));
  p2->sock_fd = -1;
  p2->root = p->root;
  if(p->root) p->root->refcount++;
  p2->cwd = p->cwd;
  if(p->cwd) p->cwd->refcount++;
  return p2;
}

void process_free(struct process *p)
{
  if(p->root) filesys_obj_free(p->root);
  if(p->cwd) dir_stack_free(p->cwd);
  free(p);
}

void process_check(struct process *p)
{
  if(p->root) assert(p->root->refcount > 0);
  if(p->cwd) assert(p->cwd->refcount > 0);
}


/* Sets up the arguments to select().  Needs to be called every time the
   process list is changed. */
void init_fd_set(struct server_state *state)
{
  struct process_list *node;
  state->max_fd = 0;
  FD_ZERO(&state->set);
  for(node = state->list.next; node->id; node = node->next) {
    int fd = node->comm->sock;
    if(state->max_fd < fd+1) state->max_fd = fd+1;
    FD_SET(fd, &state->set);
  }
}


void init_server_state(struct server_state *state)
{
  state->list.id = 0;
  state->list.prev = &state->list;
  state->list.next = &state->list;
  state->next_proc_id = 1;

  state->log = 0;
  state->log_summary = 0;
  state->log_messages = 0;
}

/* This should fill out "reply" ("reply_fds" defaults to being empty),
   and can allocate the message from region r. */
void process_handle_msg(region_t r, struct server_state *state,
			struct process *proc,
			seqf_t msg_orig, fds_t fds_orig,
			seqt_t *reply, fds_t *reply_fds,
			seqt_t *log_msg, seqt_t *log_reply)
{
  {
    seqf_t msg = msg_orig;
    int ok = 1;
    m_str(&ok, &msg, "Fork");
    m_end(&ok, &msg);
    if(ok) {
      int socks[2];
      struct process *proc2;
      struct process_list *node_new;
      int new_id = state->next_proc_id++;

      *log_msg = mk_string(r, "fork");
      
      if(MOD_DEBUG) fprintf(server_log, MOD_MSG "forking new process, %i\n", new_id);
      if(socketpair(AF_LOCAL, SOCK_STREAM, 0, socks) < 0) {
	*reply = cat2(r, mk_string(r, "Fail"),
		      mk_int(r, errno));
	*log_reply = mk_string(r, "fail");
	return;
      }
      set_close_on_exec_flag(socks[0], 1);
      set_close_on_exec_flag(socks[1], 1);
      proc2 = process_copy(proc);
      proc2->sock_fd = socks[1];
      node_new = amalloc(sizeof(struct process_list));
      node_new->id = new_id;
      node_new->comm = comm_init(socks[1]);
      node_new->proc = proc2;
      node_new->prev = state->list.prev;
      node_new->next = &state->list;
      state->list.prev->next = node_new;
      state->list.prev = node_new;

      init_fd_set(state);
      
      *reply = mk_string(r, "RFrk");
      *reply_fds = mk_fds1(r, socks[0]);
      *log_reply = mk_printf(r, "ok, created #%i", new_id);
      return;
    }
  }
  handle_fs_op_message(r, proc, msg_orig, fds_orig,
		       reply, reply_fds,
		       log_msg, log_reply);
}

/* Insert a process into the server's list of processes. */
void add_process(struct server_state *state, struct process *initial_proc)
{
  struct process_list *node_new = amalloc(sizeof(struct process_list));
  node_new->id = state->next_proc_id++;
  node_new->comm = comm_init(initial_proc->sock_fd);
  node_new->proc = initial_proc;
  node_new->prev = state->list.prev;
  node_new->next = &state->list;
  state->list.prev->next = node_new;
  state->list.prev = node_new;
}

void remove_process(struct process_list *node)
{
  if(close(node->comm->sock) > 0) perror("close");
  comm_free(node->comm);
  process_free(node->proc);
  node->prev->next = node->next;
  node->next->prev = node->prev;
  free(node);
}

void run_server(struct server_state *state)
{
  init_fd_set(state);

  while(state->list.next->id) { /* while the process list is non-empty */
    int result;
    fd_set read_fds = state->set;
    /* struct timeval timeout; */
    /* timeout.tv_sec = 0;
       timeout.tv_usec = 0; */
    
    if(MOD_DEBUG) fprintf(server_log, MOD_MSG "calling select()\n");
    result = select(state->max_fd, &read_fds, 0, 0, 0 /* &timeout */);
    if(result < 0) { perror("select"); }

    {
      struct process_list *node;
      for(node = state->list.next; node->id && result > 0; node = node->next) {
	if(FD_ISSET(node->comm->sock, &read_fds)) {
	  int r;
	  seqf_t msg;
	  fds_t fds;
    
	  result--;
	  r = comm_read(node->comm);
	  if(r < 0 || r == COMM_END) {
	    struct process_list *node_next = node->next;
	    if(DO_LOG_MESSAGES(state)) fprintf(state->log, "\nprocess %i error/end\n", node->id);

	    /* Close socket and remove process from list */
	    remove_process(node);
	    node = node_next;
	    init_fd_set(state);
	  }
	  else while(1) {
	    r = comm_try_get(node->comm, &msg, &fds);
	    if(r == COMM_AVAIL) {
	      region_t r = region_make();
	      seqt_t reply = seqt_empty;
	      fds_t reply_fds = fds_empty;
	      seqt_t log_msg = mk_string(r, "?");
	      seqt_t log_reply = mk_string(r, "?");
	      
	      if(DO_LOG_MESSAGES(state)) {
		fprintf(state->log, "\nmessage from process %i\n", node->id);
		fprint_data(state->log, msg);
	      }
	      process_check(node->proc);
	      process_handle_msg(r, state, node->proc, msg, fds,
				 &reply, &reply_fds, &log_msg, &log_reply);
	      if(DO_LOG_MESSAGES(state)) {
		fprintf(state->log, "reply with %i FDs and this data:\n", reply_fds.count);
		fprint_data(state->log, flatten(r, reply));
	      }
	      if(DO_LOG_SUMMARY(state)) {
		fprintf(state->log, "#%i: ", node->id);
		fprint_d(state->log, flatten(r, log_msg));
		fprintf(state->log, ": ");
		fprint_d(state->log, flatten(r, log_reply));
		fprintf(state->log, "\n");
	      }
	      comm_send(r, node->proc->sock_fd, reply, reply_fds);
	      close_fds(fds);
	      close_fds(reply_fds);
	      region_free(r);
	    }
	    else break;
	  }
	}
      }
    }
  }
}
