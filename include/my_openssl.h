/* Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MY_OPENSSL_INCLUDED
#define MY_OPENSSL_INCLUDED

#include <openssl/ssl.h>

#ifdef  __cplusplus
extern "C" {
#endif

/* Visual Studio requires '__inline' for C code */
#if !defined(__cplusplus) && defined(_WIN32) && !defined(inline)
#define inline __inline
#endif

/*
  HAVE_STATIC_OPENSSL:
  This is OpenSSL version >= 1.1.1 and we are linking statically.
  We must disable all atexit() processing in OpenSSL, otherwise
  dlclose() of plugins might destroy data structures which are needed
  by the application.

  Otherwise: "system" OpenSSL may be any version > 1.0.0
  For 1.0.x : SSL_library_init is a function.
  For 1.1.y : SSL_library_init is a macro: OPENSSL_init_ssl(0, NULL)

  Note that we cannot in general call OPENSSL_cleanup(). Doing so from plugins
  would break the embedding main program. Doing so from client code may
  break e.g. ODBC clients (if the client also uses SSL).
 */
static inline int mysql_OPENSSL_init()
{
#if defined(HAVE_STATIC_OPENSSL)
  return OPENSSL_init_crypto(OPENSSL_INIT_NO_ATEXIT, NULL);
#else
  return SSL_library_init();
#endif
}

#ifdef  __cplusplus
}
#endif

#endif  // MY_OPENSSL_INCLUDED
