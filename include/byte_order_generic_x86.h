/* Copyright (c) 2001, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef BYTE_ORDER_GENERIC_X86_INCLUDED
#define BYTE_ORDER_GENERIC_X86_INCLUDED

/**
  @@file include/byte_order_generic_x86.h
  Optimized functions for the x86 architecture (_WIN32 included).

  x86 handles misaligned reads and writes just fine, so suppress
  UBSAN warnings for these functions.
*/

// IWYU pragma: private, include "my_byteorder.h"

#ifndef MY_BYTEORDER_INCLUDED
#error This file should never be #included directly; use my_byteorder.h.
#endif

#include "my_compiler.h"
#include "my_inttypes.h"

static inline int16  sint2korr(const uchar *A) SUPPRESS_UBSAN;
static inline int16  sint2korr(const uchar *A) { return *((int16*) A); }

static inline int32 sint4korr(const uchar *A) SUPPRESS_UBSAN;
static inline int32 sint4korr(const uchar *A) { return *((int32*) A); }

static inline uint16 uint2korr(const uchar *A) SUPPRESS_UBSAN;
static inline uint16 uint2korr(const uchar *A) { return *((uint16*) A); }

static inline uint32 uint4korr(const uchar *A) SUPPRESS_UBSAN;
static inline uint32 uint4korr(const uchar *A) { return *((uint32*) A); }

static inline ulonglong uint8korr(const uchar *A) SUPPRESS_UBSAN;
static inline ulonglong uint8korr(const uchar *A) { return *((ulonglong*) A);}

static inline longlong  sint8korr(const uchar *A) SUPPRESS_UBSAN;
static inline longlong  sint8korr(const uchar *A) { return *((longlong*) A); }

static inline void int2store(uchar *T, uint16 A) SUPPRESS_UBSAN;
static inline void int2store(uchar *T, uint16 A)
{
  *((uint16*) T)= A;
}

static inline void int4store(uchar *T, uint32 A) SUPPRESS_UBSAN;
static inline void int4store(uchar *T, uint32 A)
{
  *((uint32*) T)= A;
}

static inline void int7store(uchar *T, ulonglong A)
{
  *(T)=  (uchar) (A);
  *(T+1)=(uchar) (A >> 8);
  *(T+2)=(uchar) (A >> 16);
  *(T+3)=(uchar) (A >> 24);
  *(T+4)=(uchar) (A >> 32);
  *(T+5)=(uchar) (A >> 40);
  *(T+6)=(uchar) (A >> 48);
}

static inline void int8store(uchar *T, ulonglong A) SUPPRESS_UBSAN;
static inline void int8store(uchar *T, ulonglong A)
{
  *((ulonglong*) T)= A;
}

#endif /* BYTE_ORDER_GENERIC_X86_INCLUDED */
