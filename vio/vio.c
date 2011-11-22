/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Note that we can't have assertion on file descriptors;  The reason for
  this is that during mysql shutdown, another thread can close a file
  we are working on.  In this case we should just return read errors from
  the file descriptior.
*/

#include "vio_priv.h"

#if defined(__WIN__) || defined(HAVE_SMEM)

/**
  Stub poll_read method that defaults to indicate that there
  is data to read.

  Used for named pipe and shared memory VIO types.

  @param vio      Unused.
  @param timeout  Unused.

  @retval FALSE   There is data to read.
*/

static my_bool no_poll_read(Vio *vio __attribute__((unused)),
                            uint timeout __attribute__((unused)))
{
  return FALSE;
}

#endif

static my_bool has_no_data(Vio *vio __attribute__((unused)))
{
  return FALSE;
}

/*
 * Helper to fill most of the Vio* with defaults.
 */

static void vio_init(Vio* vio, enum enum_vio_type type,
                     my_socket sd, HANDLE hPipe, uint flags)
{
  DBUG_ENTER("vio_init");
  DBUG_PRINT("enter", ("type: %d  sd: %d  flags: %d", type, sd, flags));

#ifndef HAVE_VIO_READ_BUFF
  flags&= ~VIO_BUFFERED_READ;
#endif
  bzero((char*) vio, sizeof(*vio));
  vio->type	= type;
  vio->sd	= sd;
  vio->hPipe	= hPipe;
  vio->localhost= flags & VIO_LOCALHOST;
  if ((flags & VIO_BUFFERED_READ) &&
      !(vio->read_buffer= (char*)my_malloc(VIO_READ_BUFFER_SIZE, MYF(MY_WME))))
    flags&= ~VIO_BUFFERED_READ;
#ifdef _WIN32
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
    vio->vioblocking	=vio_blocking;
    vio->is_blocking	=vio_is_blocking;

    vio->poll_read      =no_poll_read;
    vio->is_connected   =vio_is_connected_pipe;
    vio->has_data       =has_no_data;

    vio->timeout=vio_win32_timeout;
    /* Set default timeout */
    vio->read_timeout_ms= INFINITE;
    vio->write_timeout_ms= INFINITE;
    vio->pipe_overlapped.hEvent= CreateEvent(NULL, TRUE, FALSE, NULL);
    DBUG_VOID_RETURN;
  }
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
    vio->vioblocking	=vio_blocking;
    vio->is_blocking	=vio_is_blocking;

    vio->poll_read      =no_poll_read;
    vio->is_connected   =vio_is_connected_shared_memory;
    vio->has_data       =has_no_data;

    /* Currently, shared memory is on Windows only, hence the below is ok*/
    vio->timeout= vio_win32_timeout; 
    /* Set default timeout */
    vio->read_timeout_ms= INFINITE;
    vio->write_timeout_ms= INFINITE;
    DBUG_VOID_RETURN;
  }
#endif   
#ifdef HAVE_OPENSSL 
  if (type == VIO_TYPE_SSL)
  {
    vio->viodelete	=vio_ssl_delete;
    vio->vioerrno	=vio_errno;
    vio->read		=vio_ssl_read;
    vio->write		=vio_ssl_write;
    vio->fastsend	=vio_fastsend;
    vio->viokeepalive	=vio_keepalive;
    vio->should_retry	=vio_should_retry;
    vio->was_interrupted=vio_was_interrupted;
    vio->vioclose	=vio_ssl_close;
    vio->peer_addr	=vio_peer_addr;
    vio->vioblocking	=vio_ssl_blocking;
    vio->is_blocking	=vio_is_blocking;
    vio->timeout	=vio_timeout;
    vio->poll_read      =vio_poll_read;
    vio->is_connected   =vio_is_connected;
    vio->has_data       =vio_ssl_has_data;
    DBUG_VOID_RETURN;
  }
#endif /* HAVE_OPENSSL */
  vio->viodelete        =vio_delete;
  vio->vioerrno         =vio_errno;
  vio->read=            (flags & VIO_BUFFERED_READ) ? vio_read_buff : vio_read;
  vio->write            =vio_write;
  vio->fastsend         =vio_fastsend;
  vio->viokeepalive     =vio_keepalive;
  vio->should_retry     =vio_should_retry;
  vio->was_interrupted  =vio_was_interrupted;
  vio->vioclose         =vio_close;
  vio->peer_addr        =vio_peer_addr;
  vio->vioblocking      =vio_blocking;
  vio->is_blocking      =vio_is_blocking;
  vio->timeout          =vio_timeout;
  vio->poll_read        =vio_poll_read;
  vio->is_connected     =vio_is_connected;
  vio->has_data=        (flags & VIO_BUFFERED_READ) ?
                            vio_buff_has_data : has_no_data;
  DBUG_VOID_RETURN;
}


/* Reset initialized VIO to use with another transport type */

void vio_reset(Vio* vio, enum enum_vio_type type,
               my_socket sd, HANDLE hPipe, uint flags)
{
  my_free(vio->read_buffer);
  vio_init(vio, type, sd, hPipe, flags);
}


/* Open the socket or TCP/IP connection and read the fnctl() status */

Vio *vio_new(my_socket sd, enum enum_vio_type type, uint flags)
{
  Vio *vio;
  DBUG_ENTER("vio_new");
  DBUG_PRINT("enter", ("sd: %d", sd));
  if ((vio = (Vio*) my_malloc(sizeof(*vio),MYF(MY_WME))))
  {
    vio_init(vio, type, sd, 0, flags);
    sprintf(vio->desc,
	    (vio->type == VIO_TYPE_SOCKET ? "socket (%d)" : "TCP/IP (%d)"),
	    vio->sd);
#if !defined(__WIN__)
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
    fcntl(sd, F_SETFL, 0);
    vio->fcntl_mode= fcntl(sd, F_GETFL);
#elif defined(HAVE_SYS_IOCTL_H)			/* hpux */
    /* Non blocking sockets doesn't work good on HPUX 11.0 */
    (void) ioctl(sd,FIOSNBIO,0);
    vio->fcntl_mode &= ~O_NONBLOCK;
#endif
#else /* !defined(__WIN__) */
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
    vio_init(vio, VIO_TYPE_NAMEDPIPE, 0, hPipe, VIO_LOCALHOST);
    strmov(vio->desc, "named pipe");
  }
  DBUG_RETURN(vio);
}

#ifdef HAVE_SMEM
Vio *vio_new_win32shared_memory(HANDLE handle_file_map, HANDLE handle_map,
                                HANDLE event_server_wrote, HANDLE event_server_read,
                                HANDLE event_client_wrote, HANDLE event_client_read,
                                HANDLE event_conn_closed)
{
  Vio *vio;
  DBUG_ENTER("vio_new_win32shared_memory");
  if ((vio = (Vio*) my_malloc(sizeof(Vio),MYF(MY_WME))))
  {
    vio_init(vio, VIO_TYPE_SHARED_MEMORY, 0, 0, VIO_LOCALHOST);
    vio->handle_file_map= handle_file_map;
    vio->handle_map= handle_map;
    vio->event_server_wrote= event_server_wrote;
    vio->event_server_read= event_server_read;
    vio->event_client_wrote= event_client_wrote;
    vio->event_client_read= event_client_read;
    vio->event_conn_closed= event_conn_closed;
    vio->shared_memory_remain= 0;
    vio->shared_memory_pos= handle_map;
    strmov(vio->desc, "shared memory");
  }
  DBUG_RETURN(vio);
}
#endif
#endif


void vio_delete(Vio* vio)
{
  if (!vio)
    return; /* It must be safe to delete null pointers. */

  if (vio->type != VIO_CLOSED)
    vio->vioclose(vio);
  my_free(vio->read_buffer);
  my_free(vio);
}


/*
  Cleanup memory allocated by vio or the
  components below it when application finish

*/
void vio_end(void)
{
#ifdef HAVE_YASSL
  yaSSL_CleanUp();
#endif
}
