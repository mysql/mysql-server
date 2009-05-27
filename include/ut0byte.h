/*****************************************************************************

Copyright (c) 1994, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/******************************************************************//**
@file include/ut0byte.h
Utilities for byte operations

Created 1/20/1994 Heikki Tuuri
***********************************************************************/

#ifndef ut0byte_h
#define ut0byte_h


#include "univ.i"

/** Pair of ulint integers. */
typedef	struct dulint_struct	dulint;
/** Type definition for a 64-bit unsigned integer, which works also
in 32-bit machines. NOTE! Access the fields only with the accessor
functions. This definition appears here only for the compiler to
know the size of a dulint. */
struct dulint_struct{
	ulint	high;	/*!< most significant 32 bits */
	ulint	low;	/*!< least significant 32 bits */
};

/** Zero value for a dulint */
extern const dulint	ut_dulint_zero;

/** Maximum value for a dulint */
extern const dulint	ut_dulint_max;

/*******************************************************//**
Creates a 64-bit dulint out of two ulints.
@return	created dulint */
UNIV_INLINE
dulint
ut_dulint_create(
/*=============*/
	ulint	high,	/*!< in: high-order 32 bits */
	ulint	low);	/*!< in: low-order 32 bits */
/*******************************************************//**
Gets the high-order 32 bits of a dulint.
@return	32 bits in ulint */
UNIV_INLINE
ulint
ut_dulint_get_high(
/*===============*/
	dulint	d);	/*!< in: dulint */
/*******************************************************//**
Gets the low-order 32 bits of a dulint.
@return	32 bits in ulint */
UNIV_INLINE
ulint
ut_dulint_get_low(
/*==============*/
	dulint	d);	/*!< in: dulint */
/*******************************************************//**
Converts a dulint (a struct of 2 ulints) to ib_int64_t, which is a 64-bit
integer type.
@return	value in ib_int64_t type */
UNIV_INLINE
ib_int64_t
ut_conv_dulint_to_longlong(
/*=======================*/
	dulint	d);	/*!< in: dulint */
/*******************************************************//**
Tests if a dulint is zero.
@return	TRUE if zero */
UNIV_INLINE
ibool
ut_dulint_is_zero(
/*==============*/
	dulint	a);	/*!< in: dulint */
/*******************************************************//**
Compares two dulints.
@return	-1 if a < b, 0 if a == b, 1 if a > b */
UNIV_INLINE
int
ut_dulint_cmp(
/*==========*/
	dulint	a,	/*!< in: dulint */
	dulint	b);	/*!< in: dulint */
/*******************************************************//**
Calculates the max of two dulints.
@return	max(a, b) */
UNIV_INLINE
dulint
ut_dulint_get_max(
/*==============*/
	dulint	a,	/*!< in: dulint */
	dulint	b);	/*!< in: dulint */
/*******************************************************//**
Calculates the min of two dulints.
@return	min(a, b) */
UNIV_INLINE
dulint
ut_dulint_get_min(
/*==============*/
	dulint	a,	/*!< in: dulint */
	dulint	b);	/*!< in: dulint */
/*******************************************************//**
Adds a ulint to a dulint.
@return	sum a + b */
UNIV_INLINE
dulint
ut_dulint_add(
/*==========*/
	dulint	a,	/*!< in: dulint */
	ulint	b);	/*!< in: ulint */
/*******************************************************//**
Subtracts a ulint from a dulint.
@return	a - b */
UNIV_INLINE
dulint
ut_dulint_subtract(
/*===============*/
	dulint	a,	/*!< in: dulint */
	ulint	b);	/*!< in: ulint, b <= a */
/*******************************************************//**
Subtracts a dulint from another. NOTE that the difference must be positive
and smaller that 4G.
@return	a - b */
UNIV_INLINE
ulint
ut_dulint_minus(
/*============*/
	dulint	a,	/*!< in: dulint; NOTE a must be >= b and at most
			2 to power 32 - 1 greater */
	dulint	b);	/*!< in: dulint */
/********************************************************//**
Rounds a dulint downward to a multiple of a power of 2.
@return	rounded value */
UNIV_INLINE
dulint
ut_dulint_align_down(
/*=================*/
	dulint	 n,		/*!< in: number to be rounded */
	ulint	 align_no);	/*!< in: align by this number which must be a
				power of 2 */
/********************************************************//**
Rounds a dulint upward to a multiple of a power of 2.
@return	rounded value */
UNIV_INLINE
dulint
ut_dulint_align_up(
/*===============*/
	dulint	 n,		/*!< in: number to be rounded */
	ulint	 align_no);	/*!< in: align by this number which must be a
				power of 2 */
/********************************************************//**
Rounds a dulint downward to a multiple of a power of 2.
@return	rounded value */
UNIV_INLINE
ib_uint64_t
ut_uint64_align_down(
/*=================*/
	ib_uint64_t	 n,		/*!< in: number to be rounded */
	ulint		 align_no);	/*!< in: align by this number
					which must be a power of 2 */
/********************************************************//**
Rounds ib_uint64_t upward to a multiple of a power of 2.
@return	rounded value */
UNIV_INLINE
ib_uint64_t
ut_uint64_align_up(
/*===============*/
	ib_uint64_t	 n,		/*!< in: number to be rounded */
	ulint		 align_no);	/*!< in: align by this number
					which must be a power of 2 */
/*******************************************************//**
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
/*******************************************************//**
Tests if two dulints are equal. */
#define UT_DULINT_EQ(D1, D2)	(((D1).low == (D2).low)\
						&& ((D1).high == (D2).high))
#ifdef notdefined
/************************************************************//**
Sort function for dulint arrays. */
UNIV_INTERN
void
ut_dulint_sort(
/*===========*/
	dulint*	arr,	/*!< in/out: array to be sorted */
	dulint*	aux_arr,/*!< in/out: auxiliary array (same size as arr) */
	ulint	low,	/*!< in: low bound of sort interval, inclusive */
	ulint	high);	/*!< in: high bound of sort interval, noninclusive */
#endif /* notdefined */

/*********************************************************//**
The following function rounds up a pointer to the nearest aligned address.
@return	aligned pointer */
UNIV_INLINE
void*
ut_align(
/*=====*/
	void*	ptr,		/*!< in: pointer */
	ulint	align_no);	/*!< in: align by this number */
/*********************************************************//**
The following function rounds down a pointer to the nearest
aligned address.
@return	aligned pointer */
UNIV_INLINE
void*
ut_align_down(
/*==========*/
	const void*	ptr,		/*!< in: pointer */
	ulint		align_no)	/*!< in: align by this number */
		__attribute__((const));
/*********************************************************//**
The following function computes the offset of a pointer from the nearest
aligned address.
@return	distance from aligned pointer */
UNIV_INLINE
ulint
ut_align_offset(
/*============*/
	const void*	ptr,		/*!< in: pointer */
	ulint		align_no)	/*!< in: align by this number */
			__attribute__((const));
/*****************************************************************//**
Gets the nth bit of a ulint.
@return	TRUE if nth bit is 1; 0th bit is defined to be the least significant */
UNIV_INLINE
ibool
ut_bit_get_nth(
/*===========*/
	ulint	a,	/*!< in: ulint */
	ulint	n);	/*!< in: nth bit requested */
/*****************************************************************//**
Sets the nth bit of a ulint.
@return	the ulint with the bit set as requested */
UNIV_INLINE
ulint
ut_bit_set_nth(
/*===========*/
	ulint	a,	/*!< in: ulint */
	ulint	n,	/*!< in: nth bit requested */
	ibool	val);	/*!< in: value for the bit to set */

#ifndef UNIV_NONINL
#include "ut0byte.ic"
#endif

#endif
