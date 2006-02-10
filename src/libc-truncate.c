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

#include <fcntl.h>
#include <unistd.h>

#include "region.h"
#include "comms.h"
#include "libc-comms.h"


/* EXPORT: new_truncate => truncate __GI_truncate */
int new_truncate(const char *path, off_t length)
{
  int rc;
  int fd = new_open(path, O_WRONLY, 0);
  if(fd < 0) return -1;
  rc = ftruncate(fd, length);
  close(fd);
  return rc;
}
