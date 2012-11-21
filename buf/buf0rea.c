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
@file buf/buf0rea.c
The database buffer read

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#include "buf0rea.h"

#include "fil0fil.h"
#include "mtr0mtr.h"

#include "buf0buf.h"
#include "buf0flu.h"
#include "buf0lru.h"
#include "ibuf0ibuf.h"
#include "log0recv.h"
#include "trx0sys.h"
#include "os0file.h"
#include "srv0start.h"
#include "srv0srv.h"
#include "mysql/plugin.h"
#include "mysql/service_thd_wait.h"

/** There must be at least this many pages in buf_pool in the area to start
a random read-ahead */
#define BUF_READ_AHEAD_RANDOM_THRESHOLD(b)	\
				(5 + BUF_READ_AHEAD_AREA(b) / 8)

/** If there are buf_pool->curr_size per the number below pending reads, then
read-ahead is not done: this is to prevent flooding the buffer pool with
i/o-fixed buffer blocks */
#define BUF_READ_AHEAD_PEND_LIMIT	2

/********************************************************************//**
Unfixes the pages, unlatches the page,
removes it from page_hash and removes it from LRU. */
static
void
buf_read_page_handle_error(
/*=======================*/
	buf_page_t*	bpage)	/*!< in: pointer to the block */
{
	buf_pool_t*	buf_pool = buf_pool_from_bpage(bpage);
	const ibool	uncompressed = (buf_page_get_state(bpage)
					== BUF_BLOCK_FILE_PAGE);

	/* First unfix and release lock on the bpage */
	mutex_enter(&buf_pool->LRU_list_mutex);
	mutex_enter(buf_page_get_mutex(bpage));
	ut_ad(buf_page_get_io_fix(bpage) == BUF_IO_READ);
	ut_ad(bpage->buf_fix_count == 0);

	/* Set BUF_IO_NONE before we remove the block from LRU list */
	buf_page_set_io_fix(bpage, BUF_IO_NONE);

	if (uncompressed) {
		rw_lock_x_unlock_gen(
			&((buf_block_t*) bpage)->lock,
			BUF_IO_READ);
	}

	/* remove the block from LRU list */
	buf_LRU_free_one_page(bpage);

	ut_ad(buf_pool->n_pend_reads > 0);
	buf_pool->n_pend_reads--;

	mutex_exit(buf_page_get_mutex(bpage));
	mutex_exit(&buf_pool->LRU_list_mutex);
}

/********************************************************************//**
Low-level function which reads a page asynchronously from a file to the
buffer buf_pool if it is not already there, in which case does nothing.
Sets the io_fix flag and sets an exclusive lock on the buffer frame. The
flag is cleared and the x-lock released by an i/o-handler thread.
@return 1 if a read request was queued, 0 if the page already resided
in buf_pool, or if the page is in the doublewrite buffer blocks in
which case it is never read into the pool, or if the tablespace does
not exist or is being dropped 
@return 1 if read request is issued. 0 if it is not */
UNIV_INTERN
ulint
buf_read_page_low(
/*==============*/
	ulint*	err,	/*!< out: DB_SUCCESS or DB_TABLESPACE_DELETED if we are
			trying to read from a non-existent tablespace, or a
			tablespace which is just now being dropped */
	ibool	sync,	/*!< in: TRUE if synchronous aio is desired */
	ulint	mode,	/*!< in: BUF_READ_IBUF_PAGES_ONLY, ...,
			ORed to OS_AIO_SIMULATED_WAKE_LATER (see below
			at read-ahead functions) */
	ulint	space,	/*!< in: space id */
	ulint	zip_size,/*!< in: compressed page size, or 0 */
	ibool	unzip,	/*!< in: TRUE=request uncompressed page */
	ib_int64_t tablespace_version, /*!< in: if the space memory object has
			this timestamp different from what we are giving here,
			treat the tablespace as dropped; this is a timestamp we
			use to stop dangling page reads from a tablespace
			which we have DISCARDed + IMPORTed back */
	ulint	offset,	/*!< in: page number */
	trx_t*	trx)
{
	buf_page_t*	bpage;
	ulint		wake_later;

	*err = DB_SUCCESS;

	wake_later = mode & OS_AIO_SIMULATED_WAKE_LATER;
	mode = mode & ~OS_AIO_SIMULATED_WAKE_LATER;

	if (trx_doublewrite
	    && (space == TRX_SYS_SPACE
		|| (srv_doublewrite_file && space == TRX_DOUBLEWRITE_SPACE))
	    && (   (offset >= trx_doublewrite->block1
		    && offset < trx_doublewrite->block1
		    + TRX_SYS_DOUBLEWRITE_BLOCK_SIZE)
		   || (offset >= trx_doublewrite->block2
		       && offset < trx_doublewrite->block2
		       + TRX_SYS_DOUBLEWRITE_BLOCK_SIZE))) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Warning: trying to read"
			" doublewrite buffer page %lu\n",
			(ulong) offset);

		return(0);
	}

	if (ibuf_bitmap_page(zip_size, offset)
	    || trx_sys_hdr_page(space, offset)) {

		/* Trx sys header is so low in the latching order that we play
		safe and do not leave the i/o-completion to an asynchronous
		i/o-thread. Ibuf bitmap pages must always be read with
		syncronous i/o, to make sure they do not get involved in
		thread deadlocks. */

		sync = TRUE;
	}

	/* The following call will also check if the tablespace does not exist
	or is being dropped; if we succeed in initing the page in the buffer
	pool for read, then DISCARD cannot proceed until the read has
	completed */
	bpage = buf_page_init_for_read(err, mode, space, zip_size, unzip,
				       tablespace_version, offset);
	if (bpage == NULL) {
		/* bugfix: http://bugs.mysql.com/bug.php?id=43948 */
		if (recv_recovery_is_on() && *err == DB_TABLESPACE_DELETED) {
			/* hashed log recs must be treated here */
			recv_addr_t*    recv_addr;

			mutex_enter(&(recv_sys->mutex));

			if (recv_sys->apply_log_recs == FALSE) {
				mutex_exit(&(recv_sys->mutex));
				goto not_to_recover;
			}

			/* recv_get_fil_addr_struct() */
			recv_addr = HASH_GET_FIRST(recv_sys->addr_hash,
					hash_calc_hash(ut_fold_ulint_pair(space, offset),
						recv_sys->addr_hash));
			while (recv_addr) {
				if ((recv_addr->space == space)
					&& (recv_addr->page_no == offset)) {
					break;
				}
				recv_addr = HASH_GET_NEXT(addr_hash, recv_addr);
			}

			if ((recv_addr == NULL)
			    || (recv_addr->state == RECV_BEING_PROCESSED)
			    || (recv_addr->state == RECV_PROCESSED)) {
				mutex_exit(&(recv_sys->mutex));
				goto not_to_recover;
			}

			fprintf(stderr, " (cannot find space: %lu)", space);
			recv_addr->state = RECV_PROCESSED;

			ut_a(recv_sys->n_addrs);
			recv_sys->n_addrs--;

			mutex_exit(&(recv_sys->mutex));
		}
not_to_recover:

		return(0);
	}

#ifdef UNIV_DEBUG
	if (buf_debug_prints) {
		fprintf(stderr,
			"Posting read request for page %lu, sync %lu\n",
			(ulong) offset,
			(ulong) sync);
	}
#endif

	ut_ad(buf_page_in_file(bpage));

	thd_wait_begin(NULL, THD_WAIT_DISKIO);
	if (zip_size) {
		*err = _fil_io(OS_FILE_READ | wake_later,
			      sync, space, zip_size, offset, 0, zip_size,
			      bpage->zip.data, bpage, trx);
	} else {
		ut_a(buf_page_get_state(bpage) == BUF_BLOCK_FILE_PAGE);

		*err = _fil_io(OS_FILE_READ | wake_later,
			      sync, space, 0, offset, 0, UNIV_PAGE_SIZE,
			      ((buf_block_t*) bpage)->frame, bpage, trx);
	}
	thd_wait_end(NULL);

	if (*err == DB_TABLESPACE_DELETED) {
		buf_read_page_handle_error(bpage);
		return(0);
	}

	if (srv_pass_corrupt_table) {
		if (*err != DB_SUCCESS) {
			bpage->is_corrupt = TRUE;
		}
	} else {
	ut_a(*err == DB_SUCCESS);
	}

	if (sync) {
		/* The i/o is already completed when we arrive from
		fil_read */
		if (!buf_page_io_complete(bpage)) {
			return(0);
		}
	}

	return(1);
}

/********************************************************************//**
Applies a random read-ahead in buf_pool if there are at least a threshold
value of accessed pages from the random read-ahead area. Does not read any
page, not even the one at the position (space, offset), if the read-ahead
mechanism is not activated. NOTE 1: the calling thread may own latches on
pages: to avoid deadlocks this function must be written such that it cannot
end up waiting for these latches! NOTE 2: the calling thread must want
access to the page given: this rule is set to prevent unintended read-aheads
performed by ibuf routines, a situation which could result in a deadlock if
the OS does not support asynchronous i/o.
@return number of page read requests issued; NOTE that if we read ibuf
pages, it may happen that the page at the given page number does not
get read even if we return a positive value!
@return	number of page read requests issued */
UNIV_INTERN
ulint
buf_read_ahead_random(
/*==================*/
	ulint	space,		/*!< in: space id */
	ulint	zip_size,	/*!< in: compressed page size in bytes,
				or 0 */
	ulint	offset,		/*!< in: page number of a page which
				the current thread wants to access */
	ibool	inside_ibuf,	/*!< in: TRUE if we are inside ibuf
				routine */
	trx_t*	trx)
{
	buf_pool_t*	buf_pool = buf_pool_get(space, offset);
	ib_int64_t	tablespace_version;
	ulint		recent_blocks	= 0;
	ulint		ibuf_mode;
	ulint		count;
	ulint		low, high;
	ulint		err;
	ulint		i;
	const ulint	buf_read_ahead_random_area
				= BUF_READ_AHEAD_AREA(buf_pool);

	if (!srv_random_read_ahead) {
		/* Disabled by user */
		return(0);
	}

	if (srv_startup_is_before_trx_rollback_phase) {
		/* No read-ahead to avoid thread deadlocks */
		return(0);
	}

	if (ibuf_bitmap_page(zip_size, offset)
	    || trx_sys_hdr_page(space, offset)) {

		/* If it is an ibuf bitmap page or trx sys hdr, we do
		no read-ahead, as that could break the ibuf page access
		order */

		return(0);
	}

	/* Remember the tablespace version before we ask te tablespace size
	below: if DISCARD + IMPORT changes the actual .ibd file meanwhile, we
	do not try to read outside the bounds of the tablespace! */

	tablespace_version = fil_space_get_version(space);

	low  = (offset / buf_read_ahead_random_area)
		* buf_read_ahead_random_area;
	high = (offset / buf_read_ahead_random_area + 1)
		* buf_read_ahead_random_area;
	if (high > fil_space_get_size(space)) {

		high = fil_space_get_size(space);
	}

	buf_pool_mutex_enter(buf_pool);

	if (buf_pool->n_pend_reads
	    > buf_pool->curr_size / BUF_READ_AHEAD_PEND_LIMIT) {
		buf_pool_mutex_exit(buf_pool);

		return(0);
	}

	/* Count how many blocks in the area have been recently accessed,
	that is, reside near the start of the LRU list. */

	for (i = low; i < high; i++) {
		const buf_page_t* bpage =
			buf_page_hash_get(buf_pool, space, i);

		if (bpage
		    && buf_page_is_accessed(bpage)
		    && buf_page_peek_if_young(bpage)) {

			recent_blocks++;

			if (recent_blocks
			    >= BUF_READ_AHEAD_RANDOM_THRESHOLD(buf_pool)) {

				buf_pool_mutex_exit(buf_pool);
				goto read_ahead;
			}
		}
	}

	buf_pool_mutex_exit(buf_pool);
	/* Do nothing */
	return(0);

read_ahead:
	/* Read all the suitable blocks within the area */

	if (inside_ibuf) {
		ibuf_mode = BUF_READ_IBUF_PAGES_ONLY;
	} else {
		ibuf_mode = BUF_READ_ANY_PAGE;
	}

	count = 0;

	for (i = low; i < high; i++) {
		/* It is only sensible to do read-ahead in the non-sync aio
		mode: hence FALSE as the first parameter */

		if (!ibuf_bitmap_page(zip_size, i)) {
			count += buf_read_page_low(
				&err, FALSE,
				ibuf_mode | OS_AIO_SIMULATED_WAKE_LATER,
				space, zip_size, FALSE,
				tablespace_version, i, trx);
			if (err == DB_TABLESPACE_DELETED) {
				ut_print_timestamp(stderr);
				fprintf(stderr,
					"  InnoDB: Warning: in random"
					" readahead trying to access\n"
					"InnoDB: tablespace %lu page %lu,\n"
					"InnoDB: but the tablespace does not"
					" exist or is just being dropped.\n",
					(ulong) space, (ulong) i);
			}
		}
	}

	/* In simulated aio we wake the aio handler threads only after
	queuing all aio requests, in native aio the following call does
	nothing: */

	os_aio_simulated_wake_handler_threads();

#ifdef UNIV_DEBUG
	if (buf_debug_prints && (count > 0)) {
		fprintf(stderr,
			"Random read-ahead space %lu offset %lu pages %lu\n",
			(ulong) space, (ulong) offset,
			(ulong) count);
	}
#endif /* UNIV_DEBUG */

	/* Read ahead is considered one I/O operation for the purpose of
	LRU policy decision. */
	buf_LRU_stat_inc_io();

	buf_pool->stat.n_ra_pages_read_rnd += count;
	srv_buf_pool_reads += count;
	return(count);
}

/********************************************************************//**
High-level function which reads a page asynchronously from a file to the
buffer buf_pool if it is not already there. Sets the io_fix flag and sets
an exclusive lock on the buffer frame. The flag is cleared and the x-lock
released by the i/o-handler thread.
@return TRUE if page has been read in, FALSE in case of failure */
UNIV_INTERN
ibool
buf_read_page(
/*==========*/
	ulint	space,	/*!< in: space id */
	ulint	zip_size,/*!< in: compressed page size in bytes, or 0 */
	ulint	offset,	/*!< in: page number */
	trx_t*	trx)
{
	buf_pool_t*	buf_pool = buf_pool_get(space, offset);
	ib_int64_t	tablespace_version;
	ulint		count;
	ulint		err;

	tablespace_version = fil_space_get_version(space);

	/* We do the i/o in the synchronous aio mode to save thread
	switches: hence TRUE */

	count = buf_read_page_low(&err, TRUE, BUF_READ_ANY_PAGE, space,
				  zip_size, FALSE,
				  tablespace_version, offset, trx);
	srv_buf_pool_reads += count;
	if (err == DB_TABLESPACE_DELETED) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Error: trying to access"
			" tablespace %lu page no. %lu,\n"
			"InnoDB: but the tablespace does not exist"
			" or is just being dropped.\n",
			(ulong) space, (ulong) offset);
	}

	/* Flush pages from the end of the LRU list if necessary */
	buf_flush_free_margin(buf_pool, TRUE);

	/* Increment number of I/O operations used for LRU policy. */
	buf_LRU_stat_inc_io();

	return(count > 0);
}

/********************************************************************//**
Applies linear read-ahead if in the buf_pool the page is a border page of
a linear read-ahead area and all the pages in the area have been accessed.
Does not read any page if the read-ahead mechanism is not activated. Note
that the algorithm looks at the 'natural' adjacent successor and
predecessor of the page, which on the leaf level of a B-tree are the next
and previous page in the chain of leaves. To know these, the page specified
in (space, offset) must already be present in the buf_pool. Thus, the
natural way to use this function is to call it when a page in the buf_pool
is accessed the first time, calling this function just after it has been
bufferfixed.
NOTE 1: as this function looks at the natural predecessor and successor
fields on the page, what happens, if these are not initialized to any
sensible value? No problem, before applying read-ahead we check that the
area to read is within the span of the space, if not, read-ahead is not
applied. An uninitialized value may result in a useless read operation, but
only very improbably.
NOTE 2: the calling thread may own latches on pages: to avoid deadlocks this
function must be written such that it cannot end up waiting for these
latches!
NOTE 3: the calling thread must want access to the page given: this rule is
set to prevent unintended read-aheads performed by ibuf routines, a situation
which could result in a deadlock if the OS does not support asynchronous io.
@return	number of page read requests issued */
UNIV_INTERN
ulint
buf_read_ahead_linear(
/*==================*/
	ulint	space,		/*!< in: space id */
	ulint	zip_size,	/*!< in: compressed page size in bytes, or 0 */
	ulint	offset,		/*!< in: page number; see NOTE 3 above */
	ibool	inside_ibuf,	/*!< in: TRUE if we are inside ibuf routine */
	trx_t*	trx)
{
	buf_pool_t*	buf_pool = buf_pool_get(space, offset);
	ib_int64_t	tablespace_version;
	buf_page_t*	bpage;
	buf_frame_t*	frame;
	buf_page_t*	pred_bpage	= NULL;
	ulint		pred_offset;
	ulint		succ_offset;
	ulint		count;
	int		asc_or_desc;
	ulint		new_offset;
	ulint		fail_count;
	ulint		ibuf_mode;
	ulint		low, high;
	ulint		err;
	ulint		i;
	const ulint	buf_read_ahead_linear_area
		= BUF_READ_AHEAD_AREA(buf_pool);
	ulint		threshold;

	if (!(srv_read_ahead & 2)) {
		return(0);
	}

	if (UNIV_UNLIKELY(srv_startup_is_before_trx_rollback_phase)) {
		/* No read-ahead to avoid thread deadlocks */
		return(0);
	}

	low  = (offset / buf_read_ahead_linear_area)
		* buf_read_ahead_linear_area;
	high = (offset / buf_read_ahead_linear_area + 1)
		* buf_read_ahead_linear_area;

	if ((offset != low) && (offset != high - 1)) {
		/* This is not a border page of the area: return */

		return(0);
	}

	if (ibuf_bitmap_page(zip_size, offset)
	    || trx_sys_hdr_page(space, offset)) {

		/* If it is an ibuf bitmap page or trx sys hdr, we do
		no read-ahead, as that could break the ibuf page access
		order */

		return(0);
	}

	/* Remember the tablespace version before we ask te tablespace size
	below: if DISCARD + IMPORT changes the actual .ibd file meanwhile, we
	do not try to read outside the bounds of the tablespace! */

	tablespace_version = fil_space_get_version(space);

	buf_pool_mutex_enter(buf_pool);

	if (high > fil_space_get_size(space)) {
		buf_pool_mutex_exit(buf_pool);
		/* The area is not whole, return */

		return(0);
	}

	if (buf_pool->n_pend_reads
	    > buf_pool->curr_size / BUF_READ_AHEAD_PEND_LIMIT) {
		buf_pool_mutex_exit(buf_pool);

		return(0);
	}
	buf_pool_mutex_exit(buf_pool);

	/* Check that almost all pages in the area have been accessed; if
	offset == low, the accesses must be in a descending order, otherwise,
	in an ascending order. */

	asc_or_desc = 1;

	if (offset == low) {
		asc_or_desc = -1;
	}

	/* How many out of order accessed pages can we ignore
	when working out the access pattern for linear readahead */
	threshold = ut_min((64 - srv_read_ahead_threshold),
			   BUF_READ_AHEAD_AREA(buf_pool));

	fail_count = 0;

	rw_lock_s_lock(&buf_pool->page_hash_latch);
	for (i = low; i < high; i++) {
		bpage = buf_page_hash_get(buf_pool, space, i);

		if (bpage == NULL || !buf_page_is_accessed(bpage)) {
			/* Not accessed */
			fail_count++;

		} else if (pred_bpage) {
			/* Note that buf_page_is_accessed() returns
			the time of the first access.  If some blocks
			of the extent existed in the buffer pool at
			the time of a linear access pattern, the first
			access times may be nonmonotonic, even though
			the latest access times were linear.  The
			threshold (srv_read_ahead_factor) should help
			a little against this. */
			int res = ut_ulint_cmp(
				buf_page_is_accessed(bpage),
				buf_page_is_accessed(pred_bpage));
			/* Accesses not in the right order */
			if (res != 0 && res != asc_or_desc) {
				fail_count++;
			}
		}

		if (fail_count > threshold) {
			/* Too many failures: return */
			//buf_pool_mutex_exit(buf_pool);
			rw_lock_s_unlock(&buf_pool->page_hash_latch);
			return(0);
		}

		if (bpage && buf_page_is_accessed(bpage)) {
			pred_bpage = bpage;
		}
	}

	/* If we got this far, we know that enough pages in the area have
	been accessed in the right order: linear read-ahead can be sensible */

	bpage = buf_page_hash_get(buf_pool, space, offset);

	if (bpage == NULL) {
		//buf_pool_mutex_exit(buf_pool);
		rw_lock_s_unlock(&buf_pool->page_hash_latch);

		return(0);
	}

	switch (buf_page_get_state(bpage)) {
	case BUF_BLOCK_ZIP_PAGE:
		frame = bpage->zip.data;
		break;
	case BUF_BLOCK_FILE_PAGE:
		frame = ((buf_block_t*) bpage)->frame;
		break;
	default:
		ut_error;
		break;
	}

	/* Read the natural predecessor and successor page addresses from
	the page; NOTE that because the calling thread may have an x-latch
	on the page, we do not acquire an s-latch on the page, this is to
	prevent deadlocks. Even if we read values which are nonsense, the
	algorithm will work. */

	pred_offset = fil_page_get_prev(frame);
	succ_offset = fil_page_get_next(frame);

	//buf_pool_mutex_exit(buf_pool);
	rw_lock_s_unlock(&buf_pool->page_hash_latch);

	if ((offset == low) && (succ_offset == offset + 1)) {

		/* This is ok, we can continue */
		new_offset = pred_offset;

	} else if ((offset == high - 1) && (pred_offset == offset - 1)) {

		/* This is ok, we can continue */
		new_offset = succ_offset;
	} else {
		/* Successor or predecessor not in the right order */

		return(0);
	}

	low  = (new_offset / buf_read_ahead_linear_area)
		* buf_read_ahead_linear_area;
	high = (new_offset / buf_read_ahead_linear_area + 1)
		* buf_read_ahead_linear_area;

	if ((new_offset != low) && (new_offset != high - 1)) {
		/* This is not a border page of the area: return */

		return(0);
	}

	if (high > fil_space_get_size(space)) {
		/* The area is not whole, return */

		return(0);
	}

	/* If we got this far, read-ahead can be sensible: do it */

	ibuf_mode = inside_ibuf
		? BUF_READ_IBUF_PAGES_ONLY | OS_AIO_SIMULATED_WAKE_LATER
		: BUF_READ_ANY_PAGE | OS_AIO_SIMULATED_WAKE_LATER;

	count = 0;

	/* Since Windows XP seems to schedule the i/o handler thread
	very eagerly, and consequently it does not wait for the
	full read batch to be posted, we use special heuristics here */

	os_aio_simulated_put_read_threads_to_sleep();

	for (i = low; i < high; i++) {
		/* It is only sensible to do read-ahead in the non-sync
		aio mode: hence FALSE as the first parameter */

		if (!ibuf_bitmap_page(zip_size, i)) {
			count += buf_read_page_low(
				&err, FALSE,
				ibuf_mode,
				space, zip_size, FALSE, tablespace_version, i, trx);
			if (err == DB_TABLESPACE_DELETED) {
				ut_print_timestamp(stderr);
				fprintf(stderr,
					"  InnoDB: Warning: in"
					" linear readahead trying to access\n"
					"InnoDB: tablespace %lu page %lu,\n"
					"InnoDB: but the tablespace does not"
					" exist or is just being dropped.\n",
					(ulong) space, (ulong) i);
			}
		}
	}

	/* In simulated aio we wake the aio handler threads only after
	queuing all aio requests, in native aio the following call does
	nothing: */

	os_aio_simulated_wake_handler_threads();

	/* Flush pages from the end of the LRU list if necessary */
	buf_flush_free_margin(buf_pool, TRUE);

#ifdef UNIV_DEBUG
	if (buf_debug_prints && (count > 0)) {
		fprintf(stderr,
			"LINEAR read-ahead space %lu offset %lu pages %lu\n",
			(ulong) space, (ulong) offset, (ulong) count);
	}
#endif /* UNIV_DEBUG */

	/* Read ahead is considered one I/O operation for the purpose of
	LRU policy decision. */
	buf_LRU_stat_inc_io();

	buf_pool->stat.n_ra_pages_read += count;
	return(count);
}

/********************************************************************//**
Issues read requests for pages which the ibuf module wants to read in, in
order to contract the insert buffer tree. Technically, this function is like
a read-ahead function. */
UNIV_INTERN
void
buf_read_ibuf_merge_pages(
/*======================*/
	ibool		sync,		/*!< in: TRUE if the caller
					wants this function to wait
					for the highest address page
					to get read in, before this
					function returns */
	const ulint*	space_ids,	/*!< in: array of space ids */
	const ib_int64_t* space_versions,/*!< in: the spaces must have
					this version number
					(timestamp), otherwise we
					discard the read; we use this
					to cancel reads if DISCARD +
					IMPORT may have changed the
					tablespace size */
	const ulint*	page_nos,	/*!< in: array of page numbers
					to read, with the highest page
					number the last in the
					array */
	ulint		n_stored)	/*!< in: number of elements
					in the arrays */
{
	ulint	i;

#ifdef UNIV_IBUF_DEBUG
	ut_a(n_stored < UNIV_PAGE_SIZE);
#endif

	for (i = 0; i < n_stored; i++) {
		ulint		err;
		buf_pool_t*	buf_pool;
		ulint		zip_size = fil_space_get_zip_size(space_ids[i]);

		buf_pool = buf_pool_get(space_ids[i], page_nos[i]);

		while (buf_pool->n_pend_reads
		       > buf_pool->curr_size / BUF_READ_AHEAD_PEND_LIMIT) {
			os_thread_sleep(500000);
		}

		if (UNIV_UNLIKELY(zip_size == ULINT_UNDEFINED)) {

			goto tablespace_deleted;
		}

		buf_read_page_low(&err, sync && (i + 1 == n_stored),
				  BUF_READ_ANY_PAGE, space_ids[i],
				  zip_size, TRUE, space_versions[i],
				  page_nos[i], NULL);

		if (UNIV_UNLIKELY(err == DB_TABLESPACE_DELETED)) {
tablespace_deleted:
			/* We have deleted or are deleting the single-table
			tablespace: remove the entries for that page */

			ibuf_merge_or_delete_for_page(NULL, space_ids[i],
						      page_nos[i],
						      zip_size, FALSE);
		}
	}

	os_aio_simulated_wake_handler_threads();

	/* Flush pages from the end of all the LRU lists if necessary */
	buf_flush_free_margins(FALSE);

#ifdef UNIV_DEBUG
	if (buf_debug_prints) {
		fprintf(stderr,
			"Ibuf merge read-ahead space %lu pages %lu\n",
			(ulong) space_ids[0], (ulong) n_stored);
	}
#endif /* UNIV_DEBUG */
}

/********************************************************************//**
Issues read requests for pages which recovery wants to read in. */
UNIV_INTERN
void
buf_read_recv_pages(
/*================*/
	ibool		sync,		/*!< in: TRUE if the caller
					wants this function to wait
					for the highest address page
					to get read in, before this
					function returns */
	ulint		space,		/*!< in: space id */
	ulint		zip_size,	/*!< in: compressed page size in
					bytes, or 0 */
	const ulint*	page_nos,	/*!< in: array of page numbers
					to read, with the highest page
					number the last in the
					array */
	ulint		n_stored)	/*!< in: number of page numbers
					in the array */
{
	ib_int64_t	tablespace_version;
	ulint		count;
	ulint		err;
	ulint		i;

	zip_size = fil_space_get_zip_size(space);

	if (UNIV_UNLIKELY(zip_size == ULINT_UNDEFINED)) {
		/* It is a single table tablespace and the .ibd file is
		missing: do nothing */

		/* the log records should be treated here same reason
		for http://bugs.mysql.com/bug.php?id=43948 */

		if (recv_recovery_is_on()) {
			recv_addr_t*    recv_addr;

			mutex_enter(&(recv_sys->mutex));

			if (recv_sys->apply_log_recs == FALSE) {
				mutex_exit(&(recv_sys->mutex));
				goto not_to_recover;
			}

			for (i = 0; i < n_stored; i++) {
				/* recv_get_fil_addr_struct() */
				recv_addr = HASH_GET_FIRST(recv_sys->addr_hash,
						hash_calc_hash(ut_fold_ulint_pair(space, page_nos[i]),
							recv_sys->addr_hash));
				while (recv_addr) {
					if ((recv_addr->space == space)
						&& (recv_addr->page_no == page_nos[i])) {
						break;
					}
					recv_addr = HASH_GET_NEXT(addr_hash, recv_addr);
				}

				if ((recv_addr == NULL)
				    || (recv_addr->state == RECV_BEING_PROCESSED)
				    || (recv_addr->state == RECV_PROCESSED)) {
					continue;
				}

				recv_addr->state = RECV_PROCESSED;

				ut_a(recv_sys->n_addrs);
				recv_sys->n_addrs--;
			}

			mutex_exit(&(recv_sys->mutex));

			fprintf(stderr, " (cannot find space: %lu)", space);
		}
not_to_recover:

		return;
	}

	tablespace_version = fil_space_get_version(space);

	for (i = 0; i < n_stored; i++) {
		buf_pool_t*	buf_pool;

		count = 0;

		os_aio_print_debug = FALSE;
		buf_pool = buf_pool_get(space, page_nos[i]);
		while (buf_pool->n_pend_reads >= recv_n_pool_free_frames / 2) {

			os_aio_simulated_wake_handler_threads();
			os_thread_sleep(10000);

			count++;

			if (count > 1000) {
				fprintf(stderr,
					"InnoDB: Error: InnoDB has waited for"
					" 10 seconds for pending\n"
					"InnoDB: reads to the buffer pool to"
					" be finished.\n"
					"InnoDB: Number of pending reads %lu,"
					" pending pread calls %lu\n",
					(ulong) buf_pool->n_pend_reads,
					(ulong)os_file_n_pending_preads);

				os_aio_print_debug = TRUE;
			}
		}

		os_aio_print_debug = FALSE;

		if ((i + 1 == n_stored) && sync) {
			buf_read_page_low(&err, TRUE, BUF_READ_ANY_PAGE, space,
					  zip_size, TRUE, tablespace_version,
					  page_nos[i], NULL);
		} else {
			buf_read_page_low(&err, FALSE, BUF_READ_ANY_PAGE
					  | OS_AIO_SIMULATED_WAKE_LATER,
					  space, zip_size, TRUE,
					  tablespace_version, page_nos[i], NULL);
		}
	}

	os_aio_simulated_wake_handler_threads();

	/* Flush pages from the end of all the LRU lists if necessary */
	buf_flush_free_margins(FALSE);

#ifdef UNIV_DEBUG
	if (buf_debug_prints) {
		fprintf(stderr,
			"Recovery applies read-ahead pages %lu\n",
			(ulong) n_stored);
	}
#endif /* UNIV_DEBUG */
}
