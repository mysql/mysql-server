/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

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
  qsort implementation optimized for comparison of pointers
  Inspired by the qsort implementations by Douglas C. Schmidt,
  and Bentley & McIlroy's "Engineering a Sort Function".
*/


#include "mysys_priv.h"
#include "my_sys.h"
#include <m_string.h>

/* We need to use qsort with 2 different compare functions */
#ifdef QSORT_EXTRA_CMP_ARGUMENT
#define CMP(A,B) ((*cmp)(cmp_argument,(A),(B)))
#else
#define CMP(A,B) ((*cmp)((A),(B)))
#endif

#define SWAP(A, B, size,swap_ptrs)			\
do {							\
   if (swap_ptrs)					\
   {							\
     char **a = (char**) (A), **b = (char**) (B);  \
     char *tmp = *a; *a++ = *b; *b++ = tmp;		\
   }							\
   else							\
   {							\
     char *a = (A), *b = (B);			\
     char *end= a+size;				\
     do							\
     {							\
       char tmp = *a; *a++ = *b; *b++ = tmp;		\
     } while (a < end);					\
   }							\
} while (0)

/* Put the median in the middle argument */
#define MEDIAN(low, mid, high)				\
{							\
    if (CMP(high,low) < 0)				\
      SWAP(high, low, size, ptr_cmp);			\
    if (CMP(mid, low) < 0)				\
      SWAP(mid, low, size, ptr_cmp);			\
    else if (CMP(high, mid) < 0)			\
      SWAP(mid, high, size, ptr_cmp);			\
}

/* The following node is used to store ranges to avoid recursive calls */

typedef struct st_stack
{
  char *low,*high;
} stack_node;

#define PUSH(LOW,HIGH)  {stack_ptr->low = LOW; stack_ptr++->high = HIGH;}
#define POP(LOW,HIGH)   {LOW = (--stack_ptr)->low; HIGH = stack_ptr->high;}

/* The following stack size is enough for ulong ~0 elements */
#define STACK_SIZE	(8 * sizeof(unsigned long int))
#define THRESHOLD_FOR_INSERT_SORT 10

/****************************************************************************
** 'standard' quicksort with the following extensions:
**
** Can be compiled with the qsort2_cmp compare function
** Store ranges on stack to avoid recursion
** Use insert sort on small ranges
** Optimize for sorting of pointers (used often by MySQL)
** Use median comparison to find partition element
*****************************************************************************/

#ifdef QSORT_EXTRA_CMP_ARGUMENT
void my_qsort2(void *base_ptr, size_t count, size_t size, qsort2_cmp cmp,
               const void *cmp_argument)
#else
void my_qsort(void *base_ptr, size_t count, size_t size, qsort_cmp cmp)
#endif
{
  char *low, *high, *pivot;
  stack_node stack[STACK_SIZE], *stack_ptr;
  my_bool ptr_cmp;
  /* Handle the simple case first */
  /* This will also make the rest of the code simpler */
  if (count <= 1)
    return;

  low  = (char*) base_ptr;
  high = low+ size * (count - 1);
  stack_ptr = stack + 1;
  pivot = (char *) my_alloca((int) size);
  ptr_cmp= size == sizeof(char*) && !((low - (char*) 0)& (sizeof(char*)-1));

  /* The following loop sorts elements between high and low */
  do
  {
    char *low_ptr, *high_ptr, *mid;

    count=((size_t) (high - low) / size)+1;
    /* If count is small, then an insert sort is faster than qsort */
    if (count < THRESHOLD_FOR_INSERT_SORT)
    {
      for (low_ptr = low + size; low_ptr <= high; low_ptr += size)
      {
	char *ptr;
	for (ptr = low_ptr; ptr > low && CMP(ptr - size, ptr) > 0;
	     ptr -= size)
	  SWAP(ptr, ptr - size, size, ptr_cmp);
      }
      POP(low, high);
      continue;
    }

    /* Try to find a good middle element */
    mid= low + size * (count >> 1);
    if (count > 40)				/* Must be bigger than 24 */
    {
      size_t step = size* (count / 8);
      MEDIAN(low, low + step, low+step*2);
      MEDIAN(mid - step, mid, mid+step);
      MEDIAN(high - 2 * step, high-step, high);
      /* Put best median in 'mid' */
      MEDIAN(low+step, mid, high-step);
      low_ptr  = low;
      high_ptr = high;
    }
    else
    {
      MEDIAN(low, mid, high);
      /* The low and high argument are already in sorted against 'pivot' */
      low_ptr  = low + size;
      high_ptr = high - size;
    }
    memcpy(pivot, mid, size);

    do
    {
      while (CMP(low_ptr, pivot) < 0)
	low_ptr += size;
      while (CMP(pivot, high_ptr) < 0)
	high_ptr -= size;

      if (low_ptr < high_ptr)
      {
	SWAP(low_ptr, high_ptr, size, ptr_cmp);
	low_ptr += size;
	high_ptr -= size;
      }
      else 
      {
	if (low_ptr == high_ptr)
	{
	  low_ptr += size;
	  high_ptr -= size;
	}
	break;
      }
    }
    while (low_ptr <= high_ptr);

    /*
      Prepare for next iteration.
       Skip partitions of size 1 as these doesn't have to be sorted
       Push the larger partition and sort the smaller one first.
       This ensures that the stack is keept small.
    */

    if ((int) (high_ptr - low) <= 0)
    {
      if ((int) (high - low_ptr) <= 0)
      {
	POP(low, high);			/* Nothing more to sort */
      }
      else
	low = low_ptr;			/* Ignore small left part. */
    }
    else if ((int) (high - low_ptr) <= 0)
      high = high_ptr;			/* Ignore small right part. */
    else if ((high_ptr - low) > (high - low_ptr))
    {
      PUSH(low, high_ptr);		/* Push larger left part */
      low = low_ptr;
    }
    else
    {
      PUSH(low_ptr, high);		/* Push larger right part */
      high = high_ptr;
    }
  } while (stack_ptr > stack);
  return;
}
