/*
  get_ptr_compare(len) returns a pointer to a optimal byte-compare function
  for a array of stringpointer where all strings have size len.
  The bytes are compare as unsigned chars.
  Because the size is saved in a static variable.
  When using threads the program must have called my_init and the thread
  my_init_thread()
  */

#include <global.h>
#include "m_string.h"

static int ptr_compare(uchar **a, uchar **b);
static int ptr_compare_0(uchar **a, uchar **b);
static int ptr_compare_1(uchar **a, uchar **b);
static int ptr_compare_2(uchar **a, uchar **b);
static int ptr_compare_3(uchar **a, uchar **b);

#ifdef THREAD
#include <my_pthread.h>
#define compare_length my_thread_var->cmp_length
#else
static uint compare_length;
#endif

	/* Get a pointer to a optimal byte-compare function for a given size */

qsort_cmp get_ptr_compare (uint size)
{
  compare_length=size;			/* Remember for loop */

  if (size < 4)
    return (qsort_cmp) ptr_compare;
  switch (size & 3) {
    case 0: return (qsort_cmp) ptr_compare_0;
    case 1: return (qsort_cmp) ptr_compare_1;
    case 2: return (qsort_cmp) ptr_compare_2;
    case 3: return (qsort_cmp) ptr_compare_3;
    }
  return 0;					/* Impossible */
}


	/*
	  Compare to keys to see witch is smaller.
	  Loop unrolled to make it quick !!
	*/

#define cmp(N) if (first[N] != last[N]) return (int) first[N] - (int) last[N]

static int ptr_compare(uchar **a, uchar **b)
{
  reg3 int length= compare_length;
  reg1 uchar *first,*last;

  first= *a; last= *b;
  while (--length)
  {
    if (*first++ != *last++)
      return (int) first[-1] - (int) last[-1];
  }
  return (int) first[0] - (int) last[0];
}


static int ptr_compare_0(uchar **a, uchar **b)
{
  reg3 int length= compare_length;
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


static int ptr_compare_1(uchar **a, uchar **b)
{
  reg3 int length= compare_length-1;
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

static int ptr_compare_2(uchar **a, uchar **b)
{
  reg3 int length= compare_length-2;
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

static int ptr_compare_3(uchar **a, uchar **b)
{
  reg3 int length= compare_length-3;
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
