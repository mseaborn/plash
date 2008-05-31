
/* For offsetof */
#define _GNU_SOURCE

#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>

#define PROG "listen: "

int main()
{
  const char *pathname = "socket";
  int len = strlen(pathname);
  struct sockaddr_un name;
  int sock_fd;
  int fd2;
  
  sock_fd = socket(PF_LOCAL, SOCK_STREAM, 0);
  if(sock_fd < 0) { perror(PROG "socket"); return 1; }

  name.sun_family = AF_LOCAL;
  memcpy(name.sun_path, pathname, len);
  name.sun_path[len] = 0;
  if(bind(sock_fd, &name,
	  offsetof(struct sockaddr_un, sun_path) + len + 1) < 0) {
    perror(PROG "bind");
    return 1;
  }
  if(listen(sock_fd, 10) < 0) {
    perror(PROG "listen");
    return 1;
  }
  fd2 = accept(sock_fd, NULL, NULL);
  if(fd2 < 0) {
    perror(PROG "accept");
    return 1;
  }
  printf("you lose: received connection\n");
  return 1;
}
