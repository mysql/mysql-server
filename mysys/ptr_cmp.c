/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/*
  get_ptr_compare(len) returns a pointer to a optimal byte-compare function
  for a array of stringpointer where all strings have size len.
  The bytes are compare as unsigned chars.
  */

#include "mysys_priv.h"

static int ptr_compare(uint *compare_length, uchar **a, uchar **b);
static int ptr_compare_0(uint *compare_length, uchar **a, uchar **b);
static int ptr_compare_1(uint *compare_length, uchar **a, uchar **b);
static int ptr_compare_2(uint *compare_length, uchar **a, uchar **b);
static int ptr_compare_3(uint *compare_length, uchar **a, uchar **b);

	/* Get a pointer to a optimal byte-compare function for a given size */

qsort2_cmp get_ptr_compare (uint size)
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


	/*
	  Compare to keys to see witch is smaller.
	  Loop unrolled to make it quick !!
	*/

#define cmp(N) if (first[N] != last[N]) return (int) first[N] - (int) last[N]

static int ptr_compare(uint *compare_length, uchar **a, uchar **b)
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


static int ptr_compare_0(uint *compare_length,uchar **a, uchar **b)
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


static int ptr_compare_1(uint *compare_length,uchar **a, uchar **b)
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

static int ptr_compare_2(uint *compare_length,uchar **a, uchar **b)
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

static int ptr_compare_3(uint *compare_length,uchar **a, uchar **b)
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
