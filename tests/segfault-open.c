
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main()
{
  if(open((char *) 1, 0) < 0) { perror("open"); return 0; }
  fprintf(stderr, "success\n");
  return 0;
}
