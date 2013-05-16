/*****************************************************************************

Copyright (c) 1994, 2013, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/mem0mem.h
The memory management

Created 6/9/1994 Heikki Tuuri
*******************************************************/

#ifndef mem0mem_h
#define mem0mem_h

#include "univ.i"
#include "ut0mem.h"
#include "ut0byte.h"
#include "ut0rnd.h"
#ifndef UNIV_HOTBACKUP
# include "sync0sync.h"
#endif /* UNIV_HOTBACKUP */
#include "ut0lst.h"
#include "mach0data.h"

/* -------------------- MEMORY HEAPS ----------------------------- */

/* A block of a memory heap consists of the info structure
followed by an area of memory */
typedef struct mem_block_info_t	mem_block_t;

/* A memory heap is a nonempty linear list of memory blocks */
typedef mem_block_t		mem_heap_t;

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

/* Different type of heaps in terms of which datastructure is using them */
#define MEM_HEAP_FOR_BTR_SEARCH		(MEM_HEAP_BTR_SEARCH | MEM_HEAP_BUFFER)
#define MEM_HEAP_FOR_PAGE_HASH		(MEM_HEAP_DYNAMIC)
#define MEM_HEAP_FOR_RECV_SYS		(MEM_HEAP_BUFFER)
#define MEM_HEAP_FOR_LOCK_HEAP		(MEM_HEAP_BUFFER)

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

/******************************************************************//**
Initializes the memory system. */
UNIV_INTERN
void
mem_init(
/*=====*/
	ulint	size);	/*!< in: common pool size in bytes */
/******************************************************************//**
Closes the memory system. */
UNIV_INTERN
void
mem_close(void);
/*===========*/

/**************************************************************//**
Use this macro instead of the corresponding function! Macro for memory
heap creation. */

#define mem_heap_create(N)	mem_heap_create_func(\
		(N), MEM_HEAP_DYNAMIC, __FILE__, __LINE__)
/**************************************************************//**
Use this macro instead of the corresponding function! Macro for memory
heap creation. */

#define mem_heap_create_typed(N, T)	mem_heap_create_func(\
		(N), (T), __FILE__, __LINE__)
/**************************************************************//**
Use this macro instead of the corresponding function! Macro for memory
heap freeing. */

#define mem_heap_free(heap) mem_heap_free_func(\
					  (heap), __FILE__, __LINE__)
/*****************************************************************//**
NOTE: Use the corresponding macros instead of this function. Creates a
memory heap. For debugging purposes, takes also the file name and line as
arguments.
@return own: memory heap, NULL if did not succeed (only possible for
MEM_HEAP_BTR_SEARCH type heaps) */
UNIV_INLINE
mem_heap_t*
mem_heap_create_func(
/*=================*/
	ulint		n,		/*!< in: desired start block size,
					this means that a single user buffer
					of size n will fit in the block,
					0 creates a default size block */
	ulint		type,		/*!< in: heap type */
	const char*	file_name,	/*!< in: file name where created */
	ulint		line);		/*!< in: line where created */
/*****************************************************************//**
NOTE: Use the corresponding macro instead of this function. Frees the space
occupied by a memory heap. In the debug version erases the heap memory
blocks. */
UNIV_INLINE
void
mem_heap_free_func(
/*===============*/
	mem_heap_t*	heap,		/*!< in, own: heap to be freed */
	const char*	file_name,	/*!< in: file name where freed */
	ulint		line);		/*!< in: line where freed */
/***************************************************************//**
Allocates and zero-fills n bytes of memory from a memory heap.
@return	allocated, zero-filled storage */
UNIV_INLINE
void*
mem_heap_zalloc(
/*============*/
	mem_heap_t*	heap,	/*!< in: memory heap */
	ulint		n);	/*!< in: number of bytes; if the heap is allowed
				to grow into the buffer pool, this must be
				<= MEM_MAX_ALLOC_IN_BUF */
/***************************************************************//**
Allocates n bytes of memory from a memory heap.
@return allocated storage, NULL if did not succeed (only possible for
MEM_HEAP_BTR_SEARCH type heaps) */
UNIV_INLINE
void*
mem_heap_alloc(
/*===========*/
	mem_heap_t*	heap,	/*!< in: memory heap */
	ulint		n);	/*!< in: number of bytes; if the heap is allowed
				to grow into the buffer pool, this must be
				<= MEM_MAX_ALLOC_IN_BUF */
/*****************************************************************//**
Returns a pointer to the heap top.
@return	pointer to the heap top */
UNIV_INLINE
byte*
mem_heap_get_heap_top(
/*==================*/
	mem_heap_t*	heap);	/*!< in: memory heap */
/*****************************************************************//**
Frees the space in a memory heap exceeding the pointer given. The
pointer must have been acquired from mem_heap_get_heap_top. The first
memory block of the heap is not freed. */
UNIV_INLINE
void
mem_heap_free_heap_top(
/*===================*/
	mem_heap_t*	heap,	/*!< in: heap from which to free */
	byte*		old_top);/*!< in: pointer to old top of heap */
/*****************************************************************//**
Empties a memory heap. The first memory block of the heap is not freed. */
UNIV_INLINE
void
mem_heap_empty(
/*===========*/
	mem_heap_t*	heap);	/*!< in: heap to empty */
/*****************************************************************//**
Returns a pointer to the topmost element in a memory heap.
The size of the element must be given.
@return	pointer to the topmost element */
UNIV_INLINE
void*
mem_heap_get_top(
/*=============*/
	mem_heap_t*	heap,	/*!< in: memory heap */
	ulint		n);	/*!< in: size of the topmost element */
/*****************************************************************//**
Checks if a given chunk of memory is the topmost element stored in the
heap. If this is the case, then calling mem_heap_free_top() would free
that element from the heap.
@return	true if topmost */
UNIV_INLINE
bool
mem_heap_is_top(
/*============*/
	mem_heap_t*	heap,	/*!< in: memory heap */
	const void*	buf,	/*!< in: presumed topmost element */
	ulint		buf_sz)	/*!< in: size of buf in bytes */
	__attribute__((warn_unused_result));
/*****************************************************************//**
Allocate a new chunk of memory from a memory heap, possibly discarding
the topmost element. If the memory chunk specified with (top, top_sz)
is the topmost element, then it will be discarded, otherwise it will
be left untouched and this function will be equivallent to
mem_heap_alloc().
@return allocated storage, NULL if did not succeed (only possible for
MEM_HEAP_BTR_SEARCH type heaps) */
UNIV_INLINE
void*
mem_heap_replace(
/*=============*/
	mem_heap_t*	heap,	/*!< in/out: memory heap */
	const void*	top,	/*!< in: chunk to discard if possible */
	ulint		top_sz,	/*!< in: size of top in bytes */
	ulint		new_sz);/*!< in: desired size of the new chunk */
/*****************************************************************//**
Allocate a new chunk of memory from a memory heap, possibly discarding
the topmost element and then copy the specified data to it. If the memory
chunk specified with (top, top_sz) is the topmost element, then it will be
discarded, otherwise it will be left untouched and this function will be
equivallent to mem_heap_dup().
@return allocated storage, NULL if did not succeed (only possible for
MEM_HEAP_BTR_SEARCH type heaps) */
UNIV_INLINE
void*
mem_heap_dup_replace(
/*=================*/
	mem_heap_t*	heap,	/*!< in/out: memory heap */
	const void*	top,	/*!< in: chunk to discard if possible */
	ulint		top_sz,	/*!< in: size of top in bytes */
	const void*	data,	/*!< in: new data to duplicate */
	ulint		data_sz);/*!< in: size of data in bytes */
/*****************************************************************//**
Allocate a new chunk of memory from a memory heap, possibly discarding
the topmost element and then copy the specified string to it. If the memory
chunk specified with (top, top_sz) is the topmost element, then it will be
discarded, otherwise it will be left untouched and this function will be
equivallent to mem_heap_strdup().
@return allocated string, NULL if did not succeed (only possible for
MEM_HEAP_BTR_SEARCH type heaps) */
UNIV_INLINE
char*
mem_heap_strdup_replace(
/*====================*/
	mem_heap_t*	heap,	/*!< in/out: memory heap */
	const void*	top,	/*!< in: chunk to discard if possible */
	ulint		top_sz,	/*!< in: size of top in bytes */
	const char*	str);	/*!< in: new data to duplicate */
/*****************************************************************//**
Frees the topmost element in a memory heap.
The size of the element must be given. */
UNIV_INLINE
void
mem_heap_free_top(
/*==============*/
	mem_heap_t*	heap,	/*!< in: memory heap */
	ulint		n);	/*!< in: size of the topmost element */
/*****************************************************************//**
Returns the space in bytes occupied by a memory heap. */
UNIV_INLINE
ulint
mem_heap_get_size(
/*==============*/
	mem_heap_t*	heap);		/*!< in: heap */
/**************************************************************//**
Use this macro instead of the corresponding function!
Macro for memory buffer allocation */

#define mem_zalloc(N)	memset(mem_alloc(N), 0, (N))

#define mem_alloc(N)	mem_alloc_func((N), NULL, __FILE__, __LINE__)
#define mem_alloc2(N,S)	mem_alloc_func((N), (S), __FILE__, __LINE__)
/***************************************************************//**
NOTE: Use the corresponding macro instead of this function.
Allocates a single buffer of memory from the dynamic memory of
the C compiler. Is like malloc of C. The buffer must be freed
with mem_free.
@return	own: free storage */
UNIV_INLINE
void*
mem_alloc_func(
/*===========*/
	ulint		n,		/*!< in: requested size in bytes */
	ulint*		size,		/*!< out: allocated size in bytes,
					or NULL */
	const char*	file_name,	/*!< in: file name where created */
	ulint		line);		/*!< in: line where created */

/**************************************************************//**
Use this macro instead of the corresponding function!
Macro for memory buffer freeing */

#define mem_free(PTR)	mem_free_func((PTR), __FILE__, __LINE__)
/***************************************************************//**
NOTE: Use the corresponding macro instead of this function.
Frees a single buffer of storage from
the dynamic memory of C compiler. Similar to free of C. */
UNIV_INLINE
void
mem_free_func(
/*==========*/
	void*		ptr,		/*!< in, own: buffer to be freed */
	const char*	file_name,	/*!< in: file name where created */
	ulint		line);		/*!< in: line where created */

/**********************************************************************//**
Duplicates a NUL-terminated string.
@return	own: a copy of the string, must be deallocated with mem_free */
UNIV_INLINE
char*
mem_strdup(
/*=======*/
	const char*	str);	/*!< in: string to be copied */
/**********************************************************************//**
Makes a NUL-terminated copy of a nonterminated string.
@return	own: a copy of the string, must be deallocated with mem_free */
UNIV_INLINE
char*
mem_strdupl(
/*========*/
	const char*	str,	/*!< in: string to be copied */
	ulint		len);	/*!< in: length of str, in bytes */

/**********************************************************************//**
Duplicates a NUL-terminated string, allocated from a memory heap.
@return	own: a copy of the string */
UNIV_INTERN
char*
mem_heap_strdup(
/*============*/
	mem_heap_t*	heap,	/*!< in: memory heap where string is allocated */
	const char*	str);	/*!< in: string to be copied */
/**********************************************************************//**
Makes a NUL-terminated copy of a nonterminated string,
allocated from a memory heap.
@return	own: a copy of the string */
UNIV_INLINE
char*
mem_heap_strdupl(
/*=============*/
	mem_heap_t*	heap,	/*!< in: memory heap where string is allocated */
	const char*	str,	/*!< in: string to be copied */
	ulint		len);	/*!< in: length of str, in bytes */

/**********************************************************************//**
Concatenate two strings and return the result, using a memory heap.
@return	own: the result */
UNIV_INTERN
char*
mem_heap_strcat(
/*============*/
	mem_heap_t*	heap,	/*!< in: memory heap where string is allocated */
	const char*	s1,	/*!< in: string 1 */
	const char*	s2);	/*!< in: string 2 */

/**********************************************************************//**
Duplicate a block of data, allocated from a memory heap.
@return	own: a copy of the data */
UNIV_INTERN
void*
mem_heap_dup(
/*=========*/
	mem_heap_t*	heap,	/*!< in: memory heap where copy is allocated */
	const void*	data,	/*!< in: data to be copied */
	ulint		len);	/*!< in: length of data, in bytes */

/****************************************************************//**
A simple sprintf replacement that dynamically allocates the space for the
formatted string from the given heap. This supports a very limited set of
the printf syntax: types 's' and 'u' and length modifier 'l' (which is
required for the 'u' type).
@return	heap-allocated formatted string */
UNIV_INTERN
char*
mem_heap_printf(
/*============*/
	mem_heap_t*	heap,	/*!< in: memory heap */
	const char*	format,	/*!< in: format string */
	...) __attribute__ ((format (printf, 2, 3)));

#ifdef MEM_PERIODIC_CHECK
/******************************************************************//**
Goes through the list of all allocated mem blocks, checks their magic
numbers, and reports possible corruption. */
UNIV_INTERN
void
mem_validate_all_blocks(void);
/*=========================*/
#endif

/*#######################################################################*/

/** The info structure stored at the beginning of a heap block */
struct mem_block_info_t {
	ulint	magic_n;/* magic number for debugging */
	char	file_name[8];/* file name where the mem heap was created */
	ulint	line;	/*!< line number where the mem heap was created */
	UT_LIST_BASE_NODE_T(mem_block_t) base; /* In the first block in the
			the list this is the base node of the list of blocks;
			in subsequent blocks this is undefined */
	UT_LIST_NODE_T(mem_block_t) list; /* This contains pointers to next
			and prev in the list. The first block allocated
			to the heap is also the first block in this list,
			though it also contains the base node of the list. */
	ulint	len;	/*!< physical length of this block in bytes */
	ulint	total_size; /*!< physical length in bytes of all blocks
			in the heap. This is defined only in the base
			node and is set to ULINT_UNDEFINED in others. */
	ulint	type;	/*!< type of heap: MEM_HEAP_DYNAMIC, or
			MEM_HEAP_BUF possibly ORed to MEM_HEAP_BTR_SEARCH */
	ulint	free;	/*!< offset in bytes of the first free position for
			user data in the block */
	ulint	start;	/*!< the value of the struct field 'free' at the
			creation of the block */
#ifndef UNIV_HOTBACKUP
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
#endif /* !UNIV_HOTBACKUP */
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

/** A C++ wrapper class to the mem_heap_t routines, so that it can be used
as an STL allocator */
template<typename T>
class mem_heap_allocator
{
public:
	typedef		T		value_type;
	typedef		size_t		size_type;
	typedef		ptrdiff_t	difference_type;
	typedef		T*		pointer;
	typedef		const T*	const_pointer;
	typedef		T&		reference;
	typedef		const T&	const_reference;

	mem_heap_allocator(mem_heap_t* h): heap(h) {
	}

	~mem_heap_allocator() {
		heap = 0;
	}

	mem_heap_allocator(const mem_heap_allocator& that): heap (that.heap) {
	}

	template <typename U>
	mem_heap_allocator (const mem_heap_allocator<U> &other)
	: heap (other.heap) {
	}

	size_type max_size() const {
		return(ULONG_MAX / sizeof(T));
	}

	/** This function returns a pointer to the first element of a newly
	allocated array large enough to contain n objects of type T; only the
	memory is allocated, and the objects are not constructed. Moreover,
	an optional pointer argument (that points to an object already
	allocated by mem_heap_allocator) can be used as a hint to the
	implementation about where the new memory should be allocated in
	order to improve locality. */
	pointer	allocate(size_type n, const_pointer hint = 0) {
		DBUG_ENTER("mem_heap_allocator::allocate");

#ifdef UNIV_DEBUG
		DBUG_ASSERT(mem_heap_check(heap));
#endif /* UNIV_DEBUG */

		DBUG_RETURN((pointer) mem_heap_alloc(heap, n*sizeof(T)));
	}

	void deallocate(pointer p, size_type n) {
	}

	pointer address (reference r) const {
		return(&r);
	}

	const_pointer address (const_reference r) const {
		return(&r);
	}

	void construct(pointer p, const_reference t) {
		new (reinterpret_cast<void*>(p)) T(t);
	}

	void destroy(pointer p) {
		(reinterpret_cast<T*>(p))->~T();
	}

	/** Allocators are required to supply the below template class member
	which enables the possibility of obtaining a related allocator,
	parametrized in terms of a different type. For example, given an
	allocator type IntAllocator for objects of type int, a related
	allocator type for objects of type long could be obtained using
	IntAllocator::rebind<long>::other */
	template <typename U>
	struct rebind
	{
		typedef mem_heap_allocator<U> other ;
	};

private:
	mem_heap_t*	heap;
	template <typename U> friend class mem_heap_allocator;
};

template <class T>
bool operator== (const mem_heap_allocator<T>& left,
		 const mem_heap_allocator<T>& right)
{
	return(left.heap == right.heap);
}

template <class T>
bool operator!= (const mem_heap_allocator<T>& left,
		 const mem_heap_allocator<T>& right)
{
	return(left.heap != right.heap);
}

#endif
