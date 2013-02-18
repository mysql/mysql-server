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
@file include/buf0flu.h
The database buffer pool flush algorithm

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#ifndef buf0flu_h
#define buf0flu_h

#include "univ.i"
#include "ut0byte.h"
#include "log0log.h"
#ifndef UNIV_HOTBACKUP
#include "mtr0types.h"
#include "buf0types.h"

/** Flag indicating if the page_cleaner is in active state. */
extern ibool buf_page_cleaner_is_active;

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
#endif /* !UNIV_HOTBACKUP */
/********************************************************************//**
Initializes a page for writing to the tablespace. */
UNIV_INTERN
void
buf_flush_init_for_writing(
/*=======================*/
	byte*	page,		/*!< in/out: page */
	void*	page_zip_,	/*!< in/out: compressed page, or NULL */
	lsn_t	newest_lsn);	/*!< in: newest modification lsn
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
/********************************************************************//**
Flush a batch of writes to the datafiles that have already been
written by the OS. */
UNIV_INTERN
void
buf_flush_sync_datafiles(void);
/*==========================*/
/*******************************************************************//**
This utility flushes dirty blocks from the end of the flush list of
all buffer pool instances.
NOTE: The calling thread is not allowed to own any latches on pages!
@return true if a batch was queued successfully for each buffer pool
instance. false if another batch of same type was already running in
at least one of the buffer pool instance */
UNIV_INTERN
bool
buf_flush_list(
/*===========*/
	ulint		min_n,		/*!< in: wished minimum mumber of blocks
					flushed (it is not guaranteed that the
					actual number is that big, though) */
	lsn_t		lsn_limit,	/*!< in the case BUF_FLUSH_LIST all
					blocks whose oldest_modification is
					smaller than this should be flushed
					(if their number does not exceed
					min_n), otherwise ignored */
	ulint*		n_processed);	/*!< out: the number of pages
					which were processed is passed
					back to caller. Ignored if NULL */
/******************************************************************//**
This function picks up a single dirty page from the tail of the LRU
list, flushes it, removes it from page_hash and LRU list and puts
it on the free list. It is called from user threads when they are
unable to find a replacable page at the tail of the LRU list i.e.:
when the background LRU flushing in the page_cleaner thread is not
fast enough to keep pace with the workload.
@return TRUE if success. */
UNIV_INTERN
ibool
buf_flush_single_page_from_LRU(
/*===========================*/
	buf_pool_t*	buf_pool);	/*!< in/out: buffer pool instance */
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
	lsn_t		start_lsn,	/*!< in: start lsn of the first mtr in a
					set of mtr's */
	lsn_t		end_lsn);	/*!< in: end lsn of the last mtr in the
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
/******************************************************************//**
page_cleaner thread tasked with flushing dirty pages from the buffer
pools. As of now we'll have only one instance of this thread.
@return a dummy parameter */
extern "C" UNIV_INTERN
os_thread_ret_t
DECLARE_THREAD(buf_flush_page_cleaner_thread)(
/*==========================================*/
	void*	arg);		/*!< in: a dummy parameter required by
				os_thread_create */
/*********************************************************************//**
Clears up tail of the LRU lists:
* Put replaceable pages at the tail of LRU to the free list
* Flush dirty pages at the tail of LRU to the disk
The depth to which we scan each buffer pool is controlled by dynamic
config parameter innodb_LRU_scan_depth.
@return total pages flushed */
UNIV_INTERN
ulint
buf_flush_LRU_tail(void);
/*====================*/
/*********************************************************************//**
Wait for any possible LRU flushes that are in progress to end. */
UNIV_INTERN
void
buf_flush_wait_LRU_batch_end(void);
/*==============================*/

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

/********************************************************************//**
Writes a flushable page asynchronously from the buffer pool to a file.
NOTE: in simulated aio we must call
os_aio_simulated_wake_handler_threads after we have posted a batch of
writes! NOTE: buf_pool->mutex and buf_page_get_mutex(bpage) must be
held upon entering this function, and they will be released by this
function. */
UNIV_INTERN
void
buf_flush_page(
/*===========*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	buf_page_t*	bpage,		/*!< in: buffer control block */
	buf_flush	flush_type)	/*!< in: type of flush */
	__attribute__((nonnull));

#ifdef UNIV_DEBUG
/******************************************************************//**
Check if there are any dirty pages that belong to a space id in the flush
list in a particular buffer pool.
@return	number of dirty pages present in a single buffer pool */
UNIV_INTERN
ulint
buf_pool_get_dirty_pages_count(
/*===========================*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool */
	ulint		id);		/*!< in: space id to check */
/******************************************************************//**
Check if there are any dirty pages that belong to a space id in the flush list.
@return	count of dirty pages present in all the buffer pools */
UNIV_INTERN
ulint
buf_flush_get_dirty_pages_count(
/*============================*/
	ulint		id);		/*!< in: space id to check */
#endif /* UNIV_DEBUG */

#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_NONINL
#include "buf0flu.ic"
#endif

#endif
