/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Note that we can't have assertion on file descriptors;  The reason for
  this is that during mysql shutdown, another thread can close a file
  we are working on.  In this case we should just return read errors from
  the file descriptior.
*/

#define DONT_MAP_VIO
#include <global.h>
#include <mysql_com.h>
#include <violite.h>

#include <errno.h>
#include <assert.h>
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


/*
 * Helper to fill most of the Vio* with defaults.
 */

void vio_reset(Vio* vio, enum enum_vio_type type,
		      my_socket sd, HANDLE hPipe,
		      my_bool localhost)
{
  bzero((char*) vio, sizeof(*vio));
  vio->type	= type;
  vio->sd	= sd;
  vio->hPipe	= hPipe;
  vio->localhost= localhost;
#ifdef HAVE_VIO
  if (type == VIO_TYPE_SSL)
  {
    vio->viodelete	=vio_ssl_delete;
    vio->vioerrno	=vio_ssl_errno;
    vio->read		=vio_ssl_read;
    vio->write		=vio_ssl_write;
    vio->fastsend	=vio_ssl_fastsend;
    vio->viokeepalive	=vio_ssl_keepalive;
    vio->should_retry	=vio_ssl_should_retry;
    vio->vioclose	=vio_ssl_close;
    vio->peer_addr	=vio_ssl_peer_addr;
    vio->in_addr	=vio_ssl_in_addr;
    vio->poll_read	=vio_ssl_poll_read;
  }
  else					/* default is VIO_TYPE_TCPIP */
  {
    vio->viodelete	=vio_delete;
    vio->vioerrno	=vio_errno;
    vio->read		=vio_read;
    vio->write		=vio_write;
    vio->fastsend	=vio_fastsend;
    vio->viokeepalive	=vio_keepalive;
    vio->should_retry	=vio_should_retry;
    vio->vioclose	=vio_close;
    vio->peer_addr	=vio_peer_addr;
    vio->in_addr	=vio_in_addr;
    vio->poll_read	=vio_poll_read;
  }
#endif /* HAVE_VIO */
}

/* Open the socket or TCP/IP connection and read the fnctl() status */

Vio *vio_new(my_socket sd, enum enum_vio_type type, my_bool localhost)
{
  Vio *vio;
  DBUG_ENTER("vio_new");
  DBUG_PRINT("enter", ("sd=%d", sd));
  if ((vio = (Vio*) my_malloc(sizeof(*vio),MYF(MY_WME))))
  {
    vio_reset(vio, type, sd, 0, localhost);
    sprintf(vio->desc,
	    (vio->type == VIO_TYPE_SOCKET ? "socket (%d)" : "TCP/IP (%d)"),
	    vio->sd);
#if !defined(___WIN__) && !defined(__EMX__)
#if !defined(NO_FCNTL_NONBLOCK)
    vio->fcntl_mode = fcntl(sd, F_GETFL);
#elif defined(HAVE_SYS_IOCTL_H)			/* hpux */
    /* Non blocking sockets doesn't work good on HPUX 11.0 */
    (void) ioctl(sd,FIOSNBIO,0);
#endif
#else /* !defined(__WIN__) && !defined(__EMX__) */
    {
      /* set to blocking mode by default */
      ulong arg=0, r;
      r = ioctlsocket(sd,FIONBIO,(void*) &arg, sizeof(arg));
    }
#endif
  }
  DBUG_RETURN(vio);
}


#ifdef __WIN__

Vio *vio_new_win32pipe(HANDLE hPipe)
{
  Vio *vio;
  DBUG_ENTER("vio_new_handle");
  if ((vio = (Vio*) my_malloc(sizeof(Vio),MYF(MY_WME))))
  {
    vio_reset(vio, VIO_TYPE_NAMEDPIPE, 0, hPipe, TRUE);
    strmov(vio->desc, "named pipe");
  }
  DBUG_RETURN(vio);
}

#endif
