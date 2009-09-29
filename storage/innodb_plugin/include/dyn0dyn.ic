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
@file include/dyn0dyn.ic
The dynamically allocated array

Created 2/5/1996 Heikki Tuuri
*******************************************************/

/** Value of dyn_block_struct::magic_n */
#define DYN_BLOCK_MAGIC_N	375767
/** Flag for dyn_block_struct::used that indicates a full block */
#define DYN_BLOCK_FULL_FLAG	0x1000000UL

/************************************************************//**
Adds a new block to a dyn array.
@return	created block */
UNIV_INTERN
dyn_block_t*
dyn_array_add_block(
/*================*/
	dyn_array_t*	arr);	/*!< in: dyn array */


/************************************************************//**
Gets the first block in a dyn array. */
UNIV_INLINE
dyn_block_t*
dyn_array_get_first_block(
/*======================*/
	dyn_array_t*	arr)	/*!< in: dyn array */
{
	return(arr);
}

/************************************************************//**
Gets the last block in a dyn array. */
UNIV_INLINE
dyn_block_t*
dyn_array_get_last_block(
/*=====================*/
	dyn_array_t*	arr)	/*!< in: dyn array */
{
	if (arr->heap == NULL) {

		return(arr);
	}

	return(UT_LIST_GET_LAST(arr->base));
}

/********************************************************************//**
Gets the next block in a dyn array.
@return	pointer to next, NULL if end of list */
UNIV_INLINE
dyn_block_t*
dyn_array_get_next_block(
/*=====================*/
	dyn_array_t*	arr,	/*!< in: dyn array */
	dyn_block_t*	block)	/*!< in: dyn array block */
{
	ut_ad(arr && block);

	if (arr->heap == NULL) {
		ut_ad(arr == block);

		return(NULL);
	}

	return(UT_LIST_GET_NEXT(list, block));
}

/********************************************************************//**
Gets the number of used bytes in a dyn array block.
@return	number of bytes used */
UNIV_INLINE
ulint
dyn_block_get_used(
/*===============*/
	dyn_block_t*	block)	/*!< in: dyn array block */
{
	ut_ad(block);

	return((block->used) & ~DYN_BLOCK_FULL_FLAG);
}

/********************************************************************//**
Gets pointer to the start of data in a dyn array block.
@return	pointer to data */
UNIV_INLINE
byte*
dyn_block_get_data(
/*===============*/
	dyn_block_t*	block)	/*!< in: dyn array block */
{
	ut_ad(block);

	return(block->data);
}

/*********************************************************************//**
Initializes a dynamic array.
@return	initialized dyn array */
UNIV_INLINE
dyn_array_t*
dyn_array_create(
/*=============*/
	dyn_array_t*	arr)	/*!< in: pointer to a memory buffer of
				size sizeof(dyn_array_t) */
{
	ut_ad(arr);
#if DYN_ARRAY_DATA_SIZE >= DYN_BLOCK_FULL_FLAG
# error "DYN_ARRAY_DATA_SIZE >= DYN_BLOCK_FULL_FLAG"
#endif

	arr->heap = NULL;
	arr->used = 0;

#ifdef UNIV_DEBUG
	arr->buf_end = 0;
	arr->magic_n = DYN_BLOCK_MAGIC_N;
#endif
	return(arr);
}

/************************************************************//**
Frees a dynamic array. */
UNIV_INLINE
void
dyn_array_free(
/*===========*/
	dyn_array_t*	arr)	/*!< in: dyn array */
{
	if (arr->heap != NULL) {
		mem_heap_free(arr->heap);
	}

#ifdef UNIV_DEBUG
	arr->magic_n = 0;
#endif
}

/*********************************************************************//**
Makes room on top of a dyn array and returns a pointer to the added element.
The caller must copy the element to the pointer returned.
@return	pointer to the element */
UNIV_INLINE
void*
dyn_array_push(
/*===========*/
	dyn_array_t*	arr,	/*!< in: dynamic array */
	ulint		size)	/*!< in: size in bytes of the element */
{
	dyn_block_t*	block;
	ulint		used;

	ut_ad(arr);
	ut_ad(arr->magic_n == DYN_BLOCK_MAGIC_N);
	ut_ad(size <= DYN_ARRAY_DATA_SIZE);
	ut_ad(size);

	block = arr;
	used = block->used;

	if (used + size > DYN_ARRAY_DATA_SIZE) {
		/* Get the last array block */

		block = dyn_array_get_last_block(arr);
		used = block->used;

		if (used + size > DYN_ARRAY_DATA_SIZE) {
			block = dyn_array_add_block(arr);
			used = block->used;
		}
	}

	block->used = used + size;
	ut_ad(block->used <= DYN_ARRAY_DATA_SIZE);

	return((block->data) + used);
}

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
{
	dyn_block_t*	block;
	ulint		used;

	ut_ad(arr);
	ut_ad(arr->magic_n == DYN_BLOCK_MAGIC_N);
	ut_ad(size <= DYN_ARRAY_DATA_SIZE);
	ut_ad(size);

	block = arr;
	used = block->used;

	if (used + size > DYN_ARRAY_DATA_SIZE) {
		/* Get the last array block */

		block = dyn_array_get_last_block(arr);
		used = block->used;

		if (used + size > DYN_ARRAY_DATA_SIZE) {
			block = dyn_array_add_block(arr);
			used = block->used;
			ut_a(size <= DYN_ARRAY_DATA_SIZE);
		}
	}

	ut_ad(block->used <= DYN_ARRAY_DATA_SIZE);
#ifdef UNIV_DEBUG
	ut_ad(arr->buf_end == 0);

	arr->buf_end = used + size;
#endif
	return((block->data) + used);
}

/*********************************************************************//**
Closes the buffer returned by dyn_array_open. */
UNIV_INLINE
void
dyn_array_close(
/*============*/
	dyn_array_t*	arr,	/*!< in: dynamic array */
	byte*		ptr)	/*!< in: buffer space from ptr up was not used */
{
	dyn_block_t*	block;

	ut_ad(arr);
	ut_ad(arr->magic_n == DYN_BLOCK_MAGIC_N);

	block = dyn_array_get_last_block(arr);

	ut_ad(arr->buf_end + block->data >= ptr);

	block->used = ptr - block->data;

	ut_ad(block->used <= DYN_ARRAY_DATA_SIZE);

#ifdef UNIV_DEBUG
	arr->buf_end = 0;
#endif
}

/************************************************************//**
Returns pointer to an element in dyn array.
@return	pointer to element */
UNIV_INLINE
void*
dyn_array_get_element(
/*==================*/
	dyn_array_t*	arr,	/*!< in: dyn array */
	ulint		pos)	/*!< in: position of element as bytes
				from array start */
{
	dyn_block_t*	block;
	ulint		used;

	ut_ad(arr);
	ut_ad(arr->magic_n == DYN_BLOCK_MAGIC_N);

	/* Get the first array block */
	block = dyn_array_get_first_block(arr);

	if (arr->heap != NULL) {
		used = dyn_block_get_used(block);

		while (pos >= used) {
			pos -= used;
			block = UT_LIST_GET_NEXT(list, block);
			ut_ad(block);

			used = dyn_block_get_used(block);
		}
	}

	ut_ad(block);
	ut_ad(dyn_block_get_used(block) >= pos);

	return(block->data + pos);
}

/************************************************************//**
Returns the size of stored data in a dyn array.
@return	data size in bytes */
UNIV_INLINE
ulint
dyn_array_get_data_size(
/*====================*/
	dyn_array_t*	arr)	/*!< in: dyn array */
{
	dyn_block_t*	block;
	ulint		sum	= 0;

	ut_ad(arr);
	ut_ad(arr->magic_n == DYN_BLOCK_MAGIC_N);

	if (arr->heap == NULL) {

		return(arr->used);
	}

	/* Get the first array block */
	block = dyn_array_get_first_block(arr);

	while (block != NULL) {
		sum += dyn_block_get_used(block);
		block = dyn_array_get_next_block(arr, block);
	}

	return(sum);
}

/********************************************************//**
Pushes n bytes to a dyn array. */
UNIV_INLINE
void
dyn_push_string(
/*============*/
	dyn_array_t*	arr,	/*!< in: dyn array */
	const byte*	str,	/*!< in: string to write */
	ulint		len)	/*!< in: string length */
{
	ulint	n_copied;

	while (len > 0) {
		if (len > DYN_ARRAY_DATA_SIZE) {
			n_copied = DYN_ARRAY_DATA_SIZE;
		} else {
			n_copied = len;
		}

		memcpy(dyn_array_push(arr, n_copied), str, n_copied);

		str += n_copied;
		len -= n_copied;
	}
}
