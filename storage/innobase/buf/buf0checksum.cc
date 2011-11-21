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
@file buf/buf0checksum.cc
Buffer pool checksum functions, also linked from /extra/innochecksum.cc

Created Aug 11, 2011 Vasil Dimov
*******************************************************/

#include "univ.i"
#include "fil0fil.h" /* FIL_* */
#include "ut0crc32.h" /* ut_crc32() */
#include "ut0rnd.h" /* ut_fold_binary() */

#ifndef UNIV_INNOCHECKSUM

#include "srv0srv.h" /* SRV_CHECKSUM_* */
#include "buf0types.h"

/** the macro MYSQL_SYSVAR_ENUM() requires "long unsigned int" and if we
use srv_checksum_algorithm_t here then we get a compiler error:
ha_innodb.cc:12251: error: cannot convert 'srv_checksum_algorithm_t*' to
  'long unsigned int*' in initialization */
UNIV_INTERN ulong	srv_checksum_algorithm = SRV_CHECKSUM_ALGORITHM_INNODB;

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
	const byte*	page)	/*!< in: buffer page */
{
	ib_uint32_t	checksum;

	/* Since the field FIL_PAGE_FILE_FLUSH_LSN, and in versions <= 4.1.x
	FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, are written outside the buffer pool
	to the first pages of data files, we have to skip them in the page
	checksum calculation.
	We must also skip the field FIL_PAGE_SPACE_OR_CHKSUM where the
	checksum is stored, and also the last 8 bytes of page because
	there we store the old formula checksum. */

	checksum = ut_crc32(page + FIL_PAGE_OFFSET,
			    FIL_PAGE_FILE_FLUSH_LSN - FIL_PAGE_OFFSET)
		^ ut_crc32(page + FIL_PAGE_DATA,
			   UNIV_PAGE_SIZE - FIL_PAGE_DATA
			   - FIL_PAGE_END_LSN_OLD_CHKSUM);

	return(checksum);
}

/********************************************************************//**
Calculates a page checksum which is stored to the page when it is written
to a file. Note that we must be careful to calculate the same value on
32-bit and 64-bit architectures.
@return	checksum */
UNIV_INTERN
ulint
buf_calc_page_new_checksum(
/*=======================*/
	const byte*	page)	/*!< in: buffer page */
{
	ulint checksum;

	/* Since the field FIL_PAGE_FILE_FLUSH_LSN, and in versions <= 4.1.x
	FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, are written outside the buffer pool
	to the first pages of data files, we have to skip them in the page
	checksum calculation.
	We must also skip the field FIL_PAGE_SPACE_OR_CHKSUM where the
	checksum is stored, and also the last 8 bytes of page because
	there we store the old formula checksum. */

	checksum = ut_fold_binary(page + FIL_PAGE_OFFSET,
				  FIL_PAGE_FILE_FLUSH_LSN - FIL_PAGE_OFFSET)
		+ ut_fold_binary(page + FIL_PAGE_DATA,
				 UNIV_PAGE_SIZE - FIL_PAGE_DATA
				 - FIL_PAGE_END_LSN_OLD_CHKSUM);
	checksum = checksum & 0xFFFFFFFFUL;

	return(checksum);
}

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
	const byte*	page)	/*!< in: buffer page */
{
	ulint checksum;

	checksum = ut_fold_binary(page, FIL_PAGE_FILE_FLUSH_LSN);

	checksum = checksum & 0xFFFFFFFFUL;

	return(checksum);
}

#ifndef UNIV_INNOCHECKSUM

/********************************************************************//**
Return a printable string describing the checksum algorithm.
@return	algorithm name */
UNIV_INTERN
const char*
buf_checksum_algorithm_name(
/*========================*/
	srv_checksum_algorithm_t	algo)	/*!< in: algorithm */
{
	switch (algo) {
	case SRV_CHECKSUM_ALGORITHM_CRC32:
	case SRV_CHECKSUM_ALGORITHM_STRICT_CRC32:
		return("crc32");
	case SRV_CHECKSUM_ALGORITHM_INNODB:
	case SRV_CHECKSUM_ALGORITHM_STRICT_INNODB:
		return("innodb");
	case SRV_CHECKSUM_ALGORITHM_NONE:
	case SRV_CHECKSUM_ALGORITHM_STRICT_NONE:
		return("none");
	}

	ut_error;
	return(NULL);
}

#endif /* !UNIV_INNOCHECKSUM */
