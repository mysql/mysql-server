/* Copyright (C) 1991, 1992, 1996, 1997 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Written by Douglas C. Schmidt (schmidt@ics.uci.edu).

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.	*/

/*
    Modifications by monty:
  - Uses mysys include files
  - Small fixes to make the it a bit faster
  - Can be compiled with a cmp function that takes one extra argument.
*/

#include "mysys_priv.h"

/* Envoke the comparison function, returns either 0, < 0, or > 0. */
#ifdef QSORT_EXTRA_CMP_ARGUMENT
#define CMP(A,B) ((*cmp)(cmp_argument,(A),(B)))
#else
#define CMP(A,B) ((*cmp)((A),(B)))
#endif

/* Byte-wise swap two items of size SIZE. */
#define SWAP(a, b, size)						      \
  do									      \
    {									      \
      register size_t __size = (size);					      \
      register char *__a = (a), *__b = (b);				      \
      do								      \
	{								      \
	  char __tmp = *__a;						      \
	  *__a++ = *__b;						      \
	  *__b++ = __tmp;						      \
	} while (--__size > 0);						      \
    } while (0)

/* Discontinue quicksort algorithm when partition gets below this size.
   This particular magic number was chosen to work best on a Sun 4/260. */
#define MAX_THRESH 8

/* Stack node declarations used to store unfulfilled partition obligations. */
typedef struct _qsort_stack_node
  {
    char *lo;
    char *hi;
  } stack_node;

/* The next 4 #defines implement a very fast in-line stack abstraction. */
#define STACK_SIZE	(8 * sizeof(unsigned long int))
#define PUSH(LOW,HIGH) do {top->lo = LOW;top++->hi = HIGH;} while (0)
#define POP(LOW,HIGH)  do {LOW = (--top)->lo;HIGH = top->hi;} while (0)
#define STACK_NOT_EMPTY (stack < top)

/* Order size using quicksort.	This implementation incorporates
   four optimizations discussed in Sedgewick:

   1. Non-recursive, using an explicit stack of pointer that store the
      next array partition to sort.  To save time, this maximum amount
      of space required to store an array of MAX_INT is allocated on the
      stack.  Assuming a 32-bit integer, this needs only 32 *
      sizeof (stack_node) == 136 bits.	Pretty cheap, actually.

   2. Chose the pivot element using a median-of-three decision tree.
      This reduces the probability of selecting a bad pivot value and
      eliminates certain extraneous comparisons.

   3. Only quicksorts TOTAL_ELEMS / MAX_THRESH partitions, leaving
      insertion sort to order the MAX_THRESH items within each partition.
      This is a big win, since insertion sort is faster for small, mostly
      sorted array segments.

   4. The larger of the two sub-partitions is always pushed onto the
      stack first, with the algorithm then concentrating on the
      smaller partition.  This *guarantees* no more than log (n)
      stack size is needed (actually O(1) in this case)! */

#if defined(QSORT_TYPE_IS_VOID)
#define SORT_RETURN return
#else
#define SORT_RETURN return 0
#endif

#ifdef QSORT_EXTRA_CMP_ARGUMENT
qsort_t qsort2(void *base_ptr, size_t total_elems, size_t size, qsort2_cmp cmp,
	       void *cmp_argument)
#else
qsort_t qsort(void *base_ptr, size_t total_elems, size_t size, qsort_cmp cmp)
#endif
{
  /* Allocating SIZE bytes for a pivot buffer facilitates a better
     algorithm below since we can do comparisons directly on the pivot.
     */
  size_t max_thresh   = (size_t) (MAX_THRESH * size);
  if (total_elems <= 1)
    SORT_RETURN;		/* Crashes on MSDOS if continues */

  if (total_elems > MAX_THRESH)
  {
    char       *lo = (char*) base_ptr;
    char       *hi = &lo[size * (total_elems - 1)];
    stack_node stack[STACK_SIZE]; /* Largest size needed for 32-bit int!!! */
    stack_node *top = stack + 1;
    char *pivot = (char *) my_alloca ((int) size);
#ifdef HAVE_purify
    stack[0].lo=stack[0].hi=0;
#endif

    do
    {
      char *left_ptr,*right_ptr;

      /* Select median value from among LO, MID, and HI. Rearrange
	 LO and HI so the three values are sorted. This lowers the
	 probability of picking a pathological pivot value and
	 skips a comparison for both the LEFT_PTR and RIGHT_PTR. */

      char *mid = lo + size * (((ulong) (hi - lo) / (ulong) size) >> 1);

      if (CMP(hi,lo) < 0)
	SWAP (hi, lo, size);
      if (CMP (mid, lo) < 0)
	SWAP (mid, lo, size);
      else if (CMP (hi, mid) < 0)
	SWAP (mid, hi, size);
      memcpy (pivot, mid, size);

      left_ptr	= lo + size;
      right_ptr = hi - size;

      /* Here's the famous ``collapse the walls'' section of quicksort.
	 Gotta like those tight inner loops!	They are the main reason
	 that this algorithm runs much faster than others. */
      do
      {
	while (CMP (left_ptr, pivot) < 0)
	  left_ptr += size;

	while (CMP (pivot, right_ptr) < 0)
	  right_ptr -= size;

	if (left_ptr < right_ptr)
	{
	  SWAP (left_ptr, right_ptr, size);
	  left_ptr += size;
	  right_ptr -= size;
	}
	else if (left_ptr == right_ptr)
	{
	  left_ptr += size;
	  right_ptr -= size;
	  break;
	}
	else
	  break;				/* left_ptr > right_ptr */
      }
      while (left_ptr <= right_ptr);


      /* Set up pointers for next iteration.  First determine whether
	 left and right partitions are below the threshold size. If so,
	 ignore one or both.  Otherwise, push the larger partition's
	 bounds on the stack and continue sorting the smaller one. */

      if ((size_t) (right_ptr - lo) <= max_thresh)
      {
	if ((size_t) (hi - left_ptr) <= max_thresh)
	  POP (lo, hi);			/* Ignore both small partitions. */
	else
	  lo = left_ptr;		/* Ignore small left part. */
      }
      else if ((size_t) (hi - left_ptr) <= max_thresh)
	hi = right_ptr;			/* Ignore small right partition. */
      else if ((right_ptr - lo) > (hi - left_ptr))
      {
	PUSH (lo, right_ptr);		/* Push larger left part */
	lo = left_ptr;
      }
      else
      {
	PUSH (left_ptr, hi);		/* Push larger right part */
	hi = right_ptr;
      }
    } while (STACK_NOT_EMPTY);
    my_afree(pivot);
  }

  /* Once the BASE_PTR array is partially sorted by quicksort the rest
     is completely sorted using insertion sort, since this is efficient
     for partitions below MAX_THRESH size. BASE_PTR points to the beginning
     of the array to sort, and END_PTR points at the very last element in
     the array (*not* one beyond it!). */

  {
    char *end_ptr = (char*) base_ptr + size * (total_elems - 1);
    char *tmp_ptr = (char*) base_ptr;
    char *thresh  = min (end_ptr, (char*) base_ptr + max_thresh);
    register char *run_ptr;

    /* Find smallest element in first threshold and place it at the
       array's beginning.  This is the smallest array element,
       and the operation speeds up insertion sort's inner loop. */

    for (run_ptr = tmp_ptr + size; run_ptr <= thresh; run_ptr += size)
      if (CMP (run_ptr, tmp_ptr) < 0)
	tmp_ptr = run_ptr;

    if (tmp_ptr != (char*) base_ptr)
      SWAP (tmp_ptr, (char*) base_ptr, size);

    /* Insertion sort, running from left-hand-side up to right-hand-side.  */

    for (run_ptr = (char*) base_ptr + size;
	 (run_ptr += size) <= end_ptr; )
    {
      if (CMP (run_ptr, (tmp_ptr = run_ptr-size)) < 0)
      {
	char *trav;
	while (CMP (run_ptr, tmp_ptr -= size) < 0) ;
	tmp_ptr += size;

	/* Shift down all smaller elements, put found element in 'run_ptr' */
	for (trav = run_ptr + size; --trav >= run_ptr;)
	{
	  char c = *trav;
	  char *hi, *lo;

	  for (hi = lo = trav; (lo -= size) >= tmp_ptr; hi = lo)
	    *hi = *lo;
	  *hi = c;
	}
      }
    }
  }
  SORT_RETURN;
}
