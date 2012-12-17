/*****************************************************************************

Copyright (c) 1995, 2011, Oracle and/or its affiliates. All Rights Reserved.

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
@file buf/buf0flu.c
The database buffer buf_pool flush algorithm

Created 11/11/1995 Heikki Tuuri
*******************************************************/

#include "buf0flu.h"

#ifdef UNIV_NONINL
#include "buf0flu.ic"
#endif

#include "buf0buf.h"
#include "srv0srv.h"
#include "page0zip.h"
#ifndef UNIV_HOTBACKUP
#include "ut0byte.h"
#include "ut0lst.h"
#include "page0page.h"
#include "fil0fil.h"
#include "buf0lru.h"
#include "buf0rea.h"
#include "ibuf0ibuf.h"
#include "log0log.h"
#include "os0file.h"
#include "trx0sys.h"
#include "mysql/plugin.h"
#include "mysql/service_thd_wait.h"

/**********************************************************************
These statistics are generated for heuristics used in estimating the
rate at which we should flush the dirty blocks to avoid bursty IO
activity. Note that the rate of flushing not only depends on how many
dirty pages we have in the buffer pool but it is also a fucntion of
how much redo the workload is generating and at what rate. */
/* @{ */

/** Number of intervals for which we keep the history of these stats.
Each interval is 1 second, defined by the rate at which
srv_error_monitor_thread() calls buf_flush_stat_update(). */
#define BUF_FLUSH_STAT_N_INTERVAL 20

/** Sampled values buf_flush_stat_cur.
Not protected by any mutex.  Updated by buf_flush_stat_update(). */
static buf_flush_stat_t	buf_flush_stat_arr[BUF_FLUSH_STAT_N_INTERVAL];

/** Cursor to buf_flush_stat_arr[]. Updated in a round-robin fashion. */
static ulint		buf_flush_stat_arr_ind;

/** Values at start of the current interval. Reset by
buf_flush_stat_update(). */
static buf_flush_stat_t	buf_flush_stat_cur;

/** Running sum of past values of buf_flush_stat_cur.
Updated by buf_flush_stat_update(). Not protected by any mutex. */
static buf_flush_stat_t	buf_flush_stat_sum;

/** Number of pages flushed through non flush_list flushes. */
static ulint buf_lru_flush_page_count = 0;

/* @} */

/******************************************************************//**
Increases flush_list size in bytes with zip_size for compressed page,
UNIV_PAGE_SIZE for uncompressed page in inline function */
static inline
void
incr_flush_list_size_in_bytes(
/*==========================*/
	buf_block_t*	block,		/*!< in: control block */
	buf_pool_t*	buf_pool)	/*!< in: buffer pool instance */
{
	ulint		zip_size;
	ut_ad(buf_flush_list_mutex_own(buf_pool));
	zip_size = page_zip_get_size(&block->page.zip);
	buf_pool->stat.flush_list_bytes += zip_size ? zip_size : UNIV_PAGE_SIZE;
	ut_ad(buf_pool->stat.flush_list_bytes <= buf_pool->curr_pool_size);
}

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/******************************************************************//**
Validates the flush list.
@return	TRUE if ok */
static
ibool
buf_flush_validate_low(
/*===================*/
	buf_pool_t*	buf_pool);	/*!< in: Buffer pool instance */

/******************************************************************//**
Validates the flush list some of the time.
@return	TRUE if ok or the check was skipped */
static
ibool
buf_flush_validate_skip(
/*====================*/
	buf_pool_t*	buf_pool)	/*!< in: Buffer pool instance */
{
/** Try buf_flush_validate_low() every this many times */
# define BUF_FLUSH_VALIDATE_SKIP	23

	/** The buf_flush_validate_low() call skip counter.
	Use a signed type because of the race condition below. */
	static int buf_flush_validate_count = BUF_FLUSH_VALIDATE_SKIP;

	/* There is a race condition below, but it does not matter,
	because this call is only for heuristic purposes. We want to
	reduce the call frequency of the costly buf_flush_validate_low()
	check in debug builds. */
	if (--buf_flush_validate_count > 0) {
		return(TRUE);
	}

	buf_flush_validate_count = BUF_FLUSH_VALIDATE_SKIP;
	return(buf_flush_validate_low(buf_pool));
}
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

/******************************************************************//**
Insert a block in the flush_rbt and returns a pointer to its
predecessor or NULL if no predecessor. The ordering is maintained
on the basis of the <oldest_modification, space, offset> key.
@return	pointer to the predecessor or NULL if no predecessor. */
static
buf_page_t*
buf_flush_insert_in_flush_rbt(
/*==========================*/
	buf_page_t*	bpage)	/*!< in: bpage to be inserted. */
{
	const ib_rbt_node_t*	c_node;
	const ib_rbt_node_t*	p_node;
	buf_page_t*		prev = NULL;
	buf_pool_t*		buf_pool = buf_pool_from_bpage(bpage);

	ut_ad(buf_flush_list_mutex_own(buf_pool));

	/* Insert this buffer into the rbt. */
	c_node = rbt_insert(buf_pool->flush_rbt, &bpage, &bpage);
	ut_a(c_node != NULL);

	/* Get the predecessor. */
	p_node = rbt_prev(buf_pool->flush_rbt, c_node);

	if (p_node != NULL) {
		buf_page_t**	value;
		value = rbt_value(buf_page_t*, p_node);
		prev = *value;
		ut_a(prev != NULL);
	}

	return(prev);
}

/*********************************************************//**
Delete a bpage from the flush_rbt. */
static
void
buf_flush_delete_from_flush_rbt(
/*============================*/
	buf_page_t*	bpage)	/*!< in: bpage to be removed. */
{
#ifdef UNIV_DEBUG
	ibool		ret = FALSE;
#endif /* UNIV_DEBUG */
	buf_pool_t*	buf_pool = buf_pool_from_bpage(bpage);

	ut_ad(buf_flush_list_mutex_own(buf_pool));

#ifdef UNIV_DEBUG
	ret =
#endif /* UNIV_DEBUG */
	rbt_delete(buf_pool->flush_rbt, &bpage);

	ut_ad(ret);
}

/*****************************************************************//**
Compare two modified blocks in the buffer pool. The key for comparison
is:
key = <oldest_modification, space, offset>
This comparison is used to maintian ordering of blocks in the
buf_pool->flush_rbt.
Note that for the purpose of flush_rbt, we only need to order blocks
on the oldest_modification. The other two fields are used to uniquely
identify the blocks.
@return	 < 0 if b2 < b1, 0 if b2 == b1, > 0 if b2 > b1 */
static
int
buf_flush_block_cmp(
/*================*/
	const void*	p1,		/*!< in: block1 */
	const void*	p2)		/*!< in: block2 */
{
	int			ret;
	const buf_page_t*	b1 = *(const buf_page_t**) p1;
	const buf_page_t*	b2 = *(const buf_page_t**) p2;
#ifdef UNIV_DEBUG
	buf_pool_t*		buf_pool = buf_pool_from_bpage(b1);
#endif /* UNIV_DEBUG */

	ut_ad(b1 != NULL);
	ut_ad(b2 != NULL);

	ut_ad(buf_flush_list_mutex_own(buf_pool));

	ut_ad(b1->in_flush_list);
	ut_ad(b2->in_flush_list);

	if (b2->oldest_modification > b1->oldest_modification) {
		return(1);
	} else if (b2->oldest_modification < b1->oldest_modification) {
		return(-1);
	}

	/* If oldest_modification is same then decide on the space. */
	ret = (int)(b2->space - b1->space);

	/* Or else decide ordering on the offset field. */
	return(ret ? ret : (int)(b2->offset - b1->offset));
}

/********************************************************************//**
Initialize the red-black tree to speed up insertions into the flush_list
during recovery process. Should be called at the start of recovery
process before any page has been read/written. */
UNIV_INTERN
void
buf_flush_init_flush_rbt(void)
/*==========================*/
{
	ulint	i;

	for (i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		buf_flush_list_mutex_enter(buf_pool);

		/* Create red black tree for speedy insertions in flush list. */
		buf_pool->flush_rbt = rbt_create(
			sizeof(buf_page_t*), buf_flush_block_cmp);

		buf_flush_list_mutex_exit(buf_pool);
	}
}

/********************************************************************//**
Frees up the red-black tree. */
UNIV_INTERN
void
buf_flush_free_flush_rbt(void)
/*==========================*/
{
	ulint	i;

	for (i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		buf_flush_list_mutex_enter(buf_pool);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
		ut_a(buf_flush_validate_low(buf_pool));
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

		rbt_free(buf_pool->flush_rbt);
		buf_pool->flush_rbt = NULL;

		buf_flush_list_mutex_exit(buf_pool);
	}
}

/********************************************************************//**
Inserts a modified block into the flush list. */
UNIV_INTERN
void
buf_flush_insert_into_flush_list(
/*=============================*/
	buf_pool_t*	buf_pool,	/*!< buffer pool instance */
	buf_block_t*	block,		/*!< in/out: block which is modified */
	ib_uint64_t	lsn)		/*!< in: oldest modification */
{
	ut_ad(!buf_pool_mutex_own(buf_pool));
	ut_ad(log_flush_order_mutex_own());
	ut_ad(mutex_own(&block->mutex));

	buf_flush_list_mutex_enter(buf_pool);

	ut_ad((UT_LIST_GET_FIRST(buf_pool->flush_list) == NULL)
	      || (UT_LIST_GET_FIRST(buf_pool->flush_list)->oldest_modification
		  <= lsn));

	/* If we are in the recovery then we need to update the flush
	red-black tree as well. */
	if (UNIV_LIKELY_NULL(buf_pool->flush_rbt)) {
		buf_flush_list_mutex_exit(buf_pool);
		buf_flush_insert_sorted_into_flush_list(buf_pool, block, lsn);
		return;
	}

	ut_ad(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);
	ut_ad(!block->page.in_flush_list);

	ut_d(block->page.in_flush_list = TRUE);
	block->page.oldest_modification = lsn;
	UT_LIST_ADD_FIRST(list, buf_pool->flush_list, &block->page);
	incr_flush_list_size_in_bytes(block, buf_pool);

#ifdef UNIV_DEBUG_VALGRIND
	{
		ulint	zip_size = buf_block_get_zip_size(block);

		if (UNIV_UNLIKELY(zip_size)) {
			UNIV_MEM_ASSERT_RW(block->page.zip.data, zip_size);
		} else {
			UNIV_MEM_ASSERT_RW(block->frame, UNIV_PAGE_SIZE);
		}
	}
#endif /* UNIV_DEBUG_VALGRIND */
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ut_a(buf_flush_validate_skip(buf_pool));
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

	buf_flush_list_mutex_exit(buf_pool);
}

/********************************************************************//**
Inserts a modified block into the flush list in the right sorted position.
This function is used by recovery, because there the modifications do not
necessarily come in the order of lsn's. */
UNIV_INTERN
void
buf_flush_insert_sorted_into_flush_list(
/*====================================*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	buf_block_t*	block,		/*!< in/out: block which is modified */
	ib_uint64_t	lsn)		/*!< in: oldest modification */
{
	buf_page_t*	prev_b;
	buf_page_t*	b;

	ut_ad(!buf_pool_mutex_own(buf_pool));
	ut_ad(log_flush_order_mutex_own());
	ut_ad(mutex_own(&block->mutex));
	ut_ad(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);

	buf_flush_list_mutex_enter(buf_pool);

	/* The field in_LRU_list is protected by buf_pool->mutex, which
	we are not holding.  However, while a block is in the flush
	list, it is dirty and cannot be discarded, not from the
	page_hash or from the LRU list.  At most, the uncompressed
	page frame of a compressed block may be discarded or created
	(copying the block->page to or from a buf_page_t that is
	dynamically allocated from buf_buddy_alloc()).  Because those
	transitions hold block->mutex and the flush list mutex (via
	buf_flush_relocate_on_flush_list()), there is no possibility
	of a race condition in the assertions below. */
	ut_ad(block->page.in_LRU_list);
	ut_ad(block->page.in_page_hash);
	/* buf_buddy_block_register() will take a block in the
	BUF_BLOCK_MEMORY state, not a file page. */
	ut_ad(!block->page.in_zip_hash);

	ut_ad(!block->page.in_flush_list);
	ut_d(block->page.in_flush_list = TRUE);
	block->page.oldest_modification = lsn;

#ifdef UNIV_DEBUG_VALGRIND
	{
		ulint	zip_size = buf_block_get_zip_size(block);

		if (UNIV_UNLIKELY(zip_size)) {
			UNIV_MEM_ASSERT_RW(block->page.zip.data, zip_size);
		} else {
			UNIV_MEM_ASSERT_RW(block->frame, UNIV_PAGE_SIZE);
		}
	}
#endif /* UNIV_DEBUG_VALGRIND */

	prev_b = NULL;

	/* For the most part when this function is called the flush_rbt
	should not be NULL. In a very rare boundary case it is possible
	that the flush_rbt has already been freed by the recovery thread
	before the last page was hooked up in the flush_list by the
	io-handler thread. In that case we'll  just do a simple
	linear search in the else block. */
	if (buf_pool->flush_rbt) {

		prev_b = buf_flush_insert_in_flush_rbt(&block->page);

	} else {

		b = UT_LIST_GET_FIRST(buf_pool->flush_list);

		while (b && b->oldest_modification
		       > block->page.oldest_modification) {
			ut_ad(b->in_flush_list);
			prev_b = b;
			b = UT_LIST_GET_NEXT(list, b);
		}
	}

	if (prev_b == NULL) {
		UT_LIST_ADD_FIRST(list, buf_pool->flush_list, &block->page);
	} else {
		UT_LIST_INSERT_AFTER(list, buf_pool->flush_list,
				     prev_b, &block->page);
	}

	incr_flush_list_size_in_bytes(block, buf_pool);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ut_a(buf_flush_validate_low(buf_pool));
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

	buf_flush_list_mutex_exit(buf_pool);
}

/********************************************************************//**
Returns TRUE if the file page block is immediately suitable for replacement,
i.e., the transition FILE_PAGE => NOT_USED allowed.
@return	TRUE if can replace immediately */
UNIV_INTERN
ibool
buf_flush_ready_for_replace(
/*========================*/
	buf_page_t*	bpage)	/*!< in: buffer control block, must be
				buf_page_in_file(bpage) and in the LRU list */
{
#ifdef UNIV_DEBUG
	buf_pool_t*	buf_pool = buf_pool_from_bpage(bpage);
	ut_ad(buf_pool_mutex_own(buf_pool));
#endif
	ut_ad(mutex_own(buf_page_get_mutex(bpage)));
	ut_ad(bpage->in_LRU_list);

	if (UNIV_LIKELY(buf_page_in_file(bpage))) {

		return(bpage->oldest_modification == 0
		       && buf_page_get_io_fix(bpage) == BUF_IO_NONE
		       && bpage->buf_fix_count == 0);
	}

	ut_print_timestamp(stderr);
	fprintf(stderr,
		"  InnoDB: Error: buffer block state %lu"
		" in the LRU list!\n",
		(ulong) buf_page_get_state(bpage));
	ut_print_buf(stderr, bpage, sizeof(buf_page_t));
	putc('\n', stderr);

	return(FALSE);
}

/********************************************************************//**
Returns TRUE if the block is modified and ready for flushing.
@return	TRUE if can flush immediately */
UNIV_INLINE
ibool
buf_flush_ready_for_flush(
/*======================*/
	buf_page_t*	bpage,	/*!< in: buffer control block, must be
				buf_page_in_file(bpage) */
	enum buf_flush	flush_type)/*!< in: BUF_FLUSH_LRU or BUF_FLUSH_LIST */
{
#ifdef UNIV_DEBUG
	buf_pool_t*	buf_pool = buf_pool_from_bpage(bpage);
	ut_ad(buf_pool_mutex_own(buf_pool));
#endif
	ut_a(buf_page_in_file(bpage));
	ut_ad(mutex_own(buf_page_get_mutex(bpage)));
	ut_ad(flush_type == BUF_FLUSH_LRU || BUF_FLUSH_LIST);

	if (bpage->oldest_modification != 0
	    && buf_page_get_io_fix(bpage) == BUF_IO_NONE) {
		ut_ad(bpage->in_flush_list);

		if (flush_type != BUF_FLUSH_LRU) {

			return(TRUE);

		} else if (bpage->buf_fix_count == 0) {

			/* If we are flushing the LRU list, to avoid deadlocks
			we require the block not to be bufferfixed, and hence
			not latched. */

			return(TRUE);
		}
	}

	return(FALSE);
}

/********************************************************************//**
Remove a block from the flush list of modified blocks. */
UNIV_INTERN
void
buf_flush_remove(
/*=============*/
	buf_page_t*	bpage)	/*!< in: pointer to the block in question */
{
	buf_pool_t*	buf_pool = buf_pool_from_bpage(bpage);
	ulint		zip_size;

	ut_ad(buf_pool_mutex_own(buf_pool));
	ut_ad(mutex_own(buf_page_get_mutex(bpage)));
	ut_ad(bpage->in_flush_list);

	buf_flush_list_mutex_enter(buf_pool);

	switch (buf_page_get_state(bpage)) {
	case BUF_BLOCK_ZIP_PAGE:
		/* Clean compressed pages should not be on the flush list */
	case BUF_BLOCK_ZIP_FREE:
	case BUF_BLOCK_NOT_USED:
	case BUF_BLOCK_READY_FOR_USE:
	case BUF_BLOCK_MEMORY:
	case BUF_BLOCK_REMOVE_HASH:
		ut_error;
		return;
	case BUF_BLOCK_ZIP_DIRTY:
		buf_page_set_state(bpage, BUF_BLOCK_ZIP_PAGE);
		UT_LIST_REMOVE(list, buf_pool->flush_list, bpage);
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
		buf_LRU_insert_zip_clean(bpage);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
		break;
	case BUF_BLOCK_FILE_PAGE:
		UT_LIST_REMOVE(list, buf_pool->flush_list, bpage);
		break;
	}

	/* If the flush_rbt is active then delete from there as well. */
	if (UNIV_LIKELY_NULL(buf_pool->flush_rbt)) {
		buf_flush_delete_from_flush_rbt(bpage);
	}

	/* Must be done after we have removed it from the flush_rbt
	because we assert on in_flush_list in comparison function. */
	ut_d(bpage->in_flush_list = FALSE);

	zip_size = page_zip_get_size(&bpage->zip);
	buf_pool->stat.flush_list_bytes -= zip_size ? zip_size : UNIV_PAGE_SIZE;

	bpage->oldest_modification = 0;

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ut_a(buf_flush_validate_skip(buf_pool));
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

	buf_flush_list_mutex_exit(buf_pool);
}

/*******************************************************************//**
Relocates a buffer control block on the flush_list.
Note that it is assumed that the contents of bpage have already been
copied to dpage.
IMPORTANT: When this function is called bpage and dpage are not
exact copies of each other. For example, they both will have different
::state. Also the ::list pointers in dpage may be stale. We need to
use the current list node (bpage) to do the list manipulation because
the list pointers could have changed between the time that we copied
the contents of bpage to the dpage and the flush list manipulation
below. */
UNIV_INTERN
void
buf_flush_relocate_on_flush_list(
/*=============================*/
	buf_page_t*	bpage,	/*!< in/out: control block being moved */
	buf_page_t*	dpage)	/*!< in/out: destination block */
{
	buf_page_t*	prev;
	buf_page_t* 	prev_b = NULL;
	buf_pool_t*	buf_pool = buf_pool_from_bpage(bpage);

	ut_ad(buf_pool_mutex_own(buf_pool));
	/* Must reside in the same buffer pool. */
	ut_ad(buf_pool == buf_pool_from_bpage(dpage));

	ut_ad(mutex_own(buf_page_get_mutex(bpage)));

	buf_flush_list_mutex_enter(buf_pool);

	/* FIXME: At this point we have both buf_pool and flush_list
	mutexes. Theoretically removal of a block from flush list is
	only covered by flush_list mutex but currently we do
	have buf_pool mutex in buf_flush_remove() therefore this block
	is guaranteed to be in the flush list. We need to check if
	this will work without the assumption of block removing code
	having the buf_pool mutex. */
	ut_ad(bpage->in_flush_list);
	ut_ad(dpage->in_flush_list);

	/* If recovery is active we must swap the control blocks in
	the flush_rbt as well. */
	if (UNIV_LIKELY_NULL(buf_pool->flush_rbt)) {
		buf_flush_delete_from_flush_rbt(bpage);
		prev_b = buf_flush_insert_in_flush_rbt(dpage);
	}

	/* Must be done after we have removed it from the flush_rbt
	because we assert on in_flush_list in comparison function. */
	ut_d(bpage->in_flush_list = FALSE);

	prev = UT_LIST_GET_PREV(list, bpage);
	UT_LIST_REMOVE(list, buf_pool->flush_list, bpage);

	if (prev) {
		ut_ad(prev->in_flush_list);
		UT_LIST_INSERT_AFTER(
			list,
			buf_pool->flush_list,
			prev, dpage);
	} else {
		UT_LIST_ADD_FIRST(
			list,
			buf_pool->flush_list,
			dpage);
	}

	/* Just an extra check. Previous in flush_list
	should be the same control block as in flush_rbt. */
	ut_a(!buf_pool->flush_rbt || prev_b == prev);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ut_a(buf_flush_validate_low(buf_pool));
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

	buf_flush_list_mutex_exit(buf_pool);
}

/********************************************************************//**
Updates the flush system data structures when a write is completed. */
UNIV_INTERN
void
buf_flush_write_complete(
/*=====================*/
	buf_page_t*	bpage)	/*!< in: pointer to the block in question */
{
	enum buf_flush	flush_type;
	buf_pool_t*	buf_pool = buf_pool_from_bpage(bpage);

	ut_ad(bpage);

	buf_flush_remove(bpage);

	flush_type = buf_page_get_flush_type(bpage);
	buf_pool->n_flush[flush_type]--;

	if (flush_type == BUF_FLUSH_LRU) {
		/* Put the block to the end of the LRU list to wait to be
		moved to the free list */

		buf_LRU_make_block_old(bpage);

		buf_pool->LRU_flush_ended++;
	}

	/* fprintf(stderr, "n pending flush %lu\n",
	buf_pool->n_flush[flush_type]); */

	if (buf_pool->n_flush[flush_type] == 0
	    && buf_pool->init_flush[flush_type] == FALSE) {

		/* The running flush batch has ended */

		os_event_set(buf_pool->no_flush[flush_type]);
	}
}

/********************************************************************//**
Flush a batch of writes to the datafiles that have already been
written by the OS. */
static
void
buf_flush_sync_datafiles(void)
/*==========================*/
{
	/* Wake possible simulated aio thread to actually post the
	writes to the operating system */
	os_aio_simulated_wake_handler_threads();

	/* Wait that all async writes to tablespaces have been posted to
	the OS */
	os_aio_wait_until_no_pending_writes();

	/* Now we flush the data to disk (for example, with fsync) */
	fil_flush_file_spaces(FIL_TABLESPACE);

	return;
}

/********************************************************************//**
Flushes possible buffered writes from the doublewrite memory buffer to disk,
and also wakes up the aio thread if simulated aio is used. It is very
important to call this function after a batch of writes has been posted,
and also when we may have to wait for a page latch! Otherwise a deadlock
of threads can occur. */
static
void
buf_flush_buffered_writes(void)
/*===========================*/
{
	byte*		write_buf;
	ulint		len;
	ulint		len2;
	ulint		i;

	if (!srv_use_doublewrite_buf || trx_doublewrite == NULL) {
		/* Sync the writes to the disk. */
		buf_flush_sync_datafiles();
		return;
	}

	mutex_enter(&(trx_doublewrite->mutex));

	/* Write first to doublewrite buffer blocks. We use synchronous
	aio and thus know that file write has been completed when the
	control returns. */

	if (trx_doublewrite->first_free == 0) {

		mutex_exit(&(trx_doublewrite->mutex));

		return;
	}

	for (i = 0; i < trx_doublewrite->first_free; i++) {

		const buf_block_t*	block;

		block = (buf_block_t*) trx_doublewrite->buf_block_arr[i];

		if (buf_block_get_state(block) != BUF_BLOCK_FILE_PAGE
		    || block->page.zip.data) {
			/* No simple validate for compressed pages exists. */
			continue;
		}

		if (UNIV_UNLIKELY
		    (memcmp(block->frame + (FIL_PAGE_LSN + 4),
			    block->frame + (UNIV_PAGE_SIZE
					    - FIL_PAGE_END_LSN_OLD_CHKSUM + 4),
			    4))) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: ERROR: The page to be written"
				" seems corrupt!\n"
				"InnoDB: The lsn fields do not match!"
				" Noticed in the buffer pool\n"
				"InnoDB: before posting to the"
				" doublewrite buffer.\n");
		}

		if (!block->check_index_page_at_flush) {
		} else if (page_is_comp(block->frame)) {
			if (UNIV_UNLIKELY
			    (!page_simple_validate_new(block->frame))) {
corrupted_page:
				buf_page_print(block->frame, 0,
					       BUF_PAGE_PRINT_NO_CRASH);

				ut_print_timestamp(stderr);
				fprintf(stderr,
					"  InnoDB: Apparent corruption of an"
					" index page n:o %lu in space %lu\n"
					"InnoDB: to be written to data file."
					" We intentionally crash server\n"
					"InnoDB: to prevent corrupt data"
					" from ending up in data\n"
					"InnoDB: files.\n",
					(ulong) buf_block_get_page_no(block),
					(ulong) buf_block_get_space(block));

				ut_error;
			}
		} else if (UNIV_UNLIKELY
			   (!page_simple_validate_old(block->frame))) {

			goto corrupted_page;
		}
	}

	/* increment the doublewrite flushed pages counter */
	srv_dblwr_pages_written+= trx_doublewrite->first_free;
	srv_dblwr_writes++;

	len = ut_min(TRX_SYS_DOUBLEWRITE_BLOCK_SIZE,
		     trx_doublewrite->first_free) * UNIV_PAGE_SIZE;

	write_buf = trx_doublewrite->write_buf;
	i = 0;

	fil_io(OS_FILE_WRITE, TRUE, TRX_SYS_SPACE, 0,
	       trx_doublewrite->block1, 0, len,
	       (void*) write_buf, NULL);

	for (len2 = 0; len2 + UNIV_PAGE_SIZE <= len;
	     len2 += UNIV_PAGE_SIZE, i++) {
		const buf_block_t* block = (buf_block_t*)
			trx_doublewrite->buf_block_arr[i];

		if (UNIV_LIKELY(!block->page.zip.data)
		    && UNIV_LIKELY(buf_block_get_state(block)
				   == BUF_BLOCK_FILE_PAGE)
		    && UNIV_UNLIKELY
		    (memcmp(write_buf + len2 + (FIL_PAGE_LSN + 4),
			    write_buf + len2
			    + (UNIV_PAGE_SIZE
			       - FIL_PAGE_END_LSN_OLD_CHKSUM + 4), 4))) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: ERROR: The page to be written"
				" seems corrupt!\n"
				"InnoDB: The lsn fields do not match!"
				" Noticed in the doublewrite block1.\n");
		}
	}

	if (trx_doublewrite->first_free <= TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) {
		goto flush;
	}

	len = (trx_doublewrite->first_free - TRX_SYS_DOUBLEWRITE_BLOCK_SIZE)
		* UNIV_PAGE_SIZE;

	write_buf = trx_doublewrite->write_buf
		+ TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * UNIV_PAGE_SIZE;
	ut_ad(i == TRX_SYS_DOUBLEWRITE_BLOCK_SIZE);

	fil_io(OS_FILE_WRITE, TRUE, TRX_SYS_SPACE, 0,
	       trx_doublewrite->block2, 0, len,
	       (void*) write_buf, NULL);

	for (len2 = 0; len2 + UNIV_PAGE_SIZE <= len;
	     len2 += UNIV_PAGE_SIZE, i++) {
		const buf_block_t* block = (buf_block_t*)
			trx_doublewrite->buf_block_arr[i];

		if (UNIV_LIKELY(!block->page.zip.data)
		    && UNIV_LIKELY(buf_block_get_state(block)
				   == BUF_BLOCK_FILE_PAGE)
		    && UNIV_UNLIKELY
		    (memcmp(write_buf + len2 + (FIL_PAGE_LSN + 4),
			    write_buf + len2
			    + (UNIV_PAGE_SIZE
			       - FIL_PAGE_END_LSN_OLD_CHKSUM + 4), 4))) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: ERROR: The page to be"
				" written seems corrupt!\n"
				"InnoDB: The lsn fields do not match!"
				" Noticed in"
				" the doublewrite block2.\n");
		}
	}

flush:
	/* Now flush the doublewrite buffer data to disk */

	fil_flush(TRX_SYS_SPACE);

	/* We know that the writes have been flushed to disk now
	and in recovery we will find them in the doublewrite buffer
	blocks. Next do the writes to the intended positions. */

	for (i = 0; i < trx_doublewrite->first_free; i++) {
		const buf_block_t* block = (buf_block_t*)
			trx_doublewrite->buf_block_arr[i];

		ut_a(buf_page_in_file(&block->page));
		if (UNIV_LIKELY_NULL(block->page.zip.data)) {
			fil_io(OS_FILE_WRITE | OS_AIO_SIMULATED_WAKE_LATER,
			       FALSE, buf_page_get_space(&block->page),
			       buf_page_get_zip_size(&block->page),
			       buf_page_get_page_no(&block->page), 0,
			       buf_page_get_zip_size(&block->page),
			       (void*)block->page.zip.data,
			       (void*)block);

			/* Increment the counter of I/O operations used
			for selecting LRU policy. */
			buf_LRU_stat_inc_io();

			continue;
		}

		ut_a(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);

		if (UNIV_UNLIKELY(memcmp(block->frame + (FIL_PAGE_LSN + 4),
					 block->frame
					 + (UNIV_PAGE_SIZE
					    - FIL_PAGE_END_LSN_OLD_CHKSUM + 4),
					 4))) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: ERROR: The page to be written"
				" seems corrupt!\n"
				"InnoDB: The lsn fields do not match!"
				" Noticed in the buffer pool\n"
				"InnoDB: after posting and flushing"
				" the doublewrite buffer.\n"
				"InnoDB: Page buf fix count %lu,"
				" io fix %lu, state %lu\n",
				(ulong)block->page.buf_fix_count,
				(ulong)buf_block_get_io_fix(block),
				(ulong)buf_block_get_state(block));
		}

		fil_io(OS_FILE_WRITE | OS_AIO_SIMULATED_WAKE_LATER,
		       FALSE, buf_block_get_space(block), 0,
		       buf_block_get_page_no(block), 0, UNIV_PAGE_SIZE,
		       (void*)block->frame, (void*)block);

		/* Increment the counter of I/O operations used
		for selecting LRU policy. */
		buf_LRU_stat_inc_io();
	}

	/* Sync the writes to the disk. */
	buf_flush_sync_datafiles();

	/* We can now reuse the doublewrite memory buffer: */
	trx_doublewrite->first_free = 0;

	mutex_exit(&(trx_doublewrite->mutex));
}

/********************************************************************//**
Posts a buffer page for writing. If the doublewrite memory buffer is
full, calls buf_flush_buffered_writes and waits for for free space to
appear. */
static
void
buf_flush_post_to_doublewrite_buf(
/*==============================*/
	buf_page_t*	bpage)	/*!< in: buffer block to write */
{
	ulint	zip_size;
try_again:
	mutex_enter(&(trx_doublewrite->mutex));

	ut_a(buf_page_in_file(bpage));

	if (trx_doublewrite->first_free
	    >= 2 * TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) {
		mutex_exit(&(trx_doublewrite->mutex));

		buf_flush_buffered_writes();

		goto try_again;
	}

	zip_size = buf_page_get_zip_size(bpage);

	if (UNIV_UNLIKELY(zip_size)) {
		UNIV_MEM_ASSERT_RW(bpage->zip.data, zip_size);
		/* Copy the compressed page and clear the rest. */
		memcpy(trx_doublewrite->write_buf
		       + UNIV_PAGE_SIZE * trx_doublewrite->first_free,
		       bpage->zip.data, zip_size);
		memset(trx_doublewrite->write_buf
		       + UNIV_PAGE_SIZE * trx_doublewrite->first_free
		       + zip_size, 0, UNIV_PAGE_SIZE - zip_size);
	} else {
		ut_a(buf_page_get_state(bpage) == BUF_BLOCK_FILE_PAGE);
		UNIV_MEM_ASSERT_RW(((buf_block_t*) bpage)->frame,
				   UNIV_PAGE_SIZE);

		memcpy(trx_doublewrite->write_buf
		       + UNIV_PAGE_SIZE * trx_doublewrite->first_free,
		       ((buf_block_t*) bpage)->frame, UNIV_PAGE_SIZE);
	}

	trx_doublewrite->buf_block_arr[trx_doublewrite->first_free] = bpage;

	trx_doublewrite->first_free++;

	if (trx_doublewrite->first_free
	    >= 2 * TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) {
		mutex_exit(&(trx_doublewrite->mutex));

		buf_flush_buffered_writes();

		return;
	}

	mutex_exit(&(trx_doublewrite->mutex));
}
#endif /* !UNIV_HOTBACKUP */

/********************************************************************//**
Initializes a page for writing to the tablespace. */
UNIV_INTERN
void
buf_flush_init_for_writing(
/*=======================*/
	byte*		page,		/*!< in/out: page */
	void*		page_zip_,	/*!< in/out: compressed page, or NULL */
	ib_uint64_t	newest_lsn)	/*!< in: newest modification lsn
					to the page */
{
	ut_ad(page);

	if (page_zip_) {
		page_zip_des_t*	page_zip = page_zip_;
		ulint		zip_size = page_zip_get_size(page_zip);
		ut_ad(zip_size);
		ut_ad(ut_is_2pow(zip_size));
		ut_ad(zip_size <= UNIV_PAGE_SIZE);

		switch (UNIV_EXPECT(fil_page_get_type(page), FIL_PAGE_INDEX)) {
		case FIL_PAGE_TYPE_ALLOCATED:
		case FIL_PAGE_INODE:
		case FIL_PAGE_IBUF_BITMAP:
		case FIL_PAGE_TYPE_FSP_HDR:
		case FIL_PAGE_TYPE_XDES:
			/* These are essentially uncompressed pages. */
			memcpy(page_zip->data, page, zip_size);
			/* fall through */
		case FIL_PAGE_TYPE_ZBLOB:
		case FIL_PAGE_TYPE_ZBLOB2:
		case FIL_PAGE_INDEX:
			mach_write_to_8(page_zip->data
					+ FIL_PAGE_LSN, newest_lsn);
			memset(page_zip->data + FIL_PAGE_FILE_FLUSH_LSN, 0, 8);
			mach_write_to_4(page_zip->data
					+ FIL_PAGE_SPACE_OR_CHKSUM,
					srv_use_checksums
					? page_zip_calc_checksum(
						page_zip->data, zip_size)
					: BUF_NO_CHECKSUM_MAGIC);
			return;
		}

		ut_print_timestamp(stderr);
		fputs("  InnoDB: ERROR: The compressed page to be written"
		      " seems corrupt:", stderr);
		ut_print_buf(stderr, page, zip_size);
		fputs("\nInnoDB: Possibly older version of the page:", stderr);
		ut_print_buf(stderr, page_zip->data, zip_size);
		putc('\n', stderr);
		ut_error;
	}

	/* Write the newest modification lsn to the page header and trailer */
	mach_write_to_8(page + FIL_PAGE_LSN, newest_lsn);

	mach_write_to_8(page + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM,
			newest_lsn);

	/* Store the new formula checksum */

	mach_write_to_4(page + FIL_PAGE_SPACE_OR_CHKSUM,
			srv_use_checksums
			? buf_calc_page_new_checksum(page)
			: BUF_NO_CHECKSUM_MAGIC);

	/* We overwrite the first 4 bytes of the end lsn field to store
	the old formula checksum. Since it depends also on the field
	FIL_PAGE_SPACE_OR_CHKSUM, it has to be calculated after storing the
	new formula checksum. */

	mach_write_to_4(page + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM,
			srv_use_checksums
			? buf_calc_page_old_checksum(page)
			: BUF_NO_CHECKSUM_MAGIC);
}

#ifndef UNIV_HOTBACKUP
/********************************************************************//**
Does an asynchronous write of a buffer page. NOTE: in simulated aio and
also when the doublewrite buffer is used, we must call
buf_flush_buffered_writes after we have posted a batch of writes! */
static
void
buf_flush_write_block_low(
/*======================*/
	buf_page_t*	bpage)	/*!< in: buffer block to write */
{
	ulint	zip_size	= buf_page_get_zip_size(bpage);
	page_t*	frame		= NULL;

#ifdef UNIV_DEBUG
	buf_pool_t*	buf_pool = buf_pool_from_bpage(bpage);
	ut_ad(!buf_pool_mutex_own(buf_pool));
#endif

#ifdef UNIV_LOG_DEBUG
	static ibool univ_log_debug_warned;
#endif /* UNIV_LOG_DEBUG */

	ut_ad(buf_page_in_file(bpage));

	/* We are not holding buf_pool->mutex or block_mutex here.
	Nevertheless, it is safe to access bpage, because it is
	io_fixed and oldest_modification != 0.  Thus, it cannot be
	relocated in the buffer pool or removed from flush_list or
	LRU_list. */
	ut_ad(!buf_pool_mutex_own(buf_pool));
	ut_ad(!buf_flush_list_mutex_own(buf_pool));
	ut_ad(!mutex_own(buf_page_get_mutex(bpage)));
	ut_ad(buf_page_get_io_fix(bpage) == BUF_IO_WRITE);
	ut_ad(bpage->oldest_modification != 0);

#ifdef UNIV_IBUF_COUNT_DEBUG
	ut_a(ibuf_count_get(bpage->space, bpage->offset) == 0);
#endif
	ut_ad(bpage->newest_modification != 0);

#ifdef UNIV_LOG_DEBUG
	if (!univ_log_debug_warned) {
		univ_log_debug_warned = TRUE;
		fputs("Warning: cannot force log to disk if"
		      " UNIV_LOG_DEBUG is defined!\n"
		      "Crash recovery will not work!\n",
		      stderr);
	}
#else
	/* Force the log to the disk before writing the modified block */
	log_write_up_to(bpage->newest_modification, LOG_WAIT_ALL_GROUPS, TRUE);
#endif
	switch (buf_page_get_state(bpage)) {
	case BUF_BLOCK_ZIP_FREE:
	case BUF_BLOCK_ZIP_PAGE: /* The page should be dirty. */
	case BUF_BLOCK_NOT_USED:
	case BUF_BLOCK_READY_FOR_USE:
	case BUF_BLOCK_MEMORY:
	case BUF_BLOCK_REMOVE_HASH:
		ut_error;
		break;
	case BUF_BLOCK_ZIP_DIRTY:
		frame = bpage->zip.data;
		if (UNIV_LIKELY(srv_use_checksums)) {
			ut_a(mach_read_from_4(frame + FIL_PAGE_SPACE_OR_CHKSUM)
			     == page_zip_calc_checksum(frame, zip_size));
		}
		mach_write_to_8(frame + FIL_PAGE_LSN,
				bpage->newest_modification);
		memset(frame + FIL_PAGE_FILE_FLUSH_LSN, 0, 8);
		break;
	case BUF_BLOCK_FILE_PAGE:
		frame = bpage->zip.data;
		if (!frame) {
			frame = ((buf_block_t*) bpage)->frame;
		}

		buf_flush_init_for_writing(((buf_block_t*) bpage)->frame,
					   bpage->zip.data
					   ? &bpage->zip : NULL,
					   bpage->newest_modification);
		break;
	}

	if (!srv_use_doublewrite_buf || !trx_doublewrite) {
		fil_io(OS_FILE_WRITE | OS_AIO_SIMULATED_WAKE_LATER,
		       FALSE, buf_page_get_space(bpage), zip_size,
		       buf_page_get_page_no(bpage), 0,
		       zip_size ? zip_size : UNIV_PAGE_SIZE,
		       frame, bpage);
	} else {
		buf_flush_post_to_doublewrite_buf(bpage);
	}
}

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
{
	ut_ad(buf_pool_mutex_own(buf_pool));
	ut_ad(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);
	ut_ad(mutex_own(&block->mutex));

	if (!buf_flush_ready_for_flush(&block->page, BUF_FLUSH_LRU)) {
		return(FALSE);
	}

	if (buf_pool->n_flush[BUF_FLUSH_LRU] > 0
	    || buf_pool->init_flush[BUF_FLUSH_LRU]) {
		/* There is already a flush batch of the same type running */
		return(FALSE);
	}

	buf_pool->init_flush[BUF_FLUSH_LRU] = TRUE;

	buf_page_set_io_fix(&block->page, BUF_IO_WRITE);

	buf_page_set_flush_type(&block->page, BUF_FLUSH_LRU);

	if (buf_pool->n_flush[BUF_FLUSH_LRU]++ == 0) {

		os_event_reset(buf_pool->no_flush[BUF_FLUSH_LRU]);
	}

	/* VERY IMPORTANT:
	Because any thread may call the LRU flush, even when owning
	locks on pages, to avoid deadlocks, we must make sure that the
	s-lock is acquired on the page without waiting: this is
	accomplished because buf_flush_ready_for_flush() must hold,
	and that requires the page not to be bufferfixed. */

	rw_lock_s_lock_gen(&block->lock, BUF_IO_WRITE);

	/* Note that the s-latch is acquired before releasing the
	buf_pool mutex: this ensures that the latch is acquired
	immediately. */

	mutex_exit(&block->mutex);
	buf_pool_mutex_exit(buf_pool);

	/* Even though block is not protected by any mutex at this
	point, it is safe to access block, because it is io_fixed and
	oldest_modification != 0.  Thus, it cannot be relocated in the
	buffer pool or removed from flush_list or LRU_list. */

	buf_flush_write_block_low(&block->page);

	buf_pool_mutex_enter(buf_pool);
	buf_pool->init_flush[BUF_FLUSH_LRU] = FALSE;

	if (buf_pool->n_flush[BUF_FLUSH_LRU] == 0) {
		/* The running flush batch has ended */
		os_event_set(buf_pool->no_flush[BUF_FLUSH_LRU]);
	}

	buf_pool_mutex_exit(buf_pool);
	buf_flush_buffered_writes();

	return(TRUE);
}
# endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

/********************************************************************//**
Writes a flushable page asynchronously from the buffer pool to a file.
NOTE: in simulated aio we must call
os_aio_simulated_wake_handler_threads after we have posted a batch of
writes! NOTE: buf_pool->mutex and buf_page_get_mutex(bpage) must be
held upon entering this function, and they will be released by this
function. */
static
void
buf_flush_page(
/*===========*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	buf_page_t*	bpage,		/*!< in: buffer control block */
	enum buf_flush	flush_type)	/*!< in: BUF_FLUSH_LRU
					or BUF_FLUSH_LIST */
{
	mutex_t*	block_mutex;
	ibool		is_uncompressed;

	ut_ad(flush_type == BUF_FLUSH_LRU || flush_type == BUF_FLUSH_LIST);
	ut_ad(buf_pool_mutex_own(buf_pool));
	ut_ad(buf_page_in_file(bpage));

	block_mutex = buf_page_get_mutex(bpage);
	ut_ad(mutex_own(block_mutex));

	ut_ad(buf_flush_ready_for_flush(bpage, flush_type));

	buf_page_set_io_fix(bpage, BUF_IO_WRITE);

	buf_page_set_flush_type(bpage, flush_type);

	if (buf_pool->n_flush[flush_type] == 0) {

		os_event_reset(buf_pool->no_flush[flush_type]);
	}

	buf_pool->n_flush[flush_type]++;

	is_uncompressed = (buf_page_get_state(bpage) == BUF_BLOCK_FILE_PAGE);
	ut_ad(is_uncompressed == (block_mutex != &buf_pool->zip_mutex));

	switch (flush_type) {
		ibool	is_s_latched;
	case BUF_FLUSH_LIST:
		/* If the simulated aio thread is not running, we must
		not wait for any latch, as we may end up in a deadlock:
		if buf_fix_count == 0, then we know we need not wait */

		is_s_latched = (bpage->buf_fix_count == 0);
		if (is_s_latched && is_uncompressed) {
			rw_lock_s_lock_gen(&((buf_block_t*) bpage)->lock,
					   BUF_IO_WRITE);
		}

		mutex_exit(block_mutex);
		buf_pool_mutex_exit(buf_pool);

		/* Even though bpage is not protected by any mutex at
		this point, it is safe to access bpage, because it is
		io_fixed and oldest_modification != 0.  Thus, it
		cannot be relocated in the buffer pool or removed from
		flush_list or LRU_list. */

		if (!is_s_latched) {
			buf_flush_buffered_writes();

			if (is_uncompressed) {
				rw_lock_s_lock_gen(&((buf_block_t*) bpage)
						   ->lock, BUF_IO_WRITE);
			}
		}

		break;

	case BUF_FLUSH_LRU:
		/* VERY IMPORTANT:
		Because any thread may call the LRU flush, even when owning
		locks on pages, to avoid deadlocks, we must make sure that the
		s-lock is acquired on the page without waiting: this is
		accomplished because buf_flush_ready_for_flush() must hold,
		and that requires the page not to be bufferfixed. */

		if (is_uncompressed) {
			rw_lock_s_lock_gen(&((buf_block_t*) bpage)->lock,
					   BUF_IO_WRITE);
		}

		/* Note that the s-latch is acquired before releasing the
		buf_pool mutex: this ensures that the latch is acquired
		immediately. */

		mutex_exit(block_mutex);
		buf_pool_mutex_exit(buf_pool);
		break;

	default:
		ut_error;
	}

	/* Even though bpage is not protected by any mutex at this
	point, it is safe to access bpage, because it is io_fixed and
	oldest_modification != 0.  Thus, it cannot be relocated in the
	buffer pool or removed from flush_list or LRU_list. */

#ifdef UNIV_DEBUG
	if (buf_debug_prints) {
		fprintf(stderr,
			"Flushing %u space %u page %u\n",
			flush_type, bpage->space, bpage->offset);
	}
#endif /* UNIV_DEBUG */
	buf_flush_write_block_low(bpage);
}

/***********************************************************//**
Flushes to disk all flushable pages within the flush area.
@return	number of pages flushed */
static
ulint
buf_flush_try_neighbors(
/*====================*/
	ulint		space,		/*!< in: space id */
	ulint		offset,		/*!< in: page offset */
	enum buf_flush	flush_type,	/*!< in: BUF_FLUSH_LRU or
					BUF_FLUSH_LIST */
	ulint		n_flushed,	/*!< in: number of pages
					flushed so far in this batch */
	ulint		n_to_flush)	/*!< in: maximum number of pages
					we are allowed to flush */
{
	ulint		i;
	ulint		low;
	ulint		high;
	ulint		count = 0;
	buf_pool_t*	buf_pool = buf_pool_get(space, offset);

	ut_ad(flush_type == BUF_FLUSH_LRU || flush_type == BUF_FLUSH_LIST);

	if (UT_LIST_GET_LEN(buf_pool->LRU) < BUF_LRU_OLD_MIN_LEN) {
		/* If there is little space, it is better not to flush
		any block except from the end of the LRU list */

		low = offset;
		high = offset + 1;
	} else {
		/* When flushed, dirty blocks are searched in
		neighborhoods of this size, and flushed along with the
		original page. */

		ulint	buf_flush_area;
	
		buf_flush_area	= ut_min(
			BUF_READ_AHEAD_AREA(buf_pool),
			buf_pool->curr_size / 16);

		low = (offset / buf_flush_area) * buf_flush_area;
		high = (offset / buf_flush_area + 1) * buf_flush_area;
	}

	/* fprintf(stderr, "Flush area: low %lu high %lu\n", low, high); */

	if (high > fil_space_get_size(space)) {
		high = fil_space_get_size(space);
	}

	for (i = low; i < high; i++) {

		buf_page_t*	bpage;

		if ((count + n_flushed) >= n_to_flush) {

			/* We have already flushed enough pages and
			should call it a day. There is, however, one
			exception. If the page whose neighbors we
			are flushing has not been flushed yet then
			we'll try to flush the victim that we
			selected originally. */
			if (i <= offset) {
				i = offset;
			} else {
				break;
			}
		}

		buf_pool = buf_pool_get(space, i);

		buf_pool_mutex_enter(buf_pool);

		/* We only want to flush pages from this buffer pool. */
		bpage = buf_page_hash_get(buf_pool, space, i);

		if (!bpage) {

			buf_pool_mutex_exit(buf_pool);
			continue;
		}

		ut_a(buf_page_in_file(bpage));

		/* We avoid flushing 'non-old' blocks in an LRU flush,
		because the flushed blocks are soon freed */

		if (flush_type != BUF_FLUSH_LRU
		    || i == offset
		    || buf_page_is_old(bpage)) {
			mutex_t* block_mutex = buf_page_get_mutex(bpage);

			mutex_enter(block_mutex);

			if (buf_flush_ready_for_flush(bpage, flush_type)
			    && (i == offset || !bpage->buf_fix_count)) {
				/* We only try to flush those
				neighbors != offset where the buf fix
				count is zero, as we then know that we
				probably can latch the page without a
				semaphore wait. Semaphore waits are
				expensive because we must flush the
				doublewrite buffer before we start
				waiting. */

				buf_flush_page(buf_pool, bpage, flush_type);
				ut_ad(!mutex_own(block_mutex));
				ut_ad(!buf_pool_mutex_own(buf_pool));
				count++;
				continue;
			} else {
				mutex_exit(block_mutex);
			}
		}
		buf_pool_mutex_exit(buf_pool);
	}

	return(count);
}

/********************************************************************//**
Check if the block is modified and ready for flushing. If the the block
is ready to flush then flush the page and try o flush its neighbors.

@return	TRUE if buf_pool mutex was not released during this function.
This does not guarantee that some pages were written as well.
Number of pages written are incremented to the count. */
static
ibool
buf_flush_page_and_try_neighbors(
/*=============================*/
	buf_page_t*	bpage,		/*!< in: buffer control block,
					must be
					buf_page_in_file(bpage) */
	enum buf_flush	flush_type,	/*!< in: BUF_FLUSH_LRU
					or BUF_FLUSH_LIST */
	ulint		n_to_flush,	/*!< in: number of pages to
					flush */
	ulint*		count)		/*!< in/out: number of pages
					flushed */
{
	mutex_t*	block_mutex;
	ibool		flushed = FALSE;
#ifdef UNIV_DEBUG
	buf_pool_t*	buf_pool = buf_pool_from_bpage(bpage);
#endif /* UNIV_DEBUG */

	ut_ad(buf_pool_mutex_own(buf_pool));

	block_mutex = buf_page_get_mutex(bpage);
	mutex_enter(block_mutex);

	ut_a(buf_page_in_file(bpage));

	if (buf_flush_ready_for_flush(bpage, flush_type)) {
		ulint		space;
		ulint		offset;
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_bpage(bpage);

		buf_pool_mutex_exit(buf_pool);

		/* These fields are protected by both the
		buffer pool mutex and block mutex. */
		space = buf_page_get_space(bpage);
		offset = buf_page_get_page_no(bpage);

		mutex_exit(block_mutex);

		/* Try to flush also all the neighbors */
		*count += buf_flush_try_neighbors(space,
						  offset,
						  flush_type,
						  *count,
						  n_to_flush);

		buf_pool_mutex_enter(buf_pool);
		flushed = TRUE;
	} else {
		mutex_exit(block_mutex);
	}

	ut_ad(buf_pool_mutex_own(buf_pool));

	return(flushed);
}

/*******************************************************************//**
This utility flushes dirty blocks from the end of the LRU list.
In the case of an LRU flush the calling thread may own latches to
pages: to avoid deadlocks, this function must be written so that it
cannot end up waiting for these latches!
@return number of blocks for which the write request was queued. */
static
ulint
buf_flush_LRU_list_batch(
/*=====================*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	ulint		max)		/*!< in: max of blocks to flush */
{
	buf_page_t*	bpage;
	ulint		count = 0;

	ut_ad(buf_pool_mutex_own(buf_pool));

	do {
		/* Start from the end of the list looking for a
		suitable block to be flushed. */
		bpage = UT_LIST_GET_LAST(buf_pool->LRU);

		/* Iterate backwards over the flush list till we find
		a page that isn't ready for flushing. */
		while (bpage != NULL
		       && !buf_flush_page_and_try_neighbors(
				bpage, BUF_FLUSH_LRU, max, &count)) {

			bpage = UT_LIST_GET_PREV(LRU, bpage);
		}
	} while (bpage != NULL && count < max);

	/* We keep track of all flushes happening as part of LRU
	flush. When estimating the desired rate at which flush_list
	should be flushed, we factor in this value. */
	buf_lru_flush_page_count += count;

	ut_ad(buf_pool_mutex_own(buf_pool));

	return(count);
}

/*******************************************************************//**
This utility flushes dirty blocks from the end of the flush_list.
the calling thread is not allowed to own any latches on pages!
@return number of blocks for which the write request was queued;
ULINT_UNDEFINED if there was a flush of the same type already
running */
static
ulint
buf_flush_flush_list_batch(
/*=======================*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	ulint		min_n,		/*!< in: wished minimum mumber
					of blocks flushed (it is not
					guaranteed that the actual
					number is that big, though) */
	ib_uint64_t	lsn_limit)	/*!< all blocks whose
					oldest_modification is smaller
					than this should be flushed (if
					their number does not exceed
					min_n) */
{
	ulint		len;
	buf_page_t*	bpage;
	ulint		count = 0;

	ut_ad(buf_pool_mutex_own(buf_pool));

	/* If we have flushed enough, leave the loop */
	do {
		/* Start from the end of the list looking for a suitable
		block to be flushed. */

		buf_flush_list_mutex_enter(buf_pool);

		/* We use len here because theoretically insertions can
		happen in the flush_list below while we are traversing
		it for a suitable candidate for flushing. We'd like to
		set a limit on how farther we are willing to traverse
		the list. */
		len = UT_LIST_GET_LEN(buf_pool->flush_list);
		bpage = UT_LIST_GET_LAST(buf_pool->flush_list);

		if (bpage) {
			ut_a(bpage->oldest_modification > 0);
		}

		if (!bpage || bpage->oldest_modification >= lsn_limit) {

			/* We have flushed enough */
			buf_flush_list_mutex_exit(buf_pool);
			break;
		}

		ut_a(bpage->oldest_modification > 0);

		ut_ad(bpage->in_flush_list);

		buf_flush_list_mutex_exit(buf_pool);

		/* The list may change during the flushing and we cannot
		safely preserve within this function a pointer to a
		block in the list! */
		while (bpage != NULL
		       && len > 0
		       && !buf_flush_page_and_try_neighbors(
				bpage, BUF_FLUSH_LIST, min_n, &count)) {

			buf_flush_list_mutex_enter(buf_pool);

			/* If we are here that means that buf_pool->mutex
			 was not released in buf_flush_page_and_try_neighbors()
			above and this guarantees that bpage didn't get
			relocated since we released the flush_list
			mutex above. There is a chance, however, that
			the bpage got removed from flush_list (not
			currently possible because flush_list_remove()
			also obtains buf_pool mutex but that may change
			in future). To avoid this scenario we check
			the oldest_modification and if it is zero
			we start all over again. */
			if (bpage->oldest_modification == 0) {
				buf_flush_list_mutex_exit(buf_pool);
				break;
			}

			bpage = UT_LIST_GET_PREV(list, bpage);

			ut_ad(!bpage || bpage->in_flush_list);

			buf_flush_list_mutex_exit(buf_pool);

			--len;
		}

	} while (count < min_n && bpage != NULL && len > 0);

	ut_ad(buf_pool_mutex_own(buf_pool));

	return(count);
}

/*******************************************************************//**
This utility flushes dirty blocks from the end of the LRU list or flush_list.
NOTE 1: in the case of an LRU flush the calling thread may own latches to
pages: to avoid deadlocks, this function must be written so that it cannot
end up waiting for these latches! NOTE 2: in the case of a flush list flush,
the calling thread is not allowed to own any latches on pages!
@return number of blocks for which the write request was queued;
ULINT_UNDEFINED if there was a flush of the same type already running */
static
ulint
buf_flush_batch(
/*============*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	enum buf_flush	flush_type,	/*!< in: BUF_FLUSH_LRU or
					BUF_FLUSH_LIST; if BUF_FLUSH_LIST,
					then the caller must not own any
					latches on pages */
	ulint		min_n,		/*!< in: wished minimum mumber of blocks
					flushed (it is not guaranteed that the
					actual number is that big, though) */
	ib_uint64_t	lsn_limit)	/*!< in: in the case of BUF_FLUSH_LIST
					all blocks whose oldest_modification is
					smaller than this should be flushed
					(if their number does not exceed
					min_n), otherwise ignored */
{
	ulint		count	= 0;

	ut_ad(flush_type == BUF_FLUSH_LRU || flush_type == BUF_FLUSH_LIST);
#ifdef UNIV_SYNC_DEBUG
	ut_ad((flush_type != BUF_FLUSH_LIST)
	      || sync_thread_levels_empty_except_dict());
#endif /* UNIV_SYNC_DEBUG */

	buf_pool_mutex_enter(buf_pool);

	/* Note: The buffer pool mutex is released and reacquired within
	the flush functions. */
	switch(flush_type) {
	case BUF_FLUSH_LRU:
		count = buf_flush_LRU_list_batch(buf_pool, min_n);
		break;
	case BUF_FLUSH_LIST:
		count = buf_flush_flush_list_batch(buf_pool, min_n, lsn_limit);
		break;
	default:
		ut_error;
	}

	buf_pool_mutex_exit(buf_pool);

	buf_flush_buffered_writes();

#ifdef UNIV_DEBUG
	if (buf_debug_prints && count > 0) {
		fprintf(stderr, flush_type == BUF_FLUSH_LRU
			? "Flushed %lu pages in LRU flush\n"
			: "Flushed %lu pages in flush list flush\n",
			(ulong) count);
	}
#endif /* UNIV_DEBUG */

	return(count);
}

/******************************************************************//**
Gather the aggregated stats for both flush list and LRU list flushing */
static
void
buf_flush_common(
/*=============*/
	enum buf_flush	flush_type,	/*!< in: type of flush */
	ulint		page_count)	/*!< in: number of pages flushed */
{
	buf_flush_buffered_writes();

	ut_a(flush_type == BUF_FLUSH_LRU || flush_type == BUF_FLUSH_LIST);

#ifdef UNIV_DEBUG
	if (buf_debug_prints && page_count > 0) {
		fprintf(stderr, flush_type == BUF_FLUSH_LRU
			? "Flushed %lu pages in LRU flush\n"
			: "Flushed %lu pages in flush list flush\n",
			(ulong) page_count);
	}
#endif /* UNIV_DEBUG */

	srv_buf_pool_flushed += page_count;
}

/******************************************************************//**
Start a buffer flush batch for LRU or flush list */
static
ibool
buf_flush_start(
/*============*/
	buf_pool_t*	buf_pool,	/*!< buffer pool instance */
	enum buf_flush	flush_type)	/*!< in: BUF_FLUSH_LRU
					or BUF_FLUSH_LIST */
{
	buf_pool_mutex_enter(buf_pool);

	if (buf_pool->n_flush[flush_type] > 0
	   || buf_pool->init_flush[flush_type] == TRUE) {

		/* There is already a flush batch of the same type running */

		buf_pool_mutex_exit(buf_pool);

		return(FALSE);
	}

	buf_pool->init_flush[flush_type] = TRUE;

	buf_pool_mutex_exit(buf_pool);

	return(TRUE);
}

/******************************************************************//**
End a buffer flush batch for LRU or flush list */
static
void
buf_flush_end(
/*==========*/
	buf_pool_t*	buf_pool,	/*!< buffer pool instance */
	enum buf_flush	flush_type)	/*!< in: BUF_FLUSH_LRU
					or BUF_FLUSH_LIST */
{
	buf_pool_mutex_enter(buf_pool);

	buf_pool->init_flush[flush_type] = FALSE;

	if (buf_pool->n_flush[flush_type] == 0) {

		/* The running flush batch has ended */

		os_event_set(buf_pool->no_flush[flush_type]);
	}

	buf_pool_mutex_exit(buf_pool);
}

/******************************************************************//**
Waits until a flush batch of the given type ends */
UNIV_INTERN
void
buf_flush_wait_batch_end(
/*=====================*/
	buf_pool_t*	buf_pool,	/*!< buffer pool instance */
	enum buf_flush	type)		/*!< in: BUF_FLUSH_LRU
					or BUF_FLUSH_LIST */
{
	ut_ad(type == BUF_FLUSH_LRU || type == BUF_FLUSH_LIST);

	if (buf_pool == NULL) {
		ulint	i;

		for (i = 0; i < srv_buf_pool_instances; ++i) {
			buf_pool_t*	buf_pool;

			buf_pool = buf_pool_from_array(i);

			thd_wait_begin(NULL, THD_WAIT_DISKIO);
			os_event_wait(buf_pool->no_flush[type]);
			thd_wait_end(NULL);
		}
	} else {
		thd_wait_begin(NULL, THD_WAIT_DISKIO);
		os_event_wait(buf_pool->no_flush[type]);
		thd_wait_end(NULL);
	}
}

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
	ulint		min_n)		/*!< in: wished minimum mumber of blocks
					flushed (it is not guaranteed that the
					actual number is that big, though) */
{
	ulint		page_count;

	if (!buf_flush_start(buf_pool, BUF_FLUSH_LRU)) {
		return(ULINT_UNDEFINED);
	}

	page_count = buf_flush_batch(buf_pool, BUF_FLUSH_LRU, min_n, 0);

	buf_flush_end(buf_pool, BUF_FLUSH_LRU);

	buf_flush_common(BUF_FLUSH_LRU, page_count);

	return(page_count);
}

/*******************************************************************//**
This utility flushes dirty blocks from the end of the flush list of
all buffer pool instances.
NOTE: The calling thread is not allowed to own any latches on pages!
@return number of blocks for which the write request was queued;
ULINT_UNDEFINED if there was a flush of the same type already running */
UNIV_INTERN
ulint
buf_flush_list(
/*===========*/
	ulint		min_n,		/*!< in: wished minimum mumber of blocks
					flushed (it is not guaranteed that the
					actual number is that big, though) */
	ib_uint64_t	lsn_limit)	/*!< in the case BUF_FLUSH_LIST all
					blocks whose oldest_modification is
					smaller than this should be flushed
					(if their number does not exceed
					min_n), otherwise ignored */
{
	ulint		i;
	ulint		total_page_count = 0;
	ibool		skipped = FALSE;

	if (min_n != ULINT_MAX) {
		/* Ensure that flushing is spread evenly amongst the
		buffer pool instances. When min_n is ULINT_MAX
		we need to flush everything up to the lsn limit
		so no limit here. */
		min_n = (min_n + srv_buf_pool_instances - 1)
			 / srv_buf_pool_instances;
	}

	/* Flush to lsn_limit in all buffer pool instances */
	for (i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;
		ulint		page_count = 0;

		buf_pool = buf_pool_from_array(i);

		if (!buf_flush_start(buf_pool, BUF_FLUSH_LIST)) {
			/* We have two choices here. If lsn_limit was
			specified then skipping an instance of buffer
			pool means we cannot guarantee that all pages
			up to lsn_limit has been flushed. We can
			return right now with failure or we can try
			to flush remaining buffer pools up to the
			lsn_limit. We attempt to flush other buffer
			pools based on the assumption that it will
			help in the retry which will follow the
			failure. */
			skipped = TRUE;

			continue;
		}

		page_count = buf_flush_batch(
			buf_pool, BUF_FLUSH_LIST, min_n, lsn_limit);

		buf_flush_end(buf_pool, BUF_FLUSH_LIST);

		buf_flush_common(BUF_FLUSH_LIST, page_count);

		total_page_count += page_count;
	}

	return(lsn_limit != IB_ULONGLONG_MAX && skipped
	       ? ULINT_UNDEFINED : total_page_count);
}
 
/******************************************************************//**
Gives a recommendation of how many blocks should be flushed to establish
a big enough margin of replaceable blocks near the end of the LRU list
and in the free list.
@return number of blocks which should be flushed from the end of the
LRU list */
static
ulint
buf_flush_LRU_recommendation(
/*=========================*/
	buf_pool_t*	buf_pool)		/*!< in: Buffer pool instance */
{
	buf_page_t*	bpage;
	ulint		n_replaceable;
	ulint		distance	= 0;

	buf_pool_mutex_enter(buf_pool);

	n_replaceable = UT_LIST_GET_LEN(buf_pool->free);

	bpage = UT_LIST_GET_LAST(buf_pool->LRU);

	while ((bpage != NULL)
	       && (n_replaceable < BUF_FLUSH_FREE_BLOCK_MARGIN(buf_pool)
		   + BUF_FLUSH_EXTRA_MARGIN(buf_pool))
	       && (distance < BUF_LRU_FREE_SEARCH_LEN(buf_pool))) {

		mutex_t* block_mutex = buf_page_get_mutex(bpage);

		mutex_enter(block_mutex);

		if (buf_flush_ready_for_replace(bpage)) {
			n_replaceable++;
		}

		mutex_exit(block_mutex);

		distance++;

		bpage = UT_LIST_GET_PREV(LRU, bpage);
	}

	buf_pool_mutex_exit(buf_pool);

	if (n_replaceable >= BUF_FLUSH_FREE_BLOCK_MARGIN(buf_pool)) {

		return(0);
	}

	return(BUF_FLUSH_FREE_BLOCK_MARGIN(buf_pool)
	       + BUF_FLUSH_EXTRA_MARGIN(buf_pool)
	       - n_replaceable);
}

/*********************************************************************//**
Flushes pages from the end of the LRU list if there is too small a margin
of replaceable pages there or in the free list. VERY IMPORTANT: this function
is called also by threads which have locks on pages. To avoid deadlocks, we
flush only pages such that the s-lock required for flushing can be acquired
immediately, without waiting. */
UNIV_INTERN
void
buf_flush_free_margin(
/*==================*/
	buf_pool_t*	buf_pool)		/*!< in: Buffer pool instance */
{
	ulint	n_to_flush;

	n_to_flush = buf_flush_LRU_recommendation(buf_pool);

	if (n_to_flush > 0) {
		ulint	n_flushed;

		n_flushed = buf_flush_LRU(buf_pool, n_to_flush);

		if (n_flushed == ULINT_UNDEFINED) {
			/* There was an LRU type flush batch already running;
			let us wait for it to end */

			buf_flush_wait_batch_end(buf_pool, BUF_FLUSH_LRU);
		}
	}
}

/*********************************************************************//**
Flushes pages from the end of all the LRU lists. */
UNIV_INTERN
void
buf_flush_free_margins(void)
/*========================*/
{
	ulint	i;

	for (i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		buf_flush_free_margin(buf_pool);
	}
}

/*********************************************************************
Update the historical stats that we are collecting for flush rate
heuristics at the end of each interval.
Flush rate heuristic depends on (a) rate of redo log generation and
(b) the rate at which LRU flush is happening. */
UNIV_INTERN
void
buf_flush_stat_update(void)
/*=======================*/
{
	buf_flush_stat_t*	item;
	ib_uint64_t		lsn_diff;
	ib_uint64_t		lsn;
	ulint			n_flushed;

	lsn = log_get_lsn();
	if (buf_flush_stat_cur.redo == 0) {
		/* First time around. Just update the current LSN
		and return. */
		buf_flush_stat_cur.redo = lsn;
		return;
	}

	item = &buf_flush_stat_arr[buf_flush_stat_arr_ind];

	/* values for this interval */
	lsn_diff = lsn - buf_flush_stat_cur.redo;
	n_flushed = buf_lru_flush_page_count
		    - buf_flush_stat_cur.n_flushed;

	/* add the current value and subtract the obsolete entry. */
	buf_flush_stat_sum.redo += lsn_diff - item->redo;
	buf_flush_stat_sum.n_flushed += n_flushed - item->n_flushed;

	/* put current entry in the array. */
	item->redo = lsn_diff;
	item->n_flushed = n_flushed;

	/* update the index */
	buf_flush_stat_arr_ind++;
	buf_flush_stat_arr_ind %= BUF_FLUSH_STAT_N_INTERVAL;

	/* reset the current entry. */
	buf_flush_stat_cur.redo = lsn;
	buf_flush_stat_cur.n_flushed = buf_lru_flush_page_count;
}

/*********************************************************************
Determines the fraction of dirty pages that need to be flushed based
on the speed at which we generate redo log. Note that if redo log
is generated at a significant rate without corresponding increase
in the number of dirty pages (for example, an in-memory workload)
it can cause IO bursts of flushing. This function implements heuristics
to avoid this burstiness.
@return	number of dirty pages to be flushed / second */
UNIV_INTERN
ulint
buf_flush_get_desired_flush_rate(void)
/*==================================*/
{
	ulint		i;
	lint		rate;
	ulint		redo_avg;
	ulint		n_dirty = 0;
	ulint		n_flush_req;
	ulint		lru_flush_avg;
	ib_uint64_t	lsn = log_get_lsn();
	ulint		log_capacity = log_get_capacity();

	/* log_capacity should never be zero after the initialization
	of log subsystem. */
	ut_ad(log_capacity != 0);

	/* Get total number of dirty pages. It is OK to access
	flush_list without holding any mutex as we are using this
	only for heuristics. */
	for (i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);
		n_dirty += UT_LIST_GET_LEN(buf_pool->flush_list);
	}

	/* An overflow can happen if we generate more than 2^32 bytes
	of redo in this interval i.e.: 4G of redo in 1 second. We can
	safely consider this as infinity because if we ever come close
	to 4G we'll start a synchronous flush of dirty pages. */
	/* redo_avg below is average at which redo is generated in
	past BUF_FLUSH_STAT_N_INTERVAL + redo generated in the current
	interval. */
	redo_avg = (ulint) (buf_flush_stat_sum.redo
			    / BUF_FLUSH_STAT_N_INTERVAL
			    + (lsn - buf_flush_stat_cur.redo));

	/* An overflow can happen possibly if we flush more than 2^32
	pages in BUF_FLUSH_STAT_N_INTERVAL. This is a very very
	unlikely scenario. Even when this happens it means that our
	flush rate will be off the mark. It won't affect correctness
	of any subsystem. */
	/* lru_flush_avg below is rate at which pages are flushed as
	part of LRU flush in past BUF_FLUSH_STAT_N_INTERVAL + the
	number of pages flushed in the current interval. */
	lru_flush_avg = buf_flush_stat_sum.n_flushed
			/ BUF_FLUSH_STAT_N_INTERVAL
			+ (buf_lru_flush_page_count
			   - buf_flush_stat_cur.n_flushed);

	n_flush_req = (n_dirty * redo_avg) / log_capacity;

	/* The number of pages that we want to flush from the flush
	list is the difference between the required rate and the
	number of pages that we are historically flushing from the
	LRU list */
	rate = n_flush_req - lru_flush_avg;
	return(rate > 0 ? (ulint) rate : 0);
}

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/******************************************************************//**
Validates the flush list.
@return	TRUE if ok */
static
ibool
buf_flush_validate_low(
/*===================*/
	buf_pool_t*	buf_pool)		/*!< in: Buffer pool instance */
{
	buf_page_t*		bpage;
	const ib_rbt_node_t*	rnode = NULL;

	ut_ad(buf_flush_list_mutex_own(buf_pool));

	UT_LIST_VALIDATE(list, buf_page_t, buf_pool->flush_list,
			 ut_ad(ut_list_node_313->in_flush_list));

	bpage = UT_LIST_GET_FIRST(buf_pool->flush_list);

	/* If we are in recovery mode i.e.: flush_rbt != NULL
	then each block in the flush_list must also be present
	in the flush_rbt. */
	if (UNIV_LIKELY_NULL(buf_pool->flush_rbt)) {
		rnode = rbt_first(buf_pool->flush_rbt);
	}

	while (bpage != NULL) {
		const ib_uint64_t om = bpage->oldest_modification;

		ut_ad(buf_pool_from_bpage(bpage) == buf_pool);

		ut_ad(bpage->in_flush_list);

		/* A page in buf_pool->flush_list can be in
		BUF_BLOCK_REMOVE_HASH state. This happens when a page
		is in the middle of being relocated. In that case the
		original descriptor can have this state and still be
		in the flush list waiting to acquire the
		buf_pool->flush_list_mutex to complete the relocation. */
		ut_a(buf_page_in_file(bpage)
		     || buf_page_get_state(bpage) == BUF_BLOCK_REMOVE_HASH);
		ut_a(om > 0);

		if (UNIV_LIKELY_NULL(buf_pool->flush_rbt)) {
			buf_page_t** prpage;

			ut_a(rnode);
			prpage = rbt_value(buf_page_t*, rnode);

			ut_a(*prpage);
			ut_a(*prpage == bpage);
			rnode = rbt_next(buf_pool->flush_rbt, rnode);
		}

		bpage = UT_LIST_GET_NEXT(list, bpage);

		ut_a(!bpage || om >= bpage->oldest_modification);
	}

	/* By this time we must have exhausted the traversal of
	flush_rbt (if active) as well. */
	ut_a(rnode == NULL);

	return(TRUE);
}

/******************************************************************//**
Validates the flush list.
@return	TRUE if ok */
UNIV_INTERN
ibool
buf_flush_validate(
/*===============*/
	buf_pool_t*	buf_pool)	/*!< buffer pool instance */
{
	ibool	ret;

	buf_flush_list_mutex_enter(buf_pool);

	ret = buf_flush_validate_low(buf_pool);

	buf_flush_list_mutex_exit(buf_pool);

	return(ret);
}
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
#endif /* !UNIV_HOTBACKUP */
