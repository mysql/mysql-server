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

/******************************************************
The memory management

Created 6/9/1994 Heikki Tuuri
*******************************************************/

#ifndef mem0mem_h
#define mem0mem_h

#include "univ.i"
#include "ut0mem.h"
#include "ut0byte.h"
#include "ut0ut.h"
#include "ut0rnd.h"
#include "sync0sync.h"
#include "ut0lst.h"
#include "mach0data.h"

/* -------------------- MEMORY HEAPS ----------------------------- */

/* The info structure stored at the beginning of a heap block */
typedef struct mem_block_info_struct mem_block_info_t;

/* A block of a memory heap consists of the info structure
followed by an area of memory */
typedef mem_block_info_t	mem_block_t;

/* A memory heap is a nonempty linear list of memory blocks */
typedef mem_block_t	mem_heap_t;

/* Types of allocation for memory heaps: DYNAMIC means allocation from the
dynamic memory pool of the C compiler, BUFFER means allocation from the
buffer pool; the latter method is used for very big heaps */

#define MEM_HEAP_DYNAMIC	0	/* the most common type */
#define MEM_HEAP_BUFFER		1
#define MEM_HEAP_BTR_SEARCH	2	/* this flag can optionally be
					ORed to MEM_HEAP_BUFFER, in which
					case heap->free_block is used in
					some cases for memory allocations,
					and if it's NULL, the memory
					allocation functions can return
					NULL. */

/* The following start size is used for the first block in the memory heap if
the size is not specified, i.e., 0 is given as the parameter in the call of
create. The standard size is the maximum (payload) size of the blocks used for
allocations of small buffers. */

#define MEM_BLOCK_START_SIZE		64
#define MEM_BLOCK_STANDARD_SIZE		\
	(UNIV_PAGE_SIZE >= 16384 ? 8000 : MEM_MAX_ALLOC_IN_BUF)

/* If a memory heap is allowed to grow into the buffer pool, the following
is the maximum size for a single allocated buffer: */
#define MEM_MAX_ALLOC_IN_BUF		(UNIV_PAGE_SIZE - 200)

/**********************************************************************
Initializes the memory system. */
UNIV_INTERN
void
mem_init(
/*=====*/
	ulint	size);	/* in: common pool size in bytes */
/******************************************************************
Use this macro instead of the corresponding function! Macro for memory
heap creation. */

#define mem_heap_create(N)	mem_heap_create_func(\
		(N), MEM_HEAP_DYNAMIC, __FILE__, __LINE__)
/******************************************************************
Use this macro instead of the corresponding function! Macro for memory
heap creation. */

#define mem_heap_create_in_buffer(N)	mem_heap_create_func(\
		(N), MEM_HEAP_BUFFER, __FILE__, __LINE__)
/******************************************************************
Use this macro instead of the corresponding function! Macro for memory
heap creation. */

#define mem_heap_create_in_btr_search(N)	mem_heap_create_func(\
		(N), MEM_HEAP_BTR_SEARCH | MEM_HEAP_BUFFER,\
		__FILE__, __LINE__)

/******************************************************************
Use this macro instead of the corresponding function! Macro for memory
heap freeing. */

#define mem_heap_free(heap) mem_heap_free_func(\
					  (heap), __FILE__, __LINE__)
/*********************************************************************
NOTE: Use the corresponding macros instead of this function. Creates a
memory heap. For debugging purposes, takes also the file name and line as
arguments. */
UNIV_INLINE
mem_heap_t*
mem_heap_create_func(
/*=================*/
					/* out, own: memory heap, NULL if
					did not succeed (only possible for
					MEM_HEAP_BTR_SEARCH type heaps)*/
	ulint		n,		/* in: desired start block size,
					this means that a single user buffer
					of size n will fit in the block,
					0 creates a default size block */
	ulint		type,		/* in: heap type */
	const char*	file_name,	/* in: file name where created */
	ulint		line);		/* in: line where created */
/*********************************************************************
NOTE: Use the corresponding macro instead of this function. Frees the space
occupied by a memory heap. In the debug version erases the heap memory
blocks. */
UNIV_INLINE
void
mem_heap_free_func(
/*===============*/
	mem_heap_t*	heap,		/* in, own: heap to be freed */
	const char*	file_name,	/* in: file name where freed */
	ulint		line);		/* in: line where freed */
/*******************************************************************
Allocates and zero-fills n bytes of memory from a memory heap. */
UNIV_INLINE
void*
mem_heap_zalloc(
/*============*/
				/* out: allocated, zero-filled storage */
	mem_heap_t*	heap,	/* in: memory heap */
	ulint		n);	/* in: number of bytes; if the heap is allowed
				to grow into the buffer pool, this must be
				<= MEM_MAX_ALLOC_IN_BUF */
/*******************************************************************
Allocates n bytes of memory from a memory heap. */
UNIV_INLINE
void*
mem_heap_alloc(
/*===========*/
				/* out: allocated storage, NULL if did not
				succeed (only possible for
				MEM_HEAP_BTR_SEARCH type heaps) */
	mem_heap_t*	heap,	/* in: memory heap */
	ulint		n);	/* in: number of bytes; if the heap is allowed
				to grow into the buffer pool, this must be
				<= MEM_MAX_ALLOC_IN_BUF */
/*********************************************************************
Returns a pointer to the heap top. */
UNIV_INLINE
byte*
mem_heap_get_heap_top(
/*==================*/
				/* out: pointer to the heap top */
	mem_heap_t*	heap);	/* in: memory heap */
/*********************************************************************
Frees the space in a memory heap exceeding the pointer given. The
pointer must have been acquired from mem_heap_get_heap_top. The first
memory block of the heap is not freed. */
UNIV_INLINE
void
mem_heap_free_heap_top(
/*===================*/
	mem_heap_t*	heap,	/* in: heap from which to free */
	byte*		old_top);/* in: pointer to old top of heap */
/*********************************************************************
Empties a memory heap. The first memory block of the heap is not freed. */
UNIV_INLINE
void
mem_heap_empty(
/*===========*/
	mem_heap_t*	heap);	/* in: heap to empty */
/*********************************************************************
Returns a pointer to the topmost element in a memory heap.
The size of the element must be given. */
UNIV_INLINE
void*
mem_heap_get_top(
/*=============*/
				/* out: pointer to the topmost element */
	mem_heap_t*	heap,	/* in: memory heap */
	ulint		n);	/* in: size of the topmost element */
/*********************************************************************
Frees the topmost element in a memory heap.
The size of the element must be given. */
UNIV_INLINE
void
mem_heap_free_top(
/*==============*/
	mem_heap_t*	heap,	/* in: memory heap */
	ulint		n);	/* in: size of the topmost element */
/*********************************************************************
Returns the space in bytes occupied by a memory heap. */
UNIV_INLINE
ulint
mem_heap_get_size(
/*==============*/
	mem_heap_t*	heap);		/* in: heap */
/******************************************************************
Use this macro instead of the corresponding function!
Macro for memory buffer allocation */

#define mem_zalloc(N)	memset(mem_alloc(N), 0, (N));

#define mem_alloc(N)	mem_alloc_func((N), NULL, __FILE__, __LINE__)
#define mem_alloc2(N,S)	mem_alloc_func((N), (S), __FILE__, __LINE__)
/*******************************************************************
NOTE: Use the corresponding macro instead of this function.
Allocates a single buffer of memory from the dynamic memory of
the C compiler. Is like malloc of C. The buffer must be freed
with mem_free. */
UNIV_INLINE
void*
mem_alloc_func(
/*===========*/
					/* out, own: free storage */
	ulint		n,		/* in: requested size in bytes */
	ulint*		size,		/* out: allocated size in bytes,
					or NULL */
	const char*	file_name,	/* in: file name where created */
	ulint		line);		/* in: line where created */

/******************************************************************
Use this macro instead of the corresponding function!
Macro for memory buffer freeing */

#define mem_free(PTR)	mem_free_func((PTR), __FILE__, __LINE__)
/*******************************************************************
NOTE: Use the corresponding macro instead of this function.
Frees a single buffer of storage from
the dynamic memory of C compiler. Similar to free of C. */
UNIV_INLINE
void
mem_free_func(
/*==========*/
	void*		ptr,		/* in, own: buffer to be freed */
	const char*	file_name,	/* in: file name where created */
	ulint		line		/* in: line where created */
);

/**************************************************************************
Duplicates a NUL-terminated string. */
UNIV_INLINE
char*
mem_strdup(
/*=======*/
				/* out, own: a copy of the string,
				must be deallocated with mem_free */
	const char*	str);	/* in: string to be copied */
/**************************************************************************
Makes a NUL-terminated copy of a nonterminated string. */
UNIV_INLINE
char*
mem_strdupl(
/*========*/
				/* out, own: a copy of the string,
				must be deallocated with mem_free */
	const char*	str,	/* in: string to be copied */
	ulint		len);	/* in: length of str, in bytes */

/**************************************************************************
Duplicates a NUL-terminated string, allocated from a memory heap. */
UNIV_INTERN
char*
mem_heap_strdup(
/*============*/
				/* out, own: a copy of the string */
	mem_heap_t*	heap,	/* in: memory heap where string is allocated */
	const char*	str);	/* in: string to be copied */
/**************************************************************************
Makes a NUL-terminated copy of a nonterminated string,
allocated from a memory heap. */
UNIV_INLINE
char*
mem_heap_strdupl(
/*=============*/
				/* out, own: a copy of the string */
	mem_heap_t*	heap,	/* in: memory heap where string is allocated */
	const char*	str,	/* in: string to be copied */
	ulint		len);	/* in: length of str, in bytes */

/**************************************************************************
Concatenate two strings and return the result, using a memory heap. */
UNIV_INTERN
char*
mem_heap_strcat(
/*============*/
				/* out, own: the result */
	mem_heap_t*	heap,	/* in: memory heap where string is allocated */
	const char*	s1,	/* in: string 1 */
	const char*	s2);	/* in: string 2 */

/**************************************************************************
Duplicate a block of data, allocated from a memory heap. */
UNIV_INTERN
void*
mem_heap_dup(
/*=========*/
				/* out, own: a copy of the data */
	mem_heap_t*	heap,	/* in: memory heap where copy is allocated */
	const void*	data,	/* in: data to be copied */
	ulint		len);	/* in: length of data, in bytes */

/**************************************************************************
Concatenate two memory blocks and return the result, using a memory heap. */
UNIV_INTERN
void*
mem_heap_cat(
/*=========*/
				/* out, own: the result */
	mem_heap_t*	heap,	/* in: memory heap where result is allocated */
	const void*	b1,	/* in: block 1 */
	ulint		len1,	/* in: length of b1, in bytes */
	const void*	b2,	/* in: block 2 */
	ulint		len2);	/* in: length of b2, in bytes */

/********************************************************************
A simple (s)printf replacement that dynamically allocates the space for the
formatted string from the given heap. This supports a very limited set of
the printf syntax: types 's' and 'u' and length modifier 'l' (which is
required for the 'u' type). */
UNIV_INTERN
char*
mem_heap_printf(
/*============*/
				/* out: heap-allocated formatted string */
	mem_heap_t*	heap,	/* in: memory heap */
	const char*	format,	/* in: format string */
	...) __attribute__ ((format (printf, 2, 3)));

#ifdef MEM_PERIODIC_CHECK
/**********************************************************************
Goes through the list of all allocated mem blocks, checks their magic
numbers, and reports possible corruption. */
UNIV_INTERN
void
mem_validate_all_blocks(void);
/*=========================*/
#endif

/*#######################################################################*/

/* The info header of a block in a memory heap */

struct mem_block_info_struct {
	ulint	magic_n;/* magic number for debugging */
	char	file_name[8];/* file name where the mem heap was created */
	ulint	line;	/* line number where the mem heap was created */
	UT_LIST_BASE_NODE_T(mem_block_t) base; /* In the first block in the
			the list this is the base node of the list of blocks;
			in subsequent blocks this is undefined */
	UT_LIST_NODE_T(mem_block_t) list; /* This contains pointers to next
			and prev in the list. The first block allocated
			to the heap is also the first block in this list,
			though it also contains the base node of the list. */
	ulint	len;	/* physical length of this block in bytes */
	ulint	type;	/* type of heap: MEM_HEAP_DYNAMIC, or
			MEM_HEAP_BUF possibly ORed to MEM_HEAP_BTR_SEARCH */
	ulint	free;	/* offset in bytes of the first free position for
			user data in the block */
	ulint	start;	/* the value of the struct field 'free' at the
			creation of the block */
	void*	free_block;
			/* if the MEM_HEAP_BTR_SEARCH bit is set in type,
			and this is the heap root, this can contain an
			allocated buffer frame, which can be appended as a
			free block to the heap, if we need more space;
			otherwise, this is NULL */
	void*	buf_block;
			/* if this block has been allocated from the buffer
			pool, this contains the buf_block_t handle;
			otherwise, this is NULL */
#ifdef MEM_PERIODIC_CHECK
	UT_LIST_NODE_T(mem_block_t) mem_block_list;
			/* List of all mem blocks allocated; protected
			by the mem_comm_pool mutex */
#endif
};

#define MEM_BLOCK_MAGIC_N	764741555
#define MEM_FREED_BLOCK_MAGIC_N	547711122

/* Header size for a memory heap block */
#define MEM_BLOCK_HEADER_SIZE	ut_calc_align(sizeof(mem_block_info_t),\
							UNIV_MEM_ALIGNMENT)
#include "mem0dbg.h"

#ifndef UNIV_NONINL
#include "mem0mem.ic"
#endif

#endif
