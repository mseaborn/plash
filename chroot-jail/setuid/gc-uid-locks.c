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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/file.h>

#include "config.h"


#define NAME "gc-uid-locks"
#define NAME_MSG NAME ": "


/* Don't usually print anything, because we don't want to disclose info
   to non-root users. */
int verbose = 0;


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

int is_pid_number(const char *str)
{
  for(; *str; str++) {
    if(!('0' <= *str && *str <= '9')) { return 0; }
  }
  return 1;
}


/* Look for the line beginning with `label' in the given buffer.
   Returns the line after null-terminating it.
   Returns NULL if no such line found. */
char *get_entry(char *buf, int got, const char *label)
{
  int i = 0;
  while(1) {
    if(i + strlen(label) >= got) {
      return NULL; /* No match */
    }
    if(!memcmp(buf + i, label, strlen(label))) {
      /* Line starting at index i begins with the label string. */
      int j;
      i += strlen(label);
      j = i;
      /* Find end of line. */
      while(1) {
	if(i >= got) { return NULL; }
	if(buf[i] == '\n' || buf[i] == 0) { break; }
	i++;
      }
      buf[i] = 0;
      return buf + j;
    }
    
    /* Skip to next line. */
    while(1) {
      if(i >= got) { return NULL; }
      if(buf[i] == '\n' || buf[i] == 0) { break; }
      i++;
    }
    i++;
  }
  return NULL;
}

int get_process_uids(const char *pid_number, int uids[4])
{
  int fd;
  char filename[100];
  int len = snprintf(filename, sizeof(filename),
		     "/proc/%s/status", pid_number);
  assert(len >= 0 && len+1 <= sizeof(filename));

  fd = open(filename, O_RDONLY);
  if(fd < 0) { perror(NAME_MSG "open"); return -1; }
  {
    char buf[1024];
    char *uids_line;
	
    int got = read(fd, buf, sizeof(buf));
    if(got < 0) { perror(NAME_MSG "read"); return -1; }

    if(0) {
      char *name = get_entry(buf, got, "Name:");
      if(name) { printf("name: %s\n", name); }
    }
    
    uids_line = get_entry(buf, got, "Uid:");
    if(!uids_line) {
      fprintf(stderr, NAME_MSG "uid field missing\n");
      return -1;
    }
    if(sscanf(uids_line, "%i %i %i %i",
	      &uids[0], &uids[1], &uids[2], &uids[3]) != 4) {
      fprintf(stderr, NAME_MSG "format unrecognised: %s\n",
	      uids_line);
      return -1;
    }
  }
  close(fd);
  return 0;
}


struct process_info {
  int pid;
  int uids[4];
};

void gc_uid_locks()
{
  struct process_info *pids = NULL;
  int pids_got = 0, pids_size = 0;
  int *uids = NULL;
  int uids_got = 0, uids_size = 0;
  DIR *dir;
  int i;
  
  int lock_file_fd = open(UID_LOCK_DIR "/flock-file", O_RDONLY);
  if(lock_file_fd < 0) {
    perror(NAME_MSG "open: " UID_LOCK_DIR "/flock-file");
    return;
  }
  /* Get an exclusive lock: prevent run-as-anonymous from running
     while GC is in progress.  This prevents a possible TOCTTOU race
     condition between determining whether to delete a lock file and
     actually deleting it. */
  if(flock(lock_file_fd, LOCK_EX) < 0) {
    perror(NAME_MSG "flock");
    return;
  }

  /* Read the UIDs that are currently in use. */
  /* It is important that this does not miss any UIDs, and so it must not
     miss any processes.  Otherwise Plash would re-allocate a UID that is
     already in use, and processes would not be isolated. */
  /* 2005/08/20:  I just realised there is a simpler way to find out the
     UID and GID of a process.  You can stat /proc/PID, rather than
     reading /proc/PID/status.  This wouldn't read the 4 values that the
     latter has, but that doesn't matter, because programs running under
     Plash should not have different effective and real UID/GIDs. */
  dir = opendir("/proc");
  if(!dir) { perror(NAME_MSG "opendir /proc"); return; }
  while(1) {
    struct dirent *ent = readdir(dir);
    if(!ent) { break; }

    if(is_pid_number(ent->d_name)) {
      struct process_info *info;

      /* Add the PID to the list. */
      if(pids_size - pids_got < 1) {
	pids_size += 10;
	pids = realloc(pids, pids_size * sizeof(struct process_info));
	if(!pids) { return; } /* Error */
      }
      info = &pids[pids_got++];
      info->pid = atoi(ent->d_name);
      
      if(get_process_uids(ent->d_name, info->uids) < 0) { return; }
      
      /* Resize array of UIDs if necessary. */
      if(uids_size - uids_got < 4) {
	uids_size += 100;
	uids = realloc(uids, uids_size * sizeof(int));
	if(!uids) { return; } /* Error */
      }
      if(include_uid(info->uids[0])) uids[uids_got++] = info->uids[0];
      if(include_uid(info->uids[1])) uids[uids_got++] = info->uids[1];
      if(include_uid(info->uids[2])) uids[uids_got++] = info->uids[2];
      if(include_uid(info->uids[3])) uids[uids_got++] = info->uids[3];
      if(0) { printf("uid %i,%i,%i,%i\n", uids[0], uids[1], uids[2], uids[3]); }
    }
  }
  closedir(dir);

  /* Now re-read the directory listing.  If it doesn't contain the
     same PIDs as before (in the same order, with the same UIDs),
     exit. */
  /* Why do this?  There is no guarantee that when we read the
     directory listing we get an atomic snapshot of the process list.
     A process could fork when we're halfway through reading the list.
     The new process would probably be given a high process ID, and be
     put at the end of the list, so we might see it.  But there's no
     guarantee of this.  When the kernel reaches PID 32767, it goes
     back and allocates low numbers again.  And we can't rely on any
     particular behaviour from the kernel anyway. */
  /* This is still potentially vulnerable to the fork-and-exit trick.
     If a process manages to pull this off on *both* occasions that
     we read the directory listing, it will fool us into thinking the
     UID can be reclaimed, when it cannot.  The probability of being
     able to do this seems to be very small. */
  /* Maybe the answer to this is to call getdents() with a sufficiently
     big buffer that Linux *does* read the contents atomically.
     Would Linux sit in kernel mode for that long, or does it have a limit?
     Is getdents() accessible? */
  dir = opendir("/proc");
  if(!dir) { perror(NAME_MSG "opendir /proc"); return; }
  i = 0;
  while(1) {
    struct dirent *ent = readdir(dir);
    if(!ent) { break; }

    if(is_pid_number(ent->d_name)) {
      int proc_pid = atoi(ent->d_name);
      int proc_uids[4];
      if(get_process_uids(ent->d_name, proc_uids) < 0) { return; }

      if(i >= pids_got) {
	/* error */
	fprintf(stderr, NAME_MSG "mismatch: extra process has appeared (pid %i)\n", proc_pid);
	return;
      }
      if(pids[i].pid != proc_pid ||
	 pids[i].uids[0] != proc_uids[0] ||
	 pids[i].uids[1] != proc_uids[1] ||
	 pids[i].uids[2] != proc_uids[2] ||
	 pids[i].uids[3] != proc_uids[3]) {
	/* error */
	fprintf(stderr, NAME_MSG "process list entry mismatch\n");
	return;
      }

      i++;
    }
  }
  if(i != pids_got) {
    /* error */
    fprintf(stderr, NAME_MSG "process list mismatch\n");
    return;
  }
  closedir(dir);
  free(pids);

  /* Sort the list of UIDs.  Duplicates are left in, but that's fine.
     We check membership in the list later. */
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
  /* It's okay if this misses a lock file.  That just means that the file
     won't be removed. */
  /* If we remove a file but continue reading the directory listing,
     that will be okay.  We'll probably get given the remaining entries
     successfully anyway. */
  if(chdir(UID_LOCK_DIR) < 0) { perror(NAME_MSG "chdir " UID_LOCK_DIR); return; }
  dir = opendir(".");
  if(!dir) { perror(NAME_MSG "opendir " UID_LOCK_DIR); return; }
  while(1) {
    int i, j, x;
    struct dirent *ent = readdir(dir);
    if(!ent) { break; }

    if(strlen(ent->d_name) != 8) {
      if(verbose) printf(NAME_MSG "ignoring file \"%s\"\n", ent->d_name);
      continue;
    }
    x = 0;
    for(i = 0; i < 8; i++) {
      int d = int_of_hex_digit(ent->d_name[i]);
      if(d < 0) {
	if(verbose) printf(NAME_MSG "ignoring file \"%s\"\n", ent->d_name);
	continue;
      }
      x |= d << ((7-i)*4);
    }
    if(!include_uid(x)) {
      /* If this happens, perhaps run-as-anonymous and gc-uid-locks weren't
	 compiled with the same config.h settings. */
      if(verbose) printf(NAME_MSG "uid %i (0x%x): outside range, so why is there a lock file for this?  aborting\n", x, x);
      return;
    }

    /* Do a binary search of `uids' for `x'. */
    i = 0;
    j = uids_got;
    while(i < j) {
      int mid = (i+j) / 2;
      if(x < uids[mid]) { j = mid; }
      else if(x > uids[mid]) { i = mid+1; }
      else { goto found; }
    }
    if(verbose) printf(NAME_MSG "uid %i (0x%x): not in use, removing lock\n", x, x);
    if(unlink(ent->d_name) < 0) { perror(NAME_MSG "unlink"); return; }
    continue;
  found:
    if(verbose) printf(NAME_MSG "uid %i (0x%x): in use\n", x, x);
  }
  closedir(dir);
  free(uids);
}

int main(int argc, char *argv[])
{
  if(argc == 2 && !strcmp(argv[1], "--gc")) {
    gc_uid_locks();
    return 0;
  }
  /* Print this when no arguments are given just to be informative. */
  printf("Usage: " NAME " --gc\n");
  printf("This setuid program removes lock files for UIDs that are not in use.\n");
  return 0;
}
