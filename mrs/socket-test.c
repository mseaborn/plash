
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <assert.h>


int do_recv2(int sock, int size)
{
  char *buf = alloca(size);
  struct msghdr msghdr;
  struct iovec iovec;
  msghdr.msg_name = 0;
  msghdr.msg_namelen = 0;
  msghdr.msg_iov = &iovec;
  msghdr.msg_iovlen = 1;
  msghdr.msg_control = 0;
  msghdr.msg_controllen = 0;
  msghdr.msg_flags = 0;
  iovec.iov_base = buf;
  iovec.iov_len = size;
      
  if(recvmsg(sock, &msghdr, 0) < 0) { perror("recvmsg"); return -1; }
  return 0;
}

int do_send3(int sock, int size)
{
  int *fds = 0;
  int fds_size = 0;
  int control_buf_size = CMSG_SPACE(fds_size * sizeof(int));
  char *buf = alloca(size);
  struct msghdr msghdr;
  struct iovec iovec;
  struct cmsghdr *cmsg;
  
  msghdr.msg_name = 0;
  msghdr.msg_namelen = 0;
  msghdr.msg_iov = &iovec;
  msghdr.msg_iovlen = 1;
  msghdr.msg_control = alloca(control_buf_size);
  msghdr.msg_controllen = control_buf_size;
  msghdr.msg_flags = 0;
  iovec.iov_base = buf;
  iovec.iov_len = size;
  cmsg = CMSG_FIRSTHDR(&msghdr); /* The same as msghdr.msg_control */
  cmsg->cmsg_len = CMSG_LEN(fds_size * sizeof(int));
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  memcpy(CMSG_DATA(cmsg), fds, fds_size * sizeof(int));

  if(sendmsg(sock, &msghdr, 0) < 0) { perror("sendmsg"); return -1; }
  return 0;
}

int main()
{
  int socks[2];
  int pid;
  
  if(socketpair(AF_LOCAL, SOCK_STREAM, 0, socks) < 0) {
    perror("socketpair");
    return 1;
  }

  pid = fork();
  if(pid == 0) {
    int sock = socks[0];
    close(socks[1]);

    while(1) {
      if(do_recv2(sock, 1000) < 0) exit(0);
      if(do_recv2(sock, 1000) < 0) exit(0);
      printf("got\n");
      if(do_send3(sock, 10) < 0) exit(0);
      printf("reply\n");
    }
    exit(0);
  }
  if(pid < 0) perror("fork");

  {
    int sock = socks[1];
    while(1) {
      if(do_send3(sock, 2000) < 0) exit(0);
      if(do_recv2(sock, 10) < 0) exit(0);
    }
  }
  
  return 0;
}
