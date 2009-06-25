/*****************************************************************************

Copyright (c) 2006, 2009, Innobase Oy. All Rights Reserved.

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
Binary buddy allocator for compressed pages

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
Allocate a block.  The thread calling this function must hold
buf_pool_mutex and must not hold buf_pool_zip_mutex or any
block->mutex.  The buf_pool_mutex may only be released and reacquired
if lru != NULL.  This function should only be used for allocating
compressed page frames or control blocks (buf_page_t).  Allocated
control blocks must be properly initialized immediately after
buf_buddy_alloc() has returned the memory, before releasing
buf_pool_mutex. */
UNIV_INLINE
void*
buf_buddy_alloc(
/*============*/
			/* out: allocated block,
			possibly NULL if lru == NULL */
	ulint	size,	/* in: block size, up to UNIV_PAGE_SIZE */
	ibool*	lru,	/* in: pointer to a variable that will be assigned
			TRUE if storage was allocated from the LRU list
			and buf_pool_mutex was temporarily released,
			or NULL if the LRU list should not be used */
	ibool	have_page_hash_mutex)
	__attribute__((malloc));

/**************************************************************************
Release a block. */
UNIV_INLINE
void
buf_buddy_free(
/*===========*/
	void*	buf,	/* in: block to be freed, must not be
			pointed to by the buffer pool */
	ulint	size,	/* in: block size, up to UNIV_PAGE_SIZE */
	ibool	have_page_hash_mutex)
	__attribute__((nonnull));

/** Statistics of buddy blocks of a given size. */
struct buf_buddy_stat_struct {
	/** Number of blocks allocated from the buddy system. */
	ulint		used;
	/** Number of blocks relocated by the buddy system. */
	ib_uint64_t	relocated;
	/** Total duration of block relocations, in microseconds. */
	ib_uint64_t	relocated_usec;
};

typedef struct buf_buddy_stat_struct buf_buddy_stat_t;

/** Statistics of the buddy system, indexed by block size.
Protected by buf_pool_mutex. */
extern buf_buddy_stat_t buf_buddy_stat[BUF_BUDDY_SIZES + 1];

#ifndef UNIV_NONINL
# include "buf0buddy.ic"
#endif

#endif /* buf0buddy_h */
