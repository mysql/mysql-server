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

/********************************************************************//**
@file mem/mem0mem.cc
The memory management

Created 6/9/1994 Heikki Tuuri
*************************************************************************/

#include "ha_prototypes.h"

#include "mem0mem.h"
#ifdef UNIV_NONINL
#include "mem0mem.ic"
#endif

#include "buf0buf.h"
#include "srv0srv.h"
#include <stdarg.h>

/** Duplicates a NUL-terminated string, allocated from a memory heap.
@param[in]	heap,	memory heap where string is allocated
@param[in]	str)	string to be copied
@return own: a copy of the string */
char*
mem_heap_strdup(
	mem_heap_t*	heap,
	const char*	str)
{
	return(static_cast<char*>(mem_heap_dup(heap, str, strlen(str) + 1)));
}

/**********************************************************************//**
Duplicate a block of data, allocated from a memory heap.
@return own: a copy of the data */
void*
mem_heap_dup(
/*=========*/
	mem_heap_t*	heap,	/*!< in: memory heap where copy is allocated */
	const void*	data,	/*!< in: data to be copied */
	ulint		len)	/*!< in: length of data, in bytes */
{
	return(memcpy(mem_heap_alloc(heap, len), data, len));
}

/**********************************************************************//**
Concatenate two strings and return the result, using a memory heap.
@return own: the result */
char*
mem_heap_strcat(
/*============*/
	mem_heap_t*	heap,	/*!< in: memory heap where string is allocated */
	const char*	s1,	/*!< in: string 1 */
	const char*	s2)	/*!< in: string 2 */
{
	char*	s;
	ulint	s1_len = strlen(s1);
	ulint	s2_len = strlen(s2);

	s = static_cast<char*>(mem_heap_alloc(heap, s1_len + s2_len + 1));

	memcpy(s, s1, s1_len);
	memcpy(s + s1_len, s2, s2_len);

	s[s1_len + s2_len] = '\0';

	return(s);
}


/****************************************************************//**
Helper function for mem_heap_printf.
@return length of formatted string, including terminating NUL */
static
ulint
mem_heap_printf_low(
/*================*/
	char*		buf,	/*!< in/out: buffer to store formatted string
				in, or NULL to just calculate length */
	const char*	format,	/*!< in: format string */
	va_list		ap)	/*!< in: arguments */
{
	ulint 		len = 0;

	while (*format) {

		/* Does this format specifier have the 'l' length modifier. */
		ibool	is_long = FALSE;

		/* Length of one parameter. */
		size_t	plen;

		if (*format++ != '%') {
			/* Non-format character. */

			len++;

			if (buf) {
				*buf++ = *(format - 1);
			}

			continue;
		}

		if (*format == 'l') {
			is_long = TRUE;
			format++;
		}

		switch (*format++) {
		case 's':
			/* string */
			{
				char*	s = va_arg(ap, char*);

				/* "%ls" is a non-sensical format specifier. */
				ut_a(!is_long);

				plen = strlen(s);
				len += plen;

				if (buf) {
					memcpy(buf, s, plen);
					buf += plen;
				}
			}

			break;

		case 'u':
			/* unsigned int */
			{
				char		tmp[32];
				unsigned long	val;

				/* We only support 'long' values for now. */
				ut_a(is_long);

				val = va_arg(ap, unsigned long);

				plen = sprintf(tmp, "%lu", val);
				len += plen;

				if (buf) {
					memcpy(buf, tmp, plen);
					buf += plen;
				}
			}

			break;

		case '%':

			/* "%l%" is a non-sensical format specifier. */
			ut_a(!is_long);

			len++;

			if (buf) {
				*buf++ = '%';
			}

			break;

		default:
			ut_error;
		}
	}

	/* For the NUL character. */
	len++;

	if (buf) {
		*buf = '\0';
	}

	return(len);
}

/****************************************************************//**
A simple sprintf replacement that dynamically allocates the space for the
formatted string from the given heap. This supports a very limited set of
the printf syntax: types 's' and 'u' and length modifier 'l' (which is
required for the 'u' type).
@return heap-allocated formatted string */
char*
mem_heap_printf(
/*============*/
	mem_heap_t*	heap,	/*!< in: memory heap */
	const char*	format,	/*!< in: format string */
	...)
{
	va_list		ap;
	char*		str;
	ulint 		len;

	/* Calculate length of string */
	len = 0;
	va_start(ap, format);
	len = mem_heap_printf_low(NULL, format, ap);
	va_end(ap);

	/* Now create it for real. */
	str = static_cast<char*>(mem_heap_alloc(heap, len));
	va_start(ap, format);
	mem_heap_printf_low(str, format, ap);
	va_end(ap);

	return(str);
}

#ifdef UNIV_DEBUG
/** Validates the contents of a memory heap.
Checks a memory heap for consistency, prints the contents if any error
is detected. A fatal error is logged if an error is detected.
@param[in]	heap	Memory heap to validate. */
void
mem_heap_validate(
	const mem_heap_t*	heap)
{
	ulint	size = 0;

	for (const mem_block_t* block = heap;
		block != NULL;
		block = UT_LIST_GET_NEXT(list, block)) {

		mem_block_validate(block);

		switch (block->type) {
		case MEM_HEAP_DYNAMIC:
			break;
		case MEM_HEAP_BUFFER:
		case MEM_HEAP_BUFFER | MEM_HEAP_BTR_SEARCH:
			ut_ad(block->len <= UNIV_PAGE_SIZE);
			break;
		default:
			ut_error;
		}

		size += block->len;
	}

	ut_ad(size == heap->total_size);
}
#endif /* UNIV_DEBUG */

/***************************************************************//**
Creates a memory heap block where data can be allocated.
@return own: memory heap block, NULL if did not succeed (only possible
for MEM_HEAP_BTR_SEARCH type heaps) */
mem_block_t*
mem_heap_create_block_func(
/*=======================*/
	mem_heap_t*	heap,	/*!< in: memory heap or NULL if first block
				should be created */
	ulint		n,	/*!< in: number of bytes needed for user data */
#ifdef UNIV_DEBUG
	const char*	file_name,/*!< in: file name where created */
	ulint		line,	/*!< in: line where created */
#endif /* UNIV_DEBUG */
	ulint		type)	/*!< in: type of heap: MEM_HEAP_DYNAMIC or
				MEM_HEAP_BUFFER */
{
#ifndef UNIV_HOTBACKUP
	buf_block_t*	buf_block = NULL;
#endif /* !UNIV_HOTBACKUP */
	mem_block_t*	block;
	ulint		len;

	ut_ad((type == MEM_HEAP_DYNAMIC) || (type == MEM_HEAP_BUFFER)
	      || (type == MEM_HEAP_BUFFER + MEM_HEAP_BTR_SEARCH));

	if (heap != NULL) {
		mem_block_validate(heap);
		ut_d(mem_heap_validate(heap));
	}

	/* In dynamic allocation, calculate the size: block header + data. */
	len = MEM_BLOCK_HEADER_SIZE + MEM_SPACE_NEEDED(n);

#ifndef UNIV_HOTBACKUP
	if (type == MEM_HEAP_DYNAMIC || len < UNIV_PAGE_SIZE / 2) {

		ut_ad(type == MEM_HEAP_DYNAMIC || n <= MEM_MAX_ALLOC_IN_BUF);

		block = static_cast<mem_block_t*>(ut_malloc_nokey(len));
	} else {
		len = UNIV_PAGE_SIZE;

		if ((type & MEM_HEAP_BTR_SEARCH) && heap) {
			/* We cannot allocate the block from the
			buffer pool, but must get the free block from
			the heap header free block field */

			buf_block = static_cast<buf_block_t*>(heap->free_block);
			heap->free_block = NULL;

			if (UNIV_UNLIKELY(!buf_block)) {

				return(NULL);
			}
		} else {
			buf_block = buf_block_alloc(NULL);
		}

		block = (mem_block_t*) buf_block->frame;
	}

	if (block == NULL) {
		ib::fatal() << "Unable to allocate memory of size "
			<< len << ".";
	}

	block->buf_block = buf_block;
	block->free_block = NULL;
#else /* !UNIV_HOTBACKUP */
	len = MEM_BLOCK_HEADER_SIZE + MEM_SPACE_NEEDED(n);
	block = ut_malloc_nokey(len);
	ut_ad(block);
#endif /* !UNIV_HOTBACKUP */

	block->magic_n = MEM_BLOCK_MAGIC_N;
	ut_d(ut_strlcpy_rev(block->file_name, file_name,
			    sizeof(block->file_name)));
	ut_d(block->line = line);

	mem_block_set_len(block, len);
	mem_block_set_type(block, type);
	mem_block_set_free(block, MEM_BLOCK_HEADER_SIZE);
	mem_block_set_start(block, MEM_BLOCK_HEADER_SIZE);

	if (UNIV_UNLIKELY(heap == NULL)) {
		/* This is the first block of the heap. The field
		total_size should be initialized here */
		block->total_size = len;
	} else {
		/* Not the first allocation for the heap. This block's
		total_length field should be set to undefined. */
		ut_d(block->total_size = ULINT_UNDEFINED);
		UNIV_MEM_INVALID(&block->total_size,
				 sizeof block->total_size);

		heap->total_size += len;
	}

	ut_ad((ulint)MEM_BLOCK_HEADER_SIZE < len);

	return(block);
}

/***************************************************************//**
Adds a new block to a memory heap.
@return created block, NULL if did not succeed (only possible for
MEM_HEAP_BTR_SEARCH type heaps) */
mem_block_t*
mem_heap_add_block(
/*===============*/
	mem_heap_t*	heap,	/*!< in: memory heap */
	ulint		n)	/*!< in: number of bytes user needs */
{
	mem_block_t*	block;
	mem_block_t*	new_block;
	ulint		new_size;

	ut_d(mem_block_validate(heap));

	block = UT_LIST_GET_LAST(heap->base);

	/* We have to allocate a new block. The size is always at least
	doubled until the standard size is reached. After that the size
	stays the same, except in cases where the caller needs more space. */

	new_size = 2 * mem_block_get_len(block);

	if (heap->type != MEM_HEAP_DYNAMIC) {
		/* From the buffer pool we allocate buffer frames */
		ut_a(n <= MEM_MAX_ALLOC_IN_BUF);

		if (new_size > MEM_MAX_ALLOC_IN_BUF) {
			new_size = MEM_MAX_ALLOC_IN_BUF;
		}
	} else if (new_size > MEM_BLOCK_STANDARD_SIZE) {

		new_size = MEM_BLOCK_STANDARD_SIZE;
	}

	if (new_size < n) {
		new_size = n;
	}

	new_block = mem_heap_create_block(heap, new_size, heap->type,
					  heap->file_name, heap->line);
	if (new_block == NULL) {

		return(NULL);
	}

	/* Add the new block as the last block */

	UT_LIST_INSERT_AFTER(heap->base, block, new_block);

	return(new_block);
}

/******************************************************************//**
Frees a block from a memory heap. */
void
mem_heap_block_free(
/*================*/
	mem_heap_t*	heap,	/*!< in: heap */
	mem_block_t*	block)	/*!< in: block to free */
{
	ulint		type;
	ulint		len;
#ifndef UNIV_HOTBACKUP
	buf_block_t*	buf_block;

	buf_block = static_cast<buf_block_t*>(block->buf_block);
#endif /* !UNIV_HOTBACKUP */

	mem_block_validate(block);

	UT_LIST_REMOVE(heap->base, block);

	ut_ad(heap->total_size >= block->len);
	heap->total_size -= block->len;

	type = heap->type;
	len = block->len;
	block->magic_n = MEM_FREED_BLOCK_MAGIC_N;

	UNIV_MEM_ASSERT_W(block, len);

#ifndef UNIV_HOTBACKUP
	if (type == MEM_HEAP_DYNAMIC || len < UNIV_PAGE_SIZE / 2) {

		ut_ad(!buf_block);
		ut_free(block);
	} else {
		ut_ad(type & MEM_HEAP_BUFFER);

		buf_block_free(buf_block);
	}
#else /* !UNIV_HOTBACKUP */
	ut_free(block);
#endif /* !UNIV_HOTBACKUP */
}

#ifndef UNIV_HOTBACKUP
/******************************************************************//**
Frees the free_block field from a memory heap. */
void
mem_heap_free_block_free(
/*=====================*/
	mem_heap_t*	heap)	/*!< in: heap */
{
	if (UNIV_LIKELY_NULL(heap->free_block)) {

		buf_block_free(static_cast<buf_block_t*>(heap->free_block));

		heap->free_block = NULL;
	}
}
#endif /* !UNIV_HOTBACKUP */
