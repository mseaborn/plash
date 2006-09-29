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

#ifndef serialise_h
#define serialise_h


#include "region.h"
#include "filesysobj.h"


/* Cbufs are used for creating chunks of data where the size of the data
   may not be known in advance. */

struct contiguous_buf {
  region_t r;
  seqt_t got;
  char *start, *pos, *end;
};
typedef struct contiguous_buf *cbuf_t;

cbuf_t cbuf_make(region_t r, int initial_size);
void *cbuf_alloc(cbuf_t buf, int size);
seqt_t seqt_of_cbuf(cbuf_t buf);
int cbuf_size(cbuf_t buf);

static inline void cbuf_put_int(cbuf_t buf, int x)
{
  int *p = cbuf_alloc(buf, sizeof(int));
  *p = x;
}

static inline void cbuf_put_seqf(cbuf_t buf, seqf_t data)
{
  char *x = cbuf_alloc(buf, data.size);
  memcpy(x, data.data, data.size);
}


/* Building trees */

struct arg_mk_buf {
  cbuf_t data;
  cbuf_t caps;
  cbuf_t fds;
  int caps_got, fds_got;
};
typedef struct arg_mk_buf *argmkbuf_t;

typedef int bufref_t;

argmkbuf_t argbuf_make(region_t r);

/* Constructors: */
bufref_t argmk_int(argmkbuf_t buf, int i);
bufref_t argmk_str(argmkbuf_t buf, seqt_t data);
bufref_t argmk_cap(argmkbuf_t buf, cap_t c); /* takes owning reference */
bufref_t argmk_fd(argmkbuf_t buf, int fd); /* takes owning reference */
bufref_t argmk_array(argmkbuf_t buf, int size, bufref_t **out);

static inline bufref_t argmk_pair(argmkbuf_t buf, bufref_t x0, bufref_t x1)
{
  bufref_t *a;
  bufref_t r = argmk_array(buf, 2, &a);
  a[0] = x0;
  a[1] = x1;
  return r;
}

seqt_t argbuf_data(argmkbuf_t buf);
cap_seq_t argbuf_caps(argmkbuf_t buf);
fds_t argbuf_fds(argmkbuf_t buf);

void argbuf_free_refs(argmkbuf_t buf);


struct arg_m_buf {
  seqf_t data;
  cap_seq_t caps;
  fds_t fds;
};
typedef struct arg_m_buf *argmbuf_t;

/* Unpacking trees */
/* These return non-zero if unsuccessful */
int argm_int(argmbuf_t buf, bufref_t x, int *i);
int argm_str(argmbuf_t buf, bufref_t x, seqf_t *r);
int argm_cap(argmbuf_t buf, bufref_t x, cap_t *c); /* returns non-owning ref */
int argm_fd(argmbuf_t buf, bufref_t x, int *fd); /* returns non-owning ref */
int argm_array(argmbuf_t buf, bufref_t x, int *size, const bufref_t **out);

static inline int argm_pair(argmbuf_t buf, bufref_t x, bufref_t *x0, bufref_t *x1)
{
  int size;
  const bufref_t *a;
  if(argm_array(buf, x, &size, &a)) return -1;
  if(size != 2) return -1;
  *x0 = a[0];
  *x1 = a[1];
  return 0;
}


void arg_print(FILE *f, argmbuf_t buf, bufref_t x);


#endif
