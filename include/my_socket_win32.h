
#include <winsock2.h>
#include <ws2tcpip.h>

#include <my_global.h>

#define MY_SOCKET_FORMAT "%p"
#define MY_SOCKET_FORMAT_VALUE(x) (x.s)

typedef struct { SOCKET s; } my_socket;

static inline int my_socket_valid(my_socket s)
{
  return (s.s != INVALID_SOCKET);
}

static inline my_socket* my_socket_invalidate(my_socket *s)
{
  s->s= INVALID_SOCKET;
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
  return closesocket(s.s);
}

static inline int my_socket_errno()
{
  return WSAGetLastError();
}

static inline my_socket my_socket_create(int domain, int type, int protocol)
{
  my_socket s;
  s.s= socket(domain, type, protocol);

  return s;
}

static inline int my_socket_nfds(my_socket s, int nfds)
{
  (void)s;
  return nfds;
}

static inline size_t my_recv(my_socket s, char* buf, size_t len, int flags)
{
  return recv(s.s, buf, len, flags);
}

static inline
size_t my_send(my_socket s, const char* buf, size_t len, int flags)
{
  return send(s.s, buf, len, flags);
}

static inline int my_socket_reuseaddr(my_socket s, int enable)
{
  const int on = enable;
  return setsockopt(s.s, SOL_SOCKET, SO_REUSEADDR,
                    (const char*)&on, sizeof(on));
}

static inline int my_socket_nonblock(my_socket s, int enable)
{
  unsigned long  ul = enable;

  if(ioctlsocket(s.s, FIONBIO, &ul))
    return my_socket_errno();

  return 0;
}

static inline int my_bind(my_socket s, const struct sockaddr *my_addr,
                          SOCKET_SIZE_TYPE len)
{
  return bind(s.s, my_addr, len);
}

static inline int my_bind_inet(my_socket s, const struct sockaddr_in *my_addr)
{
  return bind(s.s, (const struct sockaddr*)my_addr, sizeof(struct sockaddr_in));
}

static inline int my_socket_get_port(my_socket s, unsigned short *port)
{
  struct sockaddr_in servaddr;
  SOCKET_SIZE_TYPE sock_len = sizeof(servaddr);
  if(getsockname(s.s, (struct sockaddr*)&servaddr, &sock_len) < 0) {
    return 1;
  }

  *port= ntohs(servaddr.sin_port);
  return 0;
}

static inline int my_listen(my_socket s, int backlog)
{
  return listen(s.s, backlog);
}

static inline
my_socket my_accept(my_socket s, struct sockaddr *addr,
                    SOCKET_SIZE_TYPE *addrlen)
{
  my_socket r;
  r.s= accept(s.s, addr, addrlen);
  return r;
}

static inline int my_connect_inet(my_socket s, const struct sockaddr_in *addr)
{
  return connect(s.s, (const struct sockaddr*) addr,
                 sizeof(struct sockaddr_in));
}

static inline
int my_getsockopt(my_socket s, int level, int optname,
                  void *optval, SOCKET_SIZE_TYPE *optlen)
{
  return getsockopt(s.s, level, optname, (char*)optval, optlen);
}

static inline
int my_setsockopt(my_socket s, int level, int optname,
                  void *optval, SOCKET_SIZE_TYPE optlen)
{
  return setsockopt(s.s, level, optname, (char*)optval, optlen);
}

static inline int my_socket_connect_address(my_socket s, struct in_addr *a)
{
  struct sockaddr_in addr;
  SOCKET_SIZE_TYPE addrlen= sizeof(addr);
  if(getpeername(s.s, (struct sockaddr*)&addr, &addrlen)==SOCKET_ERROR)
    return my_socket_errno();

  *a= addr.sin_addr;
  return 0;
}

static inline int my_getpeername(my_socket s, struct sockaddr *a,
                                 SOCKET_SIZE_TYPE *addrlen)
{
  if(getpeername(s.s, a, addrlen))
    return my_socket_errno();

  return 0;
}

static inline int my_shutdown(my_socket s, int how)
{
  return shutdown(s.s, how);
}

static inline int my_socket_equal(my_socket s1, my_socket s2)
{
  return s1.s==s2.s;
}

/*
 * NOTE: the order of len and base are *DIFFERENT* on Linux and Win32.
 * casting our iovec to a WSABUF is fine as it's the same structure,
 * just with different names for the members.
 */
struct iovec {
  u_long iov_len;   /* 'u_long len' in WSABUF */
  void*  iov_base;  /* 'char*  buf' in WSABUF */
};

static inline ssize_t my_socket_readv(my_socket s, const struct iovec *iov,
                                      int iovcnt)
{
  DWORD rv=0;
  WSARecv(s.s,(LPWSABUF)iov,iovcnt,&rv,0,0,0);
  return rv;
}

static inline ssize_t my_socket_writev(my_socket s, const struct iovec *iov,
                                       int iovcnt)
{
  DWORD rv=0;
  WSASend(s.s,(LPWSABUF)iov,iovcnt,&rv,0,0,0);
  return rv;
}

#define my_FD_SET(sock,set)   FD_SET((sock).s,set)
#define my_FD_ISSET(sock,set) FD_ISSET((sock).s,set)
