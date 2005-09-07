/* Copyright (C) 2003 MySQL AB

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

/* Structures and functions private to the vio package */

#define DONT_MAP_VIO
#include <my_global.h>
#include <mysql_com.h>
#include <my_sys.h>
#include <m_string.h>
#include <violite.h>

void	vio_ignore_timeout(Vio *vio, uint which, uint timeout);

#ifdef HAVE_OPENSSL
#include "my_net.h"			/* needed because of struct in_addr */

void	vio_ssl_delete(Vio* vio);
int	vio_ssl_read(Vio *vio,gptr buf,	int size);
int	vio_ssl_write(Vio *vio,const gptr buf,int size);
void	vio_ssl_timeout(Vio *vio, uint which, uint timeout);

/* setsockopt TCP_NODELAY at IPPROTO_TCP level, when possible. */
int vio_ssl_fastsend(Vio *vio);
/* setsockopt SO_KEEPALIVE at SOL_SOCKET level, when possible. */
int vio_ssl_keepalive(Vio *vio, my_bool onoff);
/* Whenever we should retry the last read/write operation. */
my_bool vio_ssl_should_retry(Vio *vio);
/* Check that operation was timed out */
my_bool	vio_ssl_was_interrupted(Vio *vio);
/* When the workday is over... */
int vio_ssl_close(Vio *vio);
/* Return last error number */
int vio_ssl_errno(Vio *vio);
my_bool vio_ssl_peer_addr(Vio *vio, char *buf, uint16 *port);
void vio_ssl_in_addr(Vio *vio, struct in_addr *in);
int vio_ssl_blocking(Vio *vio, my_bool set_blocking_mode, my_bool *old_mode);

/* Single copy for server */
enum vio_ssl_acceptorfd_state
{
  state_connect       = 1,
  state_accept        = 2
};
#endif /* HAVE_OPENSSL */
