#ifndef COMPONENTS_SERVICES_MYSQL_SOCKET_BITS_H
#define COMPONENTS_SERVICES_MYSQL_SOCKET_BITS_H

/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; version 2 of the
  License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
  02110-1301  USA
*/

#include "my_io.h"
#include "mysql/components/services/my_io_bits.h"

/**
  An instrumented socket.
  @c MYSQL_SOCKET is a replacement for @c my_socket.
*/
struct MYSQL_SOCKET
{
  /** The real socket descriptor. */
  my_socket fd;

  /**
    The instrumentation hook.
    Note that this hook is not conditionally defined,
    for binary compatibility of the @c MYSQL_SOCKET interface.
  */
  struct PSI_socket *m_psi;
};

/**
  @def MYSQL_INVALID_SOCKET
  MYSQL_SOCKET initial value.
*/
// MYSQL_SOCKET MYSQL_INVALID_SOCKET= {INVALID_SOCKET, NULL};
#define MYSQL_INVALID_SOCKET mysql_socket_invalid()

/**
  MYSQL_SOCKET helper. Initialize instrumented socket.
  @sa mysql_socket_getfd
  @sa mysql_socket_setfd
*/
static inline MYSQL_SOCKET
mysql_socket_invalid()
{
  MYSQL_SOCKET mysql_socket = {INVALID_SOCKET, NULL};
  return mysql_socket;
}

#endif  // COMPONENTS_SERVICES_MYSQL_SOCKET_BITS_H
