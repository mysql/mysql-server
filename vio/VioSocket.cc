/* Copyright Abandoned 2000 Monty Program KB

   This file is public domain and comes with NO WARRANTY of any kind */

/* 
**  Virtual I/O library
**  Written by Andrei Errapart <andreie@no.spam.ee>
*/

#include	"vio-global.h"
#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif
#include	<assert.h>

/*
 * Probably no need to clean this up
 */

#ifdef _WIN32
#include	<winsock.h>
#endif
#include	<sys/types.h>
#if !defined(__WIN32__) && !defined(MSDOS)
#include	<sys/socket.h>
#endif
#if !defined(MSDOS) && !defined(__WIN32__) && !defined(HAVE_BROKEN_NETINET_INCLUDES)
#include	<netinet/in_systm.h>
#include	<netinet/in.h>
#include	<netinet/ip.h>
#if !defined(alpha_linux_port)
#include	<netinet/tcp.h>
#endif
#if defined(__EMX__)
#include <sys/ioctl.h>
#define  ioctlsocket(A,B,C) ioctl((A),(B),(void *)(C),sizeof(*(C)))
#undef HAVE_FCNTL
#endif
#endif

#if defined(MSDOS) || defined(__WIN32__)
#ifdef __WIN32__
#undef errno
#undef EINTR
#undef EAGAIN
#define errno WSAGetLastError()
#define EINTR  WSAEINTR
#define EAGAIN WSAEINPROGRESS
#endif
#endif
#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif

#ifdef	__cplusplus
extern "C" {					// Because of SCO 3.2V4.2
#endif
#ifndef __WIN32__
#include <sys/resource.h>
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#include <netdb.h>
#include <sys/utsname.h>
#endif // __WIN32__
#ifdef	__cplusplus
}
#endif

VIO_NS_BEGIN

#define this_ssl_cip    my_static_cast(SSL_CIPHER*)(this->ssl_cip_)

VioSocket::VioSocket(vio_socket	sd, enum_vio_type type, bool localhost)
:sd_(sd), localhost_(localhost), fcntl_(0),
 fcntl_set_(FALSE), cipher_description_(0)
{
  DBUG_ENTER("VioSocket::VioSocket");
  DBUG_PRINT("enter", ("sd=%d", sd));
  if (type == VIO_TYPE_SOCKET)
    sprintf(desc_,"Socket (%d)",sd_);
  else
    sprintf(desc_,"TCP/IP (%d)",sd_);
  DBUG_VOID_RETURN;
}

VioSocket::~VioSocket()
{
  DBUG_ENTER("VioSocket::~VioSocket");
  DBUG_PRINT("enter", ("sd_=%d", sd_));
  if (sd_>=0)
    close();
  DBUG_VOID_RETURN;
}

bool
VioSocket::is_open() const
{
  return sd_>=0;
}

int
VioSocket::read(vio_ptr buf, int size)
{
  int	r;
  DBUG_ENTER("VioSocket::read");
  DBUG_PRINT("enter", ("sd_=%d, buf=%p, size=%d", sd_, buf, size));
  assert(sd_>=0);
#if defined(MSDOS) || defined(__WIN32__)
  r = ::recv(sd_, buf, size,0);
#else
  r = ::read(sd_, buf, size);
#endif
#ifndef DBUG_OFF
  if ( r < 0)
  {
    DBUG_PRINT("error", ("Got error %d during read",errno));
  }
#endif /* DBUG_OFF */
  DBUG_PRINT("exit", ("%d", r));
  DBUG_RETURN(r);
}

int
VioSocket::write(vio_ptr buf, int size)
{
  int	r;
  DBUG_ENTER("VioSocket::write");
  DBUG_PRINT("enter", ("sd_=%d, buf=%p, size=%d", sd_, buf, size));
  assert(sd_>=0);
#if defined(__WIN32__)
  r = ::send(sd_, buf, size,0);
#else
  r = ::write(sd_, buf, size);
#endif  /* __WIN32__ */
#ifndef DBUG_OFF
  if (r < 0)
  {
    DBUG_PRINT("error", ("Got error %d on write",errno));
  }
#endif /* DBUG_OFF */
  DBUG_RETURN(r);
}

int
VioSocket::blocking(bool set_blocking_mode)
{
  int	r= 0;
  DBUG_ENTER("VioSocket::blocking");
  DBUG_PRINT("enter", ("set_blocking_mode: %d", (int) set_blocking_mode));

#if !defined(___WIN32__) && !defined(__EMX__)
#if !defined(NO_FCNTL_NONBLOCK)
  assert(sd_>=0);

  int old_fcntl=fcntl_;
  if (!fcntl_set_)
  {
    fcntl_set_ = true;
    old_fcntl= fcntl_ = fcntl(F_GETFL);
  }
  if (set_blocking_mode)
    fcntl_&=~O_NONBLOCK; //clear bit
  else
    fcntl_|=O_NONBLOCK; //set bit
  if (old_fcntl != fcntl_)
    r = ::fcntl(sd_, F_SETFL, fcntl_);
#endif /* !defined(NO_FCNTL_NONBLOCK) */
#else /* !defined(__WIN32__) && !defined(__EMX__) */
  { 
    ulong arg;
    int old_fcntl=vio->fcntl_mode;
    if (!vio->fcntl_set)
    {
      vio->fcntl_set = TRUE;
      old_fnctl=vio->fcntl_mode=0;
    }
    if (set_blocking_mode)
    {
      arg = 0;
      fcntl_&=~ O_NONBLOCK; //clear bit
    }
    else
    {
      arg = 1;
      fcntl_|= O_NONBLOCK; //set bit
    }
    if (old_fcntl != fcntl_)
      r = ioctlsocket(sd_,FIONBIO,(void*)&arg,sizeof(arg));
  }
#endif
  DBUG_RETURN(r);
}

bool
VioSocket::blocking() const
{
  DBUG_ENTER("VioSocket::blocking");
  bool r = !(fcntl_ & O_NONBLOCK);
  DBUG_PRINT("exit", ("%d", (int)r));
  DBUG_RETURN(r);
}

int
VioSocket::fastsend(bool onoff)
{
  int r=0;
  DBUG_ENTER("VioSocket::fastsend");
  DBUG_PRINT("enter", ("onoff:%d", (int)onoff));
  assert(sd_>=0);

#ifdef IPTOS_THROUGHPUT
#ifndef	__EMX__
  int	tos = IPTOS_THROUGHPUT;
  if (!setsockopt(sd_, IPPROTO_IP, IP_TOS, (void*) &tos, sizeof(tos)))
#endif /* !__EMX__ */
  {
    int	nodelay = 1;
    if (setsockopt(sd_, IPPROTO_TCP, TCP_NODELAY, (void*) &nodelay,
		   sizeof(nodelay)))
    {
      DBUG_PRINT("warning",
		 ("Couldn't set socket option for fast send"));
      r= -1;
    }
  }
#endif /* IPTOS_THROUGHPUT */
  DBUG_PRINT("exit", ("%d", r));
  DBUG_RETURN(0);
}


int
VioSocket::keepalive(bool set_keep_alive)
{
  DBUG_ENTER("VioSocket::keepalive");
  DBUG_PRINT("enter", ("sd_=%d, set_keep_alive=%d", sd_,
		       (int) set_keep_alive));
  assert(sd_>=0);
  uint opt= set_keep_alive ? 1 : 0;
  DBUG_RETURN(setsockopt(sd_, SOL_SOCKET, SO_KEEPALIVE, (char*) &opt,
			 sizeof(opt)));
}


bool
VioSocket::should_retry() const
{
  int en = errno;
  return en == EAGAIN || en == EINTR || en == EWOULDBLOCK;
}

int
VioSocket::close()
{
  DBUG_ENTER("VioSocket::close");
  assert(sd_>=0);
  int r=0;
  if (::shutdown(sd_,2))
    r= -1;
  if (::closesocket(sd_))
    r= -1;
  if (r)
  {
    DBUG_PRINT("error", ("close() failed, error: %d",errno));
    /* FIXME: error handling (not critical for MySQL) */
  }
  sd_ = -1;
  DBUG_RETURN(r);
}


int
VioSocket::shutdown(int	how)
{
  DBUG_ENTER("VioSocket::shutdown");
  DBUG_PRINT("enter", ("how=%d", how));
  assert(sd_>=0);
  int r = ::shutdown(sd_, how);
  DBUG_PRINT("exit", ("%d", r));
  DBUG_RETURN(r);
}


const char*
VioSocket::description() const
{
  return desc_;
}


bool
VioSocket::peer_addr(char *buf) const
{
  DBUG_ENTER("VioSocket::peer_addr");
  DBUG_PRINT("enter", ("sd_=%d", sd_));
  if (localhost_)
  {
    strmov(buf,"127.0.0.1");
  }
  else
  {
    size_socket addrLen= sizeof(struct sockaddr);
    if (getpeername(sd_, my_reinterpret_cast(struct sockaddr *) (&remote_),
		    &addrLen) != 0)
    {
      DBUG_PRINT("exit", ("getpeername, error: %d", errno));
      DBUG_RETURN(1);
    }
    my_inet_ntoa(remote_.sin_addr,buf);
  }
  DBUG_PRINT("exit", ("addr=%s", buf));
  DBUG_RETURN(0);
}


const char*
VioSocket::cipher_description() const
{
  DBUG_ENTER("VioSocket::cipher_description");
  char *r = cipher_description_ ? cipher_description_:"";
  DBUG_PRINT("exit", ("name: %s", r));
  DBUG_RETURN(r);
}

VIO_NS_END
