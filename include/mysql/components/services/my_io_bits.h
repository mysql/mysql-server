/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef COMPONENTS_SERVICES_MY_IO_BITS_H
#define COMPONENTS_SERVICES_MY_IO_BITS_H

/**
  @file mysql/components/services/my_io_bits.h
  Types to make file and socket I/O compatible.
*/

#ifdef _WIN32
/* Include common headers.*/
# include <io.h>       /* access(), chmod() */
#ifdef WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h> /* SOCKET */
#endif
#endif

#ifndef MYSQL_ABI_CHECK
#if !defined(_WIN32)
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <errno.h>
#include <limits.h>
#include <sys/types.h>  // Needed for mode_t, so IWYU pragma: keep.
#endif

typedef int File;           /* File descriptor */
#ifdef _WIN32
typedef int MY_MODE;
typedef int mode_t;
typedef int socket_len_t;
typedef SOCKET my_socket;
#else
typedef mode_t MY_MODE;
typedef socklen_t socket_len_t;
typedef int     my_socket;      /* File descriptor for sockets */
#endif /* _WIN32 */

#endif /* COMPONENTS_SERVICES_MY_IO_BITS_H */
