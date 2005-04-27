/**********************************************************************
Utilities for byte operations

(c) 1994, 1995 Innobase Oy

Created 1/20/1994 Heikki Tuuri
***********************************************************************/

#ifndef ut0byte_h
#define ut0byte_h


#include "univ.i"

/* Type definition for a 64-bit unsigned integer, which works also
in 32-bit machines. NOTE! Access the fields only with the accessor
functions. This definition appears here only for the compiler to
know the size of a dulint. */

typedef	struct dulint_struct	dulint;
struct dulint_struct{
	ulint	high;	/* most significant 32 bits */
	ulint	low;	/* least significant 32 bits */
};

/* Zero value for a dulint */
extern dulint	ut_dulint_zero;

/* Maximum value for a dulint */
extern dulint	ut_dulint_max;

/***********************************************************
Creates a 64-bit dulint out of two ulints. */
UNIV_INLINE
dulint
ut_dulint_create(
/*=============*/
			/* out: created dulint */
	ulint	high,	/* in: high-order 32 bits */
	ulint	low);	/* in: low-order 32 bits */
/***********************************************************
Gets the high-order 32 bits of a dulint. */
UNIV_INLINE
ulint
ut_dulint_get_high(
/*===============*/
			/* out: 32 bits in ulint */
	dulint	d);	/* in: dulint */
/***********************************************************
Gets the low-order 32 bits of a dulint. */
UNIV_INLINE
ulint
ut_dulint_get_low(
/*==============*/
			/* out: 32 bits in ulint */
	dulint	d);	/* in: dulint */
/***********************************************************
Converts a dulint (a struct of 2 ulints) to ib_longlong, which is a 64-bit
integer type. */
UNIV_INLINE
ib_longlong
ut_conv_dulint_to_longlong(
/*=======================*/
			/* out: value in ib_longlong type */
	dulint	d);	/* in: dulint */
/***********************************************************
Tests if a dulint is zero. */
UNIV_INLINE
ibool
ut_dulint_is_zero(
/*==============*/
			/* out: TRUE if zero */
	dulint	a);	/* in: dulint */
/***********************************************************
Compares two dulints. */
UNIV_INLINE
int
ut_dulint_cmp(
/*==========*/
			/* out: -1 if a < b, 0 if a == b,
			1 if a > b */ 
	dulint	a,	/* in: dulint */
	dulint	b);	/* in: dulint */
/***********************************************************
Calculates the max of two dulints. */
UNIV_INLINE
dulint
ut_dulint_get_max(
/*==============*/
			/* out: max(a, b) */
	dulint	a,	/* in: dulint */
	dulint	b);	/* in: dulint */
/***********************************************************
Calculates the min of two dulints. */
UNIV_INLINE
dulint
ut_dulint_get_min(
/*==============*/
			/* out: min(a, b) */
	dulint	a,	/* in: dulint */
	dulint	b);	/* in: dulint */
/***********************************************************
Adds a ulint to a dulint. */
UNIV_INLINE
dulint
ut_dulint_add(
/*==========*/
			/* out: sum a + b */
	dulint	a,	/* in: dulint */
	ulint	b);	/* in: ulint */
/***********************************************************
Subtracts a ulint from a dulint. */
UNIV_INLINE
dulint
ut_dulint_subtract(
/*===============*/
			/* out: a - b */
	dulint	a,	/* in: dulint */
	ulint	b);	/* in: ulint, b <= a */
/***********************************************************
Subtracts a dulint from another. NOTE that the difference must be positive
and smaller that 4G. */
UNIV_INLINE
ulint
ut_dulint_minus(
/*============*/
			/* out: a - b */
	dulint	a,	/* in: dulint; NOTE a must be >= b and at most
			2 to power 32 - 1 greater */
	dulint	b);	/* in: dulint */
/************************************************************
Rounds a dulint downward to a multiple of a power of 2. */
UNIV_INLINE
dulint
ut_dulint_align_down(
/*=================*/
				/* out: rounded value */
	dulint   n,        	/* in: number to be rounded */
	ulint    align_no);  	/* in: align by this number which must be a
				power of 2 */
/************************************************************
Rounds a dulint upward to a multiple of a power of 2. */
UNIV_INLINE
dulint
ut_dulint_align_up(
/*===============*/
				/* out: rounded value */
	dulint   n,        	/* in: number to be rounded */
	ulint    align_no);  	/* in: align by this number which must be a
				power of 2 */
/***********************************************************
Increments a dulint variable by 1. */
#define UT_DULINT_INC(D)\
{\
	if ((D).low == 0xFFFFFFFFUL) {\
		(D).high = (D).high + 1;\
		(D).low = 0;\
	} else {\
		(D).low = (D).low + 1;\
	}\
}
/***********************************************************
Tests if two dulints are equal. */
#define UT_DULINT_EQ(D1, D2)	(((D1).low == (D2).low)\
						&& ((D1).high == (D2).high))
/****************************************************************
Sort function for dulint arrays. */
void
ut_dulint_sort(dulint* arr, dulint* aux_arr, ulint low, ulint high);
/*===============================================================*/
/************************************************************
The following function calculates the value of an integer n rounded
to the least product of align_no which is >= n. align_no has to be a
power of 2. */
UNIV_INLINE
ulint
ut_calc_align(
/*==========*/
				/* out: rounded value */
	ulint    n,             /* in: number to be rounded */
	ulint    align_no);     /* in: align by this number */
/************************************************************
The following function calculates the value of an integer n rounded
to the biggest product of align_no which is <= n. align_no has to be a
power of 2. */
UNIV_INLINE
ulint
ut_calc_align_down(
/*===============*/
				/* out: rounded value */
	ulint    n,          	/* in: number to be rounded */
	ulint    align_no);	/* in: align by this number */
/*************************************************************
The following function rounds up a pointer to the nearest aligned address. */
UNIV_INLINE
void*
ut_align(
/*=====*/
				/* out: aligned pointer */
	void*   ptr,            /* in: pointer */
	ulint   align_no);     	/* in: align by this number */
/*************************************************************
The following function rounds down a pointer to the nearest
aligned address. */
UNIV_INLINE
void*
ut_align_down(
/*==========*/
				/* out: aligned pointer */
	void*   ptr,            /* in: pointer */
	ulint   align_no)      /* in: align by this number */
		__attribute__((const));
/*************************************************************
The following function computes the offset of a pointer from the nearest
aligned address. */
UNIV_INLINE
ulint
ut_align_offset(
/*==========*/
					/* out: distance from aligned
					pointer */
	const void*	ptr,		/* in: pointer */
	ulint		align_no)	/* in: align by this number */
			__attribute__((const));
/*********************************************************************
Gets the nth bit of a ulint. */
UNIV_INLINE
ibool
ut_bit_get_nth(
/*===========*/
			/* out: TRUE if nth bit is 1; 0th bit is defined to
			be the least significant */
	ulint	a,	/* in: ulint */
	ulint	n);	/* in: nth bit requested */
/*********************************************************************
Sets the nth bit of a ulint. */
UNIV_INLINE
ulint
ut_bit_set_nth(
/*===========*/
			/* out: the ulint with the bit set as requested */
	ulint	a,	/* in: ulint */
	ulint	n,	/* in: nth bit requested */
	ibool	val);	/* in: value for the bit to set */

#ifndef UNIV_NONINL
#include "ut0byte.ic"
#endif

#endif
