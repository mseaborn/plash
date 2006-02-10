
#define weak_alias(name, aliasname) \
  extern int aliasname() __attribute ((weak, alias (#name)));

int __libc_open(const char *filename, int flags, int mode);
int __libc_open64(const char *filename, int flags, int mode);

int __open(const char *filename, int flags, int mode)
{
  return __libc_open(filename, flags, mode);
}

int __open64(const char *filename, int flags, int mode)
{
  return __libc_open64(filename, flags, mode);
}

weak_alias(__open, open)
weak_alias(__open64, open64)


struct sockaddr;
typedef int socklen_t;
int __libc_connect(int sock_fd, const struct sockaddr *addr, socklen_t addr_len);

int __connect(int sock_fd, const struct sockaddr *addr, socklen_t addr_len)
{
  return __libc_connect(sock_fd, addr, addr_len);
}
weak_alias(__connect, connect)

int __libc_accept(int sock_fd, struct sockaddr *addr, socklen_t addr_len);

int __accept(int sock_fd, struct sockaddr *addr, socklen_t addr_len)
{
  return __libc_accept(sock_fd, addr, addr_len);
}
weak_alias(__accept, accept)
