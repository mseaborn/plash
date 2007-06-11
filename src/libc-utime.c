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

/* Get AT_FDCWD */
#define _GNU_SOURCE

#include <errno.h>
#include <utime.h>
#include <sys/time.h>
#include <fcntl.h>

#include "region.h"
#include "comms.h"
#include "marshal.h"
#include "marshal-pack.h"
#include "cap-utils.h"

#include "libc-comms.h"
#include "libc-fds.h"
#include "kernel-fd-ops.h"


int new_utime(const char *path, const struct utimbuf *buf);
int new_utimes(const char *path, const struct timeval times[2]);
int new_lutimes(const char *path, const struct timeval times[2]);
int new_futimesat(int dir_fd, const char *path, const struct timeval times[2]);


static int
my_utimesat_nodefault(int dir_fd, int nofollow, const char *pathname,
		      const struct timeval *atime, const struct timeval *mtime)
{
  region_t r = region_make();
  cap_t fs_op_server;
  cap_t dir_obj;
  struct cap_args result;
  int rc = -1;
  plash_libc_lock();
  if(!pathname || !atime || !mtime) {
    __set_errno(EINVAL);
    goto exit;
  }
  if(libc_get_fs_op(&fs_op_server) < 0)
    goto exit;
  if(fds_get_dir_obj(dir_fd, &dir_obj) < 0)
    goto exit;
  cap_call(fs_op_server, r,
	   pl_pack(r, METHOD_FSOP_UTIME, "diiiiiS",
		   dir_obj, nofollow,
		   atime->tv_sec, atime->tv_usec,
		   mtime->tv_sec, mtime->tv_usec,
		   seqf_string(pathname)),
	   &result);
  if(pl_unpack(r, result, METHOD_OKAY, "")) {
    rc = 0;
  }
  else {
    set_errno_from_reply(flatten_reuse(r, result.data));
    pl_args_free(&result);
  }
 exit:
  plash_libc_unlock();
  region_free(r);
  return rc;
}


export_weak_alias(new_utime, utime);
export(new_utime, __utime);
export(new_utime, __GI_utime);

int new_utime(const char *path, const struct utimbuf *buf)
{
  if(buf) {
    struct timeval atime, mtime;
    atime.tv_sec = buf->actime;
    atime.tv_usec = 0;
    mtime.tv_sec = buf->modtime;
    mtime.tv_usec = 0;
    return my_utimesat_nodefault(AT_FDCWD, FALSE /* nofollow */, path,
				 &atime, &mtime);
  }
  else {
    struct timeval time;
    if(gettimeofday(&time, 0) < 0)
      return -1;
    return my_utimesat_nodefault(AT_FDCWD, FALSE /* nofollow */, path,
				 &time, &time);
  }
}


static int my_utimesat(int dir_fd, int nofollow, const char *path,
		       const struct timeval times[2])
{
  if(times) {
    return my_utimesat_nodefault(dir_fd, nofollow, path,
				 &times[0], &times[1]);
  }
  else {
    struct timeval time;
    if(gettimeofday(&time, 0) < 0)
      return -1;
    return my_utimesat_nodefault(dir_fd, nofollow, path, &time, &time);
  }
}


export_weak_alias(new_utimes, utimes);
export(new_utimes, __utimes);

int new_utimes(const char *path, const struct timeval times[2])
{
  return my_utimesat(AT_FDCWD, FALSE /* nofollow */, path, times);
}


export_weak_alias(new_lutimes, lutimes);
export(new_lutimes, __lutimes);

int new_lutimes(const char *path, const struct timeval times[2])
{
  return my_utimesat(AT_FDCWD, TRUE /* nofollow */, path, times);
}


export(new_futimesat, futimesat);

int new_futimesat(int dir_fd, const char *path, const struct timeval times[2])
{
  return my_utimesat(dir_fd, FALSE /* nofollow */, path, times);
}
