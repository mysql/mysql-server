/*****************************************************************************

Copyright (c) 1995, 2016, Oracle and/or its affiliates. All Rights Reserved.

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

/** Calculates the CRC32 checksum of a page. The value is stored to the page
when it is written to a file and also checked for a match when reading from
the file. When reading we allow both normal CRC32 and CRC-legacy-big-endian
variants. Note that we must be careful to calculate the same value on 32-bit
and 64-bit architectures.
@param[in]	page			buffer page (UNIV_PAGE_SIZE bytes)
@param[in]	use_legacy_big_endian	if true then use big endian
byteorder when converting byte strings to integers
@return checksum */
uint32_t
buf_calc_page_crc32(
	const byte*	page,
	bool		use_legacy_big_endian = false);

/********************************************************************//**
Calculates a page checksum which is stored to the page when it is written
to a file. Note that we must be careful to calculate the same value on
32-bit and 64-bit architectures.
@return checksum */
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
@return checksum */
ulint
buf_calc_page_old_checksum(
/*=======================*/
	const byte*	page);	/*!< in: buffer page */

/********************************************************************//**
Return a printable string describing the checksum algorithm.
@return algorithm name */
const char*
buf_checksum_algorithm_name(
/*========================*/
	srv_checksum_algorithm_t	algo);	/*!< in: algorithm */

extern ulong	srv_checksum_algorithm;
extern bool	legacy_big_endian_checksum;

#endif /* buf0checksum_h */
