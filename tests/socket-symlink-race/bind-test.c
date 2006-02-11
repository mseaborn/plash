
/* Test whether bind() for Unix domain sockets follows symlinks. */
/* It doesn't on Linux 2.4.29, but it might on other versions, and
   that would be a problem. */

/* For offsetof */
#define _GNU_SOURCE

#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>

#define PROG "listen: "

int main()
{
  const char *pathname = "symlink";
  int len = strlen(pathname);
  struct sockaddr_un name;
  int sock_fd;
  int fd2;

  if(symlink("symlink-dest", "symlink") < 0) { perror(PROG "symlink"); return 1; }
  
  sock_fd = socket(PF_LOCAL, SOCK_STREAM, 0);
  if(sock_fd < 0) { perror(PROG "socket"); return 1; }

  name.sun_family = AF_LOCAL;
  memcpy(name.sun_path, pathname, len);
  name.sun_path[len] = 0;
  if(bind(sock_fd, &name,
	  offsetof(struct sockaddr_un, sun_path) + len + 1) < 0) {
    perror(PROG "bind");
    printf("bind() failed as expected\n");
    return 0;
  }

  printf("bind() succeeded unexpectedly\n");
  return 1;
}
