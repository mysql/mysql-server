#ifndef VIO_PRIV_INCLUDED
#define VIO_PRIV_INCLUDED

/* Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.
   Copyright (c) 2012, Monty Program Ab

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

/* Structures and functions private to the vio package */

#define DONT_MAP_VIO
#include <my_global.h>
#include <mysql_com.h>
#include <my_sys.h>
#include <m_string.h>
#include <violite.h>

#ifndef __WIN__
#include <sys/socket.h>
#include <netdb.h>
#endif

#ifdef _WIN32
void	vio_win32_timeout(Vio *vio, uint which, uint timeout);
#endif

#ifdef __WIN__
size_t vio_read_pipe(Vio *vio, uchar * buf, size_t size);
size_t vio_write_pipe(Vio *vio, const uchar * buf, size_t size);
my_bool vio_is_connected_pipe(Vio *vio);
int vio_close_pipe(Vio * vio);
int cancel_io(HANDLE handle, DWORD thread_id);
int vio_shutdown_pipe(Vio *vio,int how);
#endif

#ifdef HAVE_SMEM
size_t vio_read_shared_memory(Vio *vio, uchar * buf, size_t size);
size_t vio_write_shared_memory(Vio *vio, const uchar * buf, size_t size);
my_bool vio_is_connected_shared_memory(Vio *vio);
int vio_close_shared_memory(Vio * vio);
my_bool vio_shared_memory_has_data(Vio *vio);
int vio_shutdown_shared_memory(Vio *vio, int how);
#endif

int    vio_socket_shutdown(Vio *vio, int how);
void	vio_timeout(Vio *vio,uint which, uint timeout);
my_bool vio_buff_has_data(Vio *vio);

#ifdef HAVE_OPENSSL
#include "my_net.h"			/* needed because of struct in_addr */

size_t	vio_ssl_read(Vio *vio,uchar* buf,	size_t size);
size_t	vio_ssl_write(Vio *vio,const uchar* buf, size_t size);

/* When the workday is over... */
int vio_ssl_close(Vio *vio);
void vio_ssl_delete(Vio *vio);

int vio_ssl_blocking(Vio *vio, my_bool set_blocking_mode, my_bool *old_mode);

my_bool vio_ssl_has_data(Vio *vio);

#endif /* HAVE_OPENSSL */
#endif /* VIO_PRIV_INCLUDED */
