/*****************************************************************************

Copyright (c) 1994, 2011, Oracle and/or its affiliates. All Rights Reserved.

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
#include <string.h>
#ifndef UNIV_HOTBACKUP
# include "os0sync.h"

/** The total amount of memory currently allocated from the operating
system with os_mem_alloc_large() or malloc().  Does not count malloc()
if srv_use_sys_malloc is set.  Protected by ut_list_mutex. */
extern ulint		ut_total_allocated_memory;

/** Mutex protecting ut_total_allocated_memory and ut_mem_block_list */
extern os_fast_mutex_t	ut_list_mutex;
#endif /* !UNIV_HOTBACKUP */

/** Wrapper for memcpy(3).  Copy memory area when the source and
target are not overlapping.
* @param dest	in: copy to
* @param sour	in: copy from
* @param n	in: number of bytes to copy
* @return	dest */
UNIV_INLINE
void*
ut_memcpy(void* dest, const void* sour, ulint n);

/** Wrapper for memmove(3).  Copy memory area when the source and
target are overlapping.
* @param dest	in: copy to
* @param sour	in: copy from
* @param n	in: number of bytes to copy
* @return	dest */
UNIV_INLINE
void*
ut_memmove(void* dest, const void* sour, ulint n);

/** Wrapper for memcmp(3).  Compare memory areas.
* @param str1	in: first memory block to compare
* @param str2	in: second memory block to compare
* @param n	in: number of bytes to compare
* @return	negative, 0, or positive if str1 is smaller, equal,
		or greater than str2, respectively. */
UNIV_INLINE
int
ut_memcmp(const void* str1, const void* str2, ulint n);

/**********************************************************************//**
Initializes the mem block list at database startup. */
UNIV_INTERN
void
ut_mem_init(void);
/*=============*/

/**********************************************************************//**
Allocates memory.
@return	own: allocated memory */
UNIV_INTERN
void*
ut_malloc_low(
/*==========*/
	ulint	n,			/*!< in: number of bytes to allocate */
	ibool	assert_on_error)	/*!< in: if TRUE, we crash mysqld if
					the memory cannot be allocated */
	__attribute__((malloc));
/**********************************************************************//**
Allocates memory. */
#define ut_malloc(n) ut_malloc_low(n, TRUE)
/**********************************************************************//**
Frees a memory block allocated with ut_malloc. Freeing a NULL pointer is
a nop. */
UNIV_INTERN
void
ut_free(
/*====*/
	void* ptr);  /*!< in, own: memory block, can be NULL */
#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Implements realloc. This is needed by /pars/lexyy.cc. Otherwise, you should not
use this function because the allocation functions in mem0mem.h are the
recommended ones in InnoDB.

man realloc in Linux, 2004:

       realloc()  changes the size of the memory block pointed to
       by ptr to size bytes.  The contents will be  unchanged  to
       the minimum of the old and new sizes; newly allocated mem­
       ory will be uninitialized.  If ptr is NULL,  the	 call  is
       equivalent  to malloc(size); if size is equal to zero, the
       call is equivalent to free(ptr).	 Unless ptr is	NULL,  it
       must  have  been	 returned by an earlier call to malloc(),
       calloc() or realloc().

RETURN VALUE
       realloc() returns a pointer to the newly allocated memory,
       which is suitably aligned for any kind of variable and may
       be different from ptr, or NULL if the  request  fails.  If
       size  was equal to 0, either NULL or a pointer suitable to
       be passed to free() is returned.	 If realloc()  fails  the
       original	 block	is  left  untouched  - it is not freed or
       moved.
@return	own: pointer to new mem block or NULL */
UNIV_INTERN
void*
ut_realloc(
/*=======*/
	void*	ptr,	/*!< in: pointer to old block or NULL */
	ulint	size);	/*!< in: desired size */
/**********************************************************************//**
Frees in shutdown all allocated memory not freed yet. */
UNIV_INTERN
void
ut_free_all_mem(void);
/*=================*/
#endif /* !UNIV_HOTBACKUP */

/** Wrapper for strcpy(3).  Copy a NUL-terminated string.
* @param dest	in: copy to
* @param sour	in: copy from
* @return	dest */
UNIV_INLINE
char*
ut_strcpy(char* dest, const char* sour);

/** Wrapper for strlen(3).  Determine the length of a NUL-terminated string.
* @param str	in: string
* @return	length of the string in bytes, excluding the terminating NUL */
UNIV_INLINE
ulint
ut_strlen(const char* str);

/** Wrapper for strcmp(3).  Compare NUL-terminated strings.
* @param str1	in: first string to compare
* @param str2	in: second string to compare
* @return	negative, 0, or positive if str1 is smaller, equal,
		or greater than str2, respectively. */
UNIV_INLINE
int
ut_strcmp(const char* str1, const char* str2);

/**********************************************************************//**
Copies up to size - 1 characters from the NUL-terminated string src to
dst, NUL-terminating the result. Returns strlen(src), so truncation
occurred if the return value >= size.
@return	strlen(src) */
UNIV_INTERN
ulint
ut_strlcpy(
/*=======*/
	char*		dst,	/*!< in: destination buffer */
	const char*	src,	/*!< in: source buffer */
	ulint		size);	/*!< in: size of destination buffer */

/**********************************************************************//**
Like ut_strlcpy, but if src doesn't fit in dst completely, copies the last
(size - 1) bytes of src, not the first.
@return	strlen(src) */
UNIV_INTERN
ulint
ut_strlcpy_rev(
/*===========*/
	char*		dst,	/*!< in: destination buffer */
	const char*	src,	/*!< in: source buffer */
	ulint		size);	/*!< in: size of destination buffer */

/**********************************************************************//**
Return the number of times s2 occurs in s1. Overlapping instances of s2
are only counted once.
@return	the number of times s2 occurs in s1 */
UNIV_INTERN
ulint
ut_strcount(
/*========*/
	const char*	s1,	/*!< in: string to search in */
	const char*	s2);	/*!< in: string to search for */

/**********************************************************************//**
Replace every occurrence of s1 in str with s2. Overlapping instances of s1
are only replaced once.
@return	own: modified string, must be freed with mem_free() */
UNIV_INTERN
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
				freed with mem_free() */
	const char*	s1,	/* in: string 1 */
	const char*	s2,	/* in: string 2 */
	const char*	s3);	/* in: string 3 */

/**********************************************************************//**
Converts a raw binary data to a NUL-terminated hex string. The output is
truncated if there is not enough space in "hex", make sure "hex_size" is at
least (2 * raw_size + 1) if you do not want this to happen. Returns the
actual number of characters written to "hex" (including the NUL).
@return	number of chars written */
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
@return	number of bytes that were written */
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
