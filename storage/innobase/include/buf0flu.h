/*****************************************************************************

Copyright (c) 1995, 2010, Innobase Oy. All Rights Reserved.

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
@file include/buf0flu.h
The database buffer pool flush algorithm

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#ifndef buf0flu_h
#define buf0flu_h

#include "univ.i"
#include "ut0byte.h"
#ifndef UNIV_HOTBACKUP
#include "mtr0types.h"
#include "buf0types.h"
#include "log0log.h"

/********************************************************************//**
Remove a block from the flush list of modified blocks. */
UNIV_INTERN
void
buf_flush_remove(
/*=============*/
	buf_page_t*	bpage);	/*!< in: pointer to the block in question */
/*******************************************************************//**
Relocates a buffer control block on the flush_list.
Note that it is assumed that the contents of bpage has already been
copied to dpage. */
UNIV_INTERN
void
buf_flush_relocate_on_flush_list(
/*=============================*/
	buf_page_t*	bpage,	/*!< in/out: control block being moved */
	buf_page_t*	dpage);	/*!< in/out: destination block */
/********************************************************************//**
Updates the flush system data structures when a write is completed. */
UNIV_INTERN
void
buf_flush_write_complete(
/*=====================*/
	buf_page_t*	bpage);	/*!< in: pointer to the block in question */
/*********************************************************************//**
Flushes pages from the end of the LRU list if there is too small
a margin of replaceable pages there. If buffer pool is NULL it
means flush free margin on all buffer pool instances. */
UNIV_INTERN
void
buf_flush_free_margin(
/*==================*/
	 buf_pool_t*	buf_pool);
/*********************************************************************//**
Flushes pages from the end of all the LRU lists. */
UNIV_INTERN
void
buf_flush_free_margins(void);
/*=========================*/
#endif /* !UNIV_HOTBACKUP */
/********************************************************************//**
Initializes a page for writing to the tablespace. */
UNIV_INTERN
void
buf_flush_init_for_writing(
/*=======================*/
	byte*		page,		/*!< in/out: page */
	void*		page_zip_,	/*!< in/out: compressed page, or NULL */
	ib_uint64_t	newest_lsn);	/*!< in: newest modification lsn
					to the page */
#ifndef UNIV_HOTBACKUP
# if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
/********************************************************************//**
Writes a flushable page asynchronously from the buffer pool to a file.
NOTE: buf_pool->mutex and block->mutex must be held upon entering this
function, and they will be released by this function after flushing.
This is loosely based on buf_flush_batch() and buf_flush_page().
@return TRUE if the page was flushed and the mutexes released */
UNIV_INTERN
ibool
buf_flush_page_try(
/*===============*/
	buf_pool_t*	buf_pool,	/*!< in/out: buffer pool instance */
	buf_block_t*	block)		/*!< in/out: buffer control block */
	__attribute__((nonnull, warn_unused_result));
# endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */
/*******************************************************************//**
This utility flushes dirty blocks from the end of the LRU list.
NOTE: The calling thread may own latches to pages: to avoid deadlocks,
this function must be written so that it cannot end up waiting for these
latches!
@return number of blocks for which the write request was queued;
ULINT_UNDEFINED if there was a flush of the same type already running */
UNIV_INTERN
ulint
buf_flush_LRU(
/*==========*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	ulint		min_n);		/*!< in: wished minimum mumber of blocks
					flushed (it is not guaranteed that the
					actual number is that big, though) */
/*******************************************************************//**
This utility flushes dirty blocks from the end of the flush_list of
all buffer pool instances.
NOTE: The calling thread is not allowed to own any latches on pages!
@return number of blocks for which the write request was queued;
ULINT_UNDEFINED if there was a flush of the same type already running */
UNIV_INTERN
ulint
buf_flush_list(
/*============*/
	ulint		min_n,		/*!< in: wished minimum mumber of blocks
					flushed (it is not guaranteed that the
					actual number is that big, though) */
	ib_uint64_t	lsn_limit);	/*!< in the case BUF_FLUSH_LIST all
					blocks whose oldest_modification is
					smaller than this should be flushed
					(if their number does not exceed
					min_n), otherwise ignored */
/******************************************************************//**
Waits until a flush batch of the given type ends */
UNIV_INTERN
void
buf_flush_wait_batch_end(
/*=====================*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	enum buf_flush	type);		/*!< in: BUF_FLUSH_LRU
					or BUF_FLUSH_LIST */
/******************************************************************//**
Waits until a flush batch of the given type ends. This is called by
a thread that only wants to wait for a flush to end but doesn't do
any flushing itself. */
UNIV_INTERN
void
buf_flush_wait_batch_end_wait_only(
/*===============================*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	enum buf_flush	type);		/*!< in: BUF_FLUSH_LRU
					or BUF_FLUSH_LIST */
/********************************************************************//**
This function should be called at a mini-transaction commit, if a page was
modified in it. Puts the block to the list of modified blocks, if it not
already in it. */
UNIV_INLINE
void
buf_flush_note_modification(
/*========================*/
	buf_block_t*	block,	/*!< in: block which is modified */
	mtr_t*		mtr);	/*!< in: mtr */
/********************************************************************//**
This function should be called when recovery has modified a buffer page. */
UNIV_INLINE
void
buf_flush_recv_note_modification(
/*=============================*/
	buf_block_t*	block,		/*!< in: block which is modified */
	ib_uint64_t	start_lsn,	/*!< in: start lsn of the first mtr in a
					set of mtr's */
	ib_uint64_t	end_lsn);	/*!< in: end lsn of the last mtr in the
					set of mtr's */
/********************************************************************//**
Returns TRUE if the file page block is immediately suitable for replacement,
i.e., transition FILE_PAGE => NOT_USED allowed.
@return	TRUE if can replace immediately */
UNIV_INTERN
ibool
buf_flush_ready_for_replace(
/*========================*/
	buf_page_t*	bpage);	/*!< in: buffer control block, must be
				buf_page_in_file(bpage) and in the LRU list */

/** @brief Statistics for selecting flush rate based on redo log
generation speed.

These statistics are generated for heuristics used in estimating the
rate at which we should flush the dirty blocks to avoid bursty IO
activity. Note that the rate of flushing not only depends on how many
dirty pages we have in the buffer pool but it is also a fucntion of
how much redo the workload is generating and at what rate. */

struct buf_flush_stat_struct
{
	ib_uint64_t	redo;		/**< amount of redo generated. */
	ulint		n_flushed;	/**< number of pages flushed. */
};

/** Statistics for selecting flush rate of dirty pages. */
typedef struct buf_flush_stat_struct buf_flush_stat_t;
/*********************************************************************
Update the historical stats that we are collecting for flush rate
heuristics at the end of each interval. */
UNIV_INTERN
void
buf_flush_stat_update(void);
/*=======================*/
/*********************************************************************
Determines the fraction of dirty pages that need to be flushed based
on the speed at which we generate redo log. Note that if redo log
is generated at significant rate without a corresponding increase
in the number of dirty pages (for example, an in-memory workload)
it can cause IO bursts of flushing. This function implements heuristics
to avoid this burstiness.
@return	number of dirty pages to be flushed / second */
UNIV_INTERN
ulint
buf_flush_get_desired_flush_rate(void);
/*==================================*/

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/******************************************************************//**
Validates the flush list.
@return	TRUE if ok */
UNIV_INTERN
ibool
buf_flush_validate(
/*===============*/
	buf_pool_t*	buf_pool);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

/********************************************************************//**
Initialize the red-black tree to speed up insertions into the flush_list
during recovery process. Should be called at the start of recovery
process before any page has been read/written. */
UNIV_INTERN
void
buf_flush_init_flush_rbt(void);
/*==========================*/

/********************************************************************//**
Frees up the red-black tree. */
UNIV_INTERN
void
buf_flush_free_flush_rbt(void);
/*==========================*/

/** When buf_flush_free_margin is called, it tries to make this many blocks
available to replacement in the free list and at the end of the LRU list (to
make sure that a read-ahead batch can be read efficiently in a single
sweep). */
#define BUF_FLUSH_FREE_BLOCK_MARGIN(b)	(5 + BUF_READ_AHEAD_AREA(b))
/** Extra margin to apply above BUF_FLUSH_FREE_BLOCK_MARGIN */
#define BUF_FLUSH_EXTRA_MARGIN(b)	((BUF_FLUSH_FREE_BLOCK_MARGIN(b) / 4 \
					+ 100) / srv_buf_pool_instances)
#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_NONINL
#include "buf0flu.ic"
#endif

#endif
