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

#include "kernel-fd-ops.h"
#include "libc-comms.h"


__typeof__(getuid) new_getuid;
__typeof__(getgid) new_getgid;
__typeof__(geteuid) new_geteuid;
__typeof__(getegid) new_getegid;

__typeof__(setuid) new_setuid;
__typeof__(setgid) new_setgid;
__typeof__(seteuid) new_seteuid;
__typeof__(setegid) new_setegid;
__typeof__(setreuid) new_setreuid;
__typeof__(setregid) new_setregid;
__typeof__(setresuid) new_setresuid;
__typeof__(setresgid) new_setresgid;


export_weak_alias(new_getuid, getuid);
export(new_getuid, __getuid);

uid_t new_getuid(void)
{
  int uid = my_atoi(getenv("PLASH_FAKE_UID"));
  libc_log("getuid");
  if(uid < 0)
    return kernel_getuid();
  return uid;
}


export_weak_alias(new_getgid, getgid);
export(new_getgid, __getgid);

gid_t new_getgid(void)
{
  int gid = my_atoi(getenv("PLASH_FAKE_GID"));
  libc_log("getgid");
  if(gid < 0)
    return kernel_getgid();
  return gid;
}


export_weak_alias(new_geteuid, geteuid);
export(new_geteuid, __geteuid);

uid_t new_geteuid(void)
{
  int euid = my_atoi(getenv("PLASH_FAKE_EUID"));
  libc_log("geteuid");
  if(euid < 0)
    return kernel_geteuid();
  return euid;
}


export_weak_alias(new_getegid, getegid);
export(new_getegid, __getegid);

gid_t new_getegid(void)
{
  int egid = my_atoi(getenv("PLASH_FAKE_EGID"));
  libc_log("getegid");
  if(egid < 0)
    return kernel_getegid();
  return egid;
}


/* These functions simply report success.  I don't see much point in
   having them check the process's faked UIDs and GIDs. */

export_weak_alias(new_setuid, setuid);
export(new_setuid, __setuid);

int new_setuid(uid_t uid)
{
  libc_log("setuid");
  return 0;
}

export_weak_alias(new_setgid, setgid);
export(new_setgid, __setgid);

int new_setgid(gid_t gid)
{
  libc_log("setgid");
  return 0;
}


export(new_seteuid, seteuid);
export(new_seteuid, __GI_seteuid);

int new_seteuid(uid_t euid)
{
  libc_log("seteuid");
  return 0;
}

export(new_setegid, setegid);
export(new_setegid, __GI_setegid);

int new_setegid(gid_t egid)
{
  libc_log("setegid");
  return 0;
}


export_weak_alias(new_setreuid, setreuid);
export(new_setreuid, __setreuid);

int new_setreuid(uid_t ruid, uid_t euid)
{
  libc_log("setreuid");
  return 0;
}

export_weak_alias(new_setregid, setregid);
export(new_setregid, __setregid);

int new_setregid(gid_t rgid, gid_t egid)
{
  libc_log("setregid");
  return 0;
}


export_weak_alias(new_setresuid, setresuid);
export(new_setresuid, __setresuid);
export(new_setresuid, __GI___setresuid);

int new_setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
  libc_log("setresuid");
  return 0;
}

export_weak_alias(new_setresgid, setresgid);
export(new_setresgid, __setresgid);
export(new_setresgid, __GI___setresgid);

int new_setresgid(gid_t rgid, gid_t egid, gid_t sgid)
{
  libc_log("setresgid");
  return 0;
}
