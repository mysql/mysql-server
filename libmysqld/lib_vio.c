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

#include <my_global.h>
#include "mysql_embed.h"
#include "mysql.h"

#ifndef HAVE_VIO			/* is Vio enabled */

#include <errno.h>
#include <my_sys.h>
#include <violite.h>
#include <my_sys.h>
#include <my_net.h>
#include <m_string.h>
#include <dbug.h>
#include <assert.h>

#ifndef __WIN__
#define HANDLE void *
#endif

struct st_vio
{
  my_socket		sd;		/* my_socket - real or imaginary */
  HANDLE hPipe;
  my_bool		localhost;	/* Are we from localhost? */
  int			fcntl_mode;	/* Buffered fcntl(sd,F_GETFL) */
  struct sockaddr_in	local;		/* Local internet address */
  struct sockaddr_in	remote;		/* Remote internet address */
  enum enum_vio_type	type;		/* Type of connection */
  char			desc[30];	/* String description */
  void *dest_thd;
  char *packets, **last_packet;
  char *where_in_packet, *end_of_packet;
  my_bool reading;
  MEM_ROOT root;
};

/* Initialize the communication buffer */

Vio *vio_new(my_socket sd, enum enum_vio_type type, my_bool localhost)
{
  Vio * vio = NULL;
  vio = (Vio *) my_malloc (sizeof(*vio),MYF(MY_WME|MY_ZEROFILL));
  if (vio)
  {
    init_alloc_root(&vio->root, 8192, 8192);
    vio->root.min_malloc = sizeof(char *) + 4;
    vio->last_packet = &vio->packets;
  }
  return (vio);
}


#ifdef __WIN__

Vio *vio_new_win32pipe(HANDLE hPipe)
{
  return (NULL);
}

#endif

void vio_delete(Vio * vio)
{
  if (vio)
  {
    if (vio->type != VIO_CLOSED) vio_close(vio);
    free_root(&vio->root, MYF(0));
    my_free((gptr)vio, MYF(0));
  }
}

void vio_reset(Vio *vio)
{
  free_root(&vio->root, MYF(MY_KEEP_PREALLOC));
  vio->packets = vio->where_in_packet = vio->end_of_packet = 0;
  vio->last_packet = &vio->packets;
}

int vio_errno(Vio *vio __attribute__((unused)))
{
  return socket_errno;	/* On Win32 this mapped to WSAGetLastError() */
}

int vio_read(Vio * vio, gptr buf, int size)
{
  vio->reading = 1;
  if (vio->where_in_packet >= vio->end_of_packet)
  {
    dbug_assert(vio->packets);
    vio->where_in_packet = vio->packets + sizeof(char *) + 4;
    vio->end_of_packet = vio->where_in_packet +
      			 uint4korr(vio->packets + sizeof(char *));
    vio->packets = *(char **)vio->packets;
  }
  if (vio->where_in_packet + size > vio->end_of_packet)
    size = vio->end_of_packet - vio->where_in_packet;
  memcpy(buf, vio->where_in_packet, size);
  vio->where_in_packet += size;
  return (size);
}

int vio_write(Vio * vio, const gptr buf, int size)
{
  char *packet;
  if (vio->reading)
  {
    vio->reading = 0;
    vio_reset(vio);
  }
  if ((packet = alloc_root(&vio->root, sizeof(char*) + 4 + size)))
  {
    *vio->last_packet = packet;
    vio->last_packet = (char **)packet;
    *((char **)packet) = 0;	/* Set forward link to 0 */
    packet += sizeof(char *);
    int4store(packet, size);
    memcpy(packet + 4, buf, size);
  }
  else
    size= -1;
  return (size);
}

int vio_blocking(Vio * vio, my_bool set_blocking_mode)
{
  int r=0;
  return (r);
}

my_bool
vio_is_blocking(Vio * vio)
{
  my_bool r=0;
  return(r);
}

int vio_fastsend(Vio * vio)
{
  int r=0;
  return(r);
}

int vio_keepalive(Vio* vio, my_bool set_keep_alive)
{
  int r=0;
  return (r);
}


my_bool
vio_should_retry(Vio * vio __attribute__((unused)))
{
  int en = socket_errno;
  return (en == SOCKET_EAGAIN || en == SOCKET_EINTR ||
	  en == SOCKET_EWOULDBLOCK);
}


int vio_close(Vio * vio)
{
  int r=0;
  return(r);
}


const char *vio_description(Vio * vio)
{
  return "embedded vio";
}

enum enum_vio_type vio_type(Vio* vio)
{
  return VIO_CLOSED;
}

my_socket vio_fd(Vio* vio)
{
  return 0;
}


my_bool vio_peer_addr(Vio * vio, char *buf)
{
  return(0);
}


void vio_in_addr(Vio *vio, struct in_addr *in)
{
}

my_bool vio_poll_read(Vio *vio,uint timeout)
{
  return 0;
}

#endif /* HAVE_VIO */
