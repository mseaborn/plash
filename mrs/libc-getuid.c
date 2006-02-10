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


#include "out-aliases_libc-getuid.h"
