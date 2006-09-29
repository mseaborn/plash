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

#include "region.h"
#include "filesysobj.h"
#include "cap-protocol.h"
#include "marshal.h"


DECLARE_VTABLE(return_cont_vtable);
struct return_cont {
  struct filesys_obj hdr;
  /* This is set to null after the continuation is called: */
  struct return_state *state;
};

cap_t make_return_cont(struct return_state *state)
{
  struct return_cont *cont =
    filesys_obj_make(sizeof(struct return_cont), &return_cont_vtable);
  cont->state = state;
  return (struct filesys_obj *) cont;
}

/* Builds a "call" operation from the primitive non-returning "invoke"
   operation.  Creates a return continuation to pass to the object
   being invoked.  Waits for the return continuation to be invoked
   before returning. */
void generic_obj_call(struct filesys_obj *obj, region_t r,
		      struct cap_args args, struct cap_args *result)
{
  struct return_state *state = amalloc(sizeof(struct return_state));
  state->r = r;
  state->returned = 0;
  state->result = result;

  assert(result);

  {
    region_t r = region_make();
    cap_t *a = region_alloc(r, (args.caps.size + 1) * sizeof(cap_t));
    a[0] = make_return_cont(state);
    memcpy(a + 1, args.caps.caps, args.caps.size * sizeof(cap_t));
    obj->vtable->cap_invoke(obj,
			    cap_args_make(cat2(r, mk_string(r, "Call"),
					       args.data),
					  cap_seq_make(a, args.caps.size + 1),
					  args.fds));
    region_free(r);
  }

  while(!state->returned) {
    if(!cap_run_server_step()) { assert(0); }
  }
  free(state);
}

void local_obj_invoke(struct filesys_obj *obj, struct cap_args args)
{
  region_t r = region_make();
  seqf_t data = flatten_reuse(r, args.data);
  int ok = 1;
  m_int_const(&ok, &data, METHOD_CALL);
  if(ok && args.caps.size >= 1) {
    cap_t return_cont = args.caps.caps[0];
    struct cap_args result;
    
    /* Catch code that forgets to fill out all of the results. */
    result.data.size = -1;
    result.caps.size = -1;
    result.fds.count = -1;

    obj->vtable->cap_call(obj, r,
			  cap_args_make(mk_leaf(r, data),
					cap_seq_make(args.caps.caps + 1,
						     args.caps.size - 1),
					args.fds),
			  &result);

    assert(result.data.size >= 0);
    assert(result.caps.size >= 0);
    assert(result.fds.count >= 0);

    return_cont->vtable->cap_invoke(return_cont, result);
    filesys_obj_free(return_cont);
  }
  region_free(r);
  /* If the message didn't match it's simply ignored.
     There is nowhere to send an error to. */
}

void return_cont_invoke(struct filesys_obj *obj1, struct cap_args args)
{
  struct return_cont *obj = (void *) obj1;
  struct return_state *s = obj->state;
  if(s) {
    cap_t *c;
    int *f;

    s->result->data = mk_leaf(s->r, flatten(s->r, args.data));

    c = region_alloc(s->r, args.caps.size * sizeof(cap_t));
    memcpy(c, args.caps.caps, args.caps.size * sizeof(cap_t));
    s->result->caps.caps = c;
    s->result->caps.size = args.caps.size;

    f = region_alloc(s->r, args.fds.count * sizeof(int));
    memcpy(f, args.fds.fds, args.fds.count * sizeof(int));
    s->result->fds.fds = f;
    s->result->fds.count = args.fds.count;
    
    s->returned = 1;
    obj->state = 0;
  }
}

void return_cont_free(struct filesys_obj *obj1)
{
  struct return_cont *obj = (void *) obj1;
  struct return_state *s = obj->state;
  if(s) {
    s->result->data = mk_string(s->r, "ECon");
    s->result->caps = caps_empty;
    s->result->fds = fds_empty;
    
    s->returned = 1;
    obj->state = 0;
  }
}


#include "out-vtable-cap-call-return.h"
