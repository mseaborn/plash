
#ifndef comms_h
#define comms_h

#include "region.h"


struct comm {
  int sock;
  
  char *buf;
  int buf_size; /* Allocated size of buf */
  int pos; /* Position of message being received; number of consumed bytes in buf */
  int got; /* Number of bytes received after pos */

  int *fds_buf;
  int fds_buf_size;
  int fds_pos;
  int fds_got;
};

#define COMM_END 0
#define COMM_AVAIL 1
#define COMM_UNAVAIL 2

struct comm *comm_init(int sock);
void comm_free(struct comm *comm);
int comm_read(struct comm *comm);
int comm_try_get(struct comm *comm, seqf_t *result_data, fds_t *result_fds);
int comm_get(struct comm *comm, seqf_t *result_data, fds_t *result_fds);
int comm_send(region_t r, int sock, seqt_t msg, fds_t fds);


#endif
