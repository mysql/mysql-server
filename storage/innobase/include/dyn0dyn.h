/*****************************************************************************

Copyright (c) 1996, 2013, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/dyn0dyn.h
The dynamically allocated array

Created 2/5/1996 Heikki Tuuri
*******************************************************/

#ifndef dyn0dyn_h
#define dyn0dyn_h

#include "univ.i"
#include "ut0lst.h"
#include "mem0mem.h"

/** A block in a dynamically allocated array */
struct dyn_block_t;
/** Dynamically allocated array */
typedef dyn_block_t		dyn_array_t;

/** This is the initial 'payload' size of a dynamic array;
this must be > MLOG_BUF_MARGIN + 30! */
#define	DYN_ARRAY_DATA_SIZE	512

/*********************************************************************//**
Initializes a dynamic array.
@return	initialized dyn array */
UNIV_INLINE
dyn_array_t*
dyn_array_create(
/*=============*/
	dyn_array_t*	arr)	/*!< in/out memory buffer of
				size sizeof(dyn_array_t) */
	__attribute__((nonnull));
/************************************************************//**
Frees a dynamic array. */
UNIV_INLINE
void
dyn_array_free(
/*===========*/
	dyn_array_t*	arr)	/*!< in,own: dyn array */
	__attribute__((nonnull));
/*********************************************************************//**
Makes room on top of a dyn array and returns a pointer to a buffer in it.
After copying the elements, the caller must close the buffer using
dyn_array_close.
@return	pointer to the buffer */
UNIV_INLINE
byte*
dyn_array_open(
/*===========*/
	dyn_array_t*	arr,	/*!< in: dynamic array */
	ulint		size)	/*!< in: size in bytes of the buffer; MUST be
				smaller than DYN_ARRAY_DATA_SIZE! */
	__attribute__((nonnull, warn_unused_result));
/*********************************************************************//**
Closes the buffer returned by dyn_array_open. */
UNIV_INLINE
void
dyn_array_close(
/*============*/
	dyn_array_t*	arr,	/*!< in: dynamic array */
	const byte*	ptr)	/*!< in: end of used space */
	__attribute__((nonnull));
/*********************************************************************//**
Makes room on top of a dyn array and returns a pointer to
the added element. The caller must copy the element to
the pointer returned.
@return	pointer to the element */
UNIV_INLINE
void*
dyn_array_push(
/*===========*/
	dyn_array_t*	arr,	/*!< in/out: dynamic array */
	ulint		size)	/*!< in: size in bytes of the element */
	__attribute__((nonnull, warn_unused_result));
/************************************************************//**
Returns pointer to an element in dyn array.
@return	pointer to element */
UNIV_INLINE
void*
dyn_array_get_element(
/*==================*/
	const dyn_array_t*	arr,	/*!< in: dyn array */
	ulint			pos)	/*!< in: position of element
					in bytes from array start */
	__attribute__((nonnull, warn_unused_result));
/************************************************************//**
Returns the size of stored data in a dyn array.
@return	data size in bytes */
UNIV_INLINE
ulint
dyn_array_get_data_size(
/*====================*/
	const dyn_array_t*	arr)	/*!< in: dyn array */
	__attribute__((nonnull, warn_unused_result, pure));
/************************************************************//**
Gets the first block in a dyn array.
@param arr	dyn array
@return		first block */
#define dyn_array_get_first_block(arr) (arr)
/************************************************************//**
Gets the last block in a dyn array.
@param arr	dyn array
@return		last block */
#define dyn_array_get_last_block(arr)				\
	((arr)->heap ? UT_LIST_GET_LAST((arr)->base) : (arr))
/********************************************************************//**
Gets the next block in a dyn array.
@param arr	dyn array
@param block	dyn array block
@return		pointer to next, NULL if end of list */
#define dyn_array_get_next_block(arr, block)			\
	((arr)->heap ? UT_LIST_GET_NEXT(list, block) : NULL)
/********************************************************************//**
Gets the previous block in a dyn array.
@param arr	dyn array
@param block	dyn array block
@return		pointer to previous, NULL if end of list */
#define dyn_array_get_prev_block(arr, block)			\
	((arr)->heap ? UT_LIST_GET_PREV(list, block) : NULL)
/********************************************************************//**
Gets the number of used bytes in a dyn array block.
@return	number of bytes used */
UNIV_INLINE
ulint
dyn_block_get_used(
/*===============*/
	const dyn_block_t*	block)	/*!< in: dyn array block */
	__attribute__((nonnull, warn_unused_result, pure));
/********************************************************************//**
Gets pointer to the start of data in a dyn array block.
@return	pointer to data */
UNIV_INLINE
byte*
dyn_block_get_data(
/*===============*/
	const dyn_block_t*	block)	/*!< in: dyn array block */
	__attribute__((nonnull, warn_unused_result, pure));
/********************************************************//**
Pushes n bytes to a dyn array. */
UNIV_INLINE
void
dyn_push_string(
/*============*/
	dyn_array_t*	arr,	/*!< in/out: dyn array */
	const byte*	str,	/*!< in: string to write */
	ulint		len)	/*!< in: string length */
	__attribute__((nonnull));

/*#################################################################*/

/** @brief A block in a dynamically allocated array.
NOTE! Do not access the fields of the struct directly: the definition
appears here only for the compiler to know its size! */
struct dyn_block_t{
	mem_heap_t*	heap;	/*!< in the first block this is != NULL
				if dynamic allocation has been needed */
	ulint		used;	/*!< number of data bytes used in this block;
				DYN_BLOCK_FULL_FLAG is set when the block
				becomes full */
	byte		data[DYN_ARRAY_DATA_SIZE];
				/*!< storage for array elements */
	UT_LIST_BASE_NODE_T(dyn_block_t) base;
				/*!< linear list of dyn blocks: this node is
				used only in the first block */
	UT_LIST_NODE_T(dyn_block_t) list;
				/*!< linear list node: used in all blocks */
#ifdef UNIV_DEBUG
	ulint		buf_end;/*!< only in the debug version: if dyn
				array is opened, this is the buffer
				end offset, else this is 0 */
	ulint		magic_n;/*!< magic number (DYN_BLOCK_MAGIC_N) */
#endif
};


#ifndef UNIV_NONINL
#include "dyn0dyn.ic"
#endif

#endif
