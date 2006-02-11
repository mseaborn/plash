
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

/* This doesn't actually trigger any assertion failures, presumably
   because the messages are small enough to be read from and written to
   the socket atomically. */

void *loop(void *x)
{
  int *val = x;
  while(1) {
    int fd = open("/dev/null", O_RDONLY);
    if(fd < 0) { perror("open"); break; }
    if(close(fd) < 0) { perror("close"); break; }
    
    if(chdir(".") < 0) { perror("chdir"); break; }

    printf("progress %i\n", *val);
  }
  return NULL;
}

int main()
{
  int v1 = 1, v2 = 2;
  pthread_t th;
  int err = pthread_create(&th, NULL, loop, &v1);
  if(err != 0) {
    errno = err;
    perror("pthread_create");
    return 0;
  }
  loop(&v2);
  return 0;
}
