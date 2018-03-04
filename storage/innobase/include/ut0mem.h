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

/*******************************************************************//**
@file include/ut0mem.h
Memory primitives

Created 5/30/1994 Heikki Tuuri
************************************************************************/

#ifndef ut0mem_h
#define ut0mem_h

#include "univ.i"
#ifndef UNIV_HOTBACKUP
# include "os0event.h"
# include "ut0mutex.h"
#endif /* !UNIV_HOTBACKUP */

/** Wrapper for memcpy(3).  Copy memory area when the source and
target are not overlapping.
@param[in,out]	dest	copy to
@param[in]	src	copy from
@param[in]	n	number of bytes to copy
@return dest */
UNIV_INLINE
void*
ut_memcpy(void* dest, const void* src, ulint n);

/** Wrapper for memmove(3).  Copy memory area when the source and
target are overlapping.
@param[in,out]	dest	Move to
@param[in]	src	Move from
@param[in]	n	number of bytes to move
@return dest */
UNIV_INLINE
void*
ut_memmove(void* dest, const void* src, ulint n);

/** Wrapper for memcmp(3).  Compare memory areas.
@param[in]	str1	first memory block to compare
@param[in]	str2	second memory block to compare
@param[in]	n	number of bytes to compare
@return negative, 0, or positive if str1 is smaller, equal,
		or greater than str2, respectively. */
UNIV_INLINE
int
ut_memcmp(const void* str1, const void* str2, ulint n);

/** Wrapper for strcpy(3).  Copy a NUL-terminated string.
@param[in,out]	dest	Destination to copy to
@param[in]	src	Source to copy from
@return dest */
UNIV_INLINE
char*
ut_strcpy(char* dest, const char* src);

/** Wrapper for strlen(3).  Determine the length of a NUL-terminated string.
@param[in]	str	string
@return length of the string in bytes, excluding the terminating NUL */
UNIV_INLINE
ulint
ut_strlen(const char* str);

/** Wrapper for strcmp(3).  Compare NUL-terminated strings.
@param[in]	str1	first string to compare
@param[in]	str2	second string to compare
@return negative, 0, or positive if str1 is smaller, equal,
		or greater than str2, respectively. */
UNIV_INLINE
int
ut_strcmp(const char* str1, const char* str2);

/**********************************************************************//**
Copies up to size - 1 characters from the NUL-terminated string src to
dst, NUL-terminating the result. Returns strlen(src), so truncation
occurred if the return value >= size.
@return strlen(src) */
ulint
ut_strlcpy(
/*=======*/
	char*		dst,	/*!< in: destination buffer */
	const char*	src,	/*!< in: source buffer */
	ulint		size);	/*!< in: size of destination buffer */

/********************************************************************
Concatenate 3 strings.*/
char*
ut_str3cat(
/*=======*/
				/* out, own: concatenated string, must be
				freed with ut_free() */
	const char*	s1,	/* in: string 1 */
	const char*	s2,	/* in: string 2 */
	const char*	s3);	/* in: string 3 */

/** Converts a raw binary data to a NUL-terminated hex string. The output is
truncated if there is not enough space in "hex", make sure "hex_size" is at
least (2 * raw_size + 1) if you do not want this to happen. Returns the actual
number of characters written to "hex" (including the NUL).
@param[in]	raw		raw data
@param[in]	raw_size	"raw" length in bytes
@param[out]	hex		hex string
@param[in]	hex_size	"hex" size in bytes
@return number of chars written */
UNIV_INLINE
ulint
ut_raw_to_hex(
	const void*	raw,
	ulint		raw_size,
	char*		hex,
	ulint		hex_size);

/** Adds single quotes to the start and end of string and escapes any quotes by
doubling them. Returns the number of bytes that were written to "buf"
(including the terminating NUL). If buf_size is too small then the trailing
bytes from "str" are discarded.
@param[in]	str		string
@param[in]	str_len		string length in bytes
@param[out]	buf		output buffer
@param[in]	buf_size	output buffer size in bytes
@return number of bytes that were written */
UNIV_INLINE
ulint
ut_str_sql_format(
	const char*	str,
	ulint		str_len,
	char*		buf,
	ulint		buf_size);

#include "ut0mem.ic"

#endif
