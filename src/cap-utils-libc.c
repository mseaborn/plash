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

#include <stdarg.h>
#include <dlfcn.h>

#include "cap-protocol.h"
#include "cap-utils.h"
#include "plash-libc.h"


/* Don't need this, use weak symbols. */
#if 0
int plash_libc_duplicate_connection2()
{
  void *h;
  int (*f)();
  int result;
  
  h = dlopen("libc.so.6", RTLD_LAZY);
  if(!h) { return -1; }
  f = dlsym(h, "plash_libc_duplicate_connection");
  if(!f) { return -1; }
  result = f();
  dlclose(h);
  return result;
}
#endif

__asm__(".weak plash_libc_duplicate_connection");


/* This takes a list of pairs:
   const char *name, cap_t *dest
   It fills out *dest with the capability in the `name' slot. */
/* Returns -1 on failure. */
int get_process_caps(const char *arg, ...)
{
  region_t r;
  char *var;
  int sock_fd;
  int count, i;
  seqf_t cap_list, elt, list;
  cap_t *caps;

  var = getenv("PLASH_CAPS");
  if(!var) { fprintf(stderr, "PLASH_CAPS variable is not set\n"); return 1; }
  if(!plash_libc_duplicate_connection) {
    fprintf(stderr, "plash_libc_duplicate_connection not defined\n");
    return -1;
  }
  sock_fd = plash_libc_duplicate_connection();
  if(sock_fd < 0) { fprintf(stderr, "plash_libc_duplicate_connection() failed\n"); return 1; }

  cap_list = seqf_string(var);
  list = cap_list;
  count = 0;
  /* Count the number of capabilities listed */
  while(parse_cap_list(list, &elt, &list)) count++;

  r = region_make();
  caps = cap_make_connection(r, sock_fd, caps_empty, count, "to-server");

  /* Unpack the capabilities */
  {
    const char *name = arg;
    cap_t *dest;
    va_list args;
    va_start(args, arg);
    while(name) {
      dest = va_arg(args, cap_t *);
    
      list = cap_list;
      i = 0;
      while(parse_cap_list(list, &elt, &list)) {
	assert(i < count);
	if(seqf_equal(elt, seqf_string(name))) {
	  *dest = inc_ref(caps[i]);
	  goto found;
	}
	i++;
      }
      fprintf(stderr, "cap `%s' not present\n", name);
      for(i = 0; i < count; i++) filesys_obj_free(caps[i]);
      /* FIXME: the references that were copied will not be freed */
      region_free(r);
      return -1;
    found:

      name = va_arg(args, const char *);
    }
  }
  for(i = 0; i < count; i++) filesys_obj_free(caps[i]);
  region_free(r);
  return 0;
}
