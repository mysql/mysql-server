/*
   Copyright (c) 2022, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/* Include this file *after* ndb_socket.h and socket_io.h.
   It will redefine the functions that should be replaced with NdbSocket
   method calls, so that the compiler catches them as errors.

   The error message will be along the lines of "error: expected expression".
*/

#define ndb_socket_close(A)
#define ndb_socket_nonblock(A, B)
#define ndb_recv(A, B, C, D)
#define ndb_send(A, B, C, D)
#define ndb_socket_writev(A, B, C)
#define read_socket(A, B, C)
#define readln_socket(A, B, C, D, E, F)
#define write_socket(A, B, C, D, E)
