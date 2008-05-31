
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#define PROG "symlinker: "

int main()
{
  while(1) {
    int fd = open("race-socket", O_CREAT | O_EXCL, 0777);
    if(fd < 0) { perror(PROG "open"); break; }
    close(fd);
    // if(symlink("race-socket", "dangling") < 0) { perror(PROG "symlink"); break; }

    if(0) {
      /* These two operations must happen quickly. */
      if(unlink("race-socket") < 0) { perror(PROG "unlink"); break; }
      if(symlink("socket", "race-socket") < 0) { perror(PROG "symlink"); break; }
    }

    /* Replace race-socket with a symlink atomically. */
    if(symlink("socket", "tmp-symlink") < 0) { perror(PROG "symlink"); break; }
    if(rename("tmp-symlink", "race-socket") < 0) { perror(PROG "rename"); break; }
    
    if(unlink("race-socket") < 0) { perror(PROG "unlink"); break; }
  }
  printf(PROG "exited\n");
  return 0;
}
