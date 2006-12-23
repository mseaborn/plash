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

#include "libc-comms.h"


export_weak_alias(new_openat, openat);
export(new_openat, __openat);
export(new_openat, __openat_nocancel);
export(new_openat, __GI___openat);

int new_openat(int dir_fd, const char *filename, int flags, ...)
{
  __set_errno(ENOSYS);
  return -1;
}


export_weak_alias(new_openat64, openat64);
export(new_openat64, __openat64);
export(new_openat64, __openat64_nocancel);
export(new_openat64, __GI___openat64);

int new_openat64(int dir_fd, const char *filename, int flags, ...)
{
  __set_errno(ENOSYS);
  return -1;
}


export(new_fchmodat, fchmodat);

int new_fchmodat(int dir_fd, const char *pathname, unsigned int mode)
{
  __set_errno(ENOSYS);
  return -1;
}


export(new_fchownat, fchownat);

int new_fchownat(int dir_fd, const char *pathname,
		 unsigned int owner_uid,
		 unsigned int group_gid)
{
  __set_errno(ENOSYS);
  return -1;
}


export(new_futimesat, futimesat);

int new_futimesat(int dir_fd, const char *path, struct timeval times[2])
{
  __set_errno(ENOSYS);
  return -1;
}


export(new_symlinkat, symlinkat);

int new_symlinkat(int dir_fd, const char *oldpath, const char *newpath)
{
  __set_errno(ENOSYS);
  return -1;
}


export(new_mkdirat, mkdirat);

int new_mkdirat(int dir_fd, const char *pathname, unsigned int mode)
{
  __set_errno(ENOSYS);
  return -1;
}


export(new_linkat, linkat);

int new_linkat(int dir_fd, const char *oldpath,
	       int dir_fd2, const char *newpath)
{
  __set_errno(ENOSYS);
  return -1;
}


export(new_renameat, renameat);

int new_renameat(int dir_fd, const char *oldpath,
		 int dir_fd2, const char *newpath)
{
  __set_errno(ENOSYS);
  return -1;
}


export(new_unlinkat, unlinkat);

int new_unlinkat(int dir_fd, const char *pathname)
{
  __set_errno(ENOSYS);
  return -1;
}


export(new_readlinkat, readlinkat);

int new_readlinkat(int dir_fd, const char *pathname, char *buf, size_t buf_size)
{
  __set_errno(ENOSYS);
  return -1;
}


export(new_faccessat, faccessat);

int new_faccessat(int dir_fd, const char *pathname, unsigned int mode)
{
  __set_errno(ENOSYS);
  return -1;
}


export(new_mkfifoat, mkfifoat);

int new_mkfifoat(int dir_fd, const char *pathname, unsigned int mode)
{
  __set_errno(ENOSYS);
  return -1;
}


export(new_xmknodat, __xmknodat);
export(new_xmknodat, __GI___xmknodat);

int new_xmknodat(int ver, int dir_fd, const char *path, mode_t mode, dev_t dev)
{
  __set_errno(ENOSYS);
  return -1;
}
