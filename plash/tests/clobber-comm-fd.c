
/* Tests whether close() correctly refuses to close PLASH_COMM_FD. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

int main()
{
  struct stat st;
  
  char *str = getenv("PLASH_COMM_FD");
  if(!str) {
    fprintf(stderr, "PLASH_COMM_FD not defined\n");
    return 1;
  }

  /* Check whether we can communicate with the server. */
  if(stat("/", &st) < 0) {
    perror("first stat failed");
  }
  
  if(close(atoi(str)) < 0) {
    if(errno == EBADF) {
      fprintf(stderr, "close refused as expected\n");
    }
    else {
      perror("close");
    }
  }
  else {
    fprintf(stderr, "close succeeded unexpectedly\n");
  }

  /* Check again whether we can communicate with the server. */
  if(stat("/", &st) < 0) {
    perror("second stat failed");
  }
  
  return 0;
}
