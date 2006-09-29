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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA.  */


#define weak_alias(name, aliasname) \
  extern int aliasname() __attribute ((weak, alias (#name)));

int __libc_open(const char *filename, int flags, int mode);
int __libc_open64(const char *filename, int flags, int mode);

int __open(const char *filename, int flags, int mode)
{
  return __libc_open(filename, flags, mode);
}

int __open64(const char *filename, int flags, int mode)
{
  return __libc_open64(filename, flags, mode);
}

weak_alias(__open, open)
weak_alias(__open64, open64)


struct sockaddr;
typedef int socklen_t;
int __libc_connect(int sock_fd, const struct sockaddr *addr, socklen_t addr_len);

int __connect(int sock_fd, const struct sockaddr *addr, socklen_t addr_len)
{
  return __libc_connect(sock_fd, addr, addr_len);
}
weak_alias(__connect, connect)

int __libc_accept(int sock_fd, struct sockaddr *addr, socklen_t addr_len);

int __accept(int sock_fd, struct sockaddr *addr, socklen_t addr_len)
{
  return __libc_accept(sock_fd, addr, addr_len);
}
weak_alias(__accept, accept)


int __libc_close(int fd);

int __close(int fd)
{
  return __libc_close(fd);
}
weak_alias(__close, close)
