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

#include "cap-utils.h"
#include "shell-options.h"
#include "marshal.h"


void print_option(region_t r, cap_t opts, const char *name)
{
  int x = get_option(r, opts, name);
  printf("%s = %s\n", name, x ? "on" : "off");
}

int main(int argc, char *argv[])
{
  region_t r;
  cap_t fs_server;

  if(argc != 2 && argc != 4) {
    printf(_("Usage: %s /x=shell_options [OPTION_NAME on/off]\n"), argv[0]);
    return 1;
  }

  if(get_process_caps("fs_op", &fs_server, 0) < 0) return 1;

  {
    cap_t opts;
    struct cap_args result;

    r = region_make();
    cap_call(fs_server, r,
	     cap_args_make(cat2(r, mk_int(r, METHOD_FSOP_GET_OBJ),
				mk_string(r, argv[1])),
			   caps_empty, fds_empty),
	     &result);
    filesys_obj_free(fs_server);
    if(expect_cap1(result, &opts) < 0) {
      fprintf(stderr, _("%s: couldn't get options object\n"), argv[0]);
      return 1;
    }

    if(argc == 2) {
      print_option(r, opts, "log_summary");
      print_option(r, opts, "log_into_xterm");
      print_option(r, opts, "print_fs_tree");
      print_option(r, opts, "enable_x11");
    }
    else if(argc == 4) {
      int x;
      if(!strcmp(argv[3], "on")) x = 1;
      else if(!strcmp(argv[3], "off")) x = 0;
      else {
	fprintf(stderr, _("value `%s' not recognised\n"), argv[3]);
	return 1;
      }
      set_option(r, opts, argv[2], x);
    }
    else assert(0);
  }
  return 0;
}
