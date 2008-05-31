/* Copyright (C) 2004, 2005 Mark Seaborn

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
#include "cap-utils.h"
#include "serialise.h"
#include "marshal.h"


void set_option(region_t r, cap_t opts, const char *name, int v)
{
  struct cap_args result;
  argmkbuf_t argbuf = argbuf_make(r);
  bufref_t arg = argmk_pair(argbuf, argmk_str(argbuf, mk_string(r, name)),
			    argmk_int(argbuf, v));
  cap_call(opts, r,
	   cap_args_make(cat3(r, mk_int(r, METHOD_SET_OPTION),
			      mk_int(r, arg),
			      argbuf_data(argbuf)),
			 argbuf_caps(argbuf),
			 argbuf_fds(argbuf)),
	   &result);
  if(expect_ok(result) < 0) {
    fprintf(stderr, _("error setting option `%s'\n"), name);
  }
}

int get_option(region_t r, cap_t opts, const char *name)
{
  struct cap_args result;
  cap_call(opts, r,
	   cap_args_make(cat2(r, mk_int(r, METHOD_GET_OPTION),
			      mk_string(r, name)),
			 caps_empty, fds_empty),
	   &result);
  {
    seqf_t msg = flatten_reuse(r, result.data);
    int ok = 1;
    bufref_t ref;
    m_int_const(&ok, &msg, METHOD_OKAY);
    m_int(&ok, &msg, &ref);
    if(ok) {
      struct arg_m_buf argbuf = { msg, result.caps, result.fds };
      int i;
      if(!argm_int(&argbuf, ref, &i)) {
	// printf("%s = %i\n", name, i);
	caps_free(result.caps);
	close_fds(result.fds);
	return i;
      }
    }
    fprintf(stderr, _("can't get option `%s'\n"), name);
    caps_free(result.caps);
    close_fds(result.fds);
    return 0;
  }
}
