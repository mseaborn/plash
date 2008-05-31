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

#include "cap-utils.h"


int expect_ok(struct cap_args args)
{
  region_t r = region_make();
  seqf_t data = flatten_reuse(r, args.data);
  int ok = 1;
  m_str(&ok, &data, "Okay");
  m_end(&ok, &data);
  if(ok && args.caps.size == 0 && args.fds.count == 0) {
    region_free(r);
    return 0;
  }
  region_free(r);
  caps_free(args.caps);
  close_fds(args.fds);
  return -1;
}

int expect_cap1(struct cap_args args, cap_t *c)
{
  region_t r = region_make();
  seqf_t data = flatten_reuse(r, args.data);
  int ok = 1;
  m_str(&ok, &data, "Okay");
  m_end(&ok, &data);
  if(ok && args.caps.size == 1 && args.fds.count == 0) {
    *c = args.caps.caps[0];
    region_free(r);
    return 0;
  }
  region_free(r);
  caps_free(args.caps);
  close_fds(args.fds);
  return -1;
}


int parse_cap_list(seqf_t list, seqf_t *elt, seqf_t *rest)
{
  if(list.size > 0) {
    int i = 0;
    while(i < list.size) {
      if(list.data[i] == ';') {
	elt->data = list.data;
	elt->size = i;
	rest->data = list.data + i + 1;
	rest->size = list.size - i - 1;
	return 1;
      }
      i++;
    }
    elt->data = list.data;
    elt->size = i;
    rest->data = 0;
    rest->size = 0;
    return 1;
  }
  else return 0;
}
