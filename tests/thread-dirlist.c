
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <dirent.h>

void *loop(void *x)
{
  int *val = x;
  while(1) {
    DIR *dir = opendir("/");
    if(!dir) { perror("opendir"); break; }
    if(closedir(dir) < 0) { perror("closedir"); break; }

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
