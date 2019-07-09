/* Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef VIO_PRIV_INCLUDED
#define VIO_PRIV_INCLUDED

/* Structures and functions private to the vio package */

#define DONT_MAP_VIO
#include <my_global.h>
#include <mysql_com.h>
#include <my_sys.h>
#include <m_string.h>
#include <violite.h>

#include "mysql/psi/psi_memory.h"

extern PSI_memory_key key_memory_vio;
extern PSI_memory_key key_memory_vio_read_buffer;

#ifdef HAVE_OPENSSL
extern PSI_memory_key key_memory_vio_ssl_fd;
#endif


#ifdef _WIN32
size_t vio_read_pipe(Vio *vio, uchar * buf, size_t size);
size_t vio_write_pipe(Vio *vio, const uchar * buf, size_t size);
my_bool vio_is_connected_pipe(Vio *vio);
int vio_shutdown_pipe(Vio * vio);

#ifndef EMBEDDED_LIBRARY
size_t vio_read_shared_memory(Vio *vio, uchar * buf, size_t size);
size_t vio_write_shared_memory(Vio *vio, const uchar * buf, size_t size);
my_bool vio_is_connected_shared_memory(Vio *vio);
int vio_shutdown_shared_memory(Vio * vio);
void vio_delete_shared_memory(Vio *vio);
#endif /* !EMBEDDED_LIBRARY */
#endif /* _WIN32 */

my_bool vio_buff_has_data(Vio *vio);
int vio_socket_io_wait(Vio *vio, enum enum_vio_io_event event);
int vio_socket_timeout(Vio *vio, uint which, my_bool old_mode);

#ifdef HAVE_OPENSSL
size_t	vio_ssl_read(Vio *vio,uchar* buf,	size_t size);
size_t	vio_ssl_write(Vio *vio,const uchar* buf, size_t size);

/* When the workday is over... */
int vio_ssl_shutdown(Vio *vio);
void vio_ssl_delete(Vio *vio);
my_bool vio_ssl_has_data(Vio *vio);

#endif /* HAVE_OPENSSL */
#endif /* VIO_PRIV_INCLUDED */
