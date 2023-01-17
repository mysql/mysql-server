/*
   Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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
#ifndef NDB_UTIL_SSL_SOCKET_TABLE_H
#define NDB_UTIL_SSL_SOCKET_TABLE_H

/* The implementation uses a static-allocated table of size
   NDB_SSL_FIXED_TABLE_SIZE, plus a dynamic unordered map for
   descriptor values larger than NDB_SSL_FIXED_TABLE_SIZE.

   In a data node, dynamic allocation is not allowed, but we have
   full understanding of the code base. NDB_SSL_FIXED_TABLE_SIZE
   must be big enough so that all lookups in the data node use the
   fixed table.

   In an API node, dynamic allocation is okay, but the application-level
   code could require any arbitrary number of file descriptors. In this
   case the overflow table may be used.
*/
#define NDB_SSL_FIXED_TABLE_SIZE 4192

/* Enable or disable the socket table with 0 or 1 here */
#define NDB_USE_SSL_SOCKET_TABLE 1

#if NDB_USE_SSL_SOCKET_TABLE

/* Set SSL for socket */
void socket_table_set_ssl(socket_t, struct ssl_st *);

/* Clear stored SSL for socket. */
void socket_table_clear_ssl(socket_t);

/* Get SSL associated with socket, or nullptr if none.
   If an SSL is found when expected is false, this will abort in debug builds.
*/
struct ssl_st * socket_table_get_ssl(socket_t, bool expected);

#else

/* Socket table is disabled */
#define socket_table_set_ssl(A, B)
#define socket_table_clear_ssl(A)
#define socket_table_get_ssl(A, ...) nullptr

#endif   /* NDB_USE_SSL_SOCKET_TABLE */

#endif   /* NDB_UTIL_SSL_SOCKET_TABLE_H */
