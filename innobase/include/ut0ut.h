/**********************************************************************
Various utilities

(c) 1994, 1995 Innobase Oy

Created 1/20/1994 Heikki Tuuri
***********************************************************************/

#ifndef ut0ut_h
#define ut0ut_h

#include <time.h>
#include <ctype.h>

#include "univ.i"


typedef time_t	ib_time_t;

/**********************************************************
Calculates the minimum of two ulints. */
UNIV_INLINE
ulint
ut_min(
/*===*/
			/* out: minimum */
	ulint    n1,  	/* in: first number */
	ulint    n2);  	/* in: second number */
/**********************************************************
Calculates the maximum of two ulints. */
UNIV_INLINE
ulint
ut_max(
/*===*/
			/* out: maximum */
	ulint    n1,	/* in: first number */
	ulint    n2);	/* in: second number */
/********************************************************************
Calculates minimum of two ulint-pairs. */
UNIV_INLINE
void
ut_pair_min(
/*========*/
	ulint*	a,	/* out: more significant part of minimum */
	ulint*	b,	/* out: less significant part of minimum */
	ulint	a1,	/* in: more significant part of first pair */
	ulint	b1,	/* in: less significant part of first pair */
	ulint	a2,	/* in: more significant part of second pair */
	ulint	b2);	/* in: less significant part of second pair */
/**********************************************************
Compares two ulints. */
UNIV_INLINE
int
ut_ulint_cmp(
/*=========*/
			/* out: 1 if a > b, 0 if a == b, -1 if a < b */
	ulint	a,	/* in: ulint */
	ulint	b);	/* in: ulint */
/***********************************************************
Compares two pairs of ulints. */
UNIV_INLINE
int
ut_pair_cmp(
/*========*/
			/* out: -1 if a < b, 0 if a == b,
			1 if a > b */ 
	ulint	a1,	/* in: more significant part of first pair */
	ulint	a2,	/* in: less significant part of first pair */
	ulint	b1,	/* in: more significant part of second pair */
	ulint	b2);	/* in: less significant part of second pair */
/*****************************************************************
Calculates fast the remainder when divided by a power of two. */
UNIV_INLINE
ulint
ut_2pow_remainder(
/*==============*/	/* out: remainder */
	ulint	n,	/* in: number to be divided */
	ulint	m);	/* in: divisor; power of 2 */
/*****************************************************************
Calculates fast value rounded to a multiple of a power of 2. */
UNIV_INLINE
ulint
ut_2pow_round(
/*==========*/		/* out: value of n rounded down to nearest
			multiple of m */
	ulint	n,	/* in: number to be rounded */
	ulint	m);	/* in: divisor; power of 2 */
/*****************************************************************
Calculates fast the 2-logarithm of a number, rounded upward to an
integer. */
UNIV_INLINE
ulint
ut_2_log(
/*=====*/
			/* out: logarithm in the base 2, rounded upward */
	ulint	n);	/* in: number */
/*****************************************************************
Calculates 2 to power n. */
UNIV_INLINE
ulint
ut_2_exp(
/*=====*/
			/* out: 2 to power n */
	ulint	n);	/* in: number */
/*****************************************************************
Calculates fast the number rounded up to the nearest power of 2. */
UNIV_INLINE
ulint
ut_2_power_up(
/*==========*/
			/* out: first power of 2 which is >= n */
	ulint	n);	/* in: number != 0 */
/****************************************************************
Sort function for ulint arrays. */

void
ut_ulint_sort(ulint* arr, ulint* aux_arr, ulint low, ulint high);
/*============================================================*/
/************************************************************
The following function returns a clock time in milliseconds. */

ulint
ut_clock(void);
/**************************************************************
Returns system time. We do not specify the format of the time returned:
the only way to manipulate it is to use the function ut_difftime. */

ib_time_t
ut_time(void);
/*=========*/
/**************************************************************
Returns the difference of two times in seconds. */

double
ut_difftime(
/*========*/
				/* out: time2 - time1 expressed in seconds */
	ib_time_t	time2,	/* in: time */
	ib_time_t	time1);	/* in: time */
/*****************************************************************
Runs an idle loop on CPU. The argument gives the desired delay
in microseconds on 100 MHz Pentium + Visual C++. */

ulint
ut_delay(
/*=====*/
			/* out: dummy value */
	ulint	delay);	/* in: delay in microseconds on 100 MHz Pentium */
/*****************************************************************
Prints the contents of a memory buffer in hex and ascii. */

void
ut_print_buf(
/*=========*/
	byte*	buf,    /* in: memory buffer */
	ulint	len);   /* in: length of the buffer */
/*****************************************************************
Prints the contents of a memory buffer in hex and ascii. */

ulint
ut_sprintf_buf(
/*===========*/
			/* out: printed length in bytes */
	char*	str,	/* in: buffer to print to */
	byte*	buf,	/* in: memory buffer */
	ulint 	len);	/* in: length of the buffer */


#ifndef UNIV_NONINL
#include "ut0ut.ic"
#endif

#endif

