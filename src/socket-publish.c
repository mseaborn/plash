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

/* This removes warning about calling bind() */
/* #define __USE_GNU */
#define _GNU_SOURCE

#include <errno.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cap-protocol.h"
#include "cap-utils.h"


int main(int argc, char *argv[])
{
  region_t r;
  cap_t fs_server;
  cap_t obj;
  struct cap_args result;

  if(argc != 3) {
    printf(_("Usage: %s socket-pathname object-pathname\n"), argv[0]);
    return 1;
  }

  if(get_process_caps("fs_op", &fs_server, 0) < 0) return 1;

  r = region_make();
  cap_call(fs_server, r,
	   cap_args_make(cat2(r, mk_string(r, "Gobj"),
			      mk_string(r, argv[2])),
			 caps_empty, fds_empty),
	   &result);
  filesys_obj_free(fs_server);
  if(expect_cap1(result, &obj) < 0) {
    fprintf(stderr, _("%s: couldn't get object from pathname `%s'\n"),
	    argv[0], argv[2]);
    return 1;
  }

  {
    const char *pathname = argv[1];
    int len = strlen(pathname);
    struct sockaddr_un name;
    int sock_fd;
    struct stat st;

    /* If a socket already exists at the given pathname, remove it.
       I was thinking about making this optional, but it makes more sense
       as the default.  It's fairly safe -- it won't remove any files.
       Note, however, that there are some race conditions:
       between checking the filename and unlink it;
       and also, there will be a race between unlinking the object and
       putting another socket in its place.
       Maybe it would be better to rename the new socket on top of the
       old one.  Unfortunately, this doesn't work if the program has only
       been granted write/create access to one slot. */
    if(stat(pathname, &st) >= 0) {
      if(S_ISSOCK(st.st_mode)) {
	if(unlink(pathname) < 0) {
	  perror("unlink");
	}
      }
    }
    
    sock_fd = socket(PF_LOCAL, SOCK_STREAM, 0);
    if(sock_fd < 0) { perror("socket"); return 1; }

    name.sun_family = AF_LOCAL;
    memcpy(name.sun_path, pathname, len);
    name.sun_path[len] = 0;
    if(bind(sock_fd, &name,
	    offsetof(struct sockaddr_un, sun_path) + len + 1) < 0) {
      fprintf(stderr, _("%s: can't bind socket at `%s': %s\n"),
	      argv[0], pathname, strerror(errno));
      return 1;
    }
    if(listen(sock_fd, 10) < 0) {
      perror("listen");
      return 1;
    }
    printf(_("listening...\n"));
    while(1) {
      int max_fd;
      fd_set read_fds, write_fds, except_fds;
      FD_ZERO(&read_fds);
      FD_ZERO(&write_fds);
      FD_ZERO(&except_fds);

      /* Unfortunately, we only get a notification when there's a
	 connection on the socket.  When the socket file is deleted,
	 we don't get any notification (whether as read/write/except).
	 So there is no way to terminate this loop.
	 You have to kill the process.
	 
	 I suppose I could have it stat() the socket file every so
	 often, and if the inode number has changed, that means someone
	 has unlinked our socket and replaced it with a new one.
	 But it could just mean our socket has been rename()'d.

	 Another possibility is to hard link the socket somewhere else
	 (eg. into /tmp) and stat() it every so often.  When st_nlink
	 drops to 1, there are no other references, and we can delete
	 the socket and exit. */
      
      max_fd = sock_fd + 1;
      FD_SET(sock_fd, &read_fds);

      cap_add_select_fds(&max_fd, &read_fds, &write_fds, &except_fds);
      
      if(select(max_fd, &read_fds, &write_fds, &except_fds,
		NULL /* timeout */) < 0) {
	perror("select");
	return 1;
      }
      cap_handle_select_result(&read_fds, &write_fds, &except_fds);

      if(FD_ISSET(sock_fd, &read_fds)) {
	int fd;
	printf(_("getting connection... "));
	fd = accept(sock_fd, NULL /* address */, NULL);
	if(fd < 0) {
	  perror("accept");
	}
	else {
	  printf("done\n");
	  cap_make_connection(r, fd, mk_caps1(r, obj), 0, "server");
	}
      }
    }
  }
  return 0;
}
