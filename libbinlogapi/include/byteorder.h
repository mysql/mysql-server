/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef BYTEORDER_H
#define BYTEORDER_H

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include "config.h"

/*
 * Define types uint_t typedefs if stdint is not present
 */
#ifndef HAVE_STDINT_H
  #if SIZEOF_INT == 4
    typedef int int32_t;
    typedef unsigned int uint32_t;
  #elif SIZEOF_LONG == 4
    typedef long int32_t;
    typedef unsigned long uint32_t;
  #else
    #error Neither int nor long is of 4 bytes width
  #endif
  typedef unsigned short int uint16_t;
#else
  #include <stdint.h>
#endif //end ifndef HAVE_STDINT_H

/*
  Methods for reading and storing in machine independent
  format (low byte first).
*/

/*
 * Checking le16toh is required because the machine may have the header
 * but the functions might not be defined if the version of glibc < 2.9
 */
#ifdef HAVE_ENDIAN_CONVERSION_MACROS
  #include <endian.h>
#endif
#if !defined(le16toh)
uint16_t inline le16toh(uint16_t x)
{
  #ifndef IS_BIG_ENDIAN
    return x;
  #else
    return ((x >> 8) | (x << 8));
  #endif
}
#endif

#if !defined(le32toh)
uint32_t inline le32toh(uint32_t x)
{
  #ifndef IS_BIG_ENDIAN
    return x;
  #else
    return (((x >> 24) & 0xff) |
            ((x <<  8) & 0xff0000) |
            ((x >>  8) & 0xff00) |
            ((x << 24) & 0xff000000));
  #endif
}
#endif

#if !defined(be32toh)
uint32_t inline be32toh(uint32_t x)
{
  #ifndef IS_BIG_ENDIAN
     return (((x >> 24) & 0xff) |
             ((x <<  8) & 0xff0000) |
             ((x >>  8) & 0xff00) |
             ((x << 24) & 0xff000000));
  #else
     return x;
  #endif
}
#endif
#define do_compile_time_assert(X)                                              \
  do                                                                        \
  {                                                                         \
    typedef char do_compile_time_assert[(X) ? 1 : -1] __attribute__((unused)); \
  } while(0)
#endif // BYTEORDER_H
