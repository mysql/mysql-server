/*****************************************************************************

Copyright (c) 1995, 2011, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/buf0lru.h
The database buffer pool LRU replacement algorithm

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#ifndef buf0lru_h
#define buf0lru_h

#include "univ.i"
#ifndef UNIV_HOTBACKUP
#include "ut0byte.h"
#include "buf0types.h"

// Forward declaration
struct trx_t;

/******************************************************************//**
Returns TRUE if less than 25 % of the buffer pool is available. This can be
used in heuristics to prevent huge transactions eating up the whole buffer
pool for their locks.
@return	TRUE if less than 25 % of buffer pool left */
UNIV_INTERN
ibool
buf_LRU_buf_pool_running_out(void);
/*==============================*/

/*#######################################################################
These are low-level functions
#########################################################################*/

/** Minimum LRU list length for which the LRU_old pointer is defined */
#define BUF_LRU_OLD_MIN_LEN	512	/* 8 megabytes of 16k pages */

/******************************************************************//**
Flushes all dirty pages or removes all pages belonging
to a given tablespace. A PROBLEM: if readahead is being started, what
guarantees that it will not try to read in pages after this operation
has completed? */
UNIV_INTERN
void
buf_LRU_flush_or_remove_pages(
/*==========================*/
	ulint		id,		/*!< in: space id */
	buf_remove_t	buf_remove,	/*!< in: remove or flush strategy */
	const trx_t*	trx);		/*!< to check if the operation must
					be interrupted */

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/********************************************************************//**
Insert a compressed block into buf_pool->zip_clean in the LRU order. */
UNIV_INTERN
void
buf_LRU_insert_zip_clean(
/*=====================*/
	buf_page_t*	bpage);	/*!< in: pointer to the block in question */
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

/******************************************************************//**
Try to free a block.  If bpage is a descriptor of a compressed-only
page, the descriptor object will be freed as well.

NOTE: If this function returns TRUE, it will temporarily
release buf_pool->mutex.  Furthermore, the page frame will no longer be
accessible via bpage.

The caller must hold buf_pool->mutex and must not hold any
buf_page_get_mutex() when calling this function.
@return TRUE if freed, FALSE otherwise. */
UNIV_INTERN
ibool
buf_LRU_free_block(
/*===============*/
	buf_page_t*	bpage,	/*!< in: block to be freed */
	ibool		zip)	/*!< in: TRUE if should remove also the
				compressed page of an uncompressed page */
	__attribute__((nonnull));
/******************************************************************//**
Try to free a replaceable block.
@return	TRUE if found and freed */
UNIV_INTERN
ibool
buf_LRU_scan_and_free_block(
/*========================*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	ibool		scan_all)	/*!< in: scan whole LRU list
					if TRUE, otherwise scan only
					'old' blocks. */
	__attribute__((nonnull,warn_unused_result));
/******************************************************************//**
Returns a free block from the buf_pool.  The block is taken off the
free list.  If it is empty, returns NULL.
@return	a free control block, or NULL if the buf_block->free list is empty */
UNIV_INTERN
buf_block_t*
buf_LRU_get_free_only(
/*==================*/
	buf_pool_t*	buf_pool);	/*!< buffer pool instance */
/******************************************************************//**
Returns a free block from the buf_pool. The block is taken off the
free list. If it is empty, blocks are moved from the end of the
LRU list to the free list.
This function is called from a user thread when it needs a clean
block to read in a page. Note that we only ever get a block from
the free list. Even when we flush a page or find a page in LRU scan
we put it to free list to be used.
* iteration 0:
  * get a block from free list, success:done
  * if there is an LRU flush batch in progress:
    * wait for batch to end: retry free list
  * if buf_pool->try_LRU_scan is set
    * scan LRU up to srv_LRU_scan_depth to find a clean block
    * the above will put the block on free list
    * success:retry the free list
  * flush one dirty page from tail of LRU to disk
    * the above will put the block on free list
    * success: retry the free list
* iteration 1:
  * same as iteration 0 except:
    * scan whole LRU list
    * scan LRU list even if buf_pool->try_LRU_scan is not set
* iteration > 1:
  * same as iteration 1 but sleep 100ms
@return	the free control block, in state BUF_BLOCK_READY_FOR_USE */
UNIV_INTERN
buf_block_t*
buf_LRU_get_free_block(
/*===================*/
	buf_pool_t*	buf_pool)	/*!< in/out: buffer pool instance */
	__attribute__((nonnull,warn_unused_result));
/******************************************************************//**
Determines if the unzip_LRU list should be used for evicting a victim
instead of the general LRU list.
@return	TRUE if should use unzip_LRU */
UNIV_INTERN
ibool
buf_LRU_evict_from_unzip_LRU(
/*=========================*/
	buf_pool_t*	buf_pool);
/******************************************************************//**
Puts a block back to the free list. */
UNIV_INTERN
void
buf_LRU_block_free_non_file_page(
/*=============================*/
	buf_block_t*	block);	/*!< in: block, must not contain a file page */
/******************************************************************//**
Adds a block to the LRU list. Please make sure that the zip_size is
already set into the page zip when invoking the function, so that we
can get correct zip_size from the buffer page when adding a block
into LRU */
UNIV_INTERN
void
buf_LRU_add_block(
/*==============*/
	buf_page_t*	bpage,	/*!< in: control block */
	ibool		old);	/*!< in: TRUE if should be put to the old
				blocks in the LRU list, else put to the
				start; if the LRU list is very short, added to
				the start regardless of this parameter */
/******************************************************************//**
Adds a block to the LRU list of decompressed zip pages. */
UNIV_INTERN
void
buf_unzip_LRU_add_block(
/*====================*/
	buf_block_t*	block,	/*!< in: control block */
	ibool		old);	/*!< in: TRUE if should be put to the end
				of the list, else put to the start */
/******************************************************************//**
Moves a block to the start of the LRU list. */
UNIV_INTERN
void
buf_LRU_make_block_young(
/*=====================*/
	buf_page_t*	bpage);	/*!< in: control block */
/******************************************************************//**
Moves a block to the end of the LRU list. */
UNIV_INTERN
void
buf_LRU_make_block_old(
/*===================*/
	buf_page_t*	bpage);	/*!< in: control block */
/**********************************************************************//**
Updates buf_pool->LRU_old_ratio.
@return	updated old_pct */
UNIV_INTERN
ulint
buf_LRU_old_ratio_update(
/*=====================*/
	uint	old_pct,/*!< in: Reserve this percentage of
			the buffer pool for "old" blocks. */
	ibool	adjust);/*!< in: TRUE=adjust the LRU list;
			FALSE=just assign buf_pool->LRU_old_ratio
			during the initialization of InnoDB */
/********************************************************************//**
Update the historical stats that we are collecting for LRU eviction
policy at the end of each interval. */
UNIV_INTERN
void
buf_LRU_stat_update(void);
/*=====================*/

/******************************************************************//**
Remove one page from LRU list and put it to free list */
UNIV_INTERN
void
buf_LRU_free_one_page(
/*==================*/
	buf_page_t*	bpage)	/*!< in/out: block, must contain a file page and
				be in a state where it can be freed; there
				may or may not be a hash index to the page */
	__attribute__((nonnull));

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/**********************************************************************//**
Validates the LRU list.
@return	TRUE */
UNIV_INTERN
ibool
buf_LRU_validate(void);
/*==================*/
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
#if defined UNIV_DEBUG_PRINT || defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/**********************************************************************//**
Prints the LRU list. */
UNIV_INTERN
void
buf_LRU_print(void);
/*===============*/
#endif /* UNIV_DEBUG_PRINT || UNIV_DEBUG || UNIV_BUF_DEBUG */

/** @name Heuristics for detecting index scan @{ */
/** The denominator of buf_pool->LRU_old_ratio. */
#define BUF_LRU_OLD_RATIO_DIV	1024
/** Maximum value of buf_pool->LRU_old_ratio.
@see buf_LRU_old_adjust_len
@see buf_pool->LRU_old_ratio_update */
#define BUF_LRU_OLD_RATIO_MAX	BUF_LRU_OLD_RATIO_DIV
/** Minimum value of buf_pool->LRU_old_ratio.
@see buf_LRU_old_adjust_len
@see buf_pool->LRU_old_ratio_update
The minimum must exceed
(BUF_LRU_OLD_TOLERANCE + 5) * BUF_LRU_OLD_RATIO_DIV / BUF_LRU_OLD_MIN_LEN. */
#define BUF_LRU_OLD_RATIO_MIN	51

#if BUF_LRU_OLD_RATIO_MIN >= BUF_LRU_OLD_RATIO_MAX
# error "BUF_LRU_OLD_RATIO_MIN >= BUF_LRU_OLD_RATIO_MAX"
#endif
#if BUF_LRU_OLD_RATIO_MAX > BUF_LRU_OLD_RATIO_DIV
# error "BUF_LRU_OLD_RATIO_MAX > BUF_LRU_OLD_RATIO_DIV"
#endif

/** Move blocks to "new" LRU list only if the first access was at
least this many milliseconds ago.  Not protected by any mutex or latch. */
extern uint	buf_LRU_old_threshold_ms;
/* @} */

/** @brief Statistics for selecting the LRU list for eviction.

These statistics are not 'of' LRU but 'for' LRU.  We keep count of I/O
and page_zip_decompress() operations.  Based on the statistics we decide
if we want to evict from buf_pool->unzip_LRU or buf_pool->LRU. */
struct buf_LRU_stat_t
{
	ulint	io;	/**< Counter of buffer pool I/O operations. */
	ulint	unzip;	/**< Counter of page_zip_decompress operations. */
};

/** Current operation counters.  Not protected by any mutex.
Cleared by buf_LRU_stat_update(). */
extern buf_LRU_stat_t	buf_LRU_stat_cur;

/** Running sum of past values of buf_LRU_stat_cur.
Updated by buf_LRU_stat_update().  Protected by buf_pool->mutex. */
extern buf_LRU_stat_t	buf_LRU_stat_sum;

/********************************************************************//**
Increments the I/O counter in buf_LRU_stat_cur. */
#define buf_LRU_stat_inc_io() buf_LRU_stat_cur.io++
/********************************************************************//**
Increments the page_zip_decompress() counter in buf_LRU_stat_cur. */
#define buf_LRU_stat_inc_unzip() buf_LRU_stat_cur.unzip++

#ifndef UNIV_NONINL
#include "buf0lru.ic"
#endif

#endif /* !UNIV_HOTBACKUP */

#endif
