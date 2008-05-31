/* Copyright (C) 2006 Mark Seaborn

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

#include <unistd.h>
#include <fcntl.h>

#include "log.h"


struct log_shared {
  int refcount;
  
  FILE *fp;
  int next_id;
};

DECLARE_VTABLE(log_stream_vtable);
struct log_stream {
  struct filesys_obj hdr;
  
  struct log_shared *shared;
  int id;
};


static void shared_free(struct log_shared *shared)
{
  assert(shared->refcount > 0);
  if(--shared->refcount == 0) {
    fclose(shared->fp);
    free(shared);
  }
}

static void log_free(struct filesys_obj *obj)
{
  struct log_stream *log = (void *) obj;

  fprintf(log->shared->fp, "#%i end\n", log->id);

  shared_free(log->shared);
}

static void log_msg(struct filesys_obj *obj, seqf_t msg)
{
  struct log_stream *log = (void *) obj;

  region_t r = region_make();
  fprintf(log->shared->fp, "#%i: %s\n",
	  log->id, region_strdup_seqf(r, msg));
  region_free(r);
}

static struct filesys_obj *log_branch(struct filesys_obj *obj, seqf_t msg)
{
  struct log_stream *log = (void *) obj;

  struct log_stream *log2;
  int new_id = log->shared->next_id++;

  region_t r = region_make();
  fprintf(log->shared->fp, "#%i branch to #%i: %s\n",
	  log->id, new_id,
	  region_strdup_seqf(r, msg));
  region_free(r);

  /* Create new log stream object */
  log2 = filesys_obj_make(sizeof(struct log_stream), &log_stream_vtable);
  log->shared->refcount++;
  log2->shared = log->shared;
  log2->id = new_id;
  
  return (struct filesys_obj *) log2;
}


/* Takes ownership of fp. */
struct filesys_obj *make_log(FILE *fp)
{
  struct log_shared *shared;
  struct log_stream *log;
  
  shared = amalloc(sizeof(struct log_shared));
  shared->refcount = 1;
  shared->fp = fp;
  shared->next_id = 1;

  log = filesys_obj_make(sizeof(struct log_stream), &log_stream_vtable);
  log->shared = shared;
  log->id = shared->next_id++;

  return (struct filesys_obj *) log;
}

/* Creates a log object that outputs to the given file descriptor.
   Does not take ownership of "fd".  Returns NULL for error. */
struct filesys_obj *make_log_from_fd(int fd)
{
  FILE *log_fp;
  int fd_copy = dup(fd);
  if(fd_copy < 0) {
    return NULL;
  }
  if(fcntl(fd_copy, F_SETFD, FD_CLOEXEC) < 0) {
    close(fd_copy);
    return NULL;
  }
  log_fp = fdopen(fd_copy, "w");
  if(!log_fp) {
    close(fd_copy);
    return NULL;
  }
  setvbuf(log_fp, 0, _IONBF, 0);
  
  return make_log(log_fp);
}


#include "out-vtable-log.h"
