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

#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>

#include "region.h"
#include "comms.h"
#include "server.h"
#include "parse-filename.h"


int bufs_eq(seqf_t str1, seqf_t str2)
{
  int i;
  if(str1.size != str2.size) { return 0; }
  for(i = 0; i < str1.size; i++) {
    if(str1.data[i] != str2.data[i]) { return 0; }
  }
  return 1;
}

#if 0
void server(int sock)
{
  struct comm *comm = comm_init(sock);
  struct process *proc = process_create();
  proc->sock_fd = sock;
  while(1) {
    seqf_t msg;
    fds_t fds;
    if(comm_get(comm, &msg, &fds) <= 0) {
      printf("error/end\n");
      break;
    }
    else if(0) {
      printf("got message:\n");
      print_data(msg);
    }
    process_handle_msg(proc, msg, fds);
  }
}
#endif

int main()
{
  int status;
  int pid;
  int socks[2];

  {
    int pid;
    int fds[2];
    if(pipe(fds) < 0) {
      perror("pipe");
      return 1;
    }
    pid = fork();
    if(pid == 0) {
      dup2(fds[0], 3);
      close(fds[1]);
      execl("/usr/bin/X11/xterm", "xterm", "-e", "sh", "-c", "less <&3", 0);
      exit(1);
    }
    close(fds[0]);
    server_log = fdopen(fds[1], "w");
    assert(server_log);
    setvbuf(server_log, 0, _IONBF, 0);
  }
    
  if(socketpair(AF_LOCAL, SOCK_STREAM, 0, socks) < 0) {
    perror("socketpair");
    return 1;
  }

  pid = fork();
  if(pid == 0) {
    char buf[10];
    close(socks[0]);
    set_close_on_exec_flag(socks[1], 0);
    snprintf(buf, sizeof(buf), "%i", socks[1]);
    setenv("COMM_FD", buf, 1);

    /* There are two options for getting the dynamic linker to load
       the modified libc.so.  Firstly, we can use LD_PRELOAD.  Since
       our libc.so has the same soname as the original one, after the
       linker has preloaded it, it won't load the original. */
    /* However, if we're going to use run-as-nobody, you can't set
       LD_PRELOAD before calling it, because it's setuid program and it
       will unset LD_PRELOAD, and not pass it on.  (This is not necessary
       behaviour in this case.  All we want is for run-as-nobody's
       dynamic linker to ignore it.)  To solve this, use something like:
         exec /usr/local/mrs-run-as-nobody
	      /usr/bin/env LD_PRELOAD=/usr/local/mrs/libc.so.6
	      /usr/local/mrs/ld-linux.so.2 --library-path /lib
	      /bin/sh
    */
    //setenv("LD_PRELOAD", "/usr/local/mrs/libc.so.6", 1);
    /* The other option is to put our libc.so in a publically-accessible
       directory, and specify that directory in LD_LIBRARY_PATH or with
       --library-path. */

    /* This used to be necessary before stat was intercepted.  The shell
       would stat PWD and stat ".", find they're the same, so report the
       cwd as PWD. */
    unsetenv("PWD");
    
    // "/bin/sh", "-c"
    // "/bin/cat", "mrs/Sheik", "mrs/Yerbouti", 0);

    /* Necessary for security when using run-as-nobody. */
    if(chdir("/") < 0) { perror("chdir"); exit(1); }
    
    execl("/usr/local/mrs/run-as-nobody", "some-process",
	  //"/usr/bin/env", "LD_PRELOAD=/usr/local/mrs/libc.so.6",
	  "/usr/local/mrs/ld-linux.so.2", "--library-path", "/usr/local/mrs:/lib",
	  "/bin/sh", 0);//"-c", "cat mrs/Sheik mrs/Yerbouti", 0);
    perror("exec");
    exit(1);
  }
  
  close(socks[1]);

  {
    struct process *p = process_create();
    p->sock_fd = socks[0];
    start_server(p);
  }
  
  wait(&status);
  printf("done: status %i\n", status);
  return 0;
}
