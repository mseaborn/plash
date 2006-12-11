
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>


int main(int argc, char **argv)
{
  /* Not all the bytes are filled in by the kernel/libc, so the filler
     should be 0. */
  char filler = 0;
  
  int fail = 0;
  int i;
  union {
    char buf[sizeof(struct stat) + 100];
    struct stat st;
  } st;
  
  /* Expects 1 argument */
  if(argc != 2) {
    return 1;
  }

  /* Clean buffer so that we can check for overrun afterwards */
  memset(&st, filler, sizeof(st));

  if(stat(argv[1], &st.st) < 0) {
    perror("stat");
    return 1;
  }

  /* Check for overrun */
  for(i = sizeof(struct stat); i < sizeof(st); i++) {
    if(st.buf[i] != filler) {
      printf("mismatch at %i\n", i);
      fail = 1;
    }
  }

  /* Write contents of stat buffer to file descriptor 3 */
  if(write(3, &st.st, sizeof(struct stat)) != sizeof(struct stat)) {
    printf("write failed\n");
    return 1;
  }
  
  return fail;
}
