/* Copyright (c) 2007, 2026, Oracle and/or its affiliates.

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

#ifndef _lf_types_h
#define _lf_types_h

#include "my_inttypes.h"

#define LF_HASH_UNIQUE 1
#define MY_LF_ERRPTR ((void *)(intptr)1)

/**
  Callback for extracting key and key length from user data in a LF_HASH.

  @param      arg    Pointer to user data.
  @param[out] length Store key length here.
  @return            Pointer to key to be hashed.

  @note Was my_hash_get_key, with lots of C-style casting when calling
        my_hash_init. Renamed to force build error (since signature changed)
        in case someone keeps following that coding style.
 */
typedef const uchar *(*hash_get_key_function)(const uchar *arg, size_t *length);

/*
  memory allocator, lf_alloc-pin.c
*/
typedef void lf_allocator_func(uchar *);

typedef int lf_hash_match_func(const uchar *el, void *arg);

typedef void lf_hash_init_func(uchar *dst, const uchar *src);

#endif
