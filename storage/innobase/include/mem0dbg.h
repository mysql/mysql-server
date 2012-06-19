/*****************************************************************************

Copyright (c) 1994, 2010, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/mem0dbg.h
The memory management: the debug code. This is not a compilation module,
but is included in mem0mem.* !

Created 6/9/1994 Heikki Tuuri
*******************************************************/

/* In the debug version each allocated field is surrounded with
check fields whose sizes are given below */

#ifdef UNIV_MEM_DEBUG
# ifndef UNIV_HOTBACKUP
/* The mutex which protects in the debug version the hash table
containing the list of live memory heaps, and also the global
variables in mem0dbg.cc. */
extern ib_mutex_t	mem_hash_mutex;
# endif /* !UNIV_HOTBACKUP */

#define MEM_FIELD_HEADER_SIZE	ut_calc_align(2 * sizeof(ulint),\
						UNIV_MEM_ALIGNMENT)
#define MEM_FIELD_TRAILER_SIZE	sizeof(ulint)
#else
#define MEM_FIELD_HEADER_SIZE	0
#endif


/* Space needed when allocating for a user a field of
length N. The space is allocated only in multiples of
UNIV_MEM_ALIGNMENT. In the debug version there are also
check fields at the both ends of the field. */
#ifdef UNIV_MEM_DEBUG
#define MEM_SPACE_NEEDED(N) ut_calc_align((N) + MEM_FIELD_HEADER_SIZE\
		 + MEM_FIELD_TRAILER_SIZE, UNIV_MEM_ALIGNMENT)
#else
#define MEM_SPACE_NEEDED(N) ut_calc_align((N), UNIV_MEM_ALIGNMENT)
#endif

#if defined UNIV_MEM_DEBUG || defined UNIV_DEBUG
/***************************************************************//**
Checks a memory heap for consistency and prints the contents if requested.
Outputs the sum of sizes of buffers given to the user (only in
the debug version), the physical size of the heap and the number of
blocks in the heap. In case of error returns 0 as sizes and number
of blocks. */
UNIV_INTERN
void
mem_heap_validate_or_print(
/*=======================*/
	mem_heap_t*	heap,	/*!< in: memory heap */
	byte*		top,	/*!< in: calculate and validate only until
				this top pointer in the heap is reached,
				if this pointer is NULL, ignored */
	ibool		 print,	 /*!< in: if TRUE, prints the contents
				of the heap; works only in
				the debug version */
	ibool*		 error,	 /*!< out: TRUE if error */
	ulint*		us_size,/*!< out: allocated memory
				(for the user) in the heap,
				if a NULL pointer is passed as this
				argument, it is ignored; in the
				non-debug version this is always -1 */
	ulint*		ph_size,/*!< out: physical size of the heap,
				if a NULL pointer is passed as this
				argument, it is ignored */
	ulint*		n_blocks); /*!< out: number of blocks in the heap,
				if a NULL pointer is passed as this
				argument, it is ignored */
/**************************************************************//**
Validates the contents of a memory heap.
@return	TRUE if ok */
UNIV_INTERN
ibool
mem_heap_validate(
/*==============*/
	mem_heap_t*   heap);	/*!< in: memory heap */
#endif /* UNIV_MEM_DEBUG || UNIV_DEBUG */
#ifdef UNIV_DEBUG
/**************************************************************//**
Checks that an object is a memory heap (or a block of it)
@return	TRUE if ok */
UNIV_INTERN
ibool
mem_heap_check(
/*===========*/
	mem_heap_t*   heap);	/*!< in: memory heap */
#endif /* UNIV_DEBUG */
#ifdef UNIV_MEM_DEBUG
/*****************************************************************//**
TRUE if no memory is currently allocated.
@return	TRUE if no heaps exist */
UNIV_INTERN
ibool
mem_all_freed(void);
/*===============*/
/*****************************************************************//**
Validates the dynamic memory
@return	TRUE if error */
UNIV_INTERN
ibool
mem_validate_no_assert(void);
/*=========================*/
/************************************************************//**
Validates the dynamic memory
@return	TRUE if ok */
UNIV_INTERN
ibool
mem_validate(void);
/*===============*/
#endif /* UNIV_MEM_DEBUG */
/************************************************************//**
Tries to find neigboring memory allocation blocks and dumps to stderr
the neighborhood of a given pointer. */
UNIV_INTERN
void
mem_analyze_corruption(
/*===================*/
	void*	ptr);	/*!< in: pointer to place of possible corruption */
/*****************************************************************//**
Prints information of dynamic memory usage and currently allocated memory
heaps or buffers. Can only be used in the debug version. */
UNIV_INTERN
void
mem_print_info(void);
/*================*/
/*****************************************************************//**
Prints information of dynamic memory usage and currently allocated memory
heaps or buffers since the last ..._print_info or..._print_new_info. */
UNIV_INTERN
void
mem_print_new_info(void);
/*====================*/
