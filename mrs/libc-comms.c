
#include <errno.h>

#include "region.h"
#include "comms.h"
#include "libc-comms.h"


char *glibc_getenv(const char *name);


int comm_sock = -1;
struct comm *comm = 0;

static int my_atoi(const char *str)
{
  int x = 0;
  for(; *str; str++) {
    if('0' <= *str && *str <= '9') x = x*10 + (*str - '0');
    else return -1;
  }
  return x;
}

static int init()
{
  if(comm_sock == -2) {
    /* This could happen when something goes wrong in fork(). */
    __set_errno(EIO); return -1;
  }
  if(comm_sock == -1) {
    char *var = glibc_getenv("COMM_FD");
    if(var) comm_sock = my_atoi(var);
    else { __set_errno(ENOSYS); return -1; }
  }
  if(!comm) {
    comm = comm_init(comm_sock);
  }
  return 0;
}

int send_req(region_t r, seqt_t msg)
{
  if(init() < 0) { return -1; }
  return comm_send(r, comm_sock, msg, fds_empty);
}

int get_reply(seqf_t *msg, fds_t *fds)
{
  if(init() < 0) { return -1; }
  if(comm_get(comm, msg, fds) <= 0) { return -1; }
  return 0;
}

/* Send a message, expecting a reply without FDs. */
int req_and_reply_with_fds(region_t r, seqt_t msg,
			   seqf_t *reply, fds_t *reply_fds)
{
  if(init() < 0) return -1;
  if(comm_send(r, comm_sock, msg, fds_empty) < 0) return -1;
  if(comm_get(comm, reply, reply_fds) <= 0) return -1;
  return 0;
}
int req_and_reply_with_fds2(region_t r, seqt_t msg, fds_t fds,
			    seqf_t *reply, fds_t *reply_fds)
{
  if(init() < 0) return -1;
  if(comm_send(r, comm_sock, msg, fds) < 0) return -1;
  if(comm_get(comm, reply, reply_fds) <= 0) return -1;
  return 0;
}
int req_and_reply(region_t r, seqt_t msg, seqf_t *reply)
{
  fds_t fds;
  if(req_and_reply_with_fds(r, msg, reply, &fds) < 0) return -1;
  close_fds(fds);
  return 0;
}
