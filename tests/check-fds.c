
/* Check that we have not been granted unnecessary file descriptors. */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

int main()
{
  int fail = 0;
  int comm_fd = -1;
  int fd;

  char *str = getenv("PLASH_COMM_FD");
  if(str) {
    comm_fd = atoi(str);
  }

  /* stdin, stdout and stderr are OK, so skip the first 3 FDs. */
  for(fd = 3; fd < 1024; fd++) {
    if(fcntl(fd, F_GETFL) < 0) {
      if(errno != EBADF) {
	perror("fcntl/F_GETFL");
	fail = 1;
      }
      /* otherwise ok: FD is not open */
    }
    else if(fd == comm_fd) {
      fprintf(stderr, "comm_fd=%i is open as expected\n", fd);
    }
    else {
      fprintf(stderr, "unexpected: FD %i is open\n", fd);
      fail = 1;
    }
  }
  
  return fail;
}
