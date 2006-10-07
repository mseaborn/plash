/* Copyright (C) 2006 Mark Seaborn

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

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "libc-comms.h"


/* EXPORT: new_getsockopt => WEAK:getsockopt __getsockopt */
int new_getsockopt(int sock_fd, int level, int opt_name,
		   void *opt_val, socklen_t *opt_len)
{
  int rc = getsockopt(sock_fd, level, opt_name, opt_val, opt_len);

  /* If SO_PEERCRED was used, and it returned the same non-faked UID
     and GID as this process, we change its result to the faked UID
     and GID, if they are defined. */
  if(level == SOL_SOCKET &&
     opt_name == SO_PEERCRED &&
     rc == 0 &&
     *opt_len >= sizeof(struct ucred))
  {
    struct ucred *creds = opt_val;
    if(creds->uid == getuid() &&
       creds->gid == getgid())
    {
      int fake_uid = my_atoi(getenv("PLASH_FAKEUID"));
      int fake_gid = my_atoi(getenv("PLASH_FAKEGID"));
      if(fake_uid >= 0) { creds->uid = fake_uid; }
      if(fake_gid >= 0) { creds->gid = fake_gid; }
    }
  }

  return rc;
}


#include "out-aliases_libc-getsockopt.h"
