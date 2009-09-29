/*****************************************************************************

Copyright (c) 1996, 2009, Innobase Oy. All Rights Reserved.

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
typedef struct dyn_block_struct		dyn_block_t;
/** Dynamically allocated array */
typedef dyn_block_t			dyn_array_t;


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
	dyn_array_t*	arr);	/*!< in: pointer to a memory buffer of
				size sizeof(dyn_array_t) */
/************************************************************//**
Frees a dynamic array. */
UNIV_INLINE
void
dyn_array_free(
/*===========*/
	dyn_array_t*	arr);	/*!< in: dyn array */
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
	ulint		size);	/*!< in: size in bytes of the buffer; MUST be
				smaller than DYN_ARRAY_DATA_SIZE! */
/*********************************************************************//**
Closes the buffer returned by dyn_array_open. */
UNIV_INLINE
void
dyn_array_close(
/*============*/
	dyn_array_t*	arr,	/*!< in: dynamic array */
	byte*		ptr);	/*!< in: buffer space from ptr up was not used */
/*********************************************************************//**
Makes room on top of a dyn array and returns a pointer to
the added element. The caller must copy the element to
the pointer returned.
@return	pointer to the element */
UNIV_INLINE
void*
dyn_array_push(
/*===========*/
	dyn_array_t*	arr,	/*!< in: dynamic array */
	ulint		size);	/*!< in: size in bytes of the element */
/************************************************************//**
Returns pointer to an element in dyn array.
@return	pointer to element */
UNIV_INLINE
void*
dyn_array_get_element(
/*==================*/
	dyn_array_t*	arr,	/*!< in: dyn array */
	ulint		pos);	/*!< in: position of element as bytes
				from array start */
/************************************************************//**
Returns the size of stored data in a dyn array.
@return	data size in bytes */
UNIV_INLINE
ulint
dyn_array_get_data_size(
/*====================*/
	dyn_array_t*	arr);	/*!< in: dyn array */
/************************************************************//**
Gets the first block in a dyn array. */
UNIV_INLINE
dyn_block_t*
dyn_array_get_first_block(
/*======================*/
	dyn_array_t*	arr);	/*!< in: dyn array */
/************************************************************//**
Gets the last block in a dyn array. */
UNIV_INLINE
dyn_block_t*
dyn_array_get_last_block(
/*=====================*/
	dyn_array_t*	arr);	/*!< in: dyn array */
/********************************************************************//**
Gets the next block in a dyn array.
@return	pointer to next, NULL if end of list */
UNIV_INLINE
dyn_block_t*
dyn_array_get_next_block(
/*=====================*/
	dyn_array_t*	arr,	/*!< in: dyn array */
	dyn_block_t*	block);	/*!< in: dyn array block */
/********************************************************************//**
Gets the number of used bytes in a dyn array block.
@return	number of bytes used */
UNIV_INLINE
ulint
dyn_block_get_used(
/*===============*/
	dyn_block_t*	block);	/*!< in: dyn array block */
/********************************************************************//**
Gets pointer to the start of data in a dyn array block.
@return	pointer to data */
UNIV_INLINE
byte*
dyn_block_get_data(
/*===============*/
	dyn_block_t*	block);	/*!< in: dyn array block */
/********************************************************//**
Pushes n bytes to a dyn array. */
UNIV_INLINE
void
dyn_push_string(
/*============*/
	dyn_array_t*	arr,	/*!< in: dyn array */
	const byte*	str,	/*!< in: string to write */
	ulint		len);	/*!< in: string length */

/*#################################################################*/

/** @brief A block in a dynamically allocated array.
NOTE! Do not access the fields of the struct directly: the definition
appears here only for the compiler to know its size! */
struct dyn_block_struct{
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
