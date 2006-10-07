
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

int main()
{
  int fds[2];
  struct ucred cred;
  socklen_t size;

  if(socketpair(AF_LOCAL, SOCK_STREAM, 0, fds) < 0) {
    perror("socketpair");
    return 1;
  }

  fprintf(stderr, "process:  pid=%i uid=%i gid=%i\n",
	  getpid(), getuid(), getgid());

  size = sizeof(cred);
  if(getsockopt(fds[0], SOL_SOCKET, SO_PEERCRED, &cred, &size) < 0) {
    perror("getsockopt/SO_PEERCRED");
    return 1;
  }
  if(size == sizeof(cred)) {
    fprintf(stderr, "socket:   pid=%i uid=%i gid=%i\n",
	    cred.pid, cred.uid, cred.gid);

    if(getpid() == cred.pid &&
       getuid() == cred.uid &&
       getgid() == cred.gid) {
      printf("IDs are the same, as expected\n");
      return 0;
    }
    else {
      printf("IDs differ\n");
    }
  }
  return 1;
}
