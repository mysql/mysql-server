/*****************************************************************************

Copyright (c) 1995, 2011, Oracle and/or its affiliates. All Rights Reserved.

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

/**************************************************//**
@file buf/buf0checksum.h
Buffer pool checksum functions, also linked from /extra/innochecksum.cc

Created Aug 11, 2011 Vasil Dimov
*******************************************************/

#ifndef buf0checksum_h
#define buf0checksum_h

#include "univ.i"

#ifndef UNIV_INNOCHECKSUM

#include "buf0types.h"

#endif /* !UNIV_INNOCHECKSUM */

/********************************************************************//**
Calculates a page CRC32 which is stored to the page when it is written
to a file. Note that we must be careful to calculate the same value on
32-bit and 64-bit architectures.
@return	checksum */
UNIV_INTERN
ib_uint32_t
buf_calc_page_crc32(
/*================*/
	const byte*	page);	/*!< in: buffer page */

/********************************************************************//**
Calculates a page checksum which is stored to the page when it is written
to a file. Note that we must be careful to calculate the same value on
32-bit and 64-bit architectures.
@return	checksum */
UNIV_INTERN
ulint
buf_calc_page_new_checksum(
/*=======================*/
	const byte*	page);	/*!< in: buffer page */

/********************************************************************//**
In versions < 4.0.14 and < 4.1.1 there was a bug that the checksum only
looked at the first few bytes of the page. This calculates that old
checksum.
NOTE: we must first store the new formula checksum to
FIL_PAGE_SPACE_OR_CHKSUM before calculating and storing this old checksum
because this takes that field as an input!
@return	checksum */
UNIV_INTERN
ulint
buf_calc_page_old_checksum(
/*=======================*/
	const byte*	page);	/*!< in: buffer page */

#ifndef UNIV_INNOCHECKSUM

/********************************************************************//**
Return a printable string describing the checksum algorithm.
@return	algorithm name */
UNIV_INTERN
const char*
buf_checksum_algorithm_name(
/*========================*/
	srv_checksum_algorithm_t	algo);	/*!< in: algorithm */

extern ulong	srv_checksum_algorithm;

#endif /* !UNIV_INNOCHECKSUM */

#endif /* buf0checksum_h */
