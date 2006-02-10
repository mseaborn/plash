/* Copyright (C) 2005 Mark Seaborn

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
#include "cap-utils.h"
#include "serialise.h"
#include "marshal.h"


int main(int argc, const char *argv[])
{
  region_t r = region_make();
  cap_t powerbox_req;
  int i;
  
  argmkbuf_t argbuf = argbuf_make(r);
  int args_size = 10;
  bufref_t args[args_size];
  int arg = 0;

  i = 1;
  while(i < argc) {
    assert(arg < args_size);
    if(!strcmp(argv[i], "--dir") && i + 2 <= argc) {
      args[arg++] =
	argmk_pair(argbuf,
		   argmk_str(argbuf, mk_string(r, "Startdir")),
		   argmk_str(argbuf, mk_string(r, argv[i+1])));
      i += 2;
    }
    else if(!strcmp(argv[i], "--desc") && i + 2 <= argc) {
      args[arg++] =
	argmk_pair(argbuf,
		   argmk_str(argbuf, mk_string(r, "Desc")),
		   argmk_str(argbuf, mk_string(r, argv[i+1])));
      i += 2;
    }
    else if(!strcmp(argv[i], "--save")) {
      bufref_t *a;
      args[arg++] = argmk_array(argbuf, 1, &a);
      a[0] = argmk_str(argbuf, mk_string(r, "Save"));
      i++;
    }
    else {
      fprintf(stderr, "powerbox-req: unknown argument, \"%s\"\n", argv[i]);
      return 1;
    }
  }

  if(get_process_caps("powerbox_req_filename", &powerbox_req,
		      NULL) < 0) return 1;

  {
    struct cap_args result;
    
    bufref_t *a;
    bufref_t arg_list = argmk_array(argbuf, arg, &a);
    for(i = 0; i < arg; i++) { a[i] = args[i]; }
    cap_call(powerbox_req, r,
	     cap_args_d(cat3(r, mk_int(r, METHOD_POWERBOX_REQ_FILENAME),
			     mk_int(r, arg_list),
			     argbuf_data(argbuf))),
	     &result);
    {
      seqf_t data = flatten_reuse(r, result.data);
      int ok = 1;
      m_int_const(&ok, &data, METHOD_POWERBOX_RESULT_FILENAME);
      if(ok) {
	/* OK, got filename */
	fprint_d(stdout, data);
	printf("\n");
	
	region_free(r);
	return 0; /* Success */
      }
    }
  }
  fprintf(stderr, "powerbox-req: error\n");
  return 1; /* Failure */
}
