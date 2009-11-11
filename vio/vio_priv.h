/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Structures and functions private to the vio package */

#define DONT_MAP_VIO
#include <my_global.h>
#include <mysql_com.h>
#include <my_sys.h>
#include <m_string.h>
#include <violite.h>

#ifdef _WIN32
void	vio_win32_timeout(Vio *vio, uint which, uint timeout);
#endif

void	vio_timeout(Vio *vio,uint which, uint timeout);

#ifdef HAVE_OPENSSL
#include "my_net.h"			/* needed because of struct in_addr */

size_t	vio_ssl_read(Vio *vio,uchar* buf,	size_t size);
size_t	vio_ssl_write(Vio *vio,const uchar* buf, size_t size);

/* When the workday is over... */
int vio_ssl_close(Vio *vio);
void vio_ssl_delete(Vio *vio);

int vio_ssl_blocking(Vio *vio, my_bool set_blocking_mode, my_bool *old_mode);

#endif /* HAVE_OPENSSL */
