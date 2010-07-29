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

/*******************************************************//**
Creates a 64-bit integer out of two 32-bit integers.
@return	created dulint */
UNIV_INLINE
ib_uint64_t
ut_ull_create(
/*==========*/
	ulint	high,	/*!< in: high-order 32 bits */
	ulint	low)	/*!< in: low-order 32 bits */
	__attribute__((const));

/********************************************************//**
Rounds a 64-bit integer downward to a multiple of a power of 2.
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
/*********************************************************//**
The following function rounds up a pointer to the nearest aligned address.
@return	aligned pointer */
UNIV_INLINE
void*
ut_align(
/*=====*/
	const void*	ptr,		/*!< in: pointer */
	ulint		align_no);	/*!< in: align by this number */
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
