/*
   Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

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
                    const char *, va_list) ATTRIBUTE_FORMAT(printf, 4, 0);
  int vprintln_socket(NDB_SOCKET_TYPE, int timeout_ms, int *time,
                      const char *, va_list) ATTRIBUTE_FORMAT(printf, 4, 0);

#ifdef  __cplusplus
}
#endif

#endif
