/* Copyright (C) 2006 MySQL AB & Ramil Kalimullin & MySQL Finland AB
   & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "maria_def.h"

#ifdef HAVE_RTREE_KEYS

#include "ma_rt_index.h"
#include "ma_rt_mbr.h"

#define INTERSECT_CMP(amin, amax, bmin, bmax) ((amin >  bmax) || (bmin >  amax))
#define CONTAIN_CMP(amin, amax, bmin, bmax) ((bmin > amin)  || (bmax <  amax))
#define WITHIN_CMP(amin, amax, bmin, bmax) ((amin > bmin)  || (amax <  bmax))
#define DISJOINT_CMP(amin, amax, bmin, bmax) ((amin <= bmax) && (bmin <= amax))
#define EQUAL_CMP(amin, amax, bmin, bmax) ((amin != bmin) || (amax != bmax))

#define FCMP(A, B) ((int)(A) - (int)(B))
#define p_inc(A, B, X)  {A += X; B += X;}

#define RT_CMP(nextflag) \
  if (nextflag & MBR_INTERSECT) \
  { \
    if (INTERSECT_CMP(amin, amax, bmin, bmax)) \
      return 1; \
  } \
  else if (nextflag & MBR_CONTAIN) \
  { \
    if (CONTAIN_CMP(amin, amax, bmin, bmax)) \
      return 1; \
  } \
  else if (nextflag & MBR_WITHIN) \
  { \
    if (WITHIN_CMP(amin, amax, bmin, bmax)) \
      return 1; \
  } \
  else if (nextflag & MBR_EQUAL)  \
  { \
    if (EQUAL_CMP(amin, amax, bmin, bmax)) \
      return 1; \
  } \
  else if (nextflag & MBR_DISJOINT) \
  { \
    if (DISJOINT_CMP(amin, amax, bmin, bmax)) \
      return 1; \
  }\
  else /* if unknown comparison operator */ \
  { \
    DBUG_ASSERT(0); \
  }

#define RT_CMP_KORR(type, korr_func, len, nextflag) \
{ \
  type amin, amax, bmin, bmax; \
  amin= korr_func(a); \
  bmin= korr_func(b); \
  amax= korr_func(a+len); \
  bmax= korr_func(b+len); \
  RT_CMP(nextflag); \
}

#define RT_CMP_GET(type, get_func, len, nextflag) \
{ \
  type amin, amax, bmin, bmax; \
  get_func(amin, a); \
  get_func(bmin, b); \
  get_func(amax, a+len); \
  get_func(bmax, b+len); \
  RT_CMP(nextflag); \
}

/*
 Compares two keys a and b depending on nextflag
 nextflag can contain these flags:
   MBR_INTERSECT(a,b)  a overlaps b
   MBR_CONTAIN(a,b)    a contains b
   MBR_DISJOINT(a,b)   a disjoint b
   MBR_WITHIN(a,b)     a within   b
   MBR_EQUAL(a,b)      All coordinates of MBRs are equal
   MBR_DATA(a,b)       Data reference is the same
 Returns 0 on success.
*/

int maria_rtree_key_cmp(HA_KEYSEG *keyseg, const uchar *b, const uchar *a,
                        uint key_length, uint32 nextflag)
{
  for (; (int) key_length > 0; keyseg += 2 )
  {
    uint32 keyseg_length;
    switch ((enum ha_base_keytype) keyseg->type) {
    case HA_KEYTYPE_INT8:
      RT_CMP_KORR(int8, mi_sint1korr, 1, nextflag);
      break;
    case HA_KEYTYPE_BINARY:
      RT_CMP_KORR(uint8, mi_uint1korr, 1, nextflag);
      break;
    case HA_KEYTYPE_SHORT_INT:
      RT_CMP_KORR(int16, mi_sint2korr, 2, nextflag);
      break;
    case HA_KEYTYPE_USHORT_INT:
      RT_CMP_KORR(uint16, mi_uint2korr, 2, nextflag);
      break;
    case HA_KEYTYPE_INT24:
      RT_CMP_KORR(int32, mi_sint3korr, 3, nextflag);
      break;
    case HA_KEYTYPE_UINT24:
      RT_CMP_KORR(uint32, mi_uint3korr, 3, nextflag);
      break;
    case HA_KEYTYPE_LONG_INT:
      RT_CMP_KORR(int32, mi_sint4korr, 4, nextflag);
      break;
    case HA_KEYTYPE_ULONG_INT:
      RT_CMP_KORR(uint32, mi_uint4korr, 4, nextflag);
      break;
#ifdef HAVE_LONG_LONG
    case HA_KEYTYPE_LONGLONG:
      RT_CMP_KORR(longlong, mi_sint8korr, 8, nextflag)
      break;
    case HA_KEYTYPE_ULONGLONG:
      RT_CMP_KORR(ulonglong, mi_uint8korr, 8, nextflag)
      break;
#endif
    case HA_KEYTYPE_FLOAT:
      /* The following should be safe, even if we compare doubles */
      RT_CMP_GET(float, mi_float4get, 4, nextflag);
      break;
    case HA_KEYTYPE_DOUBLE:
      RT_CMP_GET(double, mi_float8get, 8, nextflag);
      break;
    case HA_KEYTYPE_END:
      goto end;
    default:
      return 1;
    }
    keyseg_length= keyseg->length * 2;
    key_length-= keyseg_length;
    a+= keyseg_length;
    b+= keyseg_length;
  }

end:
  if (nextflag & MBR_DATA)
  {
    const uchar *end= a + keyseg->length;
    do
    {
      if (*a++ != *b++)
        return FCMP(a[-1], b[-1]);
    } while (a != end);
  }
  return 0;
}

#define RT_VOL_KORR(type, korr_func, len, cast) \
{ \
  type amin, amax; \
  amin= korr_func(a); \
  amax= korr_func(a+len); \
  res *= (cast(amax) - cast(amin)); \
}

#define RT_VOL_GET(type, get_func, len, cast) \
{ \
  type amin, amax; \
  get_func(amin, a); \
  get_func(amax, a+len); \
  res *= (cast(amax) - cast(amin)); \
}

/*
 Calculates rectangle volume
*/
double maria_rtree_rect_volume(HA_KEYSEG *keyseg, uchar *a, uint key_length)
{
  double res= 1;
  for (; (int)key_length > 0; keyseg += 2)
  {
    uint32 keyseg_length;
    switch ((enum ha_base_keytype) keyseg->type) {
    case HA_KEYTYPE_INT8:
      RT_VOL_KORR(int8, mi_sint1korr, 1, (double));
      break;
    case HA_KEYTYPE_BINARY:
      RT_VOL_KORR(uint8, mi_uint1korr, 1, (double));
      break;
    case HA_KEYTYPE_SHORT_INT:
      RT_VOL_KORR(int16, mi_sint2korr, 2, (double));
      break;
    case HA_KEYTYPE_USHORT_INT:
      RT_VOL_KORR(uint16, mi_uint2korr, 2, (double));
      break;
    case HA_KEYTYPE_INT24:
      RT_VOL_KORR(int32, mi_sint3korr, 3, (double));
      break;
    case HA_KEYTYPE_UINT24:
      RT_VOL_KORR(uint32, mi_uint3korr, 3, (double));
      break;
    case HA_KEYTYPE_LONG_INT:
      RT_VOL_KORR(int32, mi_sint4korr, 4, (double));
      break;
    case HA_KEYTYPE_ULONG_INT:
      RT_VOL_KORR(uint32, mi_uint4korr, 4, (double));
      break;
#ifdef HAVE_LONG_LONG
    case HA_KEYTYPE_LONGLONG:
      RT_VOL_KORR(longlong, mi_sint8korr, 8, (double));
      break;
    case HA_KEYTYPE_ULONGLONG:
      RT_VOL_KORR(longlong, mi_sint8korr, 8, ulonglong2double);
      break;
#endif
    case HA_KEYTYPE_FLOAT:
      RT_VOL_GET(float, mi_float4get, 4, (double));
      break;
    case HA_KEYTYPE_DOUBLE:
      RT_VOL_GET(double, mi_float8get, 8, (double));
      break;
    case HA_KEYTYPE_END:
      key_length= 0;
      break;
    default:
      return -1;
    }
    keyseg_length= keyseg->length * 2;
    key_length-= keyseg_length;
    a+= keyseg_length;
  }
  return res;
}

#define RT_D_MBR_KORR(type, korr_func, len, cast) \
{ \
  type amin, amax; \
  amin= korr_func(a); \
  amax= korr_func(a+len); \
  *res++= cast(amin); \
  *res++= cast(amax); \
}

#define RT_D_MBR_GET(type, get_func, len, cast) \
{ \
  type amin, amax; \
  get_func(amin, a); \
  get_func(amax, a+len); \
  *res++= cast(amin); \
  *res++= cast(amax); \
}


/*
  Creates an MBR as an array of doubles.
  Fills *res.
*/

int maria_rtree_d_mbr(const HA_KEYSEG *keyseg, const uchar *a,
                      uint key_length, double *res)
{
  for (; (int)key_length > 0; keyseg += 2)
  {
    uint32 keyseg_length;
    switch ((enum ha_base_keytype) keyseg->type) {
    case HA_KEYTYPE_INT8:
      RT_D_MBR_KORR(int8, mi_sint1korr, 1, (double));
      break;
    case HA_KEYTYPE_BINARY:
      RT_D_MBR_KORR(uint8, mi_uint1korr, 1, (double));
      break;
    case HA_KEYTYPE_SHORT_INT:
      RT_D_MBR_KORR(int16, mi_sint2korr, 2, (double));
      break;
    case HA_KEYTYPE_USHORT_INT:
      RT_D_MBR_KORR(uint16, mi_uint2korr, 2, (double));
      break;
    case HA_KEYTYPE_INT24:
      RT_D_MBR_KORR(int32, mi_sint3korr, 3, (double));
      break;
    case HA_KEYTYPE_UINT24:
      RT_D_MBR_KORR(uint32, mi_uint3korr, 3, (double));
      break;
    case HA_KEYTYPE_LONG_INT:
      RT_D_MBR_KORR(int32, mi_sint4korr, 4, (double));
      break;
    case HA_KEYTYPE_ULONG_INT:
      RT_D_MBR_KORR(uint32, mi_uint4korr, 4, (double));
      break;
#ifdef HAVE_LONG_LONG
    case HA_KEYTYPE_LONGLONG:
      RT_D_MBR_KORR(longlong, mi_sint8korr, 8, (double));
      break;
    case HA_KEYTYPE_ULONGLONG:
      RT_D_MBR_KORR(longlong, mi_sint8korr, 8, ulonglong2double);
      break;
#endif
    case HA_KEYTYPE_FLOAT:
      RT_D_MBR_GET(float, mi_float4get, 4, (double));
      break;
    case HA_KEYTYPE_DOUBLE:
      RT_D_MBR_GET(double, mi_float8get, 8, (double));
      break;
    case HA_KEYTYPE_END:
      key_length= 0;
      break;
    default:
      return 1;
    }
    keyseg_length= keyseg->length * 2;
    key_length-= keyseg_length;
    a+= keyseg_length;
  }
  return 0;
}

#define RT_COMB_KORR(type, korr_func, store_func, len) \
{ \
  type amin, amax, bmin, bmax; \
  amin= korr_func(a); \
  bmin= korr_func(b); \
  amax= korr_func(a+len); \
  bmax= korr_func(b+len); \
  amin= min(amin, bmin); \
  amax= max(amax, bmax); \
  store_func(c, amin); \
  store_func(c+len, amax); \
}

#define RT_COMB_GET(type, get_func, store_func, len) \
{ \
  type amin, amax, bmin, bmax; \
  get_func(amin, a); \
  get_func(bmin, b); \
  get_func(amax, a+len); \
  get_func(bmax, b+len); \
  amin= min(amin, bmin); \
  amax= max(amax, bmax); \
  store_func(c, amin); \
  store_func(c+len, amax); \
}

/*
  Creates common minimal bounding rectungle
  for two input rectagnles a and b
  Result is written to c
*/

int maria_rtree_combine_rect(const HA_KEYSEG *keyseg, const uchar* a,
                             const uchar* b, uchar* c,
                             uint key_length)
{
  for ( ; (int) key_length > 0 ; keyseg += 2)
  {
    uint32 keyseg_length;
    switch ((enum ha_base_keytype) keyseg->type) {
    case HA_KEYTYPE_INT8:
      RT_COMB_KORR(int8, mi_sint1korr, mi_int1store, 1);
      break;
    case HA_KEYTYPE_BINARY:
      RT_COMB_KORR(uint8, mi_uint1korr, mi_int1store, 1);
      break;
    case HA_KEYTYPE_SHORT_INT:
      RT_COMB_KORR(int16, mi_sint2korr, mi_int2store, 2);
      break;
    case HA_KEYTYPE_USHORT_INT:
      RT_COMB_KORR(uint16, mi_uint2korr, mi_int2store, 2);
      break;
    case HA_KEYTYPE_INT24:
      RT_COMB_KORR(int32, mi_sint3korr, mi_int3store, 3);
      break;
    case HA_KEYTYPE_UINT24:
      RT_COMB_KORR(uint32, mi_uint3korr, mi_int3store, 3);
      break;
    case HA_KEYTYPE_LONG_INT:
      RT_COMB_KORR(int32, mi_sint4korr, mi_int4store, 4);
      break;
    case HA_KEYTYPE_ULONG_INT:
      RT_COMB_KORR(uint32, mi_uint4korr, mi_int4store, 4);
      break;
#ifdef HAVE_LONG_LONG
    case HA_KEYTYPE_LONGLONG:
      RT_COMB_KORR(longlong, mi_sint8korr, mi_int8store, 8);
      break;
    case HA_KEYTYPE_ULONGLONG:
      RT_COMB_KORR(ulonglong, mi_uint8korr, mi_int8store, 8);
      break;
#endif
    case HA_KEYTYPE_FLOAT:
      RT_COMB_GET(float, mi_float4get, mi_float4store, 4);
      break;
    case HA_KEYTYPE_DOUBLE:
      RT_COMB_GET(double, mi_float8get, mi_float8store, 8);
      break;
    case HA_KEYTYPE_END:
      return 0;
    default:
      return 1;
    }
    keyseg_length= keyseg->length * 2;
    key_length-= keyseg_length;
    a+= keyseg_length;
    b+= keyseg_length;
    c+= keyseg_length;
  }
  return 0;
}


#define RT_OVL_AREA_KORR(type, korr_func, len) \
{ \
  type amin, amax, bmin, bmax; \
  amin= korr_func(a); \
  bmin= korr_func(b); \
  amax= korr_func(a+len); \
  bmax= korr_func(b+len); \
  amin= max(amin, bmin); \
  amax= min(amax, bmax); \
  if (amin >= amax) \
    return 0; \
  res *= amax - amin; \
}

#define RT_OVL_AREA_GET(type, get_func, len) \
{ \
  type amin, amax, bmin, bmax; \
  get_func(amin, a); \
  get_func(bmin, b); \
  get_func(amax, a+len); \
  get_func(bmax, b+len); \
  amin= max(amin, bmin); \
  amax= min(amax, bmax); \
  if (amin >= amax)  \
    return 0; \
  res *= amax - amin; \
}

/*
Calculates overlapping area of two MBRs a & b
*/
double maria_rtree_overlapping_area(HA_KEYSEG *keyseg, uchar* a, uchar* b,
                             uint key_length)
{
  double res= 1;
  for (; (int) key_length > 0 ; keyseg += 2)
  {
    uint32 keyseg_length;
    switch ((enum ha_base_keytype) keyseg->type) {
    case HA_KEYTYPE_INT8:
      RT_OVL_AREA_KORR(int8, mi_sint1korr, 1);
      break;
    case HA_KEYTYPE_BINARY:
      RT_OVL_AREA_KORR(uint8, mi_uint1korr, 1);
      break;
    case HA_KEYTYPE_SHORT_INT:
      RT_OVL_AREA_KORR(int16, mi_sint2korr, 2);
      break;
    case HA_KEYTYPE_USHORT_INT:
      RT_OVL_AREA_KORR(uint16, mi_uint2korr, 2);
      break;
    case HA_KEYTYPE_INT24:
      RT_OVL_AREA_KORR(int32, mi_sint3korr, 3);
      break;
    case HA_KEYTYPE_UINT24:
      RT_OVL_AREA_KORR(uint32, mi_uint3korr, 3);
      break;
    case HA_KEYTYPE_LONG_INT:
      RT_OVL_AREA_KORR(int32, mi_sint4korr, 4);
      break;
    case HA_KEYTYPE_ULONG_INT:
      RT_OVL_AREA_KORR(uint32, mi_uint4korr, 4);
      break;
#ifdef HAVE_LONG_LONG
    case HA_KEYTYPE_LONGLONG:
      RT_OVL_AREA_KORR(longlong, mi_sint8korr, 8);
      break;
    case HA_KEYTYPE_ULONGLONG:
      RT_OVL_AREA_KORR(longlong, mi_sint8korr, 8);
      break;
#endif
    case HA_KEYTYPE_FLOAT:
      RT_OVL_AREA_GET(float, mi_float4get, 4);
      break;
    case HA_KEYTYPE_DOUBLE:
      RT_OVL_AREA_GET(double, mi_float8get, 8);
      break;
    case HA_KEYTYPE_END:
      return res;
    default:
      return -1;
    }
    keyseg_length= keyseg->length * 2;
    key_length-= keyseg_length;
    a+= keyseg_length;
    b+= keyseg_length;
  }
  return res;
}

#define RT_AREA_INC_KORR(type, korr_func, len) \
{ \
   type amin, amax, bmin, bmax; \
   amin= korr_func(a); \
   bmin= korr_func(b); \
   amax= korr_func(a+len); \
   bmax= korr_func(b+len); \
   a_area *= (((double)amax) - ((double)amin)); \
   loc_ab_area *= ((double)max(amax, bmax) - (double)min(amin, bmin)); \
}

#define RT_AREA_INC_GET(type, get_func, len)\
{\
   type amin, amax, bmin, bmax; \
   get_func(amin, a); \
   get_func(bmin, b); \
   get_func(amax, a+len); \
   get_func(bmax, b+len); \
   a_area *= (((double)amax) - ((double)amin)); \
   loc_ab_area *= ((double)max(amax, bmax) - (double)min(amin, bmin)); \
}

/*
  Calculates MBR_AREA(a+b) - MBR_AREA(a)
  Fills *ab_area.
  Note: when 'a' and 'b' objects are far from each other,
  the area increase can be really big, so this function
  can return 'inf' as a result.
*/

double maria_rtree_area_increase(const HA_KEYSEG *keyseg, const uchar *a,
                                 const uchar *b,
                                 uint key_length, double *ab_area)
{
  double a_area= 1.0;
  double loc_ab_area= 1.0;

  *ab_area= 1.0;
  for (; (int)key_length > 0; keyseg += 2)
  {
    uint32 keyseg_length;

    if (keyseg->null_bit)                       /* Handle NULL part */
      return -1;

    switch ((enum ha_base_keytype) keyseg->type) {
    case HA_KEYTYPE_INT8:
      RT_AREA_INC_KORR(int8, mi_sint1korr, 1);
      break;
    case HA_KEYTYPE_BINARY:
      RT_AREA_INC_KORR(uint8, mi_uint1korr, 1);
      break;
    case HA_KEYTYPE_SHORT_INT:
      RT_AREA_INC_KORR(int16, mi_sint2korr, 2);
      break;
    case HA_KEYTYPE_USHORT_INT:
      RT_AREA_INC_KORR(uint16, mi_uint2korr, 2);
      break;
    case HA_KEYTYPE_INT24:
      RT_AREA_INC_KORR(int32, mi_sint3korr, 3);
      break;
    case HA_KEYTYPE_UINT24:
      RT_AREA_INC_KORR(int32, mi_uint3korr, 3);
      break;
    case HA_KEYTYPE_LONG_INT:
      RT_AREA_INC_KORR(int32, mi_sint4korr, 4);
      break;
    case HA_KEYTYPE_ULONG_INT:
      RT_AREA_INC_KORR(uint32, mi_uint4korr, 4);
      break;
#ifdef HAVE_LONG_LONG
    case HA_KEYTYPE_LONGLONG:
      RT_AREA_INC_KORR(longlong, mi_sint8korr, 8);
      break;
    case HA_KEYTYPE_ULONGLONG:
      RT_AREA_INC_KORR(longlong, mi_sint8korr, 8);
      break;
#endif
    case HA_KEYTYPE_FLOAT:
      RT_AREA_INC_GET(float, mi_float4get, 4);
      break;
    case HA_KEYTYPE_DOUBLE:
      RT_AREA_INC_GET(double, mi_float8get, 8);
      break;
    case HA_KEYTYPE_END:
      goto safe_end;
    default:
      return -1;
    }
    keyseg_length= keyseg->length * 2;
    key_length-= keyseg_length;
    a+= keyseg_length;
    b+= keyseg_length;
  }
safe_end:
  *ab_area= loc_ab_area;
  return loc_ab_area - a_area;
}

#define RT_PERIM_INC_KORR(type, korr_func, len) \
{ \
   type amin, amax, bmin, bmax; \
   amin= korr_func(a); \
   bmin= korr_func(b); \
   amax= korr_func(a+len); \
   bmax= korr_func(b+len); \
   a_perim+= (((double)amax) - ((double)amin)); \
   *ab_perim+= ((double)max(amax, bmax) - (double)min(amin, bmin)); \
}

#define RT_PERIM_INC_GET(type, get_func, len)\
{\
   type amin, amax, bmin, bmax; \
   get_func(amin, a); \
   get_func(bmin, b); \
   get_func(amax, a+len); \
   get_func(bmax, b+len); \
   a_perim+= (((double)amax) - ((double)amin)); \
   *ab_perim+= ((double)max(amax, bmax) - (double)min(amin, bmin)); \
}

/*
Calculates MBR_PERIMETER(a+b) - MBR_PERIMETER(a)
*/
double maria_rtree_perimeter_increase(HA_KEYSEG *keyseg, uchar* a, uchar* b,
				uint key_length, double *ab_perim)
{
  double a_perim= 0.0;

  *ab_perim= 0.0;
  for (; (int)key_length > 0; keyseg += 2)
  {
    uint32 keyseg_length;

    if (keyseg->null_bit)                       /* Handle NULL part */
      return -1;

    switch ((enum ha_base_keytype) keyseg->type) {
    case HA_KEYTYPE_INT8:
      RT_PERIM_INC_KORR(int8, mi_sint1korr, 1);
      break;
    case HA_KEYTYPE_BINARY:
      RT_PERIM_INC_KORR(uint8, mi_uint1korr, 1);
      break;
    case HA_KEYTYPE_SHORT_INT:
      RT_PERIM_INC_KORR(int16, mi_sint2korr, 2);
      break;
    case HA_KEYTYPE_USHORT_INT:
      RT_PERIM_INC_KORR(uint16, mi_uint2korr, 2);
      break;
    case HA_KEYTYPE_INT24:
      RT_PERIM_INC_KORR(int32, mi_sint3korr, 3);
      break;
    case HA_KEYTYPE_UINT24:
      RT_PERIM_INC_KORR(int32, mi_uint3korr, 3);
      break;
    case HA_KEYTYPE_LONG_INT:
      RT_PERIM_INC_KORR(int32, mi_sint4korr, 4);
      break;
    case HA_KEYTYPE_ULONG_INT:
      RT_PERIM_INC_KORR(uint32, mi_uint4korr, 4);
      break;
#ifdef HAVE_LONG_LONG
    case HA_KEYTYPE_LONGLONG:
      RT_PERIM_INC_KORR(longlong, mi_sint8korr, 8);
      break;
    case HA_KEYTYPE_ULONGLONG:
      RT_PERIM_INC_KORR(longlong, mi_sint8korr, 8);
      break;
#endif
    case HA_KEYTYPE_FLOAT:
      RT_PERIM_INC_GET(float, mi_float4get, 4);
      break;
    case HA_KEYTYPE_DOUBLE:
      RT_PERIM_INC_GET(double, mi_float8get, 8);
      break;
    case HA_KEYTYPE_END:
      return *ab_perim - a_perim;
    default:
      return -1;
    }
    keyseg_length= keyseg->length * 2;
    key_length-= keyseg_length;
    a+= keyseg_length;
    b+= keyseg_length;
  }
  return *ab_perim - a_perim;
}


#define RT_PAGE_MBR_KORR(share, type, korr_func, store_func, len, to)    \
{ \
  type amin, amax, bmin, bmax; \
  amin= korr_func(k + inc); \
  amax= korr_func(k + inc + len); \
  k= rt_PAGE_NEXT_KEY(share, k, k_len, nod_flag);            \
  for (; k < last; k= rt_PAGE_NEXT_KEY(share, k, k_len, nod_flag))       \
{ \
    bmin= korr_func(k + inc); \
    bmax= korr_func(k + inc + len); \
    if (amin > bmin) \
      amin= bmin; \
    if (amax < bmax) \
      amax= bmax; \
} \
  store_func(to, amin); \
  to+= len; \
  store_func(to, amax); \
  to += len;           \
  inc += 2 * len; \
}

#define RT_PAGE_MBR_GET(share, type, get_func, store_func, len, to)      \
{ \
  type amin, amax, bmin, bmax; \
  get_func(amin, k + inc); \
  get_func(amax, k + inc + len); \
  k= rt_PAGE_NEXT_KEY(share, k, k_len, nod_flag);            \
  for (; k < last; k= rt_PAGE_NEXT_KEY(share, k, k_len, nod_flag))       \
{ \
    get_func(bmin, k + inc); \
    get_func(bmax, k + inc + len); \
    if (amin > bmin) \
      amin= bmin; \
    if (amax < bmax) \
      amax= bmax; \
} \
  store_func(to, amin); \
  to+= len; \
  store_func(to, amax); \
  to+= len; \
  inc += 2 * len; \
}

/*
  Calculates key page total MBR= MBR(key1) + MBR(key2) + ...
  Stores into *to.
*/
int maria_rtree_page_mbr(const HA_KEYSEG *keyseg,
                         MARIA_PAGE *page,
                         uchar *to, uint key_length)
{
  MARIA_HA *info= page->info;
  MARIA_SHARE *share= info->s;
  uint inc= 0;
  uint k_len= key_length;
  uint nod_flag= page->node;
  const uchar *k;
  const uchar *last= rt_PAGE_END(page);

  for (; (int)key_length > 0; keyseg += 2)
  {
    key_length -= keyseg->length * 2;

    /* Handle NULL part */
    if (keyseg->null_bit)
    {
      return 1;
    }

    k= rt_PAGE_FIRST_KEY(share, page->buff, nod_flag);

    switch ((enum ha_base_keytype) keyseg->type) {
    case HA_KEYTYPE_INT8:
      RT_PAGE_MBR_KORR(share, int8, mi_sint1korr, mi_int1store, 1, to);
      break;
    case HA_KEYTYPE_BINARY:
      RT_PAGE_MBR_KORR(share, uint8, mi_uint1korr, mi_int1store, 1, to);
      break;
    case HA_KEYTYPE_SHORT_INT:
      RT_PAGE_MBR_KORR(share, int16, mi_sint2korr, mi_int2store, 2, to);
      break;
    case HA_KEYTYPE_USHORT_INT:
      RT_PAGE_MBR_KORR(share, uint16, mi_uint2korr, mi_int2store, 2, to);
      break;
    case HA_KEYTYPE_INT24:
      RT_PAGE_MBR_KORR(share, int32, mi_sint3korr, mi_int3store, 3, to);
      break;
    case HA_KEYTYPE_UINT24:
      RT_PAGE_MBR_KORR(share, uint32, mi_uint3korr, mi_int3store, 3, to);
      break;
    case HA_KEYTYPE_LONG_INT:
      RT_PAGE_MBR_KORR(share, int32, mi_sint4korr, mi_int4store, 4, to);
      break;
    case HA_KEYTYPE_ULONG_INT:
      RT_PAGE_MBR_KORR(share, uint32, mi_uint4korr, mi_int4store, 4, to);
      break;
#ifdef HAVE_LONG_LONG
    case HA_KEYTYPE_LONGLONG:
      RT_PAGE_MBR_KORR(share, longlong, mi_sint8korr, mi_int8store, 8, to);
      break;
    case HA_KEYTYPE_ULONGLONG:
      RT_PAGE_MBR_KORR(share, ulonglong, mi_uint8korr, mi_int8store, 8, to);
      break;
#endif
    case HA_KEYTYPE_FLOAT:
      RT_PAGE_MBR_GET(share, float, mi_float4get, mi_float4store, 4, to);
      break;
    case HA_KEYTYPE_DOUBLE:
      RT_PAGE_MBR_GET(share, double, mi_float8get, mi_float8store, 8, to);
      break;
    case HA_KEYTYPE_END:
      return 0;
    default:
      return 1;
    }
  }
  return 0;
}

#endif /*HAVE_RTREE_KEYS*/
