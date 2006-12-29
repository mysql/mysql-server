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
Allocate a block. */
UNIV_INLINE
void*
buf_buddy_alloc(
/*============*/
			/* out: pointer to the start of the block */
	ulint	size,	/* in: block size, up to UNIV_PAGE_SIZE */
	ibool	lru)	/* in: TRUE=allocate from the LRU list if needed */
	__attribute__((malloc));

/**************************************************************************
Release a block. */
UNIV_INLINE
void
buf_buddy_free(
/*===========*/
	void*	buf,	/* in: block to be freed, must not be
			pointed to by the buffer pool */
	ulint	size)	/* in: block size, up to UNIV_PAGE_SIZE */
	__attribute__((nonnull));

#ifndef UNIV_NONINL
# include "buf0buddy.ic"
#endif

#endif /* buf0buddy_h */
