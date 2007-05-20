/* Copyright (C) 2007 Mark Seaborn

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

#define _GNU_SOURCE

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include "test-util.h"


/* Each test case clears up after itself by removing any files or
   directories it created.  This way, they can be run repeatedly. */


int counter = 0;

char *alloc_filename(const char *base)
{
  char *str;
  int rc = asprintf(&str, "%s-%i", base, counter++);
  assert(rc >= 0);
  return str;
}


void test_create_file()
{
  char *file = alloc_filename("file");
  char *hardlink = alloc_filename("hardlink");
  char *hardlink2 = alloc_filename("hardlink2");
  struct stat st;
  int fd = open(file, O_CREAT | O_WRONLY | O_EXCL, 0666);
  t_check(fd >= 0);
  t_check_zero(fstat(fd, &st));
  t_check_zero(close(fd));

  t_check_zero(stat(file, &st));
  t_check_zero(access(file, R_OK | W_OK));
  t_check_zero(chmod(file, 0777));
  t_check_zero(chown(file, -1, -1));
  struct timeval times[2] = { { 123, 321 }, { 456, 654 } };
  t_check_zero(utimes(file, times));

  t_check_zero(link(file, hardlink));
  t_check_zero(rename(hardlink, hardlink2));
  t_check_zero(unlink(hardlink2));
  t_check_zero(unlink(file));
  free(file);
  free(hardlink);
  free(hardlink2);
}

void test_create_file_at()
{
  char *file = alloc_filename("file");
  char *hardlink = alloc_filename("hardlink");
  char *hardlink2 = alloc_filename("hardlink2");
  int dir_fd = get_dir_fd(".");
  struct stat st;
  int fd = openat(dir_fd, file, O_CREAT | O_WRONLY | O_EXCL, 0777);
  t_check(fd >= 0);
  t_check_zero(fstat(fd, &st));
  t_check_zero(close(fd));

  t_check_zero(fstatat(dir_fd, file, &st, 0));
  t_check_zero(faccessat(dir_fd, file, R_OK | W_OK, 0));
  t_check_zero(fchmodat(dir_fd, file, 0777, 0));
  t_check_zero(fchownat(dir_fd, file, -1, -1, 0));
  struct timeval times[2] = { { 123, 321 }, { 456, 654 } };
  t_check_zero(futimesat(dir_fd, file, times));

  t_check_zero(linkat(dir_fd, file, dir_fd, hardlink, 0));
  t_check_zero(renameat(dir_fd, hardlink, dir_fd, hardlink2));
  t_check_zero(unlinkat(dir_fd, hardlink2, 0));
  t_check_zero(unlinkat(dir_fd, file, 0));
  close(dir_fd);
  free(file);
  free(hardlink);
  free(hardlink2);
}


void test_create_dir()
{
  char *dir = alloc_filename("dir");
  t_check_zero(mkdir(dir, 0777));
  t_check_zero(rmdir(dir));
  free(dir);
}

void test_create_dir_at()
{
  char *dir = alloc_filename("dir");
  int dir_fd = get_dir_fd(".");
  t_check_zero(mkdirat(dir_fd, dir, 0777));
  t_check_zero(unlinkat(dir_fd, dir, AT_REMOVEDIR));
  close(dir_fd);
  free(dir);
}


void test_create_symlink()
{
  char *symlink_filename = alloc_filename("symlink");
  const char *dest_path = "dest_path";
  t_check_zero(symlink(dest_path, symlink_filename));

  char buf[100];
  int got;
  got = readlink(symlink_filename, buf, sizeof(buf));
  t_check(got >= 0);
  assert(got == strlen(dest_path));
  buf[got] = 0;
  assert(strcmp(buf, dest_path) == 0);

  t_check_zero(unlink(symlink_filename));
  free(symlink_filename);
}

void test_create_symlink_at()
{
  char *symlink_filename = alloc_filename("symlink");
  int dir_fd = get_dir_fd(".");
  const char *dest_path = "dest_path";
  t_check_zero(symlinkat(dest_path, dir_fd, symlink_filename));

  char buf[100];
  int got;
  got = readlinkat(dir_fd, symlink_filename, buf, sizeof(buf));
  t_check(got >= 0);
  assert(got == strlen(dest_path));
  buf[got] = 0;
  assert(strcmp(buf, dest_path) == 0);

  t_check_zero(unlinkat(dir_fd, symlink_filename, 0));
  close(dir_fd);
  free(symlink_filename);
}


void test_readdir()
{
  DIR *dir = opendir(".");
  t_check(dir != NULL);
  while(1) {
    struct dirent *ent = readdir(dir);
    if(ent == NULL)
      break;
  }
  t_check_zero(closedir(dir));
}

void test_open_dir_fd()
{
  int fd = open(".", O_RDONLY);
  t_check(fd >= 0);
  struct stat st;
  t_check_zero(fstat(fd, &st));
  t_check_zero(fchdir(fd));
  t_check_zero(close(fd));
}

void test_chdir()
{
  t_check_zero(chdir("."));
}

void test_getcwd()
{
  char *str = getcwd(NULL, 0);
  t_check(str != NULL);
  free(str);
}


void test_bind()
{
  char *socket_filename = alloc_filename("socket");
  int fd;
  struct sockaddr_un addr;

  fd = socket(PF_UNIX, SOCK_STREAM, 0);
  t_check(fd >= 0);
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, socket_filename);
  t_check_zero(bind(fd, (struct sockaddr *) &addr,
                    sizeof(struct sockaddr_un)));
  t_check_zero(close(fd));
  t_check_zero(unlink(socket_filename));
  free(socket_filename);
}


struct test_case {
  void (*func)(void);
};

struct test_case test_cases[] = {
  { test_create_file },
  { test_create_file_at },
  { test_create_dir },
  { test_create_dir_at },
  { test_create_symlink },
  { test_create_symlink_at },
  { test_readdir },
  { test_open_dir_fd },
  { test_chdir },
  { test_getcwd },
  { test_bind },
  { NULL },
};


void *thread(void *x)
{
  void (*func)(void) = x;
  int i;
  for(i = 0; i < 100; i++) {
    func();
  }
  return NULL;
}


int main(int argc, char **argv)
{
  assert(argc == 2);

  if(strcmp(argv[1], "test_sequential") == 0) {
    struct test_case *test_case;
    for(test_case = test_cases; test_case->func != NULL; test_case++) {
      test_case->func();
      test_case->func();
    }
    return 0;
  }
  else if(strcmp(argv[1], "test_parallel") == 0) {
    int no_test_cases = sizeof(test_cases) / sizeof(test_cases[0]);
    /* If we put this too high, we could run out of FDs. */
    int instances_of_each = 10;

    int max_threads = no_test_cases * instances_of_each;
    int threads = 0;
    pthread_t th[max_threads];

    struct test_case *test_case;
    for(test_case = test_cases; test_case->func != NULL; test_case++) {
      int i;
      for(i = 0; i < instances_of_each; i++) {
	assert(threads < max_threads);
	int err = pthread_create(&th[threads++], NULL,
				 thread, (void *) test_case->func);
	assert(err == 0);
      }
    }

    int i;
    for(i = 0; i < threads; i++) {
      pthread_join(th[i], NULL);
    }
    return 0;
  }
  else
    return 1;
}
