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
#include <utime.h>
#include <sys/time.h>

#include "region.h"
#include "comms.h"
#include "libc-comms.h"
#include "marshal.h"


int my_utimes(int nofollow, const char *pathname,
	      struct timeval *atime, struct timeval *mtime)
{
  region_t r = region_make();
  seqf_t reply;
  if(!pathname || !atime || !mtime) {
    __set_errno(EINVAL);
    goto error;
  }
  if(req_and_reply(r,
		   cat4(r, mk_int(r, METHOD_FSOP_UTIME),
			mk_int(r, nofollow),
			cat4(r,
			  mk_int(r, atime->tv_sec), mk_int(r, atime->tv_usec),
			  mk_int(r, mtime->tv_sec), mk_int(r, mtime->tv_usec)),
			mk_string(r, pathname)), &reply) < 0) goto error;
  {
    seqf_t msg = reply;
    int ok = 1;
    m_int_const(&ok, &msg, METHOD_OKAY);
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


export_weak_alias(new_utime, utime);
export(new_utime, __utime);
export(new_utime, __GI_utime);

/* OLD-EXPORT: new_utime => WEAK:utime __utime __GI_utime */
int new_utime(const char *path, struct utimbuf *buf)
{
  if(buf) {
    struct timeval atime, mtime;
    atime.tv_sec = buf->actime;
    atime.tv_usec = 0;
    mtime.tv_sec = buf->modtime;
    mtime.tv_usec = 0;
    return my_utimes(0 /* nofollow */, path, &atime, &mtime);
  }
  else {
    struct timeval time;
    if(gettimeofday(&time, 0) < 0) return -1;
    return my_utimes(0 /* nofollow */, path, &time, &time);
  }
}


export_weak_alias(new_utimes, utimes);
export(new_utimes, __utimes);

/* OLD-EXPORT: new_utimes => WEAK:utimes __utimes */
int new_utimes(const char *path, struct timeval times[2])
{
  if(times) {
    return my_utimes(0 /* nofollow */, path, &times[0], &times[1]);
  }
  else {
    struct timeval time;
    if(gettimeofday(&time, 0) < 0) return -1;
    return my_utimes(0 /* nofollow */, path, &time, &time);
  }
}


export_weak_alias(new_lutimes, lutimes);
export(new_lutimes, __lutimes);

/* OLD-EXPORT: new_lutimes => WEAK:lutimes __lutimes */
int new_lutimes(const char *path, struct timeval times[2])
{
  if(times) {
    return my_utimes(1 /* nofollow */, path, &times[0], &times[1]);
  }
  else {
    struct timeval time;
    if(gettimeofday(&time, 0) < 0) return -1;
    return my_utimes(1 /* nofollow */, path, &time, &time);
  }
}
