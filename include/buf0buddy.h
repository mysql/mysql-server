/******************************************************
Binary buddy allocator for compressed pages

(c) 2006 Innobase Oy

Created December 2006 by Marko Makela
*******************************************************/

#ifndef buf0buddy_h
#define buf0buddy_h

#ifdef UNIV_MATERIALIZE
# undef UNIV_INLINE
# define UNIV_INLINE
#endif

#include "univ.i"
#include "buf0types.h"

/**************************************************************************
Get the offset of the buddy of a compressed page frame. */
UNIV_INLINE
lint
buf_buddy_get_offset(
/*=================*/
				/* out: offset of the buddy relative to page */
	const void*	page,	/* in: compressed page */
	ulint		size)	/* in: page size in bytes */
	__attribute__((nonnull));

/**************************************************************************
Get the buddy of a compressed page frame. */
#define buf_buddy_get(page,size) ((page) + buf_buddy_get_offset((page),(size)))

/**************************************************************************
Get the index of buf_pool->zip_free[] for a given block size. */
UNIV_INLINE
ulint
buf_buddy_get_slot(
/*===============*/
			/* out: index of buf_pool->zip_free[] */
	ulint	size);	/* in: block size */

/**************************************************************************
Try to allocate a block from buf_pool->zip_free[]. */
UNIV_INLINE
void*
buf_buddy_alloc(
/*============*/
	ulint	size,	/* in: block size, up to UNIV_PAGE_SIZE / 2 */
	ibool	split)	/* in: TRUE=attempt splitting,
			FALSE=try to allocate exact size */
	__attribute__((malloc));

/**************************************************************************
Release a block. */
UNIV_INLINE
void
buf_buddy_free(
/*===========*/
	void*	buf,	/* in: block to free */
	ulint	size)	/* in: block size, up to UNIV_PAGE_SIZE / 2 */
	__attribute__((nonnull));

#ifndef UNIV_NONINL
# include "buf0buddy.ic"
#endif

#endif /* buf0buddy_h */
