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

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
/* #include <dirent.h> We have our own types */
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <arpa/inet.h>

#include "region.h"
#include "comms.h"
#include "libc-comms.h"
#include "libc-fds.h"
#include "marshal.h"


export_weak_alias(new_connect, connect);
export_weak_alias(new_connect, __connect);
export(new_connect, __libc_connect);
export(new_connect, __connect_internal);

int new_connect(int sock_fd, const struct sockaddr *addr, socklen_t addr_len)
{
  if(!addr) { __set_errno(EINVAL); return -1; }
  if(addr->sa_family == AF_LOCAL) {
    region_t r;
    seqf_t reply;
    fds_t reply_fds;
    struct sockaddr_un *addr2 = (void *) addr;
    int sock_fd_copy = dup(sock_fd);
    if(sock_fd_copy < 0) return -1;
    r = region_make();
    if(req_and_reply_with_fds2(r, cat2(r, mk_int(r, METHOD_FSOP_CONNECT),
				       mk_string(r, addr2->sun_path)),
			       mk_fds1(r, sock_fd_copy),
			       &reply, &reply_fds) < 0) goto error;
    close_fds(reply_fds);
    {
      seqf_t msg = reply;
      int ok = 1;
      m_int_const(&ok, &msg, METHOD_OKAY);
      m_end(&ok, &msg);
      if(ok) {
	/* Store the pathname for later, so that getsockname() can
	   return the correct pathname. */
	fds_resize(sock_fd);
	if(g_fds[sock_fd].fd_socket_pathname) {
	  /* Should give a warning here, because we don't expect this
	     field to have been filled out already. */
	  free(g_fds[sock_fd].fd_socket_pathname);
	}
	g_fds[sock_fd].fd_socket_pathname = strdup(addr2->sun_path);
	
	region_free(r);
	return 0;
      }
    }
    {
      seqf_t msg = reply;
      int err;
      int ok = 1;
      m_int_const(&ok, &msg, METHOD_FAIL);
      m_int(&ok, &msg, &err);
      m_end(&ok, &msg);
      if(ok) {
	__set_errno(err);
	goto error;
      }
    }
    
    __set_errno(ENOSYS);
  error:
    region_free(r);
    return -1;
  }
  else {
    if(addr->sa_family == AF_INET) {
      struct sockaddr_in *addr2 = (void *) addr;
      char addr_buf[50];
      char buf[200];
      inet_ntop(addr2->sin_family, &addr2->sin_addr,
		addr_buf, sizeof(addr_buf));
      snprintf(buf, sizeof(buf),
	       "connect AF_INET, %s:%i", addr_buf, addr2->sin_port);
      libc_log(buf);
    }
    else {
      libc_log("connect AF_?");
    }
    
    return connect(sock_fd, addr, addr_len);
  }
}


export(new_bind, bind);
export_weak_alias(new_bind, __bind);

int new_bind(int sock_fd, struct sockaddr *addr, socklen_t addr_len)
{
  if(!addr) { __set_errno(EINVAL); return -1; }
  if(addr->sa_family == AF_LOCAL) {
    region_t r;
    seqf_t reply;
    fds_t reply_fds;
    struct sockaddr_un *addr2 = (void *) addr;
    int sock_fd_copy = dup(sock_fd);
    if(sock_fd_copy < 0) return -1;
    r = region_make();
    if(req_and_reply_with_fds2(r, cat2(r, mk_int(r, METHOD_FSOP_BIND),
				       mk_string(r, addr2->sun_path)),
			       mk_fds1(r, sock_fd_copy),
			       &reply, &reply_fds) < 0) goto error;
    close_fds(reply_fds);
    {
      seqf_t msg = reply;
      int ok = 1;
      m_int_const(&ok, &msg, METHOD_OKAY);
      m_end(&ok, &msg);
      if(ok) {
	/* Store the pathname for later, so that getsockname() can
	   return the correct pathname. */
	fds_resize(sock_fd);
	if(g_fds[sock_fd].fd_socket_pathname) {
	  /* Should give a warning here, because we don't expect this
	     field to have been filled out already. */
	  free(g_fds[sock_fd].fd_socket_pathname);
	}
	g_fds[sock_fd].fd_socket_pathname = strdup(addr2->sun_path);
	
	region_free(r);
	return 0;
      }
    }
    {
      seqf_t msg = reply;
      int err;
      int ok = 1;
      m_int_const(&ok, &msg, METHOD_FAIL);
      m_int(&ok, &msg, &err);
      m_end(&ok, &msg);
      if(ok) {
	__set_errno(err);
	goto error;
      }
    }
    
    __set_errno(ENOSYS);
  error:
    region_free(r);
    return -1;
  }
  else {
    if(addr->sa_family == AF_INET) {
      struct sockaddr_in *addr2 = (void *) addr;
      char addr_buf[50];
      char buf[200];
      inet_ntop(addr2->sin_family, &addr2->sin_addr,
		addr_buf, sizeof(addr_buf));
      snprintf(buf, sizeof(buf),
	       "bind AF_INET, %s:%i", addr_buf, addr2->sin_port);
      libc_log(buf);
    }
    else {
      libc_log("bind AF_?");
    }
    
    return bind(sock_fd, addr, addr_len);
  }
}


export(my_getsockname, getsockname);
export_weak_alias(my_getsockname, __getsockname);

int my_getsockname(int sock_fd, struct sockaddr *name, socklen_t *name_len)
{
  /* Try return a filename stored by connect() or bind(). */
  if(0 <= sock_fd && sock_fd < g_fds_size) {
    char *pathname = g_fds[sock_fd].fd_socket_pathname;
    if(pathname) {
      struct sockaddr_un *name2 = (void *) name;
      int len = strlen(pathname);
      int need = offsetof(struct sockaddr_un, sun_path) + len + 1;
      if(!name_len) { __set_errno(EINVAL); return -1; }
      if(need > *name_len) { __set_errno(ENAMETOOLONG); return -1; }
      name2->sun_family = AF_LOCAL;
      memcpy(name2->sun_path, pathname, len + 1);
      *name_len = need;
      
      libc_log("getsockname: returned filename");
      return 0;
    }
  }
  
  return getsockname(sock_fd, name, name_len);
}
