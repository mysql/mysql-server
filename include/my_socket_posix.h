#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <config.h>

#define MY_SOCKET_FORMAT "%d"
#define MY_SOCKET_FORMAT_VALUE(x) (x.fd)

typedef struct { int fd; } my_socket;

static inline int my_socket_valid(my_socket s)
{
  return (s.fd != -1);
}

static inline my_socket* my_socket_invalidate(my_socket *s)
{
  s->fd= -1;
  return s;
}

static inline my_socket my_socket_create_invalid()
{
  my_socket s;
  my_socket_invalidate(&s);
  return s;
}

static inline int my_socket_close(my_socket s)
{
  return close(s.fd);
}

static inline int my_socket_errno()
{
  return errno;
}

static inline my_socket my_socket_create(int domain, int type, int protocol)
{
  my_socket s;
  s.fd= socket(domain, type, protocol);

  return s;
}

static inline int my_socket_nfds(my_socket s, int nfds)
{
  if(s.fd > nfds)
    return s.fd;
  return nfds;
}

static inline size_t my_recv(my_socket s, char* buf, size_t len, int flags)
{
  return recv(s.fd, buf, len, flags);
}

static inline
size_t my_send(my_socket s, const char* buf, size_t len, int flags)
{
  return send(s.fd, buf, len, flags);
}

static inline int my_socket_reuseaddr(my_socket s, int enable)
{
  const int on = enable;
  return setsockopt(s.fd, SOL_SOCKET, SO_REUSEADDR,
                    (const void*)&on, sizeof(on));
}

static inline int my_socket_nonblock(my_socket s, int enable)
{
  int flags;
  flags = fcntl(s.fd, F_GETFL, 0);
  if (flags < 0)
    return flags;

#if defined(O_NONBLOCK)
#define NONBLOCKFLAG O_NONBLOCK
#elif defined(O_NDELAY)
#define NONBLOCKFLAG O_NDELAY
#endif

  if(enable)
    flags |= NONBLOCKFLAG;
  else
    flags &= ~NONBLOCKFLAG;

  if (fcntl(s.fd, F_SETFL, flags) == -1)
    return my_socket_errno();

  return 0;
#undef NONBLOCKFLAG
}

static inline int my_bind(my_socket s, const struct sockaddr *my_addr,
                          SOCKET_SIZE_TYPE len)
{
  return bind(s.fd, my_addr, len);
}

static inline int my_bind_inet(my_socket s, const struct sockaddr_in *my_addr)
{
  return bind(s.fd, (struct sockaddr*)my_addr, sizeof(struct sockaddr_in));
}

static inline int my_socket_get_port(my_socket s, unsigned short *port)
{
  struct sockaddr_in servaddr;
  SOCKET_SIZE_TYPE sock_len = sizeof(servaddr);
  if(getsockname(s.fd, (struct sockaddr*)&servaddr, &sock_len) < 0) {
    return 1;
  }

  *port= ntohs(servaddr.sin_port);
  return 0;
}

static inline int my_listen(my_socket s, int backlog)
{
  return listen(s.fd, backlog);
}

static inline
my_socket my_accept(my_socket s, struct sockaddr *addr,
                    SOCKET_SIZE_TYPE *addrlen)
{
  my_socket r;
  r.fd= accept(s.fd, addr, addrlen);
  return r;
}

static inline int my_connect_inet(my_socket s, const struct sockaddr_in *addr)
{
  return connect(s.fd, (const struct sockaddr*)addr,
                 sizeof(struct sockaddr_in));
}

static inline
int my_getsockopt(my_socket s, int level, int optname,
                  void *optval, SOCKET_SIZE_TYPE *optlen)
{
  return getsockopt(s.fd, level, optname, optval, optlen);
}

static inline
int my_setsockopt(my_socket s, int level, int optname,
                  void *optval, SOCKET_SIZE_TYPE optlen)
{
  return setsockopt(s.fd, level, optname, optval, optlen);
}

static inline int my_socket_connect_address(my_socket s, struct in_addr *a)
{
  struct sockaddr_in addr;
  SOCKET_SIZE_TYPE addrlen= sizeof(addr);
  if(getpeername(s.fd, (struct sockaddr*)&addr, &addrlen))
    return my_socket_errno();

  *a= addr.sin_addr;
  return 0;
}

static inline int my_getpeername(my_socket s, struct sockaddr *a, SOCKET_SIZE_TYPE *addrlen)
{
  if(getpeername(s.fd, a, addrlen))
    return my_socket_errno();

  return 0;
}

static inline int my_shutdown(my_socket s, int how)
{
  return shutdown(s.fd, how);
}

static inline int my_socket_equal(my_socket s1, my_socket s2)
{
  return s1.fd==s2.fd;
}

static inline ssize_t my_socket_readv(my_socket s, const struct iovec *iov,
                                      int iovcnt)
{
  return readv(s.fd, iov, iovcnt);
}
static inline ssize_t my_socket_writev(my_socket s, const struct iovec *iov,
                                       int iovcnt)
{
  return writev(s.fd, iov, iovcnt);
}


#define my_FD_SET(s,set)   FD_SET(s.fd,set)
#define my_FD_ISSET(s,set) FD_ISSET(s.fd,set)
