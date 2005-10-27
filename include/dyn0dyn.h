/******************************************************
The dynamically allocated array

(c) 1996 Innobase Oy

Created 2/5/1996 Heikki Tuuri
*******************************************************/

#ifndef dyn0dyn_h
#define dyn0dyn_h

#include "univ.i"
#include "ut0lst.h"
#include "mem0mem.h"

typedef struct dyn_block_struct		dyn_block_t;
typedef dyn_block_t			dyn_array_t;


/* This is the initial 'payload' size of a dynamic array;
this must be > MLOG_BUF_MARGIN + 30! */
#define	DYN_ARRAY_DATA_SIZE	512

/*************************************************************************
Initializes a dynamic array. */
UNIV_INLINE
dyn_array_t*
dyn_array_create(
/*=============*/
				/* out: initialized dyn array */
	dyn_array_t*	arr);	/* in: pointer to a memory buffer of
				size sizeof(dyn_array_t) */
/****************************************************************
Frees a dynamic array. */
UNIV_INLINE
void
dyn_array_free(
/*===========*/
	dyn_array_t*	arr);	/* in: dyn array */
/*************************************************************************
Makes room on top of a dyn array and returns a pointer to a buffer in it.
After copying the elements, the caller must close the buffer using
dyn_array_close. */
UNIV_INLINE
byte*
dyn_array_open(
/*===========*/
				/* out: pointer to the buffer */
	dyn_array_t*	arr,	/* in: dynamic array */
	ulint		size);	/* in: size in bytes of the buffer; MUST be
				smaller than DYN_ARRAY_DATA_SIZE! */
/*************************************************************************
Closes the buffer returned by dyn_array_open. */
UNIV_INLINE
void
dyn_array_close(
/*============*/
	dyn_array_t*	arr,	/* in: dynamic array */
	byte*		ptr);	/* in: buffer space from ptr up was not used */
/*************************************************************************
Makes room on top of a dyn array and returns a pointer to
the added element. The caller must copy the element to
the pointer returned. */
UNIV_INLINE
void*
dyn_array_push(
/*===========*/
				/* out: pointer to the element */
	dyn_array_t*	arr,	/* in: dynamic array */
	ulint		size);	/* in: size in bytes of the element */
/****************************************************************
Returns pointer to an element in dyn array. */
UNIV_INLINE
void*
dyn_array_get_element(
/*==================*/
				/* out: pointer to element */
	dyn_array_t*	arr,	/* in: dyn array */
	ulint		pos);	/* in: position of element as bytes 
				from array start */
/****************************************************************
Returns the size of stored data in a dyn array. */
UNIV_INLINE
ulint
dyn_array_get_data_size(
/*====================*/
				/* out: data size in bytes */
	dyn_array_t*	arr);	/* in: dyn array */
/****************************************************************
Gets the first block in a dyn array. */
UNIV_INLINE
dyn_block_t*
dyn_array_get_first_block(
/*======================*/
	dyn_array_t*	arr);	/* in: dyn array */
/****************************************************************
Gets the last block in a dyn array. */
UNIV_INLINE
dyn_block_t*
dyn_array_get_last_block(
/*=====================*/
	dyn_array_t*	arr);	/* in: dyn array */
/************************************************************************
Gets the next block in a dyn array. */
UNIV_INLINE
dyn_block_t*
dyn_array_get_next_block(
/*=====================*/
				/* out: pointer to next, NULL if end of list */
	dyn_array_t*	arr,	/* in: dyn array */
	dyn_block_t*	block);	/* in: dyn array block */
/************************************************************************
Gets the number of used bytes in a dyn array block. */
UNIV_INLINE
ulint
dyn_block_get_used(
/*===============*/
				/* out: number of bytes used */
	dyn_block_t*	block);	/* in: dyn array block */
/************************************************************************
Gets pointer to the start of data in a dyn array block. */
UNIV_INLINE
byte*
dyn_block_get_data(
/*===============*/
				/* out: pointer to data */
	dyn_block_t*	block);	/* in: dyn array block */
/************************************************************
Pushes n bytes to a dyn array. */
UNIV_INLINE
void
dyn_push_string(
/*============*/
	dyn_array_t*	arr,	/* in: dyn array */
	const byte*	str,	/* in: string to write */
	ulint		len);	/* in: string length */

/*#################################################################*/

/* NOTE! Do not use the fields of the struct directly: the definition
appears here only for the compiler to know its size! */
struct dyn_block_struct{
	mem_heap_t*	heap;	/* in the first block this is != NULL 
				if dynamic allocation has been needed */
	ulint		used;	/* number of data bytes used in this block */
	byte		data[DYN_ARRAY_DATA_SIZE];
				/* storage for array elements */	
	UT_LIST_BASE_NODE_T(dyn_block_t) base;
				/* linear list of dyn blocks: this node is
				used only in the first block */
	UT_LIST_NODE_T(dyn_block_t) list;
				/* linear list node: used in all blocks */
#ifdef UNIV_DEBUG
	ulint		buf_end;/* only in the debug version: if dyn array is
				opened, this is the buffer end offset, else
				this is 0 */
	ulint		magic_n;
#endif
};


#ifndef UNIV_NONINL
#include "dyn0dyn.ic"
#endif

#endif 
