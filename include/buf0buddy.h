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
Allocate a block.  The thread calling this function must hold
buf_pool->mutex and must not hold buf_pool->zip_mutex or any
block->mutex.  The buf_pool->mutex may only be released and reacquired
if lru == BUF_BUDDY_USE_LRU.  This function should only be used for
allocating compressed page frames or control blocks (buf_page_t).
Allocated control blocks must be properly initialized immediately
after buf_buddy_alloc() has returned the memory, before releasing
buf_pool->mutex. */
UNIV_INLINE
void*
buf_buddy_alloc(
/*============*/
			/* out: allocated block,
			possibly NULL if lru == NULL */
	ulint	size,	/* in: block size, up to UNIV_PAGE_SIZE */
	ibool*	lru)	/* in: pointer to a variable that will be assigned
			TRUE if storage was allocated from the LRU list
			and buf_pool->mutex was temporarily released,
			or NULL if the LRU list should not be used */
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

/** Number of frames allocated from the buffer pool to the buddy system.
Protected by buf_pool->mutex. */
extern ulint buf_buddy_n_frames;
/** Preferred minimum number of frames allocated from the buffer pool
to the buddy system.  Unless this number is exceeded or the buffer
pool is scarce, the LRU algorithm will not free compressed-only pages
in order to satisfy an allocation request.  Protected by buf_pool->mutex. */
extern ulint buf_buddy_min_n_frames;
/** Preferred maximum number of frames allocated from the buffer pool
to the buddy system.  Unless this number is exceeded, the buddy allocator
will not try to free clean compressed-only pages before falling back
to the LRU algorithm.  Protected by buf_pool->mutex. */
extern ulint buf_buddy_max_n_frames;
/** Counts of blocks allocated from the buddy system.
Protected by buf_pool->mutex. */
extern ulint buf_buddy_used[BUF_BUDDY_SIZES + 1];
/** Counts of blocks relocated by the buddy system.
Protected by buf_pool->mutex. */
extern ib_uint64_t buf_buddy_relocated[BUF_BUDDY_SIZES + 1];

#ifndef UNIV_NONINL
# include "buf0buddy.ic"
#endif

#endif /* buf0buddy_h */
