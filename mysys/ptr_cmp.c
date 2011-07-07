/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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
  get_ptr_compare(len) returns a pointer to a optimal byte-compare function
  for a array of stringpointer where all strings have size len.
  The bytes are compare as unsigned chars.
  */

#include "mysys_priv.h"
#include <myisampack.h>

#ifdef __sun
/*
 * On Solaris, memcmp() is normally faster than the unrolled ptr_compare_N
 * functions, as memcmp() is usually a platform-specific implementation
 * written in assembler, provided in /usr/lib/libc/libc_hwcap*.so.1.
 * This implementation is also usually faster than the built-in memcmp
 * supplied by GCC, so it is recommended to build with "-fno-builtin-memcmp"
 * in CFLAGS if building with GCC on Solaris.
 */

#include <string.h>

static int native_compare(size_t *length, unsigned char **a, unsigned char **b)
{
  return memcmp(*a, *b, *length);
}

#else	/* __sun */

static int ptr_compare(size_t *compare_length, uchar **a, uchar **b);
static int ptr_compare_0(size_t *compare_length, uchar **a, uchar **b);
static int ptr_compare_1(size_t *compare_length, uchar **a, uchar **b);
static int ptr_compare_2(size_t *compare_length, uchar **a, uchar **b);
static int ptr_compare_3(size_t *compare_length, uchar **a, uchar **b);
#endif	/* __sun */

	/* Get a pointer to a optimal byte-compare function for a given size */

#ifdef __sun
qsort2_cmp get_ptr_compare (size_t size __attribute__((unused)))
{
  return (qsort2_cmp) native_compare;
}
#else
qsort2_cmp get_ptr_compare (size_t size)
{
  if (size < 4)
    return (qsort2_cmp) ptr_compare;
  switch (size & 3) {
    case 0: return (qsort2_cmp) ptr_compare_0;
    case 1: return (qsort2_cmp) ptr_compare_1;
    case 2: return (qsort2_cmp) ptr_compare_2;
    case 3: return (qsort2_cmp) ptr_compare_3;
    }
  return 0;					/* Impossible */
}
#endif /* __sun */


	/*
	  Compare to keys to see witch is smaller.
	  Loop unrolled to make it quick !!
	*/

#define cmp(N) if (first[N] != last[N]) return (int) first[N] - (int) last[N]

#ifndef __sun

static int ptr_compare(size_t *compare_length, uchar **a, uchar **b)
{
  reg3 int length= *compare_length;
  reg1 uchar *first,*last;

  first= *a; last= *b;
  while (--length)
  {
    if (*first++ != *last++)
      return (int) first[-1] - (int) last[-1];
  }
  return (int) first[0] - (int) last[0];
}


static int ptr_compare_0(size_t *compare_length,uchar **a, uchar **b)
{
  reg3 int length= *compare_length;
  reg1 uchar *first,*last;

  first= *a; last= *b;
 loop:
  cmp(0);
  cmp(1);
  cmp(2);
  cmp(3);
  if ((length-=4))
  {
    first+=4;
    last+=4;
    goto loop;
  }
  return (0);
}


static int ptr_compare_1(size_t *compare_length,uchar **a, uchar **b)
{
  reg3 int length= *compare_length-1;
  reg1 uchar *first,*last;

  first= *a+1; last= *b+1;
  cmp(-1);
 loop:
  cmp(0);
  cmp(1);
  cmp(2);
  cmp(3);
  if ((length-=4))
  {
    first+=4;
    last+=4;
    goto loop;
  }
  return (0);
}

static int ptr_compare_2(size_t *compare_length,uchar **a, uchar **b)
{
  reg3 int length= *compare_length-2;
  reg1 uchar *first,*last;

  first= *a +2 ; last= *b +2;
  cmp(-2);
  cmp(-1);
 loop:
  cmp(0);
  cmp(1);
  cmp(2);
  cmp(3);
  if ((length-=4))
  {
    first+=4;
    last+=4;
    goto loop;
  }
  return (0);
}

static int ptr_compare_3(size_t *compare_length,uchar **a, uchar **b)
{
  reg3 int length= *compare_length-3;
  reg1 uchar *first,*last;

  first= *a +3 ; last= *b +3;
  cmp(-3);
  cmp(-2);
  cmp(-1);
 loop:
  cmp(0);
  cmp(1);
  cmp(2);
  cmp(3);
  if ((length-=4))
  {
    first+=4;
    last+=4;
    goto loop;
  }
  return (0);
}

#endif /* !__sun */

void my_store_ptr(uchar *buff, size_t pack_length, my_off_t pos)
{
  switch (pack_length) {
#if SIZEOF_OFF_T > 4
  case 8: mi_int8store(buff,pos); break;
  case 7: mi_int7store(buff,pos); break;
  case 6: mi_int6store(buff,pos); break;
  case 5: mi_int5store(buff,pos); break;
#endif
  case 4: mi_int4store(buff,pos); break;
  case 3: mi_int3store(buff,pos); break;
  case 2: mi_int2store(buff,pos); break;
  case 1: buff[0]= (uchar) pos; break;
  default: DBUG_ASSERT(0);
  }
  return;
}

my_off_t my_get_ptr(uchar *ptr, size_t pack_length)
{
  my_off_t pos;
  switch (pack_length) {
#if SIZEOF_OFF_T > 4
  case 8: pos= (my_off_t) mi_uint8korr(ptr); break;
  case 7: pos= (my_off_t) mi_uint7korr(ptr); break;
  case 6: pos= (my_off_t) mi_uint6korr(ptr); break;
  case 5: pos= (my_off_t) mi_uint5korr(ptr); break;
#endif
  case 4: pos= (my_off_t) mi_uint4korr(ptr); break;
  case 3: pos= (my_off_t) mi_uint3korr(ptr); break;
  case 2: pos= (my_off_t) mi_uint2korr(ptr); break;
  case 1: pos= (my_off_t) *(uchar*) ptr; break;
  default: DBUG_ASSERT(0); return 0;
  }
 return pos;
}

