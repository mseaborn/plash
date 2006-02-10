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

#include "region.h"
#include "filesysobj.h"
#include "cap-protocol.h"


struct return_state {
  region_t r;
  int returned;
  /* When `returned' is set, these are filled out: */
  seqt_t data;
  cap_seq_t cap_args;
  fds_t fd_args;
};

extern struct filesys_obj_vtable return_cont_vtable;
struct return_cont {
  struct filesys_obj hdr;
  /* This is set to null after the continuation is called: */
  struct return_state *state;
};

/* Builds a "call" operation from the primitive non-returning "invoke"
   operation.  Creates a return continuation to pass to the object
   being invoked.  Waits for the return continuation to be invoked
   before returning. */
void generic_obj_call(struct filesys_obj *obj, region_t r,
		      seqt_t data, cap_seq_t cap_args, fds_t fd_args,
		      seqt_t *r_data, cap_seq_t *r_cap_args, fds_t *r_fd_args)
{
  struct return_cont *cont = amalloc(sizeof(struct return_cont));
  struct return_state *state = amalloc(sizeof(struct return_state));
  cont->hdr.refcount = 1;
  cont->hdr.vtable = &return_cont_vtable;
  cont->state = state;
  state->r = r;
  state->returned = 0;

  assert(r_data);
  assert(r_cap_args);
  assert(r_fd_args);

  {
    region_t r = region_make();
    cap_seq_t args_seq;
    cap_t *args = region_alloc(r, (cap_args.size + 1) * sizeof(cap_t));
    args[0] = (struct filesys_obj *) cont;
    memcpy(args + 1, cap_args.caps, cap_args.size * sizeof(cap_t));
    args_seq.caps = args;
    args_seq.size = cap_args.size + 1;
    obj->vtable->cap_invoke(obj,
			    cat2(r, mk_string(r, "Call"), data),
			    args_seq, fd_args);
    region_free(r);
  }

  while(!state->returned) {
    cap_run_server_step();
  }
  *r_data = state->data;
  *r_cap_args = state->cap_args;
  *r_fd_args = state->fd_args;
  free(state);
}

void local_obj_invoke(struct filesys_obj *obj,
		      seqt_t data1, cap_seq_t cap_args, fds_t fd_args)
{
  region_t r = region_make();
  seqf_t data = flatten_reuse(r, data1);
  int ok = 1;
  cap_t return_cont;
  m_str(&ok, &data, "Call");
  if(ok) {
    if(cap_args.size >= 1) {
      return_cont = cap_args.caps[0];
      cap_args.caps++;
      cap_args.size--;
    }
    else ok = 0;
  }
  if(ok) {
    seqt_t r_data;
    cap_seq_t r_caps;
    fds_t r_fds;
    obj->vtable->cap_call(obj, r, mk_leaf(r, data), cap_args, fd_args,
			  &r_data, &r_caps, &r_fds);
    return_cont->vtable->cap_invoke(return_cont, r_data, r_caps, r_fds);
    filesys_obj_free(return_cont);
  }
  region_free(r);
  /* If the message didn't match it's simply ignored.
     There is nowhere to send an error to. */
}

void return_cont_invoke(struct filesys_obj *obj1,
			seqt_t data, cap_seq_t cap_args, fds_t fd_args)
{
  struct return_cont *obj = (void *) obj1;
  struct return_state *s = obj->state;
  if(s) {
    s->data = mk_leaf(s->r, flatten(s->r, data));

    s->cap_args.size = cap_args.size;
    s->cap_args.caps = region_alloc(s->r, cap_args.size * sizeof(cap_t));
    memcpy((void *) s->cap_args.caps, cap_args.caps, cap_args.size * sizeof(cap_t));

    s->fd_args.count = fd_args.count;
    s->fd_args.fds = region_alloc(s->r, fd_args.count * sizeof(int));
    memcpy((void *) s->fd_args.fds, fd_args.fds, fd_args.count * sizeof(int));
    
    s->returned = 1;
    obj->state = 0;
  }
}

void return_cont_free(struct filesys_obj *obj1)
{
  struct return_cont *obj = (void *) obj1;
  struct return_state *s = obj->state;
  if(s) {
    s->data = mk_string(s->r, "ECon");

    s->cap_args = caps_empty;
    s->fd_args = fds_empty;
    
    s->returned = 1;
    obj->state = 0;
  }
}


struct filesys_obj_vtable return_cont_vtable = {
  /* .type = */ 0,
  /* .free = */ return_cont_free,

  /* .cap_invoke = */ return_cont_invoke,
  /* .cap_call = */ generic_obj_call,
  /* .single_use = */ 1,
  
  /* .stat = */ dummy_stat,
  /* .utimes = */ dummy_utimes,
  /* .chmod = */ dummy_chmod,
  /* .open = */ dummy_open,
  /* .connect = */ dummy_socket_connect,
  /* .traverse = */ dummy_traverse,
  /* .list = */ dummy_list,
  /* .create_file = */ dummy_create_file,
  /* .mkdir = */ dummy_mkdir,
  /* .symlink = */ dummy_symlink,
  /* .rename = */ dummy_rename_or_link,
  /* .link = */ dummy_rename_or_link,
  /* .unlink = */ dummy_unlink,
  /* .rmdir = */ dummy_rmdir,
  /* .socket_bind = */ dummy_socket_bind,
  /* .readlink = */ dummy_readlink,
  1
};
