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

#include <unistd.h>

#include "libc-comms.h"


static int my_atoi(const char *s)
{
  int x = 0;
  while(*s) {
    if(!('0' <= *s && *s <= '9')) return -1;
    x = x * 10 + *s - '0';
    s++;
  }
  return x;
}

/* EXPORT: new_getuid => WEAK:getuid __getuid */
int new_getuid()
{
  int uid;
  const char *str = getenv("PLASH_FAKE_UID");
  if(!str) return getuid();
  uid = my_atoi(str);
  if(uid < 0) return getuid();
  return uid;
}

/* EXPORT: new_getgid => WEAK:getgid __getgid */
int new_getgid()
{
  int gid;
  const char *str = getenv("PLASH_FAKE_GID");
  if(!str) return getgid();
  gid = my_atoi(str);
  if(gid < 0) return getgid();
  return gid;
}

/* EXPORT: new_geteuid => WEAK:geteuid __geteuid */
int new_geteuid()
{
  int euid;
  const char *str = getenv("PLASH_FAKE_EUID");
  if(!str) return geteuid();
  euid = my_atoi(str);
  if(euid < 0) return geteuid();
  return euid;
}

/* EXPORT: new_getegid => WEAK:getegid __getegid */
int new_getegid()
{
  int egid;
  const char *str = getenv("PLASH_FAKE_EGID");
  if(!str) return getegid();
  egid = my_atoi(str);
  if(egid < 0) return getegid();
  return egid;
}


/* These functions simply report success.  I don't see much point in
   having them check the process's faked UIDs and GIDs.
   When I add logging facilities, these should output a log message
   to indicate what the process is doing.  It's not reasonable to
   output anything to stderr. */

/* EXPORT: new_setuid => WEAK:setuid __setuid */
int new_setuid(uid_t uid)
{
  return 0;
}
/* EXPORT: new_setgid => WEAK:setgid __setgid */
int new_setgid(gid_t gid)
{
  return 0;
}

/* EXPORT: new_seteuid => seteuid __GI_seteuid */
int new_seteuid(uid_t euid)
{
  return 0;
}
/* EXPORT: new_setegid => setegid __GI_setegid */
int new_setegid(gid_t egid)
{
  return 0;
}

/* EXPORT: new_setreuid => WEAK:setreuid __setreuid */
int new_setreuid(uid_t ruid, uid_t euid)
{
  return 0;
}
/* EXPORT: new_setregid => WEAK:setregid __setregid */
int new_setregid(gid_t rgid, gid_t egid)
{
  return 0;
}

/* EXPORT: new_setresuid => WEAK:setresuid __setresuid __GI___setresuid */
int new_setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
  return 0;
}
/* EXPORT: new_setresgid => WEAK:setresgid __setresgid __GI___setresgid */
int new_setresgid(gid_t rgid, gid_t egid, gid_t sgid)
{
  return 0;
}


#include "out-aliases_libc-getuid.h"
