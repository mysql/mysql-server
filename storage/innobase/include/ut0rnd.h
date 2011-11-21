/*****************************************************************************

Copyright (c) 1994, 2009, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/******************************************************************//**
@file include/ut0rnd.h
Random numbers and hashing

Created 1/20/1994 Heikki Tuuri
***********************************************************************/

#ifndef ut0rnd_h
#define ut0rnd_h

#include "univ.i"

#ifndef UNIV_INNOCHECKSUM

#include "ut0byte.h"

/** The 'character code' for end of field or string (used
in folding records */
#define UT_END_OF_FIELD		257

/********************************************************//**
This is used to set the random number seed. */
UNIV_INLINE
void
ut_rnd_set_seed(
/*============*/
	ulint	 seed);		 /*!< in: seed */
/********************************************************//**
The following function generates a series of 'random' ulint integers.
@return	the next 'random' number */
UNIV_INLINE
ulint
ut_rnd_gen_next_ulint(
/*==================*/
	ulint	rnd);	/*!< in: the previous random number value */
/*********************************************************//**
The following function generates 'random' ulint integers which
enumerate the value space (let there be N of them) of ulint integers
in a pseudo-random fashion. Note that the same integer is repeated
always after N calls to the generator.
@return	the 'random' number */
UNIV_INLINE
ulint
ut_rnd_gen_ulint(void);
/*==================*/
/********************************************************//**
Generates a random integer from a given interval.
@return	the 'random' number */
UNIV_INLINE
ulint
ut_rnd_interval(
/*============*/
	ulint	low,	/*!< in: low limit; can generate also this value */
	ulint	high);	/*!< in: high limit; can generate also this value */
/*********************************************************//**
Generates a random iboolean value.
@return	the random value */
UNIV_INLINE
ibool
ut_rnd_gen_ibool(void);
/*=================*/
/*******************************************************//**
The following function generates a hash value for a ulint integer
to a hash table of size table_size, which should be a prime or some
random number to work reliably.
@return	hash value */
UNIV_INLINE
ulint
ut_hash_ulint(
/*==========*/
	ulint	 key,		/*!< in: value to be hashed */
	ulint	 table_size);	/*!< in: hash table size */
/*************************************************************//**
Folds a 64-bit integer.
@return	folded value */
UNIV_INLINE
ulint
ut_fold_ull(
/*========*/
	ib_uint64_t	d)	/*!< in: 64-bit integer */
	__attribute__((const));
/*************************************************************//**
Folds a character string ending in the null character.
@return	folded value */
UNIV_INLINE
ulint
ut_fold_string(
/*===========*/
	const char*	str)	/*!< in: null-terminated string */
	__attribute__((pure));
/***********************************************************//**
Looks for a prime number slightly greater than the given argument.
The prime is chosen so that it is not near any power of 2.
@return	prime */
UNIV_INTERN
ulint
ut_find_prime(
/*==========*/
	ulint	n)	/*!< in: positive number > 100 */
	__attribute__((const));

#endif /* !UNIV_INNOCHECKSUM */

/*************************************************************//**
Folds a pair of ulints.
@return	folded value */
UNIV_INLINE
ulint
ut_fold_ulint_pair(
/*===============*/
	ulint	n1,	/*!< in: ulint */
	ulint	n2)	/*!< in: ulint */
	__attribute__((const));
/*************************************************************//**
Folds a binary string.
@return	folded value */
UNIV_INLINE
ulint
ut_fold_binary(
/*===========*/
	const byte*	str,	/*!< in: string of bytes */
	ulint		len)	/*!< in: length */
	__attribute__((pure));


#ifndef UNIV_NONINL
#include "ut0rnd.ic"
#endif

#endif
