
struct iovec;

int open_not_cancel(const char *name, int flags, int mode);
int open_not_cancel_2(const char *name, int flags);
int close_not_cancel(int fd);
void close_not_cancel_no_status(int fd);
int read_not_cancel(int fd, void *buf, int n);
int write_not_cancel(int fd, const void *buf, int n);
void writev_not_cancel_no_status(int fd, const struct iovec *iov, int n);
int fcntl_not_cancel(int fd, int cmd, long val);
int waitpid_not_cancel(int pid, int *stat_loc, int options);
