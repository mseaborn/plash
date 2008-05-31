
/* The purpose of this program is simply to provide a convenient way
   to get at the kernel's execve(), bypassing Plash-libc's wrapper for
   execve().  This program should be statically linked.  An
   alternative would be to use the kernel_execve() function exported
   by Plash-libc. */

#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
  if(argc < 2) {
    const char *msg = "Usage: kernel-exec <prog> <args>...\n";
    write(STDERR_FILENO, msg, strlen(msg));
    return 1;
  }
  argv[argc] = NULL; /* Just in case it's not already null-terminated */
  execv(argv[1], argv + 1);
  perror("exec");
  return 1;
}
