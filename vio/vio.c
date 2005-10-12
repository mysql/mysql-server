/* Copyright (C) 2000 MySQL AB

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

#include "vio_priv.h"

/*
 * Helper to fill most of the Vio* with defaults.
 */

void vio_reset(Vio* vio, enum enum_vio_type type,
		      my_socket sd, HANDLE hPipe,
		      my_bool localhost)
{
  DBUG_ENTER("vio_reset");
  DBUG_PRINT("enter", ("type=%d  sd=%d  localhost=%d", type, sd, localhost));

  bzero((char*) vio, sizeof(*vio));
  vio->type	= type;
  vio->sd	= sd;
  vio->hPipe	= hPipe;
  vio->localhost= localhost;
#ifdef HAVE_VIO  
#ifdef __WIN__ 
  if (type == VIO_TYPE_NAMEDPIPE)
  {
    vio->viodelete	=vio_delete;
    vio->vioerrno	=vio_errno;
    vio->read           =vio_read_pipe;
    vio->write          =vio_write_pipe;
    vio->fastsend	=vio_fastsend;
    vio->viokeepalive	=vio_keepalive;
    vio->should_retry	=vio_should_retry;
    vio->was_interrupted=vio_was_interrupted;
    vio->vioclose	=vio_close_pipe;
    vio->peer_addr	=vio_peer_addr;
    vio->in_addr	=vio_in_addr;
    vio->vioblocking	=vio_blocking;
    vio->is_blocking	=vio_is_blocking;
    vio->timeout	=vio_ignore_timeout;
  }
  else					/* default is VIO_TYPE_TCPIP */
#endif
#ifdef HAVE_SMEM 
  if (type == VIO_TYPE_SHARED_MEMORY)
  {
    vio->viodelete	=vio_delete;
    vio->vioerrno	=vio_errno;
    vio->read           =vio_read_shared_memory;
    vio->write          =vio_write_shared_memory;
    vio->fastsend	=vio_fastsend;
    vio->viokeepalive	=vio_keepalive;
    vio->should_retry	=vio_should_retry;
    vio->was_interrupted=vio_was_interrupted;
    vio->vioclose	=vio_close_shared_memory;
    vio->peer_addr	=vio_peer_addr;
    vio->in_addr	=vio_in_addr;
    vio->vioblocking	=vio_blocking;
    vio->is_blocking	=vio_is_blocking;
    vio->timeout	=vio_ignore_timeout;
  }
  else
#endif   
#ifdef HAVE_OPENSSL 
  if (type == VIO_TYPE_SSL)
  {
    vio->viodelete	=vio_ssl_delete;
    vio->vioerrno	=vio_ssl_errno;
    vio->read		=vio_ssl_read;
    vio->write		=vio_ssl_write;
    vio->fastsend	=vio_ssl_fastsend;
    vio->viokeepalive	=vio_ssl_keepalive;
    vio->should_retry	=vio_ssl_should_retry;
    vio->was_interrupted=vio_ssl_was_interrupted;
    vio->vioclose	=vio_ssl_close;
    vio->peer_addr	=vio_ssl_peer_addr;
    vio->in_addr	=vio_ssl_in_addr;
    vio->vioblocking	=vio_ssl_blocking;
    vio->is_blocking	=vio_is_blocking;
    vio->timeout	=vio_ssl_timeout;
  }
  else					/* default is VIO_TYPE_TCPIP */
#endif /* HAVE_OPENSSL */
  {
    vio->viodelete	=vio_delete;
    vio->vioerrno	=vio_errno;
    vio->read		=vio_read;
    vio->write		=vio_write;
    vio->fastsend	=vio_fastsend;
    vio->viokeepalive	=vio_keepalive;
    vio->should_retry	=vio_should_retry;
    vio->was_interrupted=vio_was_interrupted;
    vio->vioclose	=vio_close;
    vio->peer_addr	=vio_peer_addr;
    vio->in_addr	=vio_in_addr;
    vio->vioblocking	=vio_blocking;
    vio->is_blocking	=vio_is_blocking;
    vio->timeout	=vio_timeout;
  }
#endif /* HAVE_VIO */
  DBUG_VOID_RETURN;
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
#if !defined(__WIN__) && !defined(__EMX__) && !defined(OS2)
#if !defined(NO_FCNTL_NONBLOCK)
    /*
      We call fcntl() to set the flags and then immediately read them back
      to make sure that we and the system are in agreement on the state of
      things.

      An example of why we need to do this is FreeBSD (and apparently some
      other BSD-derived systems, like Mac OS X), where the system sometimes
      reports that the socket is set for non-blocking when it really will
      block.
    */
    fcntl(sd, F_SETFL, vio->fcntl_mode);
    vio->fcntl_mode= fcntl(sd, F_GETFL);
#elif defined(HAVE_SYS_IOCTL_H)			/* hpux */
    /* Non blocking sockets doesn't work good on HPUX 11.0 */
    (void) ioctl(sd,FIOSNBIO,0);
    vio->fcntl_mode &= ~O_NONBLOCK;
#endif
#else /* !defined(__WIN__) && !defined(__EMX__) */
    {
      /* set to blocking mode by default */
      ulong arg=0, r;
      r = ioctlsocket(sd,FIONBIO,(void*) &arg);
      vio->fcntl_mode &= ~O_NONBLOCK;
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

#ifdef HAVE_SMEM
Vio *vio_new_win32shared_memory(NET *net,HANDLE handle_file_map, HANDLE handle_map,
                                HANDLE event_server_wrote, HANDLE event_server_read,
                                HANDLE event_client_wrote, HANDLE event_client_read,
                                HANDLE event_conn_closed)
{
  Vio *vio;
  DBUG_ENTER("vio_new_win32shared_memory");
  if ((vio = (Vio*) my_malloc(sizeof(Vio),MYF(MY_WME))))
  {
    vio_reset(vio, VIO_TYPE_SHARED_MEMORY, 0, 0, TRUE);
    vio->handle_file_map= handle_file_map;
    vio->handle_map= handle_map;
    vio->event_server_wrote= event_server_wrote;
    vio->event_server_read= event_server_read;
    vio->event_client_wrote= event_client_wrote;
    vio->event_client_read= event_client_read;
    vio->event_conn_closed= event_conn_closed;
    vio->shared_memory_remain= 0;
    vio->shared_memory_pos= handle_map;
    vio->net= net;
    strmov(vio->desc, "shared memory");
  }
  DBUG_RETURN(vio);
}
#endif
#endif

void vio_delete(Vio* vio)
{
  /* It must be safe to delete null pointers. */
  /* This matches the semantics of C++'s delete operator. */
  if (vio)
  {
    if (vio->type != VIO_CLOSED)
#ifdef HAVE_VIO /*WAX*/
      vio->vioclose(vio);
#else
      vio_close(vio);
#endif
    my_free((gptr) vio,MYF(0));
  }
}
