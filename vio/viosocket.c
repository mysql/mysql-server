/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/*
  Note that we can't have assertion on file descriptors;  The reason for
  this is that during mysql shutdown, another thread can close a file
  we are working on.  In this case we should just return read errors from
  the file descriptior.
*/

#define DONT_MAP_VIO
#include <global.h>
#include <mysql_com.h>

#include <errno.h>
#include <assert.h>
#include <violite.h>
#include <my_sys.h>
#include <my_net.h>
#include <m_string.h>
#ifdef HAVE_POLL
#include <sys/poll.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#if defined(__EMX__)
#define ioctlsocket ioctl
#endif	/* defined(__EMX__) */

#if defined(MSDOS) || defined(__WIN__)
#ifdef __WIN__
#undef errno
#undef EINTR
#undef EAGAIN
#define errno WSAGetLastError()
#define EINTR  WSAEINTR
#define EAGAIN WSAEINPROGRESS
#endif /* __WIN__ */
#define O_NONBLOCK 1    /* For emulation of fcntl() */
#endif
#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif

#ifndef __WIN__
#define HANDLE void *
#endif

void vio_delete(Vio* vio)
{
  /* It must be safe to delete null pointers. */
  /* This matches the semantics of C++'s delete operator. */
  if (vio)
  {
    if (vio->type != VIO_CLOSED)
      vio_close(vio);
    my_free((gptr) vio,MYF(0));
  }
}

int vio_errno(Vio *vio __attribute__((unused)))
{
  return errno;			/* On Win32 this mapped to WSAGetLastError() */
}


int vio_read(Vio * vio, gptr buf, int size)
{
  int r;
  DBUG_ENTER("vio_read");
  DBUG_PRINT("enter", ("sd=%d, buf=%p, size=%d", vio->sd, buf, size));
#ifdef __WIN__
  if (vio->type == VIO_TYPE_NAMEDPIPE)
  {
    DWORD length;
    if (!ReadFile(vio->hPipe, buf, size, &length, NULL))
      DBUG_RETURN(-1);
    DBUG_RETURN(length);
  }
  r = recv(vio->sd, buf, size,0);
#else
  errno=0;					/* For linux */
  r = read(vio->sd, buf, size);
#endif /* __WIN__ */
#ifndef DBUG_OFF
  if (r < 0)
  {
    DBUG_PRINT("vio_error", ("Got error %d during read",errno));
  }
#endif /* DBUG_OFF */
  DBUG_PRINT("exit", ("%d", r));
  DBUG_RETURN(r);
}


int vio_write(Vio * vio, const gptr buf, int size)
{
  int r;
  DBUG_ENTER("vio_write");
  DBUG_PRINT("enter", ("sd=%d, buf=%p, size=%d", vio->sd, buf, size));
#ifdef __WIN__
  if ( vio->type == VIO_TYPE_NAMEDPIPE)
  {
    DWORD length;
    if (!WriteFile(vio->hPipe, (char*) buf, size, &length, NULL))
      DBUG_RETURN(-1);
    DBUG_RETURN(length);
  }
  r = send(vio->sd, buf, size,0);
#else
  r = write(vio->sd, buf, size);
#endif /* __WIN__ */
#ifndef DBUG_OFF
  if (r < 0)
  {
    DBUG_PRINT("vio_error", ("Got error on write: %d",errno));
  }
#endif /* DBUG_OFF */
  DBUG_PRINT("exit", ("%d", r));
  DBUG_RETURN(r);
}


int vio_blocking(Vio * vio, my_bool set_blocking_mode)
{
  int r=0;
  DBUG_ENTER("vio_blocking");
  DBUG_PRINT("enter", ("set_blocking_mode: %d", (int) set_blocking_mode));

#if !defined(HAVE_OPENSSL)
#if !defined(___WIN__) && !defined(__EMX__)
#if !defined(NO_FCNTL_NONBLOCK)

  if (vio->sd >= 0)
  {
    int old_fcntl=vio->fcntl_mode;
    if (set_blocking_mode)
      vio->fcntl_mode &= ~O_NONBLOCK; /* clear bit */
    else
      vio->fcntl_mode |= O_NONBLOCK; /* set bit */
    if (old_fcntl != vio->fcntl_mode)
      r = fcntl(vio->sd, F_SETFL, vio->fcntl_mode);
  }
#endif /* !defined(NO_FCNTL_NONBLOCK) */
#else /* !defined(__WIN__) && !defined(__EMX__) */
#ifndef __EMX__
  if (vio->type != VIO_TYPE_NAMEDPIPE)  
#endif
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
      r = ioctlsocket(vio->sd,FIONBIO,(void*) &arg, sizeof(arg));
  }
#endif /* !defined(__WIN__) && !defined(__EMX__) */
#endif /* !defined (HAVE_OPENSSL) */ 
  DBUG_PRINT("exit", ("return %d", r));
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

#ifdef IPTOS_THROUGHPUT
  {
#ifndef __EMX__
    int tos = IPTOS_THROUGHPUT;
    if (!setsockopt(vio->sd, IPPROTO_IP, IP_TOS, (void *) &tos, sizeof(tos)))
#endif				/* !__EMX__ */
    {
      int nodelay = 1;
      if (setsockopt(vio->sd, IPPROTO_TCP, TCP_NODELAY, (void *) &nodelay,
		     sizeof(nodelay))) {
	DBUG_PRINT("warning",
		   ("Couldn't set socket option for fast send"));
	r= -1;
      }
    }
  }
#endif	/* IPTOS_THROUGHPUT */
  DBUG_PRINT("exit", ("%d", r));
  DBUG_RETURN(r);
}

int vio_keepalive(Vio* vio, my_bool set_keep_alive)
{
  int r=0;
  uint opt = 0;
  DBUG_ENTER("vio_keepalive");
  DBUG_PRINT("enter", ("sd=%d, set_keep_alive=%d", vio->sd, (int)
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
vio_should_retry(Vio * vio __attribute__((unused)))
{
  int en = errno;
  return en == EAGAIN || en == EINTR || en == EWOULDBLOCK;
}


int vio_close(Vio * vio)
{
  int r;
  DBUG_ENTER("vio_close");
#ifdef __WIN__
  if (vio->type == VIO_TYPE_NAMEDPIPE)
  {
#if defined(__NT__) && defined(MYSQL_SERVER)
    CancelIo(vio->hPipe);
    DisconnectNamedPipe(vio->hPipe);
#endif
    r=CloseHandle(vio->hPipe);
  }
  else if (vio->type != VIO_CLOSED)
#endif /* __WIN__ */
  {
    r=0;
    if (shutdown(vio->sd,2))
      r= -1;
    if (closesocket(vio->sd))
      r= -1;
  }
  if (r)
  {
    DBUG_PRINT("vio_error", ("close() failed, error: %d",errno));
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


my_bool vio_peer_addr(Vio * vio, char *buf)
{
  DBUG_ENTER("vio_peer_addr");
  DBUG_PRINT("enter", ("sd=%d", vio->sd));
  if (vio->localhost)
  {
    strmov(buf,"127.0.0.1");
  }
  else
  {
    size_socket addrLen = sizeof(struct sockaddr);
    if (getpeername(vio->sd, (struct sockaddr *) (& (vio->remote)),
		    &addrLen) != 0)
    {
      DBUG_PRINT("exit", ("getpeername, error: %d", errno));
      DBUG_RETURN(1);
    }
    my_inet_ntoa(vio->remote.sin_addr,buf);
  }
  DBUG_PRINT("exit", ("addr=%s", buf));
  DBUG_RETURN(0);
}


void vio_in_addr(Vio *vio, struct in_addr *in)
{
  DBUG_ENTER("vio_in_addr");
  if (vio->localhost)
    bzero((char*) in, sizeof(*in));	/* This should never be executed */
  else
    *in=vio->remote.sin_addr;
  DBUG_VOID_RETURN;
}


/* Return 0 if there is data to be read */

my_bool vio_poll_read(Vio *vio,uint timeout)
{
#ifndef HAVE_POLL
  return 0;
#else
  struct pollfd fds;
  int res;
  DBUG_ENTER("vio_poll");
  fds.fd=vio->sd;
  fds.events=POLLIN;
  fds.revents=0;
  if ((res=poll(&fds,1,(int) timeout*1000)) <= 0)
  {
    DBUG_RETURN(res < 0 ? 0 : 1);		/* Don't return 1 on errors */
  }
  DBUG_RETURN(fds.revents & POLLIN ? 0 : 1);
#endif
}
