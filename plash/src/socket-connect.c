/* Copyright (C) 2005 Mark Seaborn

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

#include <errno.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "cap-protocol.h"
#include "cap-utils.h"
#include "marshal.h"


int main(int argc, char *argv[])
{
  region_t r;
  cap_t return_cont;

  if(argc != 2) {
    printf(_("Usage: %s socket-pathname\n"), argv[0]);
    return 1;
  }

  if(get_process_caps("return_cont", &return_cont, NULL) < 0)
    return 1;

  {
    const char *pathname = argv[1];
    int len = strlen(pathname);
    struct sockaddr_un name;
    int sock_fd;
    cap_t *objs;
    
    sock_fd = socket(PF_LOCAL, SOCK_STREAM, 0);
    if(sock_fd < 0) { perror("socket"); return 1; }

    name.sun_family = AF_LOCAL;
    memcpy(name.sun_path, pathname, len);
    name.sun_path[len] = 0;
    if(connect(sock_fd, &name,
	       offsetof(struct sockaddr_un, sun_path) + len + 1) < 0) {
      fprintf(stderr, _("%s: can't connect to socket at `%s': %s\n"),
	      argv[0], pathname, strerror(errno));
      return 1;
    }

    r = region_make();
    objs = cap_make_connection(r, sock_fd, caps_empty, 1, "client");

    cap_invoke(return_cont,
	       cap_args_make(mk_int(r, METHOD_OKAY),
			     mk_caps1(r, objs[0]),
			     fds_empty));
    cap_run_server();
  }
  return 0;
}
