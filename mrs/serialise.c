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

#include <unistd.h>

#include "serialise.h"


cbuf_t cbuf_make(region_t r, int initial_size)
{
  struct contiguous_buf *buf = region_alloc(r, sizeof(struct contiguous_buf));
  buf->r = r;
  buf->got = seqt_empty;
  buf->start = region_alloc(r, initial_size);
  buf->pos = buf->start;
  buf->end = buf->start + initial_size;
  return buf;
}

void *cbuf_alloc(cbuf_t buf, int size)
{
  char *cur_pos = buf->pos;
  char *new_pos = cur_pos + size;
  if(new_pos <= buf->end) {
    /* If there's enough space in the current chunk, just move pointer on. */
    buf->pos = new_pos;
    return cur_pos;
  }
  else {
    int new_size = 1024;
    
    /* Add current chunk to list. */
    seqt_t chunk = mk_leaf2(buf->r, buf->start, buf->pos - buf->start);
    if(buf->got.size == 0) {
      buf->got = chunk;
    }
    else {
      buf->got = cat2(buf->r, buf->got, chunk);
    }

    if(new_size < size) new_size = size;
    buf->start = region_alloc(buf->r, new_size);
    buf->pos = buf->start + size;
    buf->end = buf->start + new_size;
    return buf->start;
  }
}

seqt_t seqt_of_cbuf(cbuf_t buf)
{
  seqt_t chunk = mk_leaf2(buf->r, buf->start, buf->pos - buf->start);
  if(buf->got.size == 0) {
    return chunk;
  }
  else {
    return cat2(buf->r, buf->got, chunk);
  }
}

int cbuf_size(cbuf_t buf)
{
  return buf->got.size + (buf->pos - buf->start);
}



/* References:  the lower three bits give the type.
   The upper bits give an address.
   For ints and arrays, the address is a byte offset into the data section.
   For caps and FDs, the addresses are indexes into the cap or FD arrays.

   Ints are boxed.
   The array data consists of a size followed by references for each of
   the array elements.
*/
#define TYPE_INT	0
#define TYPE_STRING	1
#define TYPE_ARRAY	2
#define TYPE_CAP	3
#define TYPE_FD		4
#define MAKE_REF(type, addr)  ((addr) << 3 | (type))
#define REF_TYPE(r)	((r) & 0x7)
#define REF_ADDR(r)	((r) >> 3)


argmkbuf_t argbuf_make(region_t r)
{
  struct arg_mk_buf *buf = region_alloc(r, sizeof(struct arg_mk_buf));
  buf->data = cbuf_make(r, 200);
  buf->caps = cbuf_make(r, 5 * sizeof(cap_t));
  buf->fds = cbuf_make(r, 5 * sizeof(int));
  buf->caps_got = 0;
  buf->fds_got = 0;
  return buf;
}

bufref_t argmk_int(argmkbuf_t buf, int i)
{
  int index = cbuf_size(buf->data);
  cbuf_put_int(buf->data, i);
  return MAKE_REF(TYPE_INT, index);
}

bufref_t argmk_str(argmkbuf_t buf, seqt_t data)
{
  int index = cbuf_size(buf->data);
  char *d;
  cbuf_put_int(buf->data, data.size);
  d = cbuf_alloc(buf->data, data.size);
  flatten_into(d, data);
  return MAKE_REF(TYPE_STRING, index);
}

bufref_t argmk_cap(argmkbuf_t buf, cap_t c)
{
  int index = buf->caps_got++;
  cap_t *p = cbuf_alloc(buf->caps, sizeof(cap_t));
  *p = c;
  return MAKE_REF(TYPE_CAP, index);
}

bufref_t argmk_fd(argmkbuf_t buf, int fd)
{
  int index = buf->fds_got++;
  cbuf_put_int(buf->fds, fd);
  return MAKE_REF(TYPE_FD, index);
}

bufref_t argmk_array(argmkbuf_t buf, int size, bufref_t **out)
{
  int index = cbuf_size(buf->data);
  int *p = cbuf_alloc(buf->data, (1 + size) * sizeof(int));
  *p = size;
  *out = p + 1;
  return MAKE_REF(TYPE_ARRAY, index);
}

seqt_t argbuf_data(argmkbuf_t buf)
{
  return seqt_of_cbuf(buf->data);
}

cap_seq_t argbuf_caps(argmkbuf_t buf)
{
  seqf_t data = flatten_reuse(buf->caps->r, seqt_of_cbuf(buf->caps));
  cap_seq_t r = { (cap_t *) data.data, buf->caps_got };
  return r;
}

fds_t argbuf_fds(argmkbuf_t buf)
{
  seqf_t data = flatten_reuse(buf->fds->r, seqt_of_cbuf(buf->fds));
  fds_t r = { (int *) data.data, buf->fds_got };
  return r;
}

void argbuf_free_refs(argmkbuf_t buf)
{
  caps_free(argbuf_caps(buf));
  close_fds(argbuf_fds(buf));
}



int argm_int(argmbuf_t buf, bufref_t x, int *i)
{
  if(REF_TYPE(x) == TYPE_INT &&
     0 <= REF_ADDR(x) && REF_ADDR(x) + sizeof(int) <= buf->data.size) {
    *i = *(int *) (buf->data.data + REF_ADDR(x));
    return 0;
  }
  return -1;
}

int argm_str(argmbuf_t buf, bufref_t x, seqf_t *r)
{
  if(REF_TYPE(x) == TYPE_STRING &&
     0 <= REF_ADDR(x) && REF_ADDR(x) + sizeof(int) <= buf->data.size) {
    int size = *(int *) (buf->data.data + REF_ADDR(x));
    if(REF_ADDR(x) + sizeof(int) + size <= buf->data.size) {
      r->size = size;
      r->data = buf->data.data + REF_ADDR(x) + sizeof(int);
      return 0;
    }
  }
  return -1;
}

int argm_cap(argmbuf_t buf, bufref_t x, cap_t *c)
{
  if(REF_TYPE(x) == TYPE_CAP &&
     0 <= REF_ADDR(x) && REF_ADDR(x) < buf->caps.size) {
    *c = buf->caps.caps[REF_ADDR(x)];
    return 0;
  }
  return -1;
}

int argm_fd(argmbuf_t buf, bufref_t x, int *fd)
{
  if(REF_TYPE(x) == TYPE_FD &&
     0 <= REF_ADDR(x) && REF_ADDR(x) < buf->fds.count) {
    *fd = buf->fds.fds[REF_ADDR(x)];
    return 0;
  }
  return -1;
}

int argm_array(argmbuf_t buf, bufref_t x, int *size, const bufref_t **out)
{
  if(REF_TYPE(x) == TYPE_ARRAY &&
     0 <= REF_ADDR(x) && REF_ADDR(x) + sizeof(int) <= buf->data.size) {
    int s = *(int *) (buf->data.data + REF_ADDR(x));
    if(REF_ADDR(x) + (1 + s) * sizeof(int) <= buf->data.size) {
      *size = s;
      *out = (int *) (buf->data.data + REF_ADDR(x) + sizeof(int));
      return 0;
    }
  }
  return -1;
}
