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

#include "region.h"
#include "comms.h"
#include "libc-comms.h"


/* EXPORT: new_connect => WEAK:connect WEAK:__connect __libc_connect __connect_internal */
int new_connect(int sock_fd, const struct sockaddr *addr, socklen_t addr_len)
{
  if(!addr) { __set_errno(EINVAL); return -1; }
  if(addr->sa_family == AF_LOCAL) {
    region_t r = region_make();
    seqf_t reply;
    fds_t reply_fds;
    struct sockaddr_un *addr2 = (void *) addr;
    if(req_and_reply_with_fds2(r, cat2(r, mk_string(r, "Fcon"),
				       mk_string(r, addr2->sun_path)),
			       mk_fds1(r, sock_fd),
			       &reply, &reply_fds) < 0) goto error;
    close_fds(reply_fds);
    {
      seqf_t msg = reply;
      int ok = 1;
      m_str(&ok, &msg, "RFco");
      m_end(&ok, &msg);
      if(ok) {
	region_free(r);
	return 0;
      }
    }
    {
      seqf_t msg = reply;
      int err;
      int ok = 1;
      m_str(&ok, &msg, "Fail");
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
    return connect(sock_fd, addr, addr_len);
  }
}

/* EXPORT: new_bind => bind WEAK:__bind */
int new_bind(int sock_fd, struct sockaddr *addr, socklen_t addr_len)
{
  if(!addr) { __set_errno(EINVAL); return -1; }
  if(addr->sa_family == AF_LOCAL) {
    region_t r = region_make();
    seqf_t reply;
    fds_t reply_fds;
    struct sockaddr_un *addr2 = (void *) addr;
    if(req_and_reply_with_fds2(r, cat2(r, mk_string(r, "Fbnd"),
				       mk_string(r, addr2->sun_path)),
			       mk_fds1(r, sock_fd),
			       &reply, &reply_fds) < 0) goto error;
    close_fds(reply_fds);
    {
      seqf_t msg = reply;
      int ok = 1;
      m_str(&ok, &msg, "RFbd");
      m_end(&ok, &msg);
      if(ok) {
	region_free(r);
	return 0;
      }
    }
    {
      seqf_t msg = reply;
      int err;
      int ok = 1;
      m_str(&ok, &msg, "Fail");
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
    return bind(sock_fd, addr, addr_len);
  }
}


#include "out-aliases_libc-connect.h"
