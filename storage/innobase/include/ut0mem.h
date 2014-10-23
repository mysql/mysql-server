/*****************************************************************************

Copyright (c) 1994, 2014, Oracle and/or its affiliates. All Rights Reserved.

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
ut_memmove(void* dest, const void* sour, ulint n);

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

/**********************************************************************//**
Like ut_strlcpy, but if src doesn't fit in dst completely, copies the last
(size - 1) bytes of src, not the first.
@return strlen(src) */
ulint
ut_strlcpy_rev(
/*===========*/
	char*		dst,	/*!< in: destination buffer */
	const char*	src,	/*!< in: source buffer */
	ulint		size);	/*!< in: size of destination buffer */

/**********************************************************************//**
Return the number of times s2 occurs in s1. Overlapping instances of s2
are only counted once.
@return the number of times s2 occurs in s1 */
ulint
ut_strcount(
/*========*/
	const char*	s1,	/*!< in: string to search in */
	const char*	s2);	/*!< in: string to search for */

/**********************************************************************//**
Replace every occurrence of s1 in str with s2. Overlapping instances of s1
are only replaced once.
@return own: modified string, must be freed with ut_free() */
char*
ut_strreplace(
/*==========*/
	const char*	str,	/*!< in: string to operate on */
	const char*	s1,	/*!< in: string to replace */
	const char*	s2);	/*!< in: string to replace s1 with */

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

/**********************************************************************//**
Converts a raw binary data to a NUL-terminated hex string. The output is
truncated if there is not enough space in "hex", make sure "hex_size" is at
least (2 * raw_size + 1) if you do not want this to happen. Returns the
actual number of characters written to "hex" (including the NUL).
@return number of chars written */
UNIV_INLINE
ulint
ut_raw_to_hex(
/*==========*/
	const void*	raw,		/*!< in: raw data */
	ulint		raw_size,	/*!< in: "raw" length in bytes */
	char*		hex,		/*!< out: hex string */
	ulint		hex_size);	/*!< in: "hex" size in bytes */

/*******************************************************************//**
Adds single quotes to the start and end of string and escapes any quotes
by doubling them. Returns the number of bytes that were written to "buf"
(including the terminating NUL). If buf_size is too small then the
trailing bytes from "str" are discarded.
@return number of bytes that were written */
UNIV_INLINE
ulint
ut_str_sql_format(
/*==============*/
	const char*	str,		/*!< in: string */
	ulint		str_len,	/*!< in: string length in bytes */
	char*		buf,		/*!< out: output buffer */
	ulint		buf_size);	/*!< in: output buffer size
					in bytes */

#ifndef UNIV_NONINL
#include "ut0mem.ic"
#endif

#endif
