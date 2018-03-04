/*****************************************************************************

Copyright (c) 1994, 2016, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/******************************************************************//**
@file include/ut0byte.h
Utilities for byte operations

Created 1/20/1994 Heikki Tuuri
***********************************************************************/

#ifndef ut0byte_h
#define ut0byte_h



#include "univ.i"

/*******************************************************//**
Creates a 64-bit integer out of two 32-bit integers.
@return created integer */
UNIV_INLINE
ib_uint64_t
ut_ull_create(
/*==========*/
	ulint	high,	/*!< in: high-order 32 bits */
	ulint	low)	/*!< in: low-order 32 bits */
	MY_ATTRIBUTE((const));

/** Rounds a 64-bit integer downward to a multiple of a power of 2.
@param[in]	n		number to be rounded
@param[in]	align_no	align by this number
@return rounded value */
UNIV_INLINE
ib_uint64_t
ut_uint64_align_down(
	ib_uint64_t	 n,
	ulint		 align_no);

/** Rounds ib_uint64_t upward to a multiple of a power of 2.
@param[in]	n		number to be rounded
@param[in]	align_no	align by this number
@return rounded value */
UNIV_INLINE
ib_uint64_t
ut_uint64_align_up(
	ib_uint64_t	 n,
	ulint		 align_no);

/** The following function rounds up a pointer to the nearest aligned address.
@param[in]	ptr		pointer
@param[in]	align_no	align by this number
@return aligned pointer */
UNIV_INLINE
void*
ut_align(
	const void*	ptr,
	ulint		align_no);

/*********************************************************//**
The following function rounds down a pointer to the nearest
aligned address.
@return aligned pointer */
UNIV_INLINE
void*
ut_align_down(
/*==========*/
	const void*	ptr,		/*!< in: pointer */
	ulint		align_no)	/*!< in: align by this number */
		MY_ATTRIBUTE((const));
/*********************************************************//**
The following function computes the offset of a pointer from the nearest
aligned address.
@return distance from aligned pointer */
UNIV_INLINE
ulint
ut_align_offset(
/*============*/
	const void*	ptr,		/*!< in: pointer */
	ulint		align_no)	/*!< in: align by this number */
			MY_ATTRIBUTE((const));

/** Gets the nth bit of a ulint.
@param[in]	a	ulint
@param[in]	n	nth bit requested
@return TRUE if nth bit is 1; 0th bit is defined to be the least significant */
UNIV_INLINE
ibool
ut_bit_get_nth(
	ulint	a,
	ulint	n);

/** Sets the nth bit of a ulint.
@param[in]	a	ulint
@param[in]	n	nth bit requested
@param[in]	val	value for the bit to set
@return the ulint with the bit set as requested */
UNIV_INLINE
ulint
ut_bit_set_nth(
	ulint	a,
	ulint	n,
	ibool	val);

#include "ut0byte.ic"

#endif
