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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA.  */

#ifndef shell_fds_h
#define shell_fds_h


#include "region.h"


void finalise_close_fd(void *obj);

/* FD array.  An entry of -1 means that the FD is not open. */
/* This is allocated in a region.  Resizing doesn't reallocate the old
   array. */
/* Like struct seq_fds, but intended to be writable. */
struct fd_array {
  int *fds;
  int count;
};

void array_set_fd(region_t r, struct fd_array *array, int j, int x);
int array_get_free_index(struct fd_array *array);
int install_fds(fds_t array);


#endif
