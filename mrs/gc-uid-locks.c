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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

#include "config.h"


char *get_entry(char *buf, int got, const char *label)
{
  int i = 0;
  while(1) {
    if(i+4 <= got && !memcmp(buf+i, label, strlen(label))) {
      int j;
      i += strlen(label);
      j = i;
      /* Find end of line. */
      while(1) {
	assert(i < got);
	if(buf[i] == '\n' || buf[i] == 0) { buf[i] = 0; break; }
	i++;
      }
      return buf + j;
    }
    /* Skip to next line. */
    while(1) {
      assert(i < got);
      if(buf[i] == '\n' || buf[i] == 0) { i++; break; }
      i++;
    }
  }
  return 0;
}

int include_uid(int uid)
{
  return UID_RANGE_START <= uid && uid < UID_RANGE_END;
}

int compare_int(const void *a, const void *b)
{
  return *(int *) a - *(int *) b;
}

int int_of_hex_digit(char c)
{
  if('0' <= c && c <= '9') return c - '0';
  if('a' <= c && c <= 'f') return c - 'a' + 10;
  if('A' <= c && c <= 'F') return c - 'A' + 10;
  return -1;
}


int main(int argc, char *argv[])
{
  int *uids = 0, uids_got = 0, uids_size = 0;
  struct dirent *ent;
  DIR *dir;

  dir = opendir("/proc");
  if(!dir) { perror("opendir /proc"); return 1; }
  while(1) {
    ent = readdir(dir);
    if(!ent) break;

    if('0' <= ent->d_name[0] && ent->d_name[0] <= '9') {
      int fd;
      const char *prefix = "/proc/";
      const char *suffix = "/status";
      int len = strlen(ent->d_name);
      char *name = malloc(strlen(prefix) + len + strlen(suffix) + 1);
      assert(name);
      memcpy(name, prefix, strlen(prefix));
      memcpy(name + strlen(prefix), ent->d_name, len);
      memcpy(name + strlen(prefix) + len, suffix, strlen(suffix) + 1);

      fd = open(name, O_RDONLY);
      free(name);
      if(fd < 0) { perror("open"); return 1; }
      {
	char buf[1024];
	int got = read(fd, buf, sizeof(buf));
	if(got < 0) { perror("read"); return 1; }

	if(0) {
	  char *name = get_entry(buf, got, "Name:");
	  if(name) printf("name: %s\n", name);
	}
	{
	  char *uids_line = get_entry(buf, got, "Uid:");
	  if(uids_line) {
	    int pid1, pid2, pid3, pid4;
	    if(sscanf(uids_line, "%i %i %i %i", &pid1, &pid2, &pid3, &pid4) != 4) {
	      printf("format unrecognised: %s\n", uids_line);
	    }
	    else {
	      if(uids_size - uids_got < 4) {
		int new_size = uids_size + 128;
		int *uids_new = malloc(new_size * sizeof(int));
		assert(uids_new);
		memcpy(uids_new, uids, uids_got * sizeof(int));
		free(uids);
		uids = uids_new;
		uids_size = new_size;
	      }
	      if(include_uid(pid1)) uids[uids_got++] = pid1;
	      if(include_uid(pid2)) uids[uids_got++] = pid2;
	      if(include_uid(pid3)) uids[uids_got++] = pid3;
	      if(include_uid(pid4)) uids[uids_got++] = pid4;
	      if(0) printf("uid %i,%i,%i,%i\n", pid1, pid2, pid3, pid4);
	    }
	  }
	}
      }
    }
  }
  closedir(dir);

  qsort(uids, uids_got, sizeof(int), compare_int);
  
  if(0) {
    int last = -1;
    int i;
    printf("sorted:\n");
    for(i = 0; i < uids_got; i++) {
      if(uids[i] != last) printf("id=%i\n", uids[i]);
      last = uids[i];
    }
  }

  /* Look at all the uids in the lock dir. */
  if(chdir(UID_LOCK_DIR) < 0) { perror("chdir " UID_LOCK_DIR); return 1; }
  dir = opendir(".");
  if(!dir) { perror("opendir " UID_LOCK_DIR); return 1; }
  while(1) {
    int i, j, x;
    ent = readdir(dir);
    if(!ent) break;

    if(strlen(ent->d_name) != 8) {
      printf("ignoring %s\n", ent->d_name);
      continue;
    }
    x = 0;
    for(i = 0; i < 8; i++) {
      int d = int_of_hex_digit(ent->d_name[i]);
      if(d < 0) {
	printf("ignoring %s\n", ent->d_name);
	continue;
      }
      x |= d << ((7-i)*4);
    }
    if(!include_uid(x)) {
      printf("outside range: %i\n", x);
      continue;
    }

    /* Do a binary search of `uids' for `x'. */
    i = 0;
    j = uids_got;
    while(i < j) {
      int mid = (i+j) / 2;
      if(x < uids[mid]) j = mid;
      else if(x > uids[mid]) i = mid+1;
      else goto found;
    }
    printf("not found %i\n", x);
    if(unlink(ent->d_name) < 0) { perror("unlink"); return 1; }
    continue;
  found:
    printf("found %i\n", x);
  }
  closedir(dir);

  return 0;
}
