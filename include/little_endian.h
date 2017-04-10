#ifndef LITTLE_ENDIAN_INCLUDED
#define LITTLE_ENDIAN_INCLUDED
/* Copyright (c) 2012, 2016, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file include/little_endian.h
  Data in little-endian format.
*/

#include <string.h>

#include "my_byteorder.h"
#include "my_inttypes.h"

static inline void float4get  (float  *V, const uchar *M) 
{
  memcpy(V, (M), sizeof(float));
}

static inline void float4store(uchar  *V, float  M)
{
  memcpy(V, (&M), sizeof(float));
}

static inline void float8get  (double *V, const uchar *M)
{
  memcpy(V,  M, sizeof(double));
}

static inline void float8store(uchar  *V, double M)
{
  memcpy(V, &M, sizeof(double));
}

static inline void floatget   (float  *V, const uchar *M) { float4get(V, M); }
static inline void floatstore (uchar  *V, float M)        { float4store(V, M); }

static inline void doublestore(uchar  *T, double V)       { memcpy(T, &V, sizeof(double)); }
static inline void doubleget  (double *V, const uchar *M) { memcpy(V, M, sizeof(double)); }

static inline void ushortget(uint16 *V, const uchar *pM) { *V= uint2korr(pM); }
static inline void shortget (int16  *V, const uchar *pM) { *V= sint2korr(pM); }
static inline void longget  (int32  *V, const uchar *pM) { *V= sint4korr(pM); }
static inline void ulongget (uint32 *V, const uchar *pM) { *V= uint4korr(pM); }
static inline void shortstore(uchar *T, int16 V) { int2store(T, V); }
static inline void longstore (uchar *T, int32 V) { int4store(T, V); }

static inline void longlongget(longlong *V, const uchar *M)
{
  memcpy(V, (M), sizeof(ulonglong));
}
static inline void longlongstore(uchar *T, longlong V)
{
  memcpy((T), &V, sizeof(ulonglong));
}

#endif /* LITTLE_ENDIAN_INCLUDED */
