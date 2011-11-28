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
@file buf/buf0flu.cc
The database buffer buf_pool flush algorithm

Created 11/11/1995 Heikki Tuuri
*******************************************************/

#include "buf0flu.h"

#ifdef UNIV_NONINL
#include "buf0flu.ic"
#endif

#include "buf0buf.h"
#include "buf0checksum.h"
#include "srv0start.h"
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
#include "srv0mon.h"
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

/** Time in milliseconds that we sleep when unable to find a slot in
the doublewrite buffer or when we have to wait for a running batch
to end. */
#define TRX_DOUBLEWRITE_BATCH_POLL_DELAY	10000

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

/** Flag indicating if the page_cleaner is in active state. This flag
is set to TRUE by the page_cleaner thread when it is spawned and is set
back to FALSE at shutdown by the page_cleaner as well. Therefore no
need to protect it by a mutex. It is only ever read by the thread
doing the shutdown */
UNIV_INTERN ibool buf_page_cleaner_is_active = FALSE;

/** LRU flush batch is further divided into this chunk size to
reduce the wait time for the threads waiting for a clean block */
#define PAGE_CLEANER_LRU_BATCH_CHUNK_SIZE	100

#ifdef UNIV_PFS_THREAD
UNIV_INTERN mysql_pfs_key_t buf_page_cleaner_thread_key;
#endif /* UNIV_PFS_THREAD */

/** If LRU list of a buf_pool is less than this size then LRU eviction
should not happen. This is because when we do LRU flushing we also put
the blocks on free list. If LRU list is very small then we can end up
in thrashing. */
#define BUF_LRU_MIN_LEN		256

/* @} */

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
	lsn_t		lsn)		/*!< in: oldest modification */
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

#ifdef UNIV_DEBUG_VALGRIND
	{
		ulint	zip_size = buf_block_get_zip_size(block);

		if (zip_size) {
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
	lsn_t		lsn)		/*!< in: oldest modification */
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

		if (zip_size) {
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

	MONITOR_INC(MONITOR_PAGE_INFLUSH);

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
	enum buf_flush	flush_type)/*!< in: type of flush */
{
#ifdef UNIV_DEBUG
	buf_pool_t*	buf_pool = buf_pool_from_bpage(bpage);
	ut_ad(buf_pool_mutex_own(buf_pool));
#endif
	ut_a(buf_page_in_file(bpage));
	ut_ad(mutex_own(buf_page_get_mutex(bpage)));
	ut_ad(flush_type < BUF_FLUSH_N_TYPES);

	if (bpage->oldest_modification == 0
	    || buf_page_get_io_fix(bpage) != BUF_IO_NONE) {
		return(FALSE);
	}

	ut_ad(bpage->in_flush_list);

	switch (flush_type) {
	case BUF_FLUSH_LIST:
		return(TRUE);

	case BUF_FLUSH_LRU:
	case BUF_FLUSH_SINGLE_PAGE:
		/* Because any thread may call single page flush, even
		when owning locks on pages, to avoid deadlocks, we must
		make sure that the that it is not buffer fixed.
		The same holds true for LRU flush because a user thread
		may end up waiting for an LRU flush to end while
		holding locks on other pages. */
		return(bpage->buf_fix_count == 0);
	case BUF_FLUSH_N_TYPES:
		break;
	}

	ut_error;
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

	bpage->oldest_modification = 0;

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ut_a(buf_flush_validate_skip(buf_pool));
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

	MONITOR_DEC(MONITOR_PAGE_INFLUSH);

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
Check the LSN values on the page. */
static
void
buf_flush_doublewrite_check_page_lsn(
/*=================================*/
	const page_t*	page)		/*!< in: page to check */
{
	if (memcmp(page + (FIL_PAGE_LSN + 4),
		   page + (UNIV_PAGE_SIZE
			   - FIL_PAGE_END_LSN_OLD_CHKSUM + 4),
		   4)) {

		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: ERROR: The page to be written"
			" seems corrupt!\n"
			"InnoDB: The low 4 bytes of LSN fields do not match "
			"(" ULINTPF " != " ULINTPF ")!"
			" Noticed in the buffer pool.\n",
			mach_read_from_4(
				page + FIL_PAGE_LSN + 4),
			mach_read_from_4(
				page + UNIV_PAGE_SIZE
				- FIL_PAGE_END_LSN_OLD_CHKSUM + 4));
	}
}

/********************************************************************//**
Asserts when a corrupt block is find during writing out data to the
disk. */
static
void
buf_flush_doublewrite_assert_on_corrupt_block(
/*==========================================*/
	const buf_block_t*	block)	/*!< in: block to check */
{
	buf_page_print(block->frame, 0);

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

/********************************************************************//**
Check the LSN values on the page with which this block is associated.
Also validate the page if the option is set. */
static
void
buf_flush_doublewrite_check_block(
/*==============================*/
	const buf_block_t*	block)	/*!< in: block to check */
{
	if (buf_block_get_state(block) != BUF_BLOCK_FILE_PAGE
	    || block->page.zip.data) {
		/* No simple validate for compressed pages exists. */
		return;
	}

	buf_flush_doublewrite_check_page_lsn(block->frame);

	if (!block->check_index_page_at_flush) {
		return;
	}

	if (page_is_comp(block->frame)) {
		if (!page_simple_validate_new(block->frame)) {
			buf_flush_doublewrite_assert_on_corrupt_block(block);
		}
	} else if (!page_simple_validate_old(block->frame)) {

		buf_flush_doublewrite_assert_on_corrupt_block(block);
	}
}

/********************************************************************//**
Writes a page that has already been written to the doublewrite buffer
to the datafile. It is the job of the caller to sync the datafile. */
static
void
buf_flush_write_block_to_datafile(
/*==============================*/
	const buf_block_t*	block)	/*!< in: block to write */
{
	ut_a(block);
	ut_a(buf_page_in_file(&block->page));

	if (block->page.zip.data) {
		fil_io(OS_FILE_WRITE | OS_AIO_SIMULATED_WAKE_LATER,
		       FALSE, buf_page_get_space(&block->page),
		       buf_page_get_zip_size(&block->page),
		       buf_page_get_page_no(&block->page), 0,
		       buf_page_get_zip_size(&block->page),
		       (void*)block->page.zip.data,
		       (void*)block);

		goto exit;
	}

	ut_a(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);
	buf_flush_doublewrite_check_page_lsn(block->frame);

	fil_io(OS_FILE_WRITE | OS_AIO_SIMULATED_WAKE_LATER,
	       FALSE, buf_block_get_space(block), 0,
	       buf_block_get_page_no(block), 0, UNIV_PAGE_SIZE,
	       (void*)block->frame, (void*)block);

exit:
	/* Increment the counter of I/O operations used
	for selecting LRU policy. */
	buf_LRU_stat_inc_io();
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

try_again:
	mutex_enter(&(trx_doublewrite->mutex));

	/* Write first to doublewrite buffer blocks. We use synchronous
	aio and thus know that file write has been completed when the
	control returns. */

	if (trx_doublewrite->first_free == 0) {

		mutex_exit(&(trx_doublewrite->mutex));

		return;
	}

	if (trx_doublewrite->batch_running) {
		mutex_exit(&trx_doublewrite->mutex);

		/* Another thread is running the batch right now. Wait
		for it to finish. */
		os_thread_sleep(TRX_DOUBLEWRITE_BATCH_POLL_DELAY);
		goto try_again;
	}

	ut_a(!trx_doublewrite->batch_running);

	/* Disallow anyone else to post to doublewrite buffer or to
	start another batch of flushing. */
	trx_doublewrite->batch_running = TRUE;

	/* Now safe to release the mutex. Note that though no other
	thread is allowed to post to the doublewrite batch flushing
	but any threads working on single page flushes are allowed
	to proceed. */
	mutex_exit(&trx_doublewrite->mutex);

	write_buf = trx_doublewrite->write_buf;

	for (len2 = 0, i = 0;
	     i < trx_doublewrite->first_free;
	     len2 += UNIV_PAGE_SIZE, i++) {

		const buf_block_t*	block;

		block = (buf_block_t*) trx_doublewrite->buf_block_arr[i];

		if (buf_block_get_state(block) != BUF_BLOCK_FILE_PAGE
		    || block->page.zip.data) {
			/* No simple validate for compressed
			pages exists. */
			continue;
		}

		/* Check that the actual page in the buffer pool is
		not corrupt and the LSN values are sane. */
		buf_flush_doublewrite_check_block(block);

		/* Check that the page as written to the doublewrite
		buffer has sane LSN values. */
		buf_flush_doublewrite_check_page_lsn(write_buf + len2);
	}

	/* Write out the first block of the doublewrite buffer */
	len = ut_min(TRX_SYS_DOUBLEWRITE_BLOCK_SIZE,
		     trx_doublewrite->first_free) * UNIV_PAGE_SIZE;

	fil_io(OS_FILE_WRITE, TRUE, TRX_SYS_SPACE, 0,
	       trx_doublewrite->block1, 0, len,
	       (void*) write_buf, NULL);

	if (trx_doublewrite->first_free <= TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) {
		/* No unwritten pages in the second block. */
		goto flush;
	}

	/* Write out the second block of the doublewrite buffer. */
	len = (trx_doublewrite->first_free - TRX_SYS_DOUBLEWRITE_BLOCK_SIZE)
	       * UNIV_PAGE_SIZE;

	write_buf = trx_doublewrite->write_buf
		    + TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * UNIV_PAGE_SIZE;

	fil_io(OS_FILE_WRITE, TRUE, TRX_SYS_SPACE, 0,
	       trx_doublewrite->block2, 0, len,
	       (void*) write_buf, NULL);

flush:
	/* increment the doublewrite flushed pages counter */
	srv_dblwr_pages_written += trx_doublewrite->first_free;
	srv_dblwr_writes++;

	/* Now flush the doublewrite buffer data to disk */
	fil_flush(TRX_SYS_SPACE);

	/* We know that the writes have been flushed to disk now
	and in recovery we will find them in the doublewrite buffer
	blocks. Next do the writes to the intended positions. */

	for (i = 0; i < trx_doublewrite->first_free; i++) {
		const buf_block_t* block = (buf_block_t*)
			trx_doublewrite->buf_block_arr[i];

		buf_flush_write_block_to_datafile(block);
	}

	/* Sync the writes to the disk. */
	buf_flush_sync_datafiles();

	mutex_enter(&trx_doublewrite->mutex);

	/* We can now reuse the doublewrite memory buffer: */
	trx_doublewrite->first_free = 0;
	trx_doublewrite->batch_running = FALSE;

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

	ut_a(buf_page_in_file(bpage));

try_again:
	mutex_enter(&(trx_doublewrite->mutex));

	ut_a(trx_doublewrite->first_free <= srv_doublewrite_batch_size);

	if (trx_doublewrite->batch_running) {
		mutex_exit(&trx_doublewrite->mutex);

		/* This not nearly as bad as it looks. There is only
		page_cleaner thread which does background flushing
		in batches therefore it is unlikely to be a contention
		point. The only exception is when a user thread is
		forced to do a flush batch because of a sync
		checkpoint. */
		os_thread_sleep(TRX_DOUBLEWRITE_BATCH_POLL_DELAY);
		goto try_again;
	}

	if (trx_doublewrite->first_free == srv_doublewrite_batch_size) {
		mutex_exit(&(trx_doublewrite->mutex));

		buf_flush_buffered_writes();

		goto try_again;
	}

	zip_size = buf_page_get_zip_size(bpage);

	if (zip_size) {
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

	if (trx_doublewrite->first_free == srv_doublewrite_batch_size) {
		mutex_exit(&(trx_doublewrite->mutex));

		buf_flush_buffered_writes();

		return;
	}

	mutex_exit(&(trx_doublewrite->mutex));
}

/********************************************************************//**
Writes a page to the doublewrite buffer on disk, sync it, then write
the page to the datafile and sync the datafile. This function is used
for single page flushes. If all the buffers allocated for single page
flushes in the doublewrite buffer are in use we wait here for one to
become free. We are guaranteed that a slot will become free because any
thread that is using a slot must also release the slot before leaving
this function. */
static
void
buf_flush_write_to_dblwr_and_datafile(
/*==================================*/
	buf_page_t*	bpage)	/*!< in: buffer block to write */
{
	ulint		n_slots;
	ulint		size;
	ulint		zip_size;
	ulint		offset;
	ulint		i;

	ut_a(buf_page_in_file(bpage));
	ut_a(srv_use_doublewrite_buf);
	ut_a(trx_doublewrite != NULL);

	/* total number of slots available for single page flushes
	starts from srv_doublewrite_batch_size to the end of the
	buffer. */
	size = 2 * TRX_SYS_DOUBLEWRITE_BLOCK_SIZE;
	ut_a(size > srv_doublewrite_batch_size);
	n_slots = size - srv_doublewrite_batch_size;

	if (buf_page_get_state(bpage) == BUF_BLOCK_FILE_PAGE) {

		/* Check that the actual page in the buffer pool is
		not corrupt and the LSN values are sane. */
		buf_flush_doublewrite_check_block((buf_block_t*) bpage);

		/* Check that the page as written to the doublewrite
		buffer has sane LSN values. */
		if (!bpage->zip.data) {
			buf_flush_doublewrite_check_page_lsn(
				((buf_block_t*) bpage)->frame);
		}
	}

retry:
	mutex_enter(&trx_doublewrite->mutex);
	if (trx_doublewrite->n_reserved == n_slots) {

		mutex_exit(&trx_doublewrite->mutex);
		/* All slots are reserved. Since it involves two IOs
		during the processing a sleep of 10ms should be
		enough. */
		os_thread_sleep(TRX_DOUBLEWRITE_BATCH_POLL_DELAY);
		goto retry;
	}

	for (i = srv_doublewrite_batch_size; i < size; ++i) {

		if (!trx_doublewrite->in_use[i]) {
			break;
		}
	}

	/* We are guaranteed to find a slot. */
	ut_a(i < size);
	trx_doublewrite->in_use[i] = TRUE;
	trx_doublewrite->n_reserved++;
	trx_doublewrite->buf_block_arr[i] = bpage;
	mutex_exit(&trx_doublewrite->mutex);

	/* Lets see if we are going to write in the first or second
	block of the doublewrite buffer. */
	if (i < TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) {
		offset = trx_doublewrite->block1 + i;
	} else {
		offset = trx_doublewrite->block2 + i
			 - TRX_SYS_DOUBLEWRITE_BLOCK_SIZE;
	}

	/* We deal with compressed and uncompressed pages a little
	differently here. In case of uncompressed pages we can
	directly write the block to the allocated slot in the
	doublewrite buffer in the system tablespace and then after
	syncing the system table space we can proceed to write the page
	in the datafile.
	In case of compressed page we first do a memcpy of the block
	to the in-memory buffer of doublewrite before proceeding to
	write it. This is so because we want to pad the remaining
	bytes in the doublewrite page with zeros. */

	zip_size = buf_page_get_zip_size(bpage);
	if (zip_size) {
		memcpy(trx_doublewrite->write_buf + UNIV_PAGE_SIZE * i,
		       bpage->zip.data, zip_size);
		memset(trx_doublewrite->write_buf + UNIV_PAGE_SIZE * i
		       + zip_size, 0, UNIV_PAGE_SIZE - zip_size);

		fil_io(OS_FILE_WRITE, TRUE, TRX_SYS_SPACE, 0,
		       offset, 0, UNIV_PAGE_SIZE,
		       (void*) (trx_doublewrite->write_buf
				+ UNIV_PAGE_SIZE * i), NULL);
	} else {
		/* It is a regular page. Write it directly to the
		doublewrite buffer */
		fil_io(OS_FILE_WRITE, TRUE, TRX_SYS_SPACE, 0,
		       offset, 0, UNIV_PAGE_SIZE,
		       (void*) ((buf_block_t*) bpage)->frame,
		       NULL);
	}

	/* Now flush the doublewrite buffer data to disk */
	fil_flush(TRX_SYS_SPACE);

	/* We know that the write has been flushed to disk now
	and during recovery we will find it in the doublewrite buffer
	blocks. Next do the write to the intended position. */
	buf_flush_write_block_to_datafile((buf_block_t*) bpage);

	/* Sync the writes to the disk. */
	buf_flush_sync_datafiles();

	mutex_enter(&trx_doublewrite->mutex);

	trx_doublewrite->n_reserved--;
	trx_doublewrite->buf_block_arr[i] = NULL;
	trx_doublewrite->in_use[i] = FALSE;

	/* increment the doublewrite flushed pages counter */
	srv_dblwr_pages_written += trx_doublewrite->first_free;
	srv_dblwr_writes++;

	mutex_exit(&(trx_doublewrite->mutex));

}
#endif /* !UNIV_HOTBACKUP */

/********************************************************************//**
Initializes a page for writing to the tablespace. */
UNIV_INTERN
void
buf_flush_init_for_writing(
/*=======================*/
	byte*	page,		/*!< in/out: page */
	void*	page_zip_,	/*!< in/out: compressed page, or NULL */
	lsn_t	newest_lsn)	/*!< in: newest modification lsn
				to the page */
{
	ib_uint32_t	checksum = 0 /* silence bogus gcc warning */;

	ut_ad(page);

	if (page_zip_) {
		page_zip_des_t*	page_zip;
		ulint		zip_size;

		page_zip = static_cast<page_zip_des_t*>(page_zip_);
		zip_size = page_zip_get_size(page_zip);

		ut_ad(zip_size);
		ut_ad(ut_is_2pow(zip_size));
		ut_ad(zip_size <= UNIV_ZIP_SIZE_MAX);

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
			checksum = page_zip_calc_checksum(
				page_zip->data, zip_size,
				static_cast<srv_checksum_algorithm_t>(
					srv_checksum_algorithm));

			mach_write_to_8(page_zip->data
					+ FIL_PAGE_LSN, newest_lsn);
			memset(page_zip->data + FIL_PAGE_FILE_FLUSH_LSN, 0, 8);
			mach_write_to_4(page_zip->data
					+ FIL_PAGE_SPACE_OR_CHKSUM,
					checksum);
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

	switch ((srv_checksum_algorithm_t) srv_checksum_algorithm) {
	case SRV_CHECKSUM_ALGORITHM_CRC32:
	case SRV_CHECKSUM_ALGORITHM_STRICT_CRC32:
		checksum = buf_calc_page_crc32(page);
		break;
	case SRV_CHECKSUM_ALGORITHM_INNODB:
	case SRV_CHECKSUM_ALGORITHM_STRICT_INNODB:
		checksum = (ib_uint32_t) buf_calc_page_new_checksum(page);
		break;
	case SRV_CHECKSUM_ALGORITHM_NONE:
	case SRV_CHECKSUM_ALGORITHM_STRICT_NONE:
		checksum = BUF_NO_CHECKSUM_MAGIC;
		break;
	/* no default so the compiler will emit a warning if new enum
	is added and not handled here */
	}

	mach_write_to_4(page + FIL_PAGE_SPACE_OR_CHKSUM, checksum);

	/* We overwrite the first 4 bytes of the end lsn field to store
	the old formula checksum. Since it depends also on the field
	FIL_PAGE_SPACE_OR_CHKSUM, it has to be calculated after storing the
	new formula checksum. */

	if (srv_checksum_algorithm == SRV_CHECKSUM_ALGORITHM_STRICT_INNODB
	    || srv_checksum_algorithm == SRV_CHECKSUM_ALGORITHM_INNODB) {

		checksum = (ib_uint32_t) buf_calc_page_old_checksum(page);

		/* In other cases we use the value assigned from above.
		If CRC32 is used then it is faster to use that checksum
		(calculated above) instead of calculating another one.
		We can afford to store something other than
		buf_calc_page_old_checksum() or BUF_NO_CHECKSUM_MAGIC in
		this field because the file will not be readable by old
		versions of MySQL/InnoDB anyway (older than MySQL 5.6.3) */
	}

	mach_write_to_4(page + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM,
			checksum);
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
	buf_page_t*	bpage,		/*!< in: buffer block to write */
	enum buf_flush	flush_type)	/*!< in: type of flush */
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

		ut_a(page_zip_verify_checksum(frame, zip_size));

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
	} else if (flush_type == BUF_FLUSH_SINGLE_PAGE) {
		buf_flush_write_to_dblwr_and_datafile(bpage);
	} else {
		buf_flush_post_to_doublewrite_buf(bpage);
	}
}

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
	enum buf_flush	flush_type)	/*!< in: type of flush */
{
	mutex_t*	block_mutex;
	ibool		is_uncompressed;

	ut_ad(flush_type < BUF_FLUSH_N_TYPES);
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
	case BUF_FLUSH_SINGLE_PAGE:
		/* VERY IMPORTANT:
		Because any thread may call single page flush, even when
		owning locks on pages, to avoid deadlocks, we must make
		sure that the s-lock is acquired on the page without
		waiting: this is accomplished because
		buf_flush_ready_for_flush() must hold, and that requires
		the page not to be bufferfixed.
		The same holds true for LRU flush because a user thread
		may end up waiting for an LRU flush to end while
		holding locks on other pages. */

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
	buf_flush_write_block_low(bpage, flush_type);
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

	if (!buf_flush_ready_for_flush(&block->page, BUF_FLUSH_SINGLE_PAGE)) {
		return(FALSE);
	}

	/* The following call will release the buffer pool and
	block mutex. */
	buf_flush_page(buf_pool, &block->page, BUF_FLUSH_SINGLE_PAGE);
	return(TRUE);
}
# endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */
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

	if (UT_LIST_GET_LEN(buf_pool->LRU) < BUF_LRU_OLD_MIN_LEN
	    || !srv_flush_neighbors) {
		/* If there is little space or neighbor flushing is
		not enabled then just flush the victim. */
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

	if (count > 0) {
		MONITOR_INC_VALUE_CUMULATIVE(
					MONITOR_FLUSH_NEIGHBOR_TOTAL_PAGE,
					MONITOR_FLUSH_NEIGHBOR_COUNT,
					MONITOR_FLUSH_NEIGHBOR_PAGES,
					(count - 1));
	}

	return(count);
}

/********************************************************************//**
Check if the block is modified and ready for flushing. If the the block
is ready to flush then flush the page and try o flush its neighbors.

@return	TRUE if buf_pool mutex was released during this function.
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
This utility moves the uncompressed frames of pages to the free list.
Note that this function does not actually flush any data to disk. It
just detaches the uncompressed frames from the compressed pages at the
tail of the unzip_LRU and puts those freed frames in the free list.
Note that it is a best effort attempt and it is not guaranteed that
after a call to this function there will be 'max' blocks in the free
list.
@return number of blocks moved to the free list. */
static
ulint
buf_free_from_unzip_LRU_list_batch(
/*===============================*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	ulint		max)		/*!< in: desired number of
					blocks in the free_list */
{
	buf_block_t*	block;
	ulint		scanned = 0;
	ulint		count = 0;
	ulint		free_len = UT_LIST_GET_LEN(buf_pool->free);
	ulint		lru_len = UT_LIST_GET_LEN(buf_pool->unzip_LRU);

	ut_ad(buf_pool_mutex_own(buf_pool));

	block = UT_LIST_GET_LAST(buf_pool->unzip_LRU);
	while (block != NULL && count < max
	       && free_len < srv_LRU_scan_depth
	       && lru_len > UT_LIST_GET_LEN(buf_pool->LRU) / 10) {

		++scanned;
		if (buf_LRU_free_block(&block->page, FALSE)) {
			/* Block was freed. buf_pool->mutex potentially
			released and reacquired */
			++count;
			block = UT_LIST_GET_LAST(buf_pool->unzip_LRU);

		} else {

			block = UT_LIST_GET_PREV(unzip_LRU, block);
		}

		free_len = UT_LIST_GET_LEN(buf_pool->free);
		lru_len = UT_LIST_GET_LEN(buf_pool->unzip_LRU);
	}

	ut_ad(buf_pool_mutex_own(buf_pool));

	if (scanned) {
		MONITOR_INC_VALUE_CUMULATIVE(
			MONITOR_LRU_BATCH_SCANNED,
			MONITOR_LRU_BATCH_SCANNED_NUM_CALL,
			MONITOR_LRU_BATCH_SCANNED_PER_CALL,
			scanned);
	}

	return(count);
}

/*******************************************************************//**
This utility flushes dirty blocks from the end of the LRU list.
The calling thread is not allowed to own any latches on pages!
It attempts to make 'max' blocks available in the free list. Note that
it is a best effort attempt and it is not guaranteed that after a call
to this function there will be 'max' blocks in the free list.
@return number of blocks for which the write request was queued. */
static
ulint
buf_flush_LRU_list_batch(
/*=====================*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	ulint		max)		/*!< in: desired number of
					blocks in the free_list */
{
	buf_page_t*	bpage;
	ulint		scanned = 0;
	ulint		count = 0;
	ulint		free_len = UT_LIST_GET_LEN(buf_pool->free);
	ulint		lru_len = UT_LIST_GET_LEN(buf_pool->LRU);

	ut_ad(buf_pool_mutex_own(buf_pool));

	bpage = UT_LIST_GET_LAST(buf_pool->LRU);
	while (bpage != NULL && count < max
	       && free_len < srv_LRU_scan_depth
	       && lru_len > BUF_LRU_MIN_LEN) {

		mutex_t* block_mutex = buf_page_get_mutex(bpage);
		ibool	 evict;

		mutex_enter(block_mutex);
		evict = buf_flush_ready_for_replace(bpage);
		mutex_exit(block_mutex);

		++scanned;

		/* If the block is ready to be replaced we try to
		free it i.e.: put it on the free list.
		Otherwise we try to flush the block and its
		neighbors. In this case we'll put it on the
		free list in the next pass. We do this extra work
		of putting blocks to the free list instead of
		just flushing them because after every flush
		we have to restart the scan from the tail of
		the LRU list and if we don't clear the tail
		of the flushed pages then the scan becomes
		O(n*n). */
		if (evict) {
			if (buf_LRU_free_block(bpage, TRUE)) {
				/* buf_pool->mutex was potentially
				released and reacquired. */
				bpage = UT_LIST_GET_LAST(buf_pool->LRU);
			} else {
				bpage = UT_LIST_GET_PREV(LRU, bpage);
			}
		} else if (buf_flush_page_and_try_neighbors(
				bpage,
				BUF_FLUSH_LRU, max, &count)) {

			/* buf_pool->mutex was released.
			Restart the scan. */
			bpage = UT_LIST_GET_LAST(buf_pool->LRU);
		} else {
			bpage = UT_LIST_GET_PREV(LRU, bpage);
		}

		free_len = UT_LIST_GET_LEN(buf_pool->free);
		lru_len = UT_LIST_GET_LEN(buf_pool->LRU);
	}

	/* We keep track of all flushes happening as part of LRU
	flush. When estimating the desired rate at which flush_list
	should be flushed, we factor in this value. */
	buf_lru_flush_page_count += count;

	ut_ad(buf_pool_mutex_own(buf_pool));

	if (scanned) {
		MONITOR_INC_VALUE_CUMULATIVE(
			MONITOR_LRU_BATCH_SCANNED,
			MONITOR_LRU_BATCH_SCANNED_NUM_CALL,
			MONITOR_LRU_BATCH_SCANNED_PER_CALL,
			scanned);
	}

	return(count);
}

/*******************************************************************//**
Flush and move pages from LRU or unzip_LRU list to the free list.
Whether LRU or unzip_LRU is used depends on the state of the system.
@return number of blocks for which either the write request was queued
or in case of unzip_LRU the number of blocks actually moved to the
free list */
static
ulint
buf_do_LRU_batch(
/*=============*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	ulint		max)		/*!< in: desired number of
					blocks in the free_list */
{
	ulint	count = 0;

	if (buf_LRU_evict_from_unzip_LRU(buf_pool)) {
		count += buf_free_from_unzip_LRU_list_batch(buf_pool, max);
	}

	if (max > count) {
		count += buf_flush_LRU_list_batch(buf_pool, max - count);
	}

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
buf_do_flush_list_batch(
/*====================*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	ulint		min_n,		/*!< in: wished minimum mumber
					of blocks flushed (it is not
					guaranteed that the actual
					number is that big, though) */
	lsn_t		lsn_limit)	/*!< all blocks whose
					oldest_modification is smaller
					than this should be flushed (if
					their number does not exceed
					min_n) */
{
	ulint		len;
	buf_page_t*	bpage;
	ulint		count = 0;
	ulint		scanned = 0;

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

			++scanned;
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

	MONITOR_INC_VALUE_CUMULATIVE(MONITOR_FLUSH_BATCH_SCANNED,
				     MONITOR_FLUSH_BATCH_SCANNED_NUM_CALL,
				     MONITOR_FLUSH_BATCH_SCANNED_PER_CALL,
				     scanned);

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
	lsn_t		lsn_limit)	/*!< in: in the case of BUF_FLUSH_LIST
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
		count = buf_do_LRU_batch(buf_pool, min_n);
		break;
	case BUF_FLUSH_LIST:
		count = buf_do_flush_list_batch(buf_pool, min_n, lsn_limit);
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

	srv_buf_pool_flushed += count;

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

	if (flush_type == BUF_FLUSH_LRU) {
		/* We keep track of all flushes happening as part of LRU
		flush. When estimating the desired rate at which flush_list
		should be flushed we factor in this value. */
		buf_lru_flush_page_count += page_count;
	}
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

	buf_pool->try_LRU_scan = TRUE;

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
This utility flushes dirty blocks from the end of the LRU list and also
puts replaceable clean pages from the end of the LRU list to the free
list.
NOTE: The calling thread is not allowed to own any latches on pages!
@return number of blocks for which the write request was queued;
ULINT_UNDEFINED if there was a flush of the same type already running */
static
ulint
buf_flush_LRU(
/*==========*/
	buf_pool_t*	buf_pool,	/*!< in/out: buffer pool instance */
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
	lsn_t		lsn_limit)	/*!< in the case BUF_FLUSH_LIST all
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

		if (page_count) {
			MONITOR_INC_VALUE_CUMULATIVE(
				MONITOR_FLUSH_BATCH_TOTAL_PAGE,
				MONITOR_FLUSH_BATCH_COUNT,
				MONITOR_FLUSH_BATCH_PAGES,
				page_count);
		}
	}

	return(lsn_limit != LSN_MAX && skipped
	       ? ULINT_UNDEFINED : total_page_count);
}

/******************************************************************//**
This function picks up a single dirty page from the tail of the LRU
list, flushes it, removes it from page_hash and LRU list and puts
it on the free list. It is called from user threads when they are
unable to find a replaceable page at the tail of the LRU list i.e.:
when the background LRU flushing in the page_cleaner thread is not
fast enough to keep pace with the workload.
@return TRUE if success. */
UNIV_INTERN
ibool
buf_flush_single_page_from_LRU(
/*===========================*/
	buf_pool_t*	buf_pool)	/*!< in/out: buffer pool instance */
{
	ulint		scanned;
	buf_page_t*	bpage;
	mutex_t*	block_mutex;
	ibool		freed;
	ibool		evict_zip;

	buf_pool_mutex_enter(buf_pool);

	for (bpage = UT_LIST_GET_LAST(buf_pool->LRU), scanned = 1;
	     bpage != NULL;
	     bpage = UT_LIST_GET_PREV(LRU, bpage), ++scanned) {

		block_mutex = buf_page_get_mutex(bpage);
		mutex_enter(block_mutex);
		if (buf_flush_ready_for_flush(bpage,
					      BUF_FLUSH_SINGLE_PAGE)) {
			/* buf_flush_page() will release the block
			mutex */
			break;
		}
		mutex_exit(block_mutex);
	}

	MONITOR_INC_VALUE_CUMULATIVE(
		MONITOR_LRU_SINGLE_FLUSH_SCANNED,
		MONITOR_LRU_SINGLE_FLUSH_SCANNED_NUM_CALL,
		MONITOR_LRU_SINGLE_FLUSH_SCANNED_PER_CALL,
		scanned);

	if (!bpage) {
		/* Can't find a single flushable page. */
		buf_pool_mutex_exit(buf_pool);
		return(FALSE);
	}

	/* The following call will release the buffer pool and
	block mutex. */
	buf_flush_page(buf_pool, bpage, BUF_FLUSH_SINGLE_PAGE);

	/* At this point the page has been written to the disk.
	As we are not holding buffer pool or block mutex therefore
	we cannot use the bpage safely. It may have been plucked out
	of the LRU list by some other thread or it may even have
	relocated in case of a compressed page. We need to start
	the scan of LRU list again to remove the block from the LRU
	list and put it on the free list. */
	buf_pool_mutex_enter(buf_pool);

	for (bpage = UT_LIST_GET_LAST(buf_pool->LRU);
	     bpage != NULL;
	     bpage = UT_LIST_GET_PREV(LRU, bpage)) {

		ibool	ready;

		block_mutex = buf_page_get_mutex(bpage);
		mutex_enter(block_mutex);
		ready = buf_flush_ready_for_replace(bpage);
		mutex_exit(block_mutex);
		if (ready) {
			break;
		}

	}

	if (!bpage) {
		/* Can't find a single replaceable page. */
		buf_pool_mutex_exit(buf_pool);
		return(FALSE);
	}

	evict_zip = !buf_LRU_evict_from_unzip_LRU(buf_pool);;

	freed = buf_LRU_free_block(bpage, evict_zip);
	buf_pool_mutex_exit(buf_pool);

	return(freed);
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
	lsn_t			lsn_diff;
	lsn_t			lsn;
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
static
ulint
buf_flush_get_desired_flush_rate(void)
/*==================================*/
{
	ulint		i;
	lsn_t		redo_avg;
	ulint		n_dirty = 0;
	ib_uint64_t	n_flush_req;
	ib_uint64_t	lru_flush_avg;
	lsn_t		lsn = log_get_lsn();
	lsn_t		log_capacity = log_get_capacity();

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
	redo_avg = buf_flush_stat_sum.redo / BUF_FLUSH_STAT_N_INTERVAL
		+ (lsn - buf_flush_stat_cur.redo);

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
	if (n_flush_req <= lru_flush_avg) {
		return(0);
	} else {
		ib_uint64_t	rate;

		rate = n_flush_req - lru_flush_avg;

		return((ulint) (rate < PCT_IO(100) ? rate : PCT_IO(100)));
	}
}

/*********************************************************************//**
Clears up tail of the LRU lists:
* Put replaceable pages at the tail of LRU to the free list
* Flush dirty pages at the tail of LRU to the disk
The depth to which we scan each buffer pool is controlled by dynamic
config parameter innodb_LRU_scan_depth.
@return total pages flushed */
UNIV_INLINE
ulint
page_cleaner_flush_LRU_tail(void)
/*=============================*/
{
	ulint	i;
	ulint	j;
	ulint	total_flushed = 0;

	for (i = 0; i < srv_buf_pool_instances; i++) {

		buf_pool_t*	buf_pool = buf_pool_from_array(i);

		/* We divide LRU flush into smaller chunks because
		there may be user threads waiting for the flush to
		end in buf_LRU_get_free_block(). */
		for (j = 0;
		     j < srv_LRU_scan_depth;
		     j += PAGE_CLEANER_LRU_BATCH_CHUNK_SIZE) {

			ulint	n_flushed = buf_flush_LRU(buf_pool,
				PAGE_CLEANER_LRU_BATCH_CHUNK_SIZE);

			/* Currently page_cleaner is the only thread
			that can trigger an LRU flush. It is possible
			that a batch triggered during last iteration is
			still running, */
			if (n_flushed != ULINT_UNDEFINED) {
				total_flushed += n_flushed;
			}
		}
	}

	if (total_flushed) {
		MONITOR_INC_VALUE_CUMULATIVE(
			MONITOR_LRU_BATCH_TOTAL_PAGE,
			MONITOR_LRU_BATCH_COUNT,
			MONITOR_LRU_BATCH_PAGES,
			total_flushed);
	}

	return(total_flushed);
}

/*********************************************************************//**
Wait for any possible LRU flushes that are in progress to end. */
UNIV_INLINE
void
page_cleaner_wait_LRU_flush(void)
/*=============================*/
{
	ulint	i;

	for (i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		buf_pool_mutex_enter(buf_pool);

		if (buf_pool->n_flush[BUF_FLUSH_LRU] > 0
		   || buf_pool->init_flush[BUF_FLUSH_LRU]) {

			buf_pool_mutex_exit(buf_pool);
			buf_flush_wait_batch_end(buf_pool, BUF_FLUSH_LRU);
		} else {
			buf_pool_mutex_exit(buf_pool);
		}
	}
}

/*********************************************************************//**
Flush a batch of dirty pages from the flush list
@return number of pages flushed, 0 if no page is flushed or if another
flush_list type batch is running */
static
ulint
page_cleaner_do_flush_batch(
/*========================*/
	ulint		n_to_flush,	/*!< in: number of pages that
					we should attempt to flush. If
					an lsn_limit is provided then
					this value will have no affect */
	lsn_t		lsn_limit)	/*!< in: LSN up to which flushing
					must happen */
{
	ulint n_flushed;

	ut_ad(n_to_flush == ULINT_MAX || lsn_limit == LSN_MAX);

	n_flushed = buf_flush_list(n_to_flush, lsn_limit);
	if (n_flushed == ULINT_UNDEFINED) {
		n_flushed = 0;
	}

	return(n_flushed);
}

/*********************************************************************//**
This function is called approximately once every second by the
page_cleaner thread. Based on various factors it decides if there is a
need to do flushing. If flushing is needed it is performed and the
number of pages flushed is returned.
@return number of pages flushed */
static
ulint
page_cleaner_flush_pages_if_needed(void)
/*====================================*/
{
	ulint	n_pages_flushed = 0;
	lsn_t	lsn_limit = log_async_flush_lsn();

	/* Currently we decide whether or not to flush and how much to
	flush based on three factors.

	1) If the amount of LSN for which pages are not flushed to disk
	yet is greater than log_sys->max_modified_age_async. This is
	the most urgent type of flush and we attempt to cleanup enough
	of the tail of the flush_list to avoid flushing inside user
	threads.

	2) If modified page ratio is greater than the one specified by
	the user. In that case we flush full 100% IO_CAPACITY of the
	server. Note that 1 and 2 are not mutually exclusive. We can
	end up executing both steps.

	3) If adaptive_flushing is set by the user and neither of 1
	or 2 has occurred above then we flush a batch based on our
	heuristics. */

	if (lsn_limit != LSN_MAX) {

		/* async flushing is requested */
		n_pages_flushed = page_cleaner_do_flush_batch(ULINT_MAX,
							      lsn_limit);

		MONITOR_INC_VALUE_CUMULATIVE(
			MONITOR_FLUSH_ASYNC_TOTAL_PAGE,
			MONITOR_FLUSH_ASYNC_COUNT,
			MONITOR_FLUSH_ASYNC_PAGES,
			n_pages_flushed);
	}

	if (UNIV_UNLIKELY(n_pages_flushed < PCT_IO(100)
			  && buf_get_modified_ratio_pct()
			     > srv_max_buf_pool_modified_pct)) {

		/* Try to keep the number of modified pages in the
		buffer pool under the limit wished by the user */

		n_pages_flushed += page_cleaner_do_flush_batch(PCT_IO(100),
							       LSN_MAX);

		MONITOR_INC_VALUE_CUMULATIVE(
			MONITOR_FLUSH_MAX_DIRTY_TOTAL_PAGE,
			MONITOR_FLUSH_MAX_DIRTY_COUNT,
			MONITOR_FLUSH_MAX_DIRTY_PAGES,
			n_pages_flushed);
	}

	if (srv_adaptive_flushing && n_pages_flushed == 0) {

		/* Try to keep the rate of flushing of dirty
		pages such that redo log generation does not
		produce bursts of IO at checkpoint time. */
		ulint n_flush = buf_flush_get_desired_flush_rate();

		ut_ad(n_flush <= PCT_IO(100));
		if (n_flush) {
			n_pages_flushed = page_cleaner_do_flush_batch(
				n_flush, LSN_MAX);

			MONITOR_INC_VALUE_CUMULATIVE(
				MONITOR_FLUSH_ADAPTIVE_TOTAL_PAGE,
				MONITOR_FLUSH_ADAPTIVE_COUNT,
				MONITOR_FLUSH_ADAPTIVE_PAGES,
				n_pages_flushed);
		}
	}

	return(n_pages_flushed);
}

/*********************************************************************//**
Puts the page_cleaner thread to sleep if it has finished work in less
than a second */
static
void
page_cleaner_sleep_if_needed(
/*=========================*/
	ulint	next_loop_time)	/*!< in: time when next loop iteration
				should start */
{
	ulint	cur_time = ut_time_ms();

	if (next_loop_time > cur_time) {
		/* Get sleep interval in micro seconds. We use
		ut_min() to avoid long sleep in case of
		wrap around. */
		os_thread_sleep(ut_min(1000000,
				(next_loop_time - cur_time)
				 * 1000));
	}
}

/******************************************************************//**
page_cleaner thread tasked with flushing dirty pages from the buffer
pools. As of now we'll have only one instance of this thread.
@return a dummy parameter */
extern "C" UNIV_INTERN
os_thread_ret_t
DECLARE_THREAD(buf_flush_page_cleaner_thread)(
/*==========================================*/
	void*	arg __attribute__((unused)))
			/*!< in: a dummy parameter required by
			os_thread_create */
{
	ulint	next_loop_time = ut_time_ms() + 1000;
	ulint	n_flushed = 0;
	ulint	last_activity = srv_get_activity_count();
	ulint	i;

#ifdef UNIV_PFS_THREAD
	pfs_register_thread(buf_page_cleaner_thread_key);
#endif /* UNIV_PFS_THREAD */

#ifdef UNIV_DEBUG_THREAD_CREATION
	fprintf(stderr, "InnoDB: page_cleaner thread running, id %lu\n",
		os_thread_pf(os_thread_get_curr_id()));
#endif /* UNIV_DEBUG_THREAD_CREATION */

	buf_page_cleaner_is_active = TRUE;

	while (srv_shutdown_state == SRV_SHUTDOWN_NONE) {

		/* The page_cleaner skips sleep if the server is
		idle and there are no pending IOs in the buffer pool
		and there is work to do. */
		if (srv_check_activity(last_activity)
		    || buf_get_n_pending_read_ios()
		    || n_flushed == 0) {
			page_cleaner_sleep_if_needed(next_loop_time);
		}

		next_loop_time = ut_time_ms() + 1000;

		if (srv_check_activity(last_activity)) {
			last_activity = srv_get_activity_count();

			/* Flush pages from end of LRU if required */
			n_flushed = page_cleaner_flush_LRU_tail();

			/* Flush pages from flush_list if required */
			n_flushed += page_cleaner_flush_pages_if_needed();
		} else {
			n_flushed = page_cleaner_do_flush_batch(
							PCT_IO(100),
							LSN_MAX);

			if (n_flushed) {
				MONITOR_INC_VALUE_CUMULATIVE(
					MONITOR_FLUSH_BACKGROUND_TOTAL_PAGE,
					MONITOR_FLUSH_BACKGROUND_COUNT,
					MONITOR_FLUSH_BACKGROUND_PAGES,
					n_flushed);
			}
		}
	}

	ut_ad(srv_shutdown_state > 0);
	if (srv_fast_shutdown == 2) {
		/* In very fast shutdown we simulate a crash of
		buffer pool. We are not required to do any flushing */
		goto thread_exit;
	}

	/* In case of normal and slow shutdown the page_cleaner thread
	must wait for all other activity in the server to die down.
	Note that we can start flushing the buffer pool as soon as the
	server enters shutdown phase but we must stay alive long enough
	to ensure that any work done by the master or purge threads is
	also flushed.
	During shutdown we pass through two stages. In the first stage,
	when SRV_SHUTDOWN_CLEANUP is set other threads like the master
	and the purge threads may be working as well. We start flushing
	the buffer pool but can't be sure that no new pages are being
	dirtied until we enter SRV_SHUTDOWN_FLUSH_PHASE phase. */

	do {
		n_flushed = page_cleaner_do_flush_batch(PCT_IO(100), LSN_MAX);

		/* We sleep only if there are no pages to flush */
		if (n_flushed == 0) {
			os_thread_sleep(100000);
		}
	} while (srv_shutdown_state == SRV_SHUTDOWN_CLEANUP);

	/* At this point all threads including the master and the purge
	thread must have been suspended. */
	ut_a(srv_get_active_thread_type() == SRV_NONE);
	ut_a(srv_shutdown_state == SRV_SHUTDOWN_FLUSH_PHASE);

	/* We can now make a final sweep on flushing the buffer pool
	and exit after we have cleaned the whole buffer pool. 
	It is important that we wait for any running batch that has
	been triggered by us to finish. Otherwise we can end up
	considering end of that batch as a finish of our final
	sweep and we'll come out of the loop leaving behind dirty pages
	in the flush_list */
	buf_flush_wait_batch_end(NULL, BUF_FLUSH_LIST);
	page_cleaner_wait_LRU_flush();

	do {

		n_flushed = buf_flush_list(PCT_IO(100), LSN_MAX);
		buf_flush_wait_batch_end(NULL, BUF_FLUSH_LIST);

	} while (n_flushed > 0);

	/* Some sanity checks */
	ut_a(srv_get_active_thread_type() == SRV_NONE);
	ut_a(srv_shutdown_state == SRV_SHUTDOWN_FLUSH_PHASE);
	for (i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t* buf_pool = buf_pool_from_array(i);
		ut_a(UT_LIST_GET_LEN(buf_pool->flush_list) == 0);
	}

	/* We have lived our life. Time to die. */

thread_exit:
	buf_page_cleaner_is_active = FALSE;

	/* We count the number of threads in os_thread_exit(). A created
	thread should always use that to exit and not use return() to exit. */
	os_thread_exit(NULL);

	OS_THREAD_DUMMY_RETURN;
}

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG

/** Functor to validate the flush list. */
struct	Check {
	void	operator()(const buf_page_t* elem)
	{
		ut_a(elem->in_flush_list);
	}
};

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

	UT_LIST_VALIDATE(list, buf_page_t, buf_pool->flush_list, Check());

	bpage = UT_LIST_GET_FIRST(buf_pool->flush_list);

	/* If we are in recovery mode i.e.: flush_rbt != NULL
	then each block in the flush_list must also be present
	in the flush_rbt. */
	if (UNIV_LIKELY_NULL(buf_pool->flush_rbt)) {
		rnode = rbt_first(buf_pool->flush_rbt);
	}

	while (bpage != NULL) {
		const lsn_t	om = bpage->oldest_modification;

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
