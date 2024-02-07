/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef _my_cleanse_h
#define _my_cleanse_h

/**
  Use when you want to "securely" clean up memory

  @param b the data to clean
  @param s the size of the data to clean

  @note Always put first in the file
*/
#if defined(_WIN32)
#define my_cleanse(b, s) SecureZeroMemory(b, s)

#elif defined(__STDC_LIB_EXT1__)
#define __STDC_WANT_LIB_EXT1__
#include <string.h>
#define my_cleanse(b, s) memset_s(b, s, 0, s)

#else
#include <openssl/crypto.h>
#define my_cleanse(b, s) OPENSSL_cleanse(b, s)

#endif /* defined(_WIN32) */

#endif /* _my_cleanse_h */
