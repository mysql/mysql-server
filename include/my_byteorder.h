#ifndef MY_BYTEORDER_INCLUDED
#define MY_BYTEORDER_INCLUDED

/* Copyright (c) 2001, 2014, Oracle and/or its affiliates. All rights reserved.

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


/*
  Functions for reading and storing in machine independent
  format (low byte first). There are 'korr' (assume 'corrector') variants
  for integer types, but 'get' (assume 'getter') for floating point types.
*/
#if defined(__i386__) || defined(_WIN32) || defined(__x86_64__)
#include "byte_order_generic_x86.h"
#else
#include "byte_order_generic.h"
#endif

static inline int32 sint3korr(const uchar *A)
{
  return
    ((int32) (((A[2]) & 128) ?
              (((uint32) 255L << 24) |
               (((uint32) A[2]) << 16) |
               (((uint32) A[1]) << 8) |
               ((uint32) A[0])) :
              (((uint32) A[2]) << 16) |
              (((uint32) A[1]) << 8) |
              ((uint32) A[0])))
    ;
}

static inline uint32 uint3korr(const uchar *A)
{
  return
    (uint32) (((uint32) (A[0])) +
              (((uint32) (A[1])) << 8) +
              (((uint32) (A[2])) << 16))
    ;
}

static inline ulonglong uint5korr(const uchar *A)
{
  return
    ((ulonglong)(((uint32) (A[0])) +
                 (((uint32) (A[1])) << 8) +
                 (((uint32) (A[2])) << 16) +
                 (((uint32) (A[3])) << 24)) +
     (((ulonglong) (A[4])) << 32))
    ;
}

static inline ulonglong uint6korr(const uchar *A)
{
  return
    ((ulonglong)(((uint32) (A[0]))          +
                 (((uint32) (A[1])) << 8)   +
                 (((uint32) (A[2])) << 16)  +
                 (((uint32) (A[3])) << 24)) +
     (((ulonglong) (A[4])) << 32) +
     (((ulonglong) (A[5])) << 40))
    ;
}

static inline void int3store(uchar *T, uint A)
{
  *(T)=   (uchar) (A);
  *(T+1)= (uchar) (A >> 8);
  *(T+2)= (uchar) (A >> 16);
}

static inline void int5store(uchar *T, ulonglong A)
{
  *(T)=   (uchar) (A);
  *(T+1)= (uchar) (A >> 8);
  *(T+2)= (uchar) (A >> 16);
  *(T+3)= (uchar) (A >> 24);
  *(T+4)= (uchar) (A >> 32);
}

static inline void int6store(uchar *T, ulonglong A)
{
  *(T)=   (uchar) (A);
  *(T+1)= (uchar) (A >> 8);
  *(T+2)= (uchar) (A >> 16);
  *(T+3)= (uchar) (A >> 24);
  *(T+4)= (uchar) (A >> 32);
  *(T+5)= (uchar) (A >> 40);
}

#ifdef __cplusplus

static inline int16 sint2korr(const char *pT)
{
  return sint2korr(static_cast<const uchar*>(static_cast<const void*>(pT)));
}

static inline uint16    uint2korr(const char *pT)
{
  return uint2korr(static_cast<const uchar*>(static_cast<const void*>(pT)));
}

static inline uint32    uint3korr(const char *pT)
{
  return uint3korr(static_cast<const uchar*>(static_cast<const void*>(pT)));
}

static inline int32     sint3korr(const char *pT)
{
  return sint3korr(static_cast<const uchar*>(static_cast<const void*>(pT)));
}

static inline uint32    uint4korr(const char *pT)
{
  return uint4korr(static_cast<const uchar*>(static_cast<const void*>(pT)));
}

static inline int32     sint4korr(const char *pT)
{
  return sint4korr(static_cast<const uchar*>(static_cast<const void*>(pT)));
}

static inline ulonglong uint6korr(const char *pT)
{
  return uint6korr(static_cast<const uchar*>(static_cast<const void*>(pT)));
}

static inline ulonglong uint8korr(const char *pT)
{
  return uint8korr(static_cast<const uchar*>(static_cast<const void*>(pT)));
}

static inline longlong  sint8korr(const char *pT)
{
  return sint8korr(static_cast<const uchar*>(static_cast<const void*>(pT)));
}


static inline void int2store(char *pT, uint16 A)
{
  int2store(static_cast<uchar*>(static_cast<void*>(pT)), A);
}

static inline void int3store(char *pT, uint A)
{
  int3store(static_cast<uchar*>(static_cast<void*>(pT)), A);
}

static inline void int4store(char *pT, uint32 A)
{
  int4store(static_cast<uchar*>(static_cast<void*>(pT)), A);
}

static inline void int5store(char *pT, ulonglong A)
{
  int5store(static_cast<uchar*>(static_cast<void*>(pT)), A);
}

static inline void int6store(char *pT, ulonglong A)
{
  int6store(static_cast<uchar*>(static_cast<void*>(pT)), A);
}

static inline void int8store(char *pT, ulonglong A)
{
  int8store(static_cast<uchar*>(static_cast<void*>(pT)), A);
}

#endif  /* __cplusplus */

/*
  Functions for reading and storing in machine format from/to
  short/long to/from some place in memory V should be a variable
  and M a pointer to byte.
*/
#ifdef WORDS_BIGENDIAN
#include "big_endian.h"
#else
#include "little_endian.h"
#endif

#ifdef __cplusplus

static inline void float4store(char *V, float M)
{
  float4store(static_cast<uchar*>(static_cast<void*>(V)), M);
}

static inline void float8get(double *V, const char *M)
{
  float8get(V, static_cast<const uchar*>(static_cast<const void*>(M)));
}

static inline void float8store(char *V, double M)
{
  float8store(static_cast<uchar*>(static_cast<void*>(V)), M);
}

#endif /* __cplusplus */

#endif /* MY_BYTEORDER_INCLUDED */
