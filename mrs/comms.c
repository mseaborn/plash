
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <assert.h>

#include "region.h"
#include "comms.h"


#define MOD_DEBUG 0
#define MOD_ERROR_PRINT 1
#define MOD_MSG HALF_NAME ":comm: "

#ifdef IN_RTLD
static int printf(const char *fmt, ...)
{
  return 0;
}
#endif


/* Tries to receive data and FDs from a socket.
   Returns 0 if there was no error, -1 otherwise. */
int recv_with_fds(int sock, char *buffer, int buffer_size,
		  int *fds, int fds_size, int *bytes_got_ret, int *fds_got_ret)
{
  int control_buf_size = CMSG_SPACE(fds_size * sizeof(int));
  struct msghdr msghdr;
  struct iovec iovec;
  struct cmsghdr *cmsg;
  int bytes_got, fds_got = 0;

  iovec.iov_base = buffer;
  iovec.iov_len = buffer_size;
  msghdr.msg_name = 0;
  msghdr.msg_namelen = 0;
  msghdr.msg_iov = &iovec;
  msghdr.msg_iovlen = 1;
  msghdr.msg_control = alloca(control_buf_size);
  msghdr.msg_controllen = control_buf_size;
  msghdr.msg_flags = 0;

  bytes_got = recvmsg(sock, &msghdr, 0);
  if(bytes_got < 0) {
    if(MOD_DEBUG) { perror("recvmsg"); }
    *bytes_got_ret = 0;
    *fds_got_ret = 0;
    return -1;
  }

  /* MSG_CTRUNC means the ancillary data was truncated, so FDs were lost.
     We don't have any way of recovering from that. */
  if(msghdr.msg_flags & MSG_CTRUNC) {
    if(MOD_ERROR_PRINT) {
      printf(MOD_MSG "ancillary data truncated: had allocated %i bytes\n",
	     control_buf_size);
    }
    assert(0);
  }

  for(cmsg = CMSG_FIRSTHDR(&msghdr); cmsg; cmsg = CMSG_NXTHDR(&msghdr, cmsg)) {
    int *data = CMSG_DATA(cmsg);
    int len = cmsg->cmsg_len - CMSG_LEN(0);
    assert(len >= 0);
    if(cmsg->cmsg_level == SOL_SOCKET &&
       cmsg->cmsg_type == SCM_RIGHTS) {
      assert(fds_got == 0);
      fds_got = len / sizeof(int);
      memcpy(fds, data, fds_got * sizeof(int));
    }
    else {
      if(MOD_ERROR_PRINT) {
	printf(MOD_MSG "unknown ancillary data, level=%i, type=%i\n",
	       cmsg->cmsg_level, cmsg->cmsg_type);
      }
    }
  }
  *bytes_got_ret = bytes_got;
  *fds_got_ret = fds_got;
  return 0;
}

/* Returns 0 if there was no error, -1 otherwise. */
int send_with_fds(int sock, const char *buffer, int buffer_size,
		  const int *fds, int fds_size)
{
  int control_buf_size = CMSG_SPACE(fds_size * sizeof(int));
  struct msghdr msghdr;
  struct iovec iovec;
  struct cmsghdr *cmsg;
  int sent;

  iovec.iov_base = (char *) buffer;
  iovec.iov_len = buffer_size;
  msghdr.msg_name = 0;
  msghdr.msg_namelen = 0;
  msghdr.msg_iov = &iovec;
  msghdr.msg_iovlen = 1;
  msghdr.msg_control = alloca(control_buf_size);
  msghdr.msg_controllen = control_buf_size;
  msghdr.msg_flags = 0;
  cmsg = CMSG_FIRSTHDR(&msghdr); /* The same as msghdr.msg_control */
  cmsg->cmsg_len = CMSG_LEN(fds_size * sizeof(int));
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  memcpy(CMSG_DATA(cmsg), fds, fds_size * sizeof(int));

  /* FDs are never partially sent.  Data may be partially sent, but in
     this case, all the FDs have been sent. */
  sent = sendmsg(sock, &msghdr, 0);
  if(sent < 0) {
    if(MOD_DEBUG) { perror("sendmsg"); }
    return -1;
  }

  while(sent < buffer_size) {
    int sent2 = send(sock, buffer + sent, buffer_size - sent, 0);
    if(sent2 < 0) {
      if(MOD_DEBUG) { perror("send"); }
      return -1;
    }
    assert(sent2 > 0);
    sent += sent2;
  }
  return 0;
}


struct comm *comm_init(int sock)
{
  struct comm *comm = amalloc(sizeof(struct comm));
  comm->sock = sock;
  comm->buf_size = 1024;
  comm->buf = amalloc(comm->buf_size);
  comm->pos = 0;
  comm->got = 0;
  comm->fds_buf_size = 10;
  comm->fds_buf = amalloc(comm->fds_buf_size * sizeof(int));
  comm->fds_pos = 0;
  comm->fds_got = 0;
  return comm;
}

void comm_free(struct comm *comm)
{
  free(comm->buf);
  free(comm->fds_buf);
  free(comm);
}

/* Reallocate or rearrange the buffer so that no more than `wastage' bytes
   are wasted (ie. storing consumed bytes), and so that at least `avail'
   bytes are available for the current message (including already received
   data).  Never shrinks the buffer. */
static void comm_resize(struct comm *comm, int wastage, int avail)
{
  if(comm->buf_size - comm->pos < avail || comm->pos > wastage) {
    if(comm->buf_size >= avail) {
      if(MOD_DEBUG) printf(MOD_MSG "data resize: shift\n");
      memmove(comm->buf, comm->buf + comm->pos, comm->got);
      comm->pos = 0;
    }
    else {
      char *b;
      if(MOD_DEBUG) printf(MOD_MSG "data resize: realloc\n");
      b = amalloc(avail);
      memcpy(b, comm->buf + comm->pos, comm->got);
      free(comm->buf);
      comm->buf = b;
      comm->buf_size = avail;
      comm->pos = 0;
    }
  }
}

static void comm_fds_resize(struct comm *comm, int avail)
{
  if(comm->fds_buf_size - comm->fds_pos < avail || comm->fds_pos > 0) {
    if(comm->fds_buf_size >= avail) {
      if(MOD_DEBUG) printf(MOD_MSG "fds resize: shift (%i FDs consumed, %i unconsumed)\n", comm->fds_pos, comm->fds_got);
      memmove(comm->fds_buf, comm->fds_buf + comm->fds_pos,
	      comm->fds_got * sizeof(int));
      comm->fds_pos = 0;
    }
    else {
      int *b;
      if(MOD_DEBUG) printf(MOD_MSG "fds resize: realloc (%i FDs consumed, %i unconsumed)\n", comm->fds_pos, comm->fds_got);
      b = amalloc(avail * sizeof(int));
      memcpy(b, comm->fds_buf + comm->fds_pos, comm->fds_got * sizeof(int));
      free(comm->fds_buf);
      comm->fds_buf = b;
      comm->fds_buf_size = avail;
      comm->fds_pos = 0;
    }
  }
}

/* Read some data into the buffer */
int comm_read(struct comm *comm)
{
  int bytes_got, fds_got;

  /* There is basically a fixed number of FDs we can receive per message,
     because if we don't allocate enough space, recvmsg drops them. */
  comm_fds_resize(comm, 20);
  {
  int offset = comm->pos + comm->got;
  int fds_offset = comm->fds_pos + comm->fds_got;
  int err =
    recv_with_fds(comm->sock,
		  comm->buf + offset, comm->buf_size - offset,
		  comm->fds_buf + fds_offset, comm->fds_buf_size - fds_offset,
		  &bytes_got, &fds_got);
  comm->got += bytes_got;
  comm->fds_got += fds_got;
  if(MOD_DEBUG) printf(MOD_MSG "read %i bytes, %i fds\n", bytes_got, fds_got);
  
  if(err < 0) { return -1; }
  if(bytes_got == 0 && fds_got == 0) { return 0; }
  return 1;
  }
}

/* Returns <0 if an error occurred;
   COMM_END at the end of the stream;
   COMM_AVAIL if a message was available (it's removed from the buffer in
     this case); returns in "result_*" data that can be used until the
     next call to this;
   COMM_UNAVAIL if the buffer doesn't contain a full message. */
int comm_try_get(struct comm *comm, seqf_t *result_data, fds_t *result_fds)
{
  int header_size = 12;
  seqf_t block_orig = { comm->buf + comm->pos, comm->got };
  seqf_t block = block_orig;
  int size, size_fds;
  int ok = 1;
  m_str(&ok, &block, "MSG!");
  m_int(&ok, &block, &size);
  m_int(&ok, &block, &size_fds);
  if(ok) {
    int size_align = (size + 3) & ~3;
    assert(sizeof(int) == 4); /* FIXME */
    if(block.size >= size_align && comm->fds_got >= size_fds) {
      seqf_t got_data = { block.data, size };
      fds_t got_fds = { comm->fds_buf + comm->fds_pos, size_fds };
      comm->pos += header_size + size_align;
      comm->got -= header_size + size_align;
      comm->fds_pos += size_fds;
      comm->fds_got -= size_fds;
      *result_data = got_data;
      *result_fds = got_fds;
      return COMM_AVAIL;
    }
    else {
      comm_resize(comm, 100, header_size + size_align);
      /* Resizing the FDs buffer here isn't actually very useful, because
	 by the time we've received the header, recvmsg would have already
	 tried to send the FDs and so would have overflowed the buffer. */
      comm_fds_resize(comm, size_fds + 10);
      return COMM_UNAVAIL;
    }
  }
  else {
    assert(block_orig.size < header_size); /* otherwise we're out of sync with the sender */
    comm_resize(comm, 100, header_size);
    return COMM_UNAVAIL;
  }
}

/* Returns in result a block that can be used until the next call to this. */
/* Like comm_read(), returns <0 if an error occurred, 0 at the end of the
   stream, and >1 for a valid message. */
int comm_get(struct comm *comm, seqf_t *result_data, fds_t *result_fds)
{
  while(1) {
    int r = comm_try_get(comm, result_data, result_fds);
    if(r != COMM_UNAVAIL) return r;
    r = comm_read(comm);
    if(r == 0) assert(0);
    if(r < 0) return r;
  }
}

int comm_send(region_t r, int sock, seqt_t msg, fds_t fds)
{
  seqf_t data = flatten(r, cat5(r, mk_string(r, "MSG!"),
				mk_int(r, msg.size),
				mk_int(r, fds.count),
				msg,
				mk_repeat(r, '\0', 3 - ((msg.size + 3) & 3))));
  return send_with_fds(sock, data.data, data.size, fds.fds, fds.count);
}
