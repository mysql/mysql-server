/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_TYPES_H
#define GCS_TYPES_H

#include <assert.h>
#include <stdlib.h>

#define compile_time_assert(X)                                          \
  do                                                                        \
  {                                                                         \
    typedef char compile_time_assert[(X) ? 1 : -1] __attribute__((unused)); \
  } while(0)

/*
  Shortcut names for common types.
*/

typedef unsigned char uchar;
typedef unsigned long long int ulonglong;
// Sizeof of long-long is *assumed* to be 8
typedef ulonglong uint64;
typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int   uint32;

#endif
