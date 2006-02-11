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
#include <errno.h>
#include <stdio.h>

#include "filesysobj.h"
#include "shell-fds.h"


void finalise_close_fd(void *obj)
{
  int fd = (int) obj;
  if(close(fd) < 0) {
    // perror("plash: close (finalise)");
    fprintf(stderr, "plash: finalise, close(%i): %s\n", fd, strerror(errno));
  }
}

void array_set_fd(region_t r, struct fd_array *array, int j, int x)
{
  /* Enlarge file descriptor array if necessary. */
  if(j >= array->count) {
    int i;
    int new_size = j + 1 + 10;
    int *fds_new = region_alloc(r, new_size * sizeof(int));
    memcpy(fds_new, array->fds, array->count * sizeof(int));
    for(i = array->count; i < new_size; i++) fds_new[i] = -1;
    array->fds = fds_new;
    array->count = new_size;
  }
  array->fds[j] = x;
}

int array_get_free_index(struct fd_array *array)
{
  int i;
  for(i = 0; i < array->count; i++) {
    if(array->fds[i] < 0) return i;
  }
  return array->count;
}

/* Copy FDs into place.  This needs to be done in two passes. */
/* Returns -1 on an error. */
int install_fds(fds_t array)
{
  int i, max_fd;
  
  /* First the FDs are copied to a position where they won't interfere
     with the destination or source locations. */
  max_fd = array.count;
  for(i = 0; i < array.count; i++) {
    if(max_fd < array.fds[i] + 1) max_fd = array.fds[i] + 1;
  }
  for(i = 0; i < array.count; i++) {
    if(array.fds[i] >= 0) {
      /* printf("dup2(%i, %i)\n", array.fds[i], max_fd + i); */
      if(dup2(array.fds[i], max_fd + i) < 0) {
	fprintf(stderr, "plash/client: dup2(%i, %i) (first): %s\n",
		array.fds[i], max_fd + i, strerror(errno));
	return -1;
      }
      if(set_close_on_exec_flag(max_fd + i, 1) < 0) perror("plash: cloexec");
    }
  }
  
  /* Now they are copied back to their real position. */
  for(i = 0; i < array.count; i++) {
    if(array.fds[i] >= 0) {
      /* printf("dup2(%i, %i)\n", max_fd + i, i); */
      if(dup2(max_fd + i, i) < 0) {
	/* Printing should still be fairly safe. */
	fprintf(stderr, "plash/client: dup2(%i, %i) (second): %s\n",
		max_fd + i, i, strerror(errno));
	return -1;
      }
    }
  }
  return 0;
}
