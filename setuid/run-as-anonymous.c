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

/* This program uses a lock directory to allocate user IDs.
   
   I considered putting the lock directory in /tmp.  We can create a
   directory that only root can read or write.  Other users should not
   be able to rename the directory.
   
   The nice thing about this is that the directory would be cleaned on
   a reboot without having to add any new scripts to the boot
   sequence.
   
   Unfortunately, it allows a possible denial-of-service attack,
   because someone could create a directory in /tmp with the same name
   as the one we're going to use.

   So the directory will have to go somewhere safer in the directory
   tree.  The problem of cleaning the directory is something we have
   to deal with between reboots anyway.

   NB. There's no point in checking the permissions and ownership of
   the directory alone, because if a non-root user owns a directory in
   its path, they can redirect the path to different directories that
   root happens to own using symlinks.
*/

#include <stdlib.h>
#include <alloca.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <grp.h>
#include <sys/time.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/file.h>

#include <string.h>
#include <signal.h>

#include "config.h"


#define NAME "run-as-anonymous"

#define UID_RANGE_LEN	(UID_RANGE_END - UID_RANGE_START)

/* In glibc, setuid() will try both the 16-bit and 32-bit versions of
   the kernel system call, while glibc provides no setuid32() symbol.
   In dietlibc, setuid() is the 16-bit syscall, and setuid32() is the
   32-bit syscall. */
/* However, x86-64 only has the new 32-bit syscall, and dietlibc doesn't
   define setuid32() there.  That omission might be a dietlibc bug. */
#if defined(__dietlibc__) && !defined(__x86_64__)
#define setuid setuid32
#define setgid setgid32
#define getuid getuid32
#define getgid getgid32
#endif


char hex_digit(int x)
{
  if(0 <= x && x <= 9) return x + '0';
  if(0xa <= x && x <= 0xf) return x - 0xa + 'a';
  assert(0);
  return 'X';
}

int pick_uid()
{
  int try_uid;
  int start_uid;
  struct timeval time;
  gettimeofday(&time, 0);

  assert(UID_RANGE_LEN > 0);
  start_uid = UID_RANGE_START + ((time.tv_sec ^ time.tv_usec) % UID_RANGE_LEN);
  try_uid = start_uid;

  while(1) {
    int fd;
    int i;
    char buf[8 + 1];
    for(i = 0; i < 8; i++) {
      buf[i] = hex_digit((try_uid >> ((7-i)*4)) & 0xf);
    }
    buf[8] = 0;

    /* fprintf(stderr, NAME ": trying uid %s\n", buf); */
    fd = open(buf, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if(fd >= 0) {
      close(fd);
      return try_uid;
    }
    if(errno != EEXIST) {
      perror(NAME ": open");
      return 0;
    }
    try_uid++;
    if(try_uid == start_uid) {
      fprintf(stderr, NAME ": out of uids\n");
      return 0;
    }
    if(try_uid >= UID_RANGE_END) try_uid = UID_RANGE_START;
  }
}

int main(int argc, char **argv)
{
  if(argc < 2) {
    fprintf(stderr, "Usage: %s [--debug] [-s ENVVAR=VALUE]... program args...\n", argv[0]);
    return 1;
  }
  else {
    char **argv2;
    int i;
    int uid;
    int lock_file_fd;

#ifdef IN_CHROOT_JAIL
    if(chdir(UID_LOCK_DIR2) < 0) {
      perror(NAME ": chdir: can't change to lock directory");
      return 1;
    }
#else
    if(chdir(UID_LOCK_DIR) < 0) {
      perror(NAME ": chdir: can't change to lock directory");
      return 1;
    }
#endif

    /* Claim non-exclusive lock before continuing. */
    /* There is no race condition between instances of run-as-anonymous
       without this (hence the non-exclusive lock), but there would be a
       race condition with gc-uid-locks.  Between creating the lock file
       for a UID and calling setgid()/setuid(), gc-uid-locks would
       think that the UID is not in use. */
    lock_file_fd = open("flock-file", O_RDONLY);
    if(lock_file_fd < 0) {
      perror(NAME ": open: " UID_LOCK_DIR "/flock-file");
      return 1;
    }
    /* Get a shared lock.  Multiple instances of run-as-anonymous can
       run concurrently with each other, but not concurrently with
       gc-uid-locks. */
    if(flock(lock_file_fd, LOCK_SH) < 0) {
      perror(NAME ": flock");
      return 1;
    }
    
    uid = pick_uid();
    if(uid == 0) return 1;

#if !defined IN_CHROOT_JAIL
    if(chroot(JAIL_DIR) < 0) { perror(NAME ": chroot: " JAIL_DIR); return 1; }
#endif
    if(chdir("/") < 0) { perror(NAME ": chdir: /"); return 1; }

    if(setgroups(0, 0) < 0) { perror(NAME ": setgroups"); return 1; }
    if(setgid(uid) < 0) { perror(NAME ": setgid"); return 1; }
    if(setuid(uid) < 0) { perror(NAME ": setuid"); return 1; }
    if(getuid() != uid) {
      fprintf(stderr, NAME ": uid not set correctly to 0x%x (got 0x%x) -- "
	      "does your kernel support 32-bit uids?\n", uid, getuid());
      return 1;
    }
    if(getgid() != uid) {
      fprintf(stderr, NAME ": gid not set correctly to 0x%x (got 0x%x) -- "
	      "does your kernel support 32-bit uids?\n", uid, getgid());
      return 1;
    }

    if(close(lock_file_fd) < 0) { perror(NAME ": close"); return 1; }

    /* From this point on, the code is running as unprivileged.
       It is not quite so critical, from the point of view of whoever
       installs this as setuid root. */

    argc--;
    argv++;

    /* Process the --debug option.  This suspends the process and
       prints the PID.  This is useful for attaching gdb to the
       process. */
#ifndef NO_DEBUG
    if(argc >= 2 && !strcmp(argv[0], "--debug")) {
      fprintf(stderr, NAME ": stopping, pid %i\n", getpid());
      if(raise(SIGSTOP) < 0) { perror(NAME ": raise(SIGSTOP)"); return 1; }
      
      argc--;
      argv++;
    }
#endif

    /* Process -s options for setting environment variables.  This is
       useful for restoring variables such as LD_LIBRARY_PATH, which
       the dynamic linker will unset when run-as-anonymous starts
       because it notices that the process is setuid. */
    while(argc >= 3 && !strcmp(argv[0], "-s")) {
      if(putenv(argv[1]) < 0) {
	fprintf(stderr, NAME ": putenv failed\n");
	return 1;
      }
      argc -= 2;
      argv += 2;
    }

    argv2 = alloca((argc + 1) * sizeof(char *));
    for(i = 0; i < argc; i++) argv2[i] = argv[i];
    argv2[i] = 0;
    
    execv(argv[0], argv2);
    fprintf(stderr, NAME ": exec: %s: %s\n", argv[0], strerror(errno));
    return 1;
  }
}
