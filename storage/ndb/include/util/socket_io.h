/*
   Copyright (C) 2003-2007 MySQL AB, 2008 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef _SOCKET_IO_H
#define _SOCKET_IO_H

#include <ndb_global.h>

#include <NdbTCP.h>

#include <NdbMutex.h>

#ifdef  __cplusplus
extern "C" {
#endif

  int read_socket(NDB_SOCKET_TYPE, int timeout_ms, char *, int len);

  int readln_socket(NDB_SOCKET_TYPE socket, int timeout_millis, int *time,
                    char * buf, int buflen, NdbMutex *mutex);

  int write_socket(NDB_SOCKET_TYPE, int timeout_ms, int *time,
                   const char[], int len);

  int print_socket(NDB_SOCKET_TYPE, int timeout_ms, int *time,
                   const char *, ...) ATTRIBUTE_FORMAT(printf, 4, 5);
  int println_socket(NDB_SOCKET_TYPE, int timeout_ms, int *time,
                     const char *, ...) ATTRIBUTE_FORMAT(printf, 4, 5);
  int vprint_socket(NDB_SOCKET_TYPE, int timeout_ms, int *time,
                    const char *, va_list);
  int vprintln_socket(NDB_SOCKET_TYPE, int timeout_ms, int *time,
                      const char *, va_list);

#ifdef  __cplusplus
}
#endif

#endif
