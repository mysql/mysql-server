/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file byteorder.h

  @brief The file contains functions to convert the byte encoding of integer
  values to and from little-endian and big-endian byte order.
*/

#ifndef BYTEORDER_INCLUDED
#define BYTEORDER_INCLUDED

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include "binlog_config.h"
#include <stdint.h>
#ifndef STANDALONE_BINLOG
#define HAVE_MYSYS 1
#endif

/*
  Methods for reading and storing in machine independent
  format (low byte first).
*/

/*
  Checking le16toh is required because the machine may have the header
  but the functions might not be defined if the version of glibc < 2.9
*/
#ifdef HAVE_ENDIAN_CONVERSION_MACROS
  #include <endian.h>
#endif

#if !defined(le16toh)
/**
  Converting a 16 bit integer from little-endian byte order to host byteorder

  @param x  16-bit integer in little endian byte order
  @return  16-bit integer in host byte order
*/
uint16_t inline le16toh(uint16_t x)
{
  #if !(IS_BIG_ENDIAN)
    return x;
  #else
    return ((x >> 8) | (x << 8));
  #endif
}
#endif

#if !defined(le32toh)
/**
  Converting a 32 bit integer from little-endian byte order to host byteorder

  @param x  32-bit integer in little endian byte order
  @return  32-bit integer in host byte order
*/
uint32_t inline le32toh(uint32_t x)
{
  #if !(IS_BIG_ENDIAN)
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
/**
  Converting a 32 bit integer from big-endian byte order to host byteorder

  @param x  32-bit integer in big endian byte order
  @return  32-bit integer in host byte order
*/
uint32_t inline be32toh(uint32_t x)
{
  #if !(IS_BIG_ENDIAN)
     return (((x >> 24) & 0xff) |
             ((x <<  8) & 0xff0000) |
             ((x >>  8) & 0xff00) |
             ((x << 24) & 0xff000000));
  #else
     return x;
  #endif
}
#endif

#if !defined(le64toh)
/**
  Converting a 64 bit integer from little-endian byte order to host byteorder

  @param x  64-bit integer in little endian byte order
  @return  64-bit integer in host byte order
*/
uint64_t inline le64toh(uint64_t x)
{
  #if !(IS_BIG_ENDIAN)
    return x;
  #else
    x = ((x << 8) & 0xff00ff00ff00ff00ULL) |
        ((x >> 8) & 0x00ff00ff00ff00ffULL);
    x = ((x << 16) & 0xffff0000ffff0000ULL) |
        ((x >> 16) & 0x0000ffff0000ffffULL);
    return (x << 32) | (x >> 32);
  #endif
}
#endif

#if !defined(float4store)
float inline float4store(float  *A)
{
  #if !(IS_BIG_ENDIAN)
   return *A;
  #else
    float ret_val= 0;
    char *in_float= (char *) A;
    char *return_float= (char*) &ret_val;
    *(return_float)= in_float[3];
    *((return_float)+1)= in_float[2];
    *((return_float)+2)= in_float[1];
    *((return_float)+3)= in_float[0];
    return ret_val;
  #endif
}
#endif

#if !defined(doublestore)
inline double doublestore(double *V)
{
  #if !(IS_BIG_ENDIAN)
   return *V;
  #else
   double ret_val;
   char * in_double= (char *) V;
   char * return_double= (char *) &ret_val;
    *((return_double)+0)= in_double[4];
    *((return_double)+1)= in_double[5];
    *((return_double)+2)= in_double[6];
    *((return_double)+3)= in_double[7];
    *((return_double)+4)= in_double[0];
    *((return_double)+5)= in_double[1];
    *((return_double)+6)= in_double[2];
    *((return_double)+7)= in_double[3];
    return ret_val;
  #endif
}
#endif
#if defined(_WIN32)
#define __attribute__(x)
#endif
#define do_compile_time_assert(X)                                              \
  do                                                                        \
  {                                                                         \
    typedef char do_compile_time_assert[(X) ? 1 : -1] __attribute__((unused)); \
  } while(0)
#endif // BYTEORDER_INCLUDED
