/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  The WolfSSL library does #define certain global symbol that pollute the
  C namespace.
  This file is to be included before the wolf headers to fix this.
*/
#ifndef WOLFSSL_FIX_NAMESPACE_POLLUTION_PRE_H
#define WOLFSSL_FIX_NAMESPACE_POLLUTION_PRE_H 1

#ifdef HAVE_WOLFSSL

#if defined(_WIN32)
/*
  Defined in wolfssl's io.h.
  Conflict with the same constants defined in mysql
  Keep in sync with my_io.h
*/
#if defined(SOCKET_EWOULDBLOCK)
#undef SOCKET_EWOULDBLOCK
#endif
#if defined(SOCKET_EAGAIN)
#undef SOCKET_EAGAIN
#endif
#if defined(SOCKET_ECONNRESET)
#define SOCKET_ECONNRESET WSAECONNRESET
#endif
#if defined(SOCKET_EINTR)
#define SOCKET_EINTR WSAEINTR
#endif

#endif /* _WIN32 */
#endif /* HAVE_WOLFSSL */

#endif /* WOLFSSL_FIX_NAMESPACE_POLLUTION_PRE_H */
