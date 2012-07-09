/*
   Copyright (c) 2001, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
   02110-1301  USA */

/*
  Note that we can't have assertion on file descriptors;  The reason for
  this is that during mysql shutdown, another thread can close a file
  we are working on.  In this case we should just return read errors from
  the file descriptior.
*/

#ifdef __WIN__
  #include <winsock2.h>
  #include <MSWSock.h>
  #pragma comment(lib, "ws2_32.lib")
#endif
#include "vio_priv.h"

#ifdef FIONREAD_IN_SYS_FILIO
# include <sys/filio.h>
#endif

int vio_errno(Vio *vio __attribute__((unused)))
{
  return socket_errno;		/* On Win32 this mapped to WSAGetLastError() */
}


size_t vio_read(Vio * vio, uchar* buf, size_t size)
{
  size_t r;
  DBUG_ENTER("vio_read");
  DBUG_PRINT("enter", ("sd: %d  buf: 0x%lx  size: %u", vio->sd, (long) buf,
                       (uint) size));

  /* Ensure nobody uses vio_read_buff and vio_read simultaneously */
  DBUG_ASSERT(vio->read_end == vio->read_pos);
#ifdef __WIN__
  r = recv(vio->sd, buf, size,0);
#else
  errno=0;					/* For linux */
  r = read(vio->sd, buf, size);
#endif /* __WIN__ */
#ifndef DBUG_OFF
  if (r == (size_t) -1)
  {
    DBUG_PRINT("vio_error", ("Got error %d during read",errno));
  }
#endif /* DBUG_OFF */
  DBUG_PRINT("exit", ("%ld", (long) r));
  DBUG_RETURN(r);
}


/*
  Buffered read: if average read size is small it may
  reduce number of syscalls.
*/

size_t vio_read_buff(Vio *vio, uchar* buf, size_t size)
{
  size_t rc;
#define VIO_UNBUFFERED_READ_MIN_SIZE 2048
  DBUG_ENTER("vio_read_buff");
  DBUG_PRINT("enter", ("sd: %d  buf: 0x%lx  size: %u", vio->sd, (long) buf,
                       (uint) size));

  if (vio->read_pos < vio->read_end)
  {
    rc= min((size_t) (vio->read_end - vio->read_pos), size);
    memcpy(buf, vio->read_pos, rc);
    vio->read_pos+= rc;
    /*
      Do not try to read from the socket now even if rc < size:
      vio_read can return -1 due to an error or non-blocking mode, and
      the safest way to handle it is to move to a separate branch.
    */
  }
  else if (size < VIO_UNBUFFERED_READ_MIN_SIZE)
  {
    rc= vio_read(vio, (uchar*) vio->read_buffer, VIO_READ_BUFFER_SIZE);
    if (rc != 0 && rc != (size_t) -1)
    {
      if (rc > size)
      {
        vio->read_pos= vio->read_buffer + size;
        vio->read_end= vio->read_buffer + rc;
        rc= size;
      }
      memcpy(buf, vio->read_buffer, rc);
    }
  }
  else
    rc= vio_read(vio, buf, size);
  DBUG_RETURN(rc);
#undef VIO_UNBUFFERED_READ_MIN_SIZE
}

my_bool vio_buff_has_data(Vio *vio)
{
  return (vio->read_pos != vio->read_end);
}

size_t vio_write(Vio * vio, const uchar* buf, size_t size)
{
  size_t r;
  DBUG_ENTER("vio_write");
  DBUG_PRINT("enter", ("sd: %d  buf: 0x%lx  size: %u", vio->sd, (long) buf,
                       (uint) size));
#ifdef __WIN__
  r = send(vio->sd, buf, size,0);
#else
  r = write(vio->sd, buf, size);
#endif /* __WIN__ */
#ifndef DBUG_OFF
  if (r == (size_t) -1)
  {
    DBUG_PRINT("vio_error", ("Got error on write: %d",socket_errno));
  }
#endif /* DBUG_OFF */
  DBUG_PRINT("exit", ("%u", (uint) r));
  DBUG_RETURN(r);
}

int vio_blocking(Vio * vio __attribute__((unused)), my_bool set_blocking_mode,
		 my_bool *old_mode)
{
  int r=0;
  DBUG_ENTER("vio_blocking");

  *old_mode= test(!(vio->fcntl_mode & O_NONBLOCK));
  DBUG_PRINT("enter", ("set_blocking_mode: %d  old_mode: %d",
		       (int) set_blocking_mode, (int) *old_mode));

#if !defined(__WIN__)
#if !defined(NO_FCNTL_NONBLOCK)
  if (vio->sd >= 0)
  {
    int old_fcntl=vio->fcntl_mode;
    if (set_blocking_mode)
      vio->fcntl_mode &= ~O_NONBLOCK; /* clear bit */
    else
      vio->fcntl_mode |= O_NONBLOCK; /* set bit */
    if (old_fcntl != vio->fcntl_mode)
    {
      r= fcntl(vio->sd, F_SETFL, vio->fcntl_mode);
      if (r == -1)
      {
        DBUG_PRINT("info", ("fcntl failed, errno %d", errno));
        vio->fcntl_mode= old_fcntl;
      }
    }
  }
#else
  r= set_blocking_mode ? 0 : 1;
#endif /* !defined(NO_FCNTL_NONBLOCK) */
#else /* !defined(__WIN__) */
  if (vio->type != VIO_TYPE_NAMEDPIPE && vio->type != VIO_TYPE_SHARED_MEMORY)
  { 
    ulong arg;
    int old_fcntl=vio->fcntl_mode;
    if (set_blocking_mode)
    {
      arg = 0;
      vio->fcntl_mode &= ~O_NONBLOCK; /* clear bit */
    }
    else
    {
      arg = 1;
      vio->fcntl_mode |= O_NONBLOCK; /* set bit */
    }
    if (old_fcntl != vio->fcntl_mode)
      r = ioctlsocket(vio->sd,FIONBIO,(void*) &arg);
  }
  else
    r=  test(!(vio->fcntl_mode & O_NONBLOCK)) != set_blocking_mode;
#endif /* !defined(__WIN__) */
  DBUG_PRINT("exit", ("%d", r));
  DBUG_RETURN(r);
}

my_bool
vio_is_blocking(Vio * vio)
{
  my_bool r;
  DBUG_ENTER("vio_is_blocking");
  r = !(vio->fcntl_mode & O_NONBLOCK);
  DBUG_PRINT("exit", ("%d", (int) r));
  DBUG_RETURN(r);
}


int vio_fastsend(Vio * vio __attribute__((unused)))
{
  int r=0;
  DBUG_ENTER("vio_fastsend");

#if defined(IPTOS_THROUGHPUT)
  {
    int tos = IPTOS_THROUGHPUT;
    r= setsockopt(vio->sd, IPPROTO_IP, IP_TOS, (void *) &tos, sizeof(tos));
  }
#endif                                    /* IPTOS_THROUGHPUT */
  if (!r)
  {
#ifdef __WIN__
    BOOL nodelay= 1;
#else
    int nodelay = 1;
#endif

    r= setsockopt(vio->sd, IPPROTO_TCP, TCP_NODELAY,
                  IF_WIN((const char*), (void*)) &nodelay,
                  sizeof(nodelay));

  }
  if (r)
  {
    DBUG_PRINT("warning", ("Couldn't set socket option for fast send"));
    r= -1;
  }
  DBUG_PRINT("exit", ("%d", r));
  DBUG_RETURN(r);
}

int vio_keepalive(Vio* vio, my_bool set_keep_alive)
{
  int r=0;
  uint opt = 0;
  DBUG_ENTER("vio_keepalive");
  DBUG_PRINT("enter", ("sd: %d  set_keep_alive: %d", vio->sd, (int)
		       set_keep_alive));
  if (vio->type != VIO_TYPE_NAMEDPIPE)
  {
    if (set_keep_alive)
      opt = 1;
    r = setsockopt(vio->sd, SOL_SOCKET, SO_KEEPALIVE, (char *) &opt,
		   sizeof(opt));
  }
  DBUG_RETURN(r);
}


my_bool
vio_should_retry(Vio * vio)
{
  int en = socket_errno;
  /*
    man 2 read write
      EAGAIN or EWOULDBLOCK when a socket is a non-blocking mode means
      that the read/write would block.
    man 7 socket
      EAGAIN or EWOULDBLOCK when a socket is in a blocking mode means
      that the corresponding receiving or sending timeout was reached.
  */
  return  en == SOCKET_EINTR ||
          (!vio_is_blocking(vio) &&
            (en == SOCKET_EAGAIN || en == SOCKET_EWOULDBLOCK));
}


my_bool
vio_was_interrupted(Vio *vio __attribute__((unused)))
{
  int en= socket_errno;
  return (en == SOCKET_EAGAIN || en == SOCKET_EINTR ||
	  en == SOCKET_EWOULDBLOCK || en == SOCKET_ETIMEDOUT);
}


int
mysql_socket_shutdown(my_socket mysql_socket, int how)
{
  int result;

#ifdef __WIN__
  static LPFN_DISCONNECTEX DisconnectEx = NULL;
  if (DisconnectEx == NULL)
  {
    DWORD dwBytesReturned;
    GUID guidDisconnectEx = WSAID_DISCONNECTEX;
    WSAIoctl(mysql_socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
             &guidDisconnectEx, sizeof(GUID),
             &DisconnectEx, sizeof(DisconnectEx), 
             &dwBytesReturned, NULL, NULL);
  }
#endif

  /* Non instrumented code */
#ifdef __WIN__
  if (DisconnectEx)
    result= (DisconnectEx(mysql_socket, (LPOVERLAPPED) NULL,
                          (DWORD) 0, (DWORD) 0) == TRUE) ? 0 : -1;
  else
#endif
    result= shutdown(mysql_socket, how);

  return result;
}


int vio_close(Vio * vio)
{
  int r=0;
  DBUG_ENTER("vio_close");

 if (vio->type != VIO_CLOSED)
  {
    DBUG_ASSERT(vio->type ==  VIO_TYPE_TCPIP ||
      vio->type == VIO_TYPE_SOCKET ||
      vio->type == VIO_TYPE_SSL);

    DBUG_ASSERT(vio->sd >= 0);
    if (mysql_socket_shutdown(vio->sd, SHUT_RDWR))
      r= -1;
    if (closesocket(vio->sd))
      r= -1;
  }
  if (r)
  {
    DBUG_PRINT("vio_error", ("close() failed, error: %d",socket_errno));
    /* FIXME: error handling (not critical for MySQL) */
  }
  vio->type= VIO_CLOSED;
  vio->sd=   -1;
  DBUG_RETURN(r);
}


const char *vio_description(Vio * vio)
{
  return vio->desc;
}

enum enum_vio_type vio_type(Vio* vio)
{
  return vio->type;
}

my_socket vio_fd(Vio* vio)
{
  return vio->sd;
}

/**
  Convert a sock-address (AF_INET or AF_INET6) into the "normalized" form,
  which is the IPv4 form for IPv4-mapped or IPv4-compatible IPv6 addresses.

  @note Background: when IPv4 and IPv6 are used simultaneously, IPv4
  addresses may be written in a form of IPv4-mapped or IPv4-compatible IPv6
  addresses. That means, one address (a.b.c.d) can be written in three forms:
    - IPv4: a.b.c.d;
    - IPv4-compatible IPv6: ::a.b.c.d;
    - IPv4-mapped IPv4: ::ffff:a.b.c.d;

  Having three forms of one address makes it a little difficult to compare
  addresses with each other (the IPv4-compatible IPv6-address of foo.bar
  will be different from the IPv4-mapped IPv6-address of foo.bar).

  @note This function can be made public when it's needed.

  @param src        [in] source IP address (AF_INET or AF_INET6).
  @param src_length [in] length of the src.
  @param dst        [out] a buffer to store normalized IP address
                          (sockaddr_storage).
  @param dst_length [out] actual length of the normalized IP address.
*/
static void vio_get_normalized_ip(const struct sockaddr *src,
                                  int src_length,
                                  struct sockaddr *dst,
                                  int *dst_length)
{
  switch (src->sa_family) {
  case AF_INET:
    memcpy(dst, src, src_length);
    *dst_length= src_length;
    break;

#ifdef HAVE_IPV6
  case AF_INET6:
  {
    const struct sockaddr_in6 *src_addr6= (const struct sockaddr_in6 *) src;
    const struct in6_addr *src_ip6= &(src_addr6->sin6_addr);
    const uint32 *src_ip6_int32= (uint32 *) src_ip6->s6_addr;

    if (IN6_IS_ADDR_V4MAPPED(src_ip6) || IN6_IS_ADDR_V4COMPAT(src_ip6))
    {
      struct sockaddr_in *dst_ip4= (struct sockaddr_in *) dst;

      /*
        This is an IPv4-mapped or IPv4-compatible IPv6 address. It should
        be converted to the IPv4 form.
      */

      *dst_length= sizeof (struct sockaddr_in);

      memset(dst_ip4, 0, *dst_length);
      dst_ip4->sin_family= AF_INET;
      dst_ip4->sin_port= src_addr6->sin6_port;

      /*
        In an IPv4 mapped or compatible address, the last 32 bits represent
        the IPv4 address. The byte orders for IPv6 and IPv4 addresses are
        the same, so a simple copy is possible.
      */
      dst_ip4->sin_addr.s_addr= src_ip6_int32[3];
    }
    else
    {
      /* This is a "native" IPv6 address. */

      memcpy(dst, src, src_length);
      *dst_length= src_length;
    }

    break;
  }
#endif /* HAVE_IPV6 */
  }
}


/**
  Return the normalized IP address string for a sock-address.

  The idea is to return an IPv4-address for an IPv4-mapped and
  IPv4-compatible IPv6 address.

  The function writes the normalized IP address to the given buffer.
  The buffer should have enough space, otherwise error flag is returned.
  The system constant INET6_ADDRSTRLEN can be used to reserve buffers of
  the right size.

  @param addr           [in]  sockaddr object (AF_INET or AF_INET6).
  @param addr_length    [in]  length of the addr.
  @param ip_string      [out] buffer to write normalized IP address.
  @param ip_string_size [in]  size of the ip_string.

  @return Error status.
  @retval TRUE in case of error (the ip_string buffer is not enough).
  @retval FALSE on success.
*/

my_bool vio_get_normalized_ip_string(const struct sockaddr *addr,
                                     int addr_length,
                                     char *ip_string,
                                     size_t ip_string_size)
{
  struct sockaddr_storage norm_addr_storage;
  struct sockaddr *norm_addr= (struct sockaddr *) &norm_addr_storage;
  int norm_addr_length;
  int err_code;

  vio_get_normalized_ip(addr, addr_length, norm_addr, &norm_addr_length);

  err_code= vio_getnameinfo(norm_addr, ip_string, ip_string_size, NULL, 0,
                            NI_NUMERICHOST);

  if (!err_code)
    return FALSE;

  DBUG_PRINT("error", ("getnameinfo() failed with %d (%s).",
                       (int) err_code,
                       (const char *) gai_strerror(err_code)));
  return TRUE;
}


/**
  Return IP address and port of a VIO client socket.

  The function returns an IPv4 address if IPv6 support is disabled.

  The function returns an IPv4 address if the client socket is associated
  with an IPv4-compatible or IPv4-mapped IPv6 address. Otherwise, the native
  IPv6 address is returned.
*/

my_bool vio_peer_addr(Vio *vio, char *ip_buffer, uint16 *port,
                      size_t ip_buffer_size)
{
  DBUG_ENTER("vio_peer_addr");
  DBUG_PRINT("enter", ("Client socked fd: %d", (int) vio->sd));

  if (vio->localhost)
  {
    /*
      Initialize vio->remote and vio->addLen. Set vio->remote to IPv4 loopback
      address.
    */
    struct in_addr *ip4= &((struct sockaddr_in *) &(vio->remote))->sin_addr;

    vio->remote.ss_family= AF_INET;
    vio->addrLen= sizeof (struct sockaddr_in);

    ip4->s_addr= htonl(INADDR_LOOPBACK);

    /* Initialize ip_buffer and port. */

    strmov(ip_buffer, "127.0.0.1");
    *port= 0;
  }
  else
  {
    int err_code;
    char port_buffer[NI_MAXSERV];

    struct sockaddr_storage addr_storage;
    struct sockaddr *addr= (struct sockaddr *) &addr_storage;
    size_socket addr_length= sizeof (addr_storage);

    /* Get sockaddr by socked fd. */

    err_code= getpeername(vio->sd, addr, &addr_length);

    if (err_code)
    {
      DBUG_PRINT("exit", ("getpeername() gave error: %d", socket_errno));
      DBUG_RETURN(TRUE);
    }

    /* Normalize IP address. */

    vio_get_normalized_ip(addr, addr_length,
                          (struct sockaddr *) &vio->remote, &vio->addrLen);

    /* Get IP address & port number. */

    err_code= vio_getnameinfo((struct sockaddr *) &vio->remote,
                              ip_buffer, ip_buffer_size,
                              port_buffer, NI_MAXSERV,
                              NI_NUMERICHOST | NI_NUMERICSERV);

    if (err_code)
    {
      DBUG_PRINT("exit", ("getnameinfo() gave error: %s",
                          gai_strerror(err_code)));
      DBUG_RETURN(TRUE);
    }

    *port= (uint16) strtol(port_buffer, NULL, 10);
  }

  DBUG_PRINT("exit", ("Client IP address: %s; port: %d",
                      (const char *) ip_buffer,
                      (int) *port));
  DBUG_RETURN(FALSE);
}


/**
  Indicate whether there is data to read on a given socket.

  @note An exceptional condition event and/or errors are
        interpreted as if there is data to read.

  @param sd       A connected socket.
  @param timeout  Maximum time in seconds to poll.

  @retval FALSE   There is data to read.
  @retval TRUE    There is no data to read.
*/

static my_bool socket_poll_read(my_socket sd, uint timeout)
{
#ifdef __WIN__
  int res;
  my_socket fd= sd;
  fd_set readfds, errorfds;
  struct timeval tm;
  DBUG_ENTER("socket_poll_read");
  tm.tv_sec= timeout;
  tm.tv_usec= 0;
  FD_ZERO(&readfds);
  FD_ZERO(&errorfds);
  FD_SET(fd, &readfds);
  FD_SET(fd, &errorfds);
  /* The first argument is ignored on Windows, so a conversion to int is OK */
  if ((res= select((int) fd, &readfds, NULL, &errorfds, &tm) <= 0))
  {
    DBUG_RETURN(res < 0 ? 0 : 1);
  }
  res= FD_ISSET(fd, &readfds) || FD_ISSET(fd, &errorfds);
  DBUG_RETURN(!res);
#elif defined(HAVE_POLL)
  struct pollfd fds;
  int res;
  DBUG_ENTER("socket_poll_read");
  fds.fd=sd;
  fds.events=POLLIN;
  fds.revents=0;
  if ((res=poll(&fds,1,(int) timeout*1000)) <= 0)
  {
    DBUG_RETURN(res < 0 ? 0 : 1);		/* Don't return 1 on errors */
  }
  DBUG_RETURN(fds.revents & (POLLIN | POLLERR | POLLHUP) ? 0 : 1);
#else
  return 0;
#endif
}


/**
  Retrieve the amount of data that can be read from a socket.

  @param vio          A VIO object.
  @param bytes[out]   The amount of bytes available.

  @retval FALSE   Success.
  @retval TRUE    Failure.
*/

static my_bool socket_peek_read(Vio *vio, uint *bytes)
{
#if defined(_WIN32)
  int len;
  if (ioctlsocket(vio->sd, FIONREAD, &len))
    return TRUE;
  *bytes= len;
  return FALSE;
#elif defined(FIONREAD_IN_SYS_IOCTL) || defined(FIONREAD_IN_SYS_FILIO)
  int len;
  if (ioctl(vio->sd, FIONREAD, &len) < 0)
    return TRUE;
  *bytes= len;
  return FALSE;
#else
  char buf[1024];
  ssize_t res= recv(vio->sd, &buf, sizeof(buf), MSG_PEEK);
  if (res < 0)
    return TRUE;
  *bytes= res;
  return FALSE;
#endif
}


/**
  Indicate whether there is data to read on a given socket.

  @remark Errors are interpreted as if there is data to read.

  @param sd       A connected socket.
  @param timeout  Maximum time in seconds to wait.

  @retval FALSE   There is data (or EOF) to read. Also FALSE if error.
  @retval TRUE    There is _NO_ data to read or timed out.
*/

my_bool vio_poll_read(Vio *vio, uint timeout)
{
  my_socket sd= vio->sd;
  DBUG_ENTER("vio_poll_read");
#ifdef HAVE_OPENSSL
  if (vio->type == VIO_TYPE_SSL)
    sd= SSL_get_fd((SSL*) vio->ssl_arg);
#endif
  DBUG_RETURN(socket_poll_read(sd, timeout));
}


/**
  Determine if the endpoint of a connection is still available.

  @remark The socket is assumed to be disconnected if an EOF
          condition is encountered.

  @param vio      The VIO object.

  @retval TRUE    EOF condition not found.
  @retval FALSE   EOF condition is signaled.
*/

my_bool vio_is_connected(Vio *vio)
{
  uint bytes= 0;
  DBUG_ENTER("vio_is_connected");

  /* In the presence of errors the socket is assumed to be connected. */

  /*
    The first step of detecting a EOF condition is veryfing
    whether there is data to read. Data in this case would
    be the EOF.
  */
  if (vio_poll_read(vio, 0))
    DBUG_RETURN(TRUE);

  /*
    The second step is read() or recv() from the socket returning
    0 (EOF). Unfortunelly, it's not possible to call read directly
    as we could inadvertently read meaningful connection data.
    Simulate a read by retrieving the number of bytes available to
    read -- 0 meaning EOF.
  */
  if (socket_peek_read(vio, &bytes))
    DBUG_RETURN(TRUE);

#ifdef HAVE_OPENSSL
  /* There might be buffered data at the SSL layer. */
  if (!bytes && vio->type == VIO_TYPE_SSL)
    bytes= SSL_pending((SSL*) vio->ssl_arg);
#endif

  DBUG_RETURN(bytes ? TRUE : FALSE);
}


void vio_timeout(Vio *vio, uint which, uint timeout)
{
#if defined(SO_SNDTIMEO) && defined(SO_RCVTIMEO)
  int r;
  DBUG_ENTER("vio_timeout");

  {
#ifdef __WIN__
  /* Windows expects time in milliseconds as int */
  int wait_timeout= (int) timeout * 1000;
#else
  /* POSIX specifies time as struct timeval. */
  struct timeval wait_timeout;
  wait_timeout.tv_sec= timeout;
  wait_timeout.tv_usec= 0;
#endif

  r= setsockopt(vio->sd, SOL_SOCKET, which ? SO_SNDTIMEO : SO_RCVTIMEO,
                IF_WIN((const char*), (const void*))&wait_timeout,
                sizeof(wait_timeout));

  }

  if (r != 0)
    DBUG_PRINT("error", ("setsockopt failed: %d, errno: %d", r, socket_errno));

  DBUG_VOID_RETURN;
#else
/*
  Platforms not suporting setting of socket timeout should either use
  thr_alarm or just run without read/write timeout(s)
*/
#endif
}


#ifdef __WIN__

/*
  Finish pending IO on pipe. Honor wait timeout
*/
static size_t pipe_complete_io(Vio* vio, char* buf, size_t size, DWORD timeout_ms)
{
  DWORD length;
  DWORD ret;

  DBUG_ENTER("pipe_complete_io");

  ret= WaitForSingleObject(vio->pipe_overlapped.hEvent, timeout_ms);
  /*
    WaitForSingleObjects will normally return WAIT_OBJECT_O (success, IO completed)
    or WAIT_TIMEOUT.
  */
  if(ret != WAIT_OBJECT_0)
  {
    CancelIo(vio->hPipe);
    DBUG_PRINT("error",("WaitForSingleObject() returned  %d", ret));
    DBUG_RETURN((size_t)-1);
  }

  if (!GetOverlappedResult(vio->hPipe,&(vio->pipe_overlapped),&length, FALSE))
  {
    DBUG_PRINT("error",("GetOverlappedResult() returned last error  %d", 
      GetLastError()));
    DBUG_RETURN((size_t)-1);
  }

  DBUG_RETURN(length);
}


size_t vio_read_pipe(Vio * vio, uchar *buf, size_t size)
{
  DWORD bytes_read;
  size_t retval;
  DBUG_ENTER("vio_read_pipe");
  DBUG_PRINT("enter", ("sd: %d  buf: 0x%lx  size: %u", vio->sd, (long) buf,
                       (uint) size));

  if (ReadFile(vio->hPipe, buf, (DWORD)size, &bytes_read,
      &(vio->pipe_overlapped)))
  {
    retval= bytes_read;
  }
  else
  {
    if (GetLastError() != ERROR_IO_PENDING)
    {
      DBUG_PRINT("error",("ReadFile() returned last error %d",
        GetLastError()));
      DBUG_RETURN((size_t)-1);
    }
    retval= pipe_complete_io(vio, buf, size,vio->read_timeout_ms);
  }

  DBUG_PRINT("exit", ("%lld", (longlong)retval));
  DBUG_RETURN(retval);
}


size_t vio_write_pipe(Vio * vio, const uchar* buf, size_t size)
{
  DWORD bytes_written;
  size_t retval;
  DBUG_ENTER("vio_write_pipe");
  DBUG_PRINT("enter", ("sd: %d  buf: 0x%lx  size: %u", vio->sd, (long) buf,
                       (uint) size));

  if (WriteFile(vio->hPipe, buf, (DWORD)size, &bytes_written, 
      &(vio->pipe_overlapped)))
  {
    retval= bytes_written;
  }
  else
  {
    if (GetLastError() != ERROR_IO_PENDING)
    {
      DBUG_PRINT("vio_error",("WriteFile() returned last error %d",
        GetLastError()));
      DBUG_RETURN((size_t)-1);
    }
    retval= pipe_complete_io(vio, (char *)buf, size, vio->write_timeout_ms);
  }

  DBUG_PRINT("exit", ("%lld", (longlong)retval));
  DBUG_RETURN(retval);
}


my_bool vio_is_connected_pipe(Vio *vio)
{
  if (PeekNamedPipe(vio->hPipe, NULL, 0, NULL, NULL, NULL))
    return TRUE;
  else
    return (GetLastError() != ERROR_BROKEN_PIPE);
}


int vio_close_pipe(Vio * vio)
{
  int r;
  DBUG_ENTER("vio_close_pipe");

  CancelIo(vio->hPipe);
  CloseHandle(vio->pipe_overlapped.hEvent);
  DisconnectNamedPipe(vio->hPipe);
  r= CloseHandle(vio->hPipe);
  if (r)
  {
    DBUG_PRINT("vio_error", ("close() failed, error: %d",GetLastError()));
    /* FIXME: error handling (not critical for MySQL) */
  }
  vio->type= VIO_CLOSED;
  vio->sd=   -1;
  DBUG_RETURN(r);
}


void vio_win32_timeout(Vio *vio, uint which , uint timeout_sec)
{
    DWORD timeout_ms;
    /*
      Windows is measuring timeouts in milliseconds. Check for possible int 
      overflow.
    */
    if (timeout_sec > UINT_MAX/1000)
      timeout_ms= INFINITE;
    else
      timeout_ms= timeout_sec * 1000;

    /* which == 1 means "write", which == 0 means "read".*/
    if(which)
      vio->write_timeout_ms= timeout_ms;
    else
      vio->read_timeout_ms= timeout_ms;
}


#ifdef HAVE_SMEM

size_t vio_read_shared_memory(Vio * vio, uchar* buf, size_t size)
{
  size_t length;
  size_t remain_local;
  char *current_position;
  HANDLE events[2];

  DBUG_ENTER("vio_read_shared_memory");
  DBUG_PRINT("enter", ("sd: %d  buf: 0x%lx  size: %d", vio->sd, (long) buf,
                       size));

  remain_local = size;
  current_position=buf;

  events[0]= vio->event_server_wrote;
  events[1]= vio->event_conn_closed;

  do
  {
    if (vio->shared_memory_remain == 0)
    {
      /*
        WaitForMultipleObjects can return next values:
         WAIT_OBJECT_0+0 - event from vio->event_server_wrote
         WAIT_OBJECT_0+1 - event from vio->event_conn_closed. We can't read
		           anything
         WAIT_ABANDONED_0 and WAIT_TIMEOUT - fail.  We can't read anything
      */
      if (WaitForMultipleObjects(array_elements(events), events, FALSE,
                                 vio->read_timeout_ms) != WAIT_OBJECT_0)
      {
        DBUG_RETURN(-1);
      };

      vio->shared_memory_pos = vio->handle_map;
      vio->shared_memory_remain = uint4korr((ulong*)vio->shared_memory_pos);
      vio->shared_memory_pos+=4;
    }

    length = size;

    if (vio->shared_memory_remain < length)
       length = vio->shared_memory_remain;
    if (length > remain_local)
       length = remain_local;

    memcpy(current_position,vio->shared_memory_pos,length);

    vio->shared_memory_remain-=length;
    vio->shared_memory_pos+=length;
    current_position+=length;
    remain_local-=length;

    if (!vio->shared_memory_remain)
    {
      if (!SetEvent(vio->event_client_read))
        DBUG_RETURN(-1);
    }
  } while (remain_local);
  length = size;

  DBUG_PRINT("exit", ("%lu", (ulong) length));
  DBUG_RETURN(length);
}


size_t vio_write_shared_memory(Vio * vio, const uchar* buf, size_t size)
{
  size_t length, remain, sz;
  HANDLE pos;
  const uchar *current_position;
  HANDLE events[2];

  DBUG_ENTER("vio_write_shared_memory");
  DBUG_PRINT("enter", ("sd: %d  buf: 0x%lx  size: %d", vio->sd, (long) buf,
                       size));

  remain = size;
  current_position = buf;

  events[0]= vio->event_server_read;
  events[1]= vio->event_conn_closed;

  while (remain != 0)
  {
    if (WaitForMultipleObjects(array_elements(events), events, FALSE,
                               vio->write_timeout_ms) != WAIT_OBJECT_0)
    {
      DBUG_RETURN((size_t) -1);
    }

    sz= (remain > shared_memory_buffer_length ? shared_memory_buffer_length :
         remain);

    int4store(vio->handle_map,sz);
    pos = vio->handle_map + 4;
    memcpy(pos,current_position,sz);
    remain-=sz;
    current_position+=sz;
    if (!SetEvent(vio->event_client_wrote))
      DBUG_RETURN((size_t) -1);
  }
  length = size;

  DBUG_PRINT("exit", ("%lu", (ulong) length));
  DBUG_RETURN(length);
}


my_bool vio_is_connected_shared_memory(Vio *vio)
{
  return (WaitForSingleObject(vio->event_conn_closed, 0) != WAIT_OBJECT_0);
}


/**
 Close shared memory and DBUG_PRINT any errors that happen on closing.
 @return Zero if all closing functions succeed, and nonzero otherwise.
*/
int vio_close_shared_memory(Vio * vio)
{
  int error_count= 0;
  DBUG_ENTER("vio_close_shared_memory");
  if (vio->type != VIO_CLOSED)
  {
    /*
      Set event_conn_closed for notification of both client and server that
      connection is closed
    */
    SetEvent(vio->event_conn_closed);
    /*
      Close all handlers. UnmapViewOfFile and CloseHandle return non-zero
      result if they are success.
    */
    if (UnmapViewOfFile(vio->handle_map) == 0) 
    {
      error_count++;
      DBUG_PRINT("vio_error", ("UnmapViewOfFile() failed"));
    }
    if (CloseHandle(vio->event_server_wrote) == 0)
    {
      error_count++;
      DBUG_PRINT("vio_error", ("CloseHandle(vio->esw) failed"));
    }
    if (CloseHandle(vio->event_server_read) == 0)
    {
      error_count++;
      DBUG_PRINT("vio_error", ("CloseHandle(vio->esr) failed"));
    }
    if (CloseHandle(vio->event_client_wrote) == 0)
    {
      error_count++;
      DBUG_PRINT("vio_error", ("CloseHandle(vio->ecw) failed"));
    }
    if (CloseHandle(vio->event_client_read) == 0)
    {
      error_count++;
      DBUG_PRINT("vio_error", ("CloseHandle(vio->ecr) failed"));
    }
    if (CloseHandle(vio->handle_file_map) == 0)
    {
      error_count++;
      DBUG_PRINT("vio_error", ("CloseHandle(vio->hfm) failed"));
    }
    if (CloseHandle(vio->event_conn_closed) == 0)
    {
      error_count++;
      DBUG_PRINT("vio_error", ("CloseHandle(vio->ecc) failed"));
    }
  }
  vio->type= VIO_CLOSED;
  vio->sd=   -1;
  DBUG_RETURN(error_count);
}
#endif /* HAVE_SMEM */
#endif /* __WIN__ */


/**
  Number of bytes in the read buffer.

  @return number of bytes in the read buffer or < 0 if error.
*/

ssize_t vio_pending(Vio *vio)
{
#ifdef HAVE_OPENSSL
  SSL *ssl= (SSL*) vio->ssl_arg;
#endif

  if (vio->read_pos < vio->read_end)
    return vio->read_end - vio->read_pos;

#ifdef HAVE_OPENSSL
  if (ssl)
    return SSL_pending(ssl);
#endif

  return 0;
}


/**
  Checks if the error code, returned by vio_getnameinfo(), means it was the
  "No-name" error.

  Windows-specific note: getnameinfo() returns WSANO_DATA instead of
  EAI_NODATA or EAI_NONAME when no reverse mapping is available at the host
  (i.e. Windows can't get hostname by IP-address). This error should be
  treated as EAI_NONAME.

  @return if the error code is actually EAI_NONAME.
  @retval true if the error code is EAI_NONAME.
  @retval false otherwise.
*/

my_bool vio_is_no_name_error(int err_code)
{
#ifdef _WIN32

  return err_code == WSANO_DATA || err_code == EAI_NONAME;

#else

  return err_code == EAI_NONAME;

#endif
}


/**
  This is a wrapper for the system getnameinfo(), because different OS
  differ in the getnameinfo() implementation:
    - Solaris 10 requires that the 2nd argument (salen) must match the
      actual size of the struct sockaddr_storage passed to it;
    - Mac OS X has sockaddr_in::sin_len and sockaddr_in6::sin6_len and
      requires them to be filled.
*/

int vio_getnameinfo(const struct sockaddr *sa,
                    char *hostname, size_t hostname_size,
                    char *port, size_t port_size,
                    int flags)
{
  int sa_length= 0;

  switch (sa->sa_family) {
  case AF_INET:
    sa_length= sizeof (struct sockaddr_in);
#ifdef HAVE_SOCKADDR_IN_SIN_LEN
    ((struct sockaddr_in *) sa)->sin_len= sa_length;
#endif /* HAVE_SOCKADDR_IN_SIN_LEN */
    break;

#ifdef HAVE_IPV6
  case AF_INET6:
    sa_length= sizeof (struct sockaddr_in6);
# ifdef HAVE_SOCKADDR_IN6_SIN6_LEN
    ((struct sockaddr_in6 *) sa)->sin6_len= sa_length;
# endif /* HAVE_SOCKADDR_IN6_SIN6_LEN */
    break;
#endif /* HAVE_IPV6 */
  }

  return getnameinfo(sa, sa_length,
                     hostname, hostname_size,
                     port, port_size,
                     flags);
}
