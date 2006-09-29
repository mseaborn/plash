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

#ifndef INC_PLASH_LIBC_FDS_H
#define INC_PLASH_LIBC_FDS_H


/* libc needs to store information about file descriptors in some
   cases.  It keeps an array mapping file descriptor slot numbers to
   `struct libc_fd'.

   Directory FDs:  These are implemented by a Plash object, which is
   actually a dir_stack rather than a directory object.  This is used
   to implement fchdir(), and fstat() on directory FDs.  The
   corresponding kernel-level FD is a "dummy" FD, created by opening
   /dev/null.

   Unix domain socket FDs:  The pathname passed to connect() or bind()
   is stored by libc so that getsockname() can return the pathname.

   The following libc calls use this array:
    * open(), when used on a directory
    * close()
    * dup2()
    * fchdir()
    * fstat()
    * connect()
    * bind()
    * getsockname()
*/

struct libc_fd {
  cap_t fd_dir_obj; /* May be NULL */
  char *fd_socket_pathname; /* String allocated with malloc(), or NULL */
};

extern struct libc_fd *g_fds;
extern int g_fds_size;

void fds_resize(int fd);
void fds_slot_clear(int fd);


#endif
