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

#include <unistd.h>

#include "libc-comms.h"


/* EXPORT: new_getuid => WEAK:getuid __getuid */
int new_getuid()
{
  int uid = my_atoi(getenv("PLASH_FAKE_UID"));
  libc_log("getuid");
  if(uid < 0) return getuid();
  return uid;
}

/* EXPORT: new_getgid => WEAK:getgid __getgid */
int new_getgid()
{
  int gid = my_atoi(getenv("PLASH_FAKE_GID"));
  libc_log("getgid");
  if(gid < 0) return getgid();
  return gid;
}

/* EXPORT: new_geteuid => WEAK:geteuid __geteuid */
int new_geteuid()
{
  int euid = my_atoi(getenv("PLASH_FAKE_EUID"));
  libc_log("geteuid");
  if(euid < 0) return geteuid();
  return euid;
}

/* EXPORT: new_getegid => WEAK:getegid __getegid */
int new_getegid()
{
  int egid = my_atoi(getenv("PLASH_FAKE_EGID"));
  libc_log("getegid");
  if(egid < 0) return getegid();
  return egid;
}


/* These functions simply report success.  I don't see much point in
   having them check the process's faked UIDs and GIDs. */

/* EXPORT: new_setuid => WEAK:setuid __setuid */
int new_setuid(uid_t uid)
{
  libc_log("setuid");
  return 0;
}
/* EXPORT: new_setgid => WEAK:setgid __setgid */
int new_setgid(gid_t gid)
{
  libc_log("setgid");
  return 0;
}

/* EXPORT: new_seteuid => seteuid __GI_seteuid */
int new_seteuid(uid_t euid)
{
  libc_log("seteuid");
  return 0;
}
/* EXPORT: new_setegid => setegid __GI_setegid */
int new_setegid(gid_t egid)
{
  libc_log("setegid");
  return 0;
}

/* EXPORT: new_setreuid => WEAK:setreuid __setreuid */
int new_setreuid(uid_t ruid, uid_t euid)
{
  libc_log("setreuid");
  return 0;
}
/* EXPORT: new_setregid => WEAK:setregid __setregid */
int new_setregid(gid_t rgid, gid_t egid)
{
  libc_log("setregid");
  return 0;
}

/* EXPORT: new_setresuid => WEAK:setresuid __setresuid __GI___setresuid */
int new_setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
  libc_log("setresuid");
  return 0;
}
/* EXPORT: new_setresgid => WEAK:setresgid __setresgid __GI___setresgid */
int new_setresgid(gid_t rgid, gid_t egid, gid_t sgid)
{
  libc_log("setresgid");
  return 0;
}


#include "out-aliases_libc-getuid.h"
