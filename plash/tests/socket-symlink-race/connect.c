
#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

#define PROG "connect: "

int main()
{
  const char *pathname = "race-socket";
  int len = strlen(pathname);
  struct sockaddr_un name;
  struct timeval t_start, t_end;

  gettimeofday(&t_start, NULL);

  name.sun_family = AF_LOCAL;
  memcpy(name.sun_path, pathname, len);
  name.sun_path[len] = 0;

  while(1) {
    int sock_fd;
    int rc;
    
    sock_fd = socket(PF_LOCAL, SOCK_STREAM, 0);
    if(sock_fd < 0) { perror(PROG "socket"); break; }

    rc = connect(sock_fd, (struct sockaddr *) &name,
		 offsetof(struct sockaddr_un, sun_path) + len + 1);
    if(rc < 0) {
      // perror(PROG "connect");
      // if(errno != ENOENT && errno != ECONNREFUSED) { break; }
      if(errno == ENOSYS) { break; }
    }
    else {
      printf("you lose: connected\n");
      break;
    }
    close(sock_fd);
  }
  gettimeofday(&t_end, NULL);
  printf(PROG "exited, after %f seconds\n",
	 (double) (t_end.tv_sec - t_start.tv_sec) +
	 (double) (t_end.tv_usec - t_start.tv_usec) * 1e-6);
  return 1;
}
