/* Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.

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


#ifndef NODE_CONNECTION_H
#define	NODE_CONNECTION_H

#include <stdlib.h>

#ifdef XCOM_HAVE_OPENSSL
#include "openssl/ssl.h"
#endif

/* YaSSL does not have ERR_clear_error() */
#if defined(HAVE_YASSL)
#define ERR_clear_error()
#endif

#include "xcom_proto.h"
#include "xcom_vp.h"

#ifdef	__cplusplus
extern "C"
{
#endif

	enum con_state {
		CON_NULL,
		CON_FD,
		CON_PROTO
	};
	typedef enum con_state con_state;

	struct connection_descriptor {
		int fd;
#ifdef XCOM_HAVE_OPENSSL
		SSL *ssl_fd;
#endif
		con_state connected_;
		unsigned int snd_tag;
		xcom_proto x_proto;
	};

	typedef struct connection_descriptor connection_descriptor;

#ifdef XCOM_HAVE_OPENSSL
	static inline connection_descriptor *new_connection(int fd, SSL *ssl_fd)
	{
		connection_descriptor *c = (connection_descriptor *) calloc(1,sizeof(connection_descriptor));
		c->fd = fd;
		c->ssl_fd = ssl_fd;
		c->connected_ = CON_NULL;
		return c;
	}
#else
	static inline connection_descriptor *new_connection(int fd)
	{
		connection_descriptor *c = (connection_descriptor *) calloc(1,sizeof(connection_descriptor));
		c->fd = fd;
		c->connected_ = CON_NULL;
		return c;
	}
#endif
	static inline int is_connected(connection_descriptor *con)
	{
		return con->connected_ >= CON_FD;
	}

	static inline int proto_done(connection_descriptor *con)
	{
		return con->connected_ == CON_PROTO;
	}

	static inline void set_connected(connection_descriptor *con, con_state val)
	{
		con->connected_ = val;
	}

#ifdef	__cplusplus
}
#endif

#endif	/* NODE_CONNECTION_H */

