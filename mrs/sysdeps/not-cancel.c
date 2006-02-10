
#include <errno.h>
#include <sysdep.h>
#include <not-cancel.h>


/*
int open_not_cancel(const char *name, int flags, int mode);
int open_not_cancel_2(const char *name, int flags);
*/

int close_not_cancel(int fd)
{
  return INLINE_SYSCALL (close, 1, fd);
}

void close_not_cancel_no_status(int fd)
{
  INTERNAL_SYSCALL_DECL (err);
  INTERNAL_SYSCALL (close, err, 1, (fd));
}

int read_not_cancel(int fd, void *buf, int n)
{
  return INLINE_SYSCALL (read, 3, (fd), (buf), (n));
}

int write_not_cancel(int fd, const void *buf, int n)
{
  return INLINE_SYSCALL (write, 3, (fd), (buf), (n));
}

void writev_not_cancel_no_status(int fd, const struct iovec *iov, int n)
{
  INTERNAL_SYSCALL_DECL (err);				      \
  INTERNAL_SYSCALL (writev, err, 3, (fd), (iov), (n));
}

int __fcntl_nocancel(int fd, int cmd, long val); /* Defined in io/fcntl.os */
int fcntl_not_cancel(int fd, int cmd, long val)
{
  return __fcntl_nocancel (fd, cmd, val);
}

int waitpid_not_cancel(int pid, int *stat_loc, int options)
{
#ifdef __NR_waitpid
  return INLINE_SYSCALL (waitpid, 3, pid, stat_loc, options);
#else
  return INLINE_SYSCALL (wait4, 4, pid, stat_loc, options, NULL);
#endif
}
