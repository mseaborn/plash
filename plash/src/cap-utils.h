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

#ifndef cap_utils_h
#define cap_utils_h

#include "filesysobj.h"


static inline void cap_call(cap_t c, region_t r,
			    struct cap_args args, struct cap_args *result)
{
  c->vtable->cap_call(c, r, args, result);
}

static inline void cap_invoke(cap_t c, struct cap_args args)
{
  c->vtable->cap_invoke(c, args);
}

int expect_ok(struct cap_args args);
int expect_cap1(struct cap_args args, cap_t *c);

int parse_cap_list(seqf_t list, seqf_t *elt, seqf_t *rest);
int get_process_caps(const char *arg, ...) __attribute__((sentinel));


#endif
