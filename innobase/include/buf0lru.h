/******************************************************
The database buffer pool LRU replacement algorithm

(c) 1995 Innobase Oy

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#ifndef buf0lru_h
#define buf0lru_h

#include "univ.i"
#include "ut0byte.h"
#include "buf0types.h"

/**********************************************************************
Tries to remove LRU flushed blocks from the end of the LRU list and put them
to the free list. This is beneficial for the efficiency of the insert buffer
operation, as flushed pages from non-unique non-clustered indexes are here
taken out of the buffer pool, and their inserts redirected to the insert
buffer. Otherwise, the flushed blocks could get modified again before read
operations need new buffer blocks, and the i/o work done in flushing would be
wasted. */

void
buf_LRU_try_free_flushed_blocks(void);
/*==================================*/

/*#######################################################################
These are low-level functions
#########################################################################*/

/* Minimum LRU list length for which the LRU_old pointer is defined */

#define BUF_LRU_OLD_MIN_LEN	80

#define BUF_LRU_FREE_SEARCH_LEN		(5 + 2 * BUF_READ_AHEAD_AREA)

/**********************************************************************
Gets the minimum LRU_position field for the blocks in an initial segment
(determined by BUF_LRU_INITIAL_RATIO) of the LRU list. The limit is not
guaranteed to be precise, because the ulint_clock may wrap around. */

ulint
buf_LRU_get_recent_limit(void);
/*==========================*/
			/* out: the limit; zero if could not determine it */
/**********************************************************************
Returns a free block from the buf_pool. The block is taken off the
free list. If it is empty, blocks are moved from the end of the
LRU list to the free list. */

buf_block_t*
buf_LRU_get_free_block(void);
/*=========================*/
				/* out: the free control block */
/**********************************************************************
Puts a block back to the free list. */

void
buf_LRU_block_free_non_file_page(
/*=============================*/
	buf_block_t*	block);	/* in: block, must not contain a file page */
/**********************************************************************
Adds a block to the LRU list. */

void
buf_LRU_add_block(
/*==============*/
	buf_block_t*	block,	/* in: control block */
	ibool		old);	/* in: TRUE if should be put to the old
				blocks in the LRU list, else put to the
				start; if the LRU list is very short, added to
				the start regardless of this parameter */
/**********************************************************************
Moves a block to the start of the LRU list. */

void
buf_LRU_make_block_young(
/*=====================*/
	buf_block_t*	block);	/* in: control block */
/**********************************************************************
Moves a block to the end of the LRU list. */

void
buf_LRU_make_block_old(
/*===================*/
	buf_block_t*	block);	/* in: control block */
/**********************************************************************
Look for a replaceable block from the end of the LRU list and put it to
the free list if found. */

ibool
buf_LRU_search_and_free_block(
/*==========================*/
				/* out: TRUE if freed */
	ulint	n_iterations);	/* in: how many times this has been called
				repeatedly without result: a high value
				means that we should search farther */
/**************************************************************************
Validates the LRU list. */

ibool
buf_LRU_validate(void);
/*==================*/
/**************************************************************************
Prints the LRU list. */

void
buf_LRU_print(void);
/*===============*/

#ifndef UNIV_NONINL
#include "buf0lru.ic"
#endif

#endif
