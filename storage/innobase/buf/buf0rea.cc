/*****************************************************************************

Copyright (c) 1995, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************//**
@file buf/buf0rea.cc
The database buffer read

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#include <mysql/service_thd_wait.h>
#include <stddef.h>

#include "buf0buf.h"
#include "buf0dblwr.h"
#include "buf0flu.h"
#include "buf0lru.h"
#include "buf0rea.h"
#include "fil0fil.h"
#include "ha_prototypes.h"
#include "ibuf0ibuf.h"
#include "log0recv.h"
#include "mtr0mtr.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "os0file.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "trx0sys.h"

/** There must be at least this many pages in buf_pool in the area to start
a random read-ahead */
#define BUF_READ_AHEAD_RANDOM_THRESHOLD(b)	\
				(5 + BUF_READ_AHEAD_AREA(b) / 8)

/** If there are buf_pool->curr_size per the number below pending reads, then
read-ahead is not done: this is to prevent flooding the buffer pool with
i/o-fixed buffer blocks */
#define BUF_READ_AHEAD_PEND_LIMIT	2

/** Low-level function which reads a page asynchronously from a file to the
buffer buf_pool if it is not already there, in which case does nothing.
Sets the io_fix flag and sets an exclusive lock on the buffer frame. The
flag is cleared and the x-lock released by an i/o-handler thread.
@param[out]	err		DB_SUCCESS or DB_TABLESPACE_DELETED
				if we are trying to read from a non-existent
				tablespace or a tablespace which is just now
				being dropped
@param[in]	sync		whether synchronous aio is desired
@param[in]	type		Request type
@param[in]	mode		BUF_READ_IBUF_PAGES_ONLY, ...
@param[in]	page_id		page id
@param[in]	page_size	page size
@param[in]	unzip		true=request uncompressed page
@return 1 if a read request was queued, 0 if the page already resided in
buf_pool, or if the page is in the doublewrite buffer blocks in which case it
is never read into the pool, or if the tablespace does not exist or is being
dropped */
static
ulint
buf_read_page_low(
	dberr_t*		err,
	bool			sync,
	ulint			type,
	ulint			mode,
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	bool			unzip)
{
	buf_page_t*	bpage;

	*err = DB_SUCCESS;

	if (page_id.space() == TRX_SYS_SPACE
	    && buf_dblwr_page_inside(page_id.page_no())) {

		ib::error() << "Trying to read doublewrite buffer page "
			<< page_id;
		return(0);
	}

	if (ibuf_bitmap_page(page_id, page_size) || trx_sys_hdr_page(page_id)) {

		/* Trx sys header is so low in the latching order that we play
		safe and do not leave the i/o-completion to an asynchronous
		i/o-thread. Ibuf bitmap pages must always be read with
		syncronous i/o, to make sure they do not get involved in
		thread deadlocks. */

		sync = true;
	}

	/* The following call will also check if the tablespace does not exist
	or is being dropped; if we succeed in initing the page in the buffer
	pool for read, then DISCARD cannot proceed until the read has
	completed */
	bpage = buf_page_init_for_read(err, mode, page_id, page_size, unzip);

	if (bpage == NULL) {

		return(0);
	}

	DBUG_PRINT("ib_buf", ("read page %u:%u size=%u unzip=%u,%s",
			      (unsigned) page_id.space(),
			      (unsigned) page_id.page_no(),
			      (unsigned) page_size.physical(),
			      (unsigned) unzip,
			      sync ? "sync" : "async"));

	ut_ad(buf_page_in_file(bpage));
	ut_ad(!mutex_own(&buf_pool_from_bpage(bpage)->LRU_list_mutex));

	if (sync) {
		thd_wait_begin(NULL, THD_WAIT_DISKIO);
	}

	void*	dst;

	if (page_size.is_compressed()) {
		dst = bpage->zip.data;
	} else {
		ut_a(buf_page_get_state(bpage) == BUF_BLOCK_FILE_PAGE);

		dst = ((buf_block_t*) bpage)->frame;
	}

	IORequest	request(type | IORequest::READ);

	*err = fil_io(
		request, sync, page_id, page_size, 0, page_size.physical(),
		dst, bpage);

	if (sync) {
		thd_wait_end(NULL);
	}

	if (*err != DB_SUCCESS) {
		if (IORequest::ignore_missing(type)
		    || *err == DB_TABLESPACE_DELETED) {
			buf_read_page_handle_error(bpage);
			return(0);
		}

		ut_error;
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

/** Applies a random read-ahead in buf_pool if there are at least a threshold
value of accessed pages from the random read-ahead area. Does not read any
page, not even the one at the position (space, offset), if the read-ahead
mechanism is not activated. NOTE 1: the calling thread may own latches on
pages: to avoid deadlocks this function must be written such that it cannot
end up waiting for these latches! NOTE 2: the calling thread must want
access to the page given: this rule is set to prevent unintended read-aheads
performed by ibuf routines, a situation which could result in a deadlock if
the OS does not support asynchronous i/o.
@param[in]	page_id		page id of a page which the current thread
wants to access
@param[in]	page_size	page size
@param[in]	inside_ibuf	TRUE if we are inside ibuf routine
@return number of page read requests issued; NOTE that if we read ibuf
pages, it may happen that the page at the given page number does not
get read even if we return a positive value! */
ulint
buf_read_ahead_random(
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	ibool			inside_ibuf)
{
	buf_pool_t*	buf_pool = buf_pool_get(page_id);
	ulint		recent_blocks	= 0;
	ulint		ibuf_mode;
	ulint		count;
	page_no_t	low, high;
	dberr_t		err;
	page_no_t	i;
	const page_no_t	buf_read_ahead_random_area
				= BUF_READ_AHEAD_AREA(buf_pool);

	if (!srv_random_read_ahead) {
		/* Disabled by user */
		return(0);
	}

	if (srv_startup_is_before_trx_rollback_phase) {
		/* No read-ahead to avoid thread deadlocks */
		return(0);
	}

	if (ibuf_bitmap_page(page_id, page_size) || trx_sys_hdr_page(page_id)) {

		/* If it is an ibuf bitmap page or trx sys hdr, we do
		no read-ahead, as that could break the ibuf page access
		order */

		return(0);
	}

	low  = (page_id.page_no() / buf_read_ahead_random_area)
		* buf_read_ahead_random_area;

	high = (page_id.page_no() / buf_read_ahead_random_area + 1)
		* buf_read_ahead_random_area;

	/* Remember the tablespace version before we ask the tablespace size
	below: if DISCARD + IMPORT changes the actual .ibd file meanwhile, we
	do not try to read outside the bounds of the tablespace! */
	if (fil_space_t* space = fil_space_acquire(page_id.space())) {
		if (high > space->size) {
			high = space->size;
		}
		fil_space_release(space);
	} else {
		return(0);
	}

	os_rmb;
	if (buf_pool->n_pend_reads
	    > buf_pool->curr_size / BUF_READ_AHEAD_PEND_LIMIT) {

		return(0);
	}

	/* Count how many blocks in the area have been recently accessed,
	that is, reside near the start of the LRU list. */

	for (i = low; i < high; i++) {

		rw_lock_t*		hash_lock;
		const buf_page_t*	bpage;

		bpage = buf_page_hash_get_s_locked(
			buf_pool, page_id_t(page_id.space(), i), &hash_lock);

		if (bpage != NULL
		    && buf_page_is_accessed(bpage)
		    && buf_page_peek_if_young(bpage)) {

			recent_blocks++;

			if (recent_blocks
			    >= BUF_READ_AHEAD_RANDOM_THRESHOLD(buf_pool)) {

				rw_lock_s_unlock(hash_lock);
				goto read_ahead;
			}
		}

		if (bpage != NULL) {
			rw_lock_s_unlock(hash_lock);
		}
	}

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

		const page_id_t	cur_page_id(page_id.space(), i);

		if (!ibuf_bitmap_page(cur_page_id, page_size)) {

			count += buf_read_page_low(
				&err, false,
				IORequest::DO_NOT_WAKE,
				ibuf_mode,
				cur_page_id, page_size, false);

			if (err == DB_TABLESPACE_DELETED) {
				ib::warn() << "Random readahead trying to"
					" access page " << cur_page_id
					<< " in nonexisting or"
					" being-dropped tablespace";
				break;
			}
		}
	}

	/* In simulated aio we wake the aio handler threads only after
	queuing all aio requests, in native aio the following call does
	nothing: */

	os_aio_simulated_wake_handler_threads();

	if (count) {
		DBUG_PRINT("ib_buf", ("random read-ahead %u pages, %u:%u",
				      (unsigned) count,
				      (unsigned) page_id.space(),
				      (unsigned) page_id.page_no()));
	}

	/* Read ahead is considered one I/O operation for the purpose of
	LRU policy decision. */
	buf_LRU_stat_inc_io();

	buf_pool->stat.n_ra_pages_read_rnd += count;
	srv_stats.buf_pool_reads.add(count);
	return(count);
}

/** High-level function which reads a page asynchronously from a file to the
buffer buf_pool if it is not already there. Sets the io_fix flag and sets
an exclusive lock on the buffer frame. The flag is cleared and the x-lock
released by the i/o-handler thread.
@param[in]	page_id		page id
@param[in]	page_size	page size
@return TRUE if page has been read in, FALSE in case of failure */
ibool
buf_read_page(
	const page_id_t&	page_id,
	const page_size_t&	page_size)
{
	ulint		count;
	dberr_t		err;

	count = buf_read_page_low(
		&err, true,
		0, BUF_READ_ANY_PAGE, page_id, page_size, false);

	srv_stats.buf_pool_reads.add(count);

	if (err == DB_TABLESPACE_DELETED) {
		ib::error() << "trying to read page " << page_id
			<< " in nonexisting or being-dropped tablespace";
	}

	/* Increment number of I/O operations used for LRU policy. */
	buf_LRU_stat_inc_io();

	return(count > 0);
}

/** High-level function which reads a page asynchronously from a file to the
buffer buf_pool if it is not already there. Sets the io_fix flag and sets
an exclusive lock on the buffer frame. The flag is cleared and the x-lock
released by the i/o-handler thread.
@param[in]	page_id		page id
@param[in]	page_size	page size
@param[in]	sync		true if synchronous aio is desired
@return TRUE if page has been read in, FALSE in case of failure */
ibool
buf_read_page_background(
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	bool			sync)
{
	ulint		count;
	dberr_t		err;

	count = buf_read_page_low(
		&err, sync,
		IORequest::DO_NOT_WAKE | IORequest::IGNORE_MISSING,
		BUF_READ_ANY_PAGE,
		page_id, page_size, false);

	srv_stats.buf_pool_reads.add(count);

	/* We do not increment number of I/O operations used for LRU policy
	here (buf_LRU_stat_inc_io()). We use this in heuristics to decide
	about evicting uncompressed version of compressed pages from the
	buffer pool. Since this function is called from buffer pool load
	these IOs are deliberate and are not part of normal workload we can
	ignore these in our heuristics. */

	return(count > 0);
}

/** Applies linear read-ahead if in the buf_pool the page is a border page of
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
@param[in]	page_id		page id; see NOTE 3 above
@param[in]	page_size	page size
@param[in]	inside_ibuf	TRUE if we are inside ibuf routine
@return number of page read requests issued */
ulint
buf_read_ahead_linear(
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	ibool			inside_ibuf)
{
	buf_pool_t*	buf_pool = buf_pool_get(page_id);
	buf_page_t*	bpage;
	buf_frame_t*	frame;
	buf_page_t*	pred_bpage	= NULL;
	unsigned	pred_bpage_is_accessed = 0;
	page_no_t	pred_offset;
	page_no_t	succ_offset;
	int		asc_or_desc;
	page_no_t	new_offset;
	ulint		fail_count;
	page_no_t	low, high;
	dberr_t		err;
	page_no_t	i;
	const page_no_t	buf_read_ahead_linear_area
		= BUF_READ_AHEAD_AREA(buf_pool);
	page_no_t	threshold;

	/* check if readahead is disabled */
	if (!srv_read_ahead_threshold) {
		return(0);
	}

	if (srv_startup_is_before_trx_rollback_phase) {
		/* No read-ahead to avoid thread deadlocks */
		return(0);
	}

	low  = (page_id.page_no() / buf_read_ahead_linear_area)
		* buf_read_ahead_linear_area;
	high = (page_id.page_no() / buf_read_ahead_linear_area + 1)
		* buf_read_ahead_linear_area;

	if ((page_id.page_no() != low) && (page_id.page_no() != high - 1)) {
		/* This is not a border page of the area: return */

		return(0);
	}

	if (ibuf_bitmap_page(page_id, page_size) || trx_sys_hdr_page(page_id)) {

		/* If it is an ibuf bitmap page or trx sys hdr, we do
		no read-ahead, as that could break the ibuf page access
		order */

		return(0);
	}

	/* Remember the tablespace version before we ask te tablespace size
	below: if DISCARD + IMPORT changes the actual .ibd file meanwhile, we
	do not try to read outside the bounds of the tablespace! */
	ulint	space_size;

	if (fil_space_t* space = fil_space_acquire(page_id.space())) {

		space_size = space->size;

		fil_space_release(space);

		if (high > space_size) {
			/* The area is not whole */
			return(0);
		}
	} else {
		return(0);
	}

	/* Read memory barrier */

	os_rmb;

	if (buf_pool->n_pend_reads
	    > buf_pool->curr_size / BUF_READ_AHEAD_PEND_LIMIT) {

		return(0);
	}

	/* Check that almost all pages in the area have been accessed; if
	offset == low, the accesses must be in a descending order, otherwise,
	in an ascending order. */

	asc_or_desc = 1;

	if (page_id.page_no() == low) {
		asc_or_desc = -1;
	}

	/* How many out of order accessed pages can we ignore
	when working out the access pattern for linear readahead */
	threshold = std::min(static_cast<page_no_t>(
				     64 - srv_read_ahead_threshold),
			     BUF_READ_AHEAD_AREA(buf_pool));

	fail_count = 0;

	rw_lock_t*	hash_lock;

	for (i = low; i < high; i++) {

		bpage = buf_page_hash_get_s_locked(buf_pool,
						   page_id_t(page_id.space(),
							     i), &hash_lock);

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
				pred_bpage_is_accessed);
			/* Accesses not in the right order */
			if (res != 0 && res != asc_or_desc) {
				fail_count++;
			}
		}

		if (fail_count > threshold) {
			/* Too many failures: return */
			if (bpage) {
				rw_lock_s_unlock(hash_lock);
			}
			return(0);
		}

		if (bpage) {
			if (buf_page_is_accessed(bpage)) {
				pred_bpage = bpage;
				pred_bpage_is_accessed
					= buf_page_is_accessed(bpage);
			}

			rw_lock_s_unlock(hash_lock);
		}
	}

	/* If we got this far, we know that enough pages in the area have
	been accessed in the right order: linear read-ahead can be sensible */

	bpage = buf_page_hash_get_s_locked(buf_pool, page_id, &hash_lock);

	if (bpage == NULL) {

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

	rw_lock_s_unlock(hash_lock);

	if ((page_id.page_no() == low)
	    && (succ_offset == page_id.page_no() + 1)) {

		/* This is ok, we can continue */
		new_offset = pred_offset;

	} else if ((page_id.page_no() == high - 1)
		   && (pred_offset == page_id.page_no() - 1)) {

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

	if (high > space_size) {
		/* The area is not whole, return */

		return(0);
	}

	ulint	count = 0;

	/* If we got this far, read-ahead can be sensible: do it */

	ulint	ibuf_mode;

	ibuf_mode = inside_ibuf ? BUF_READ_IBUF_PAGES_ONLY : BUF_READ_ANY_PAGE;

	/* Since Windows XP seems to schedule the i/o handler thread
	very eagerly, and consequently it does not wait for the
	full read batch to be posted, we use special heuristics here */

	os_aio_simulated_put_read_threads_to_sleep();

	for (i = low; i < high; i++) {
		/* It is only sensible to do read-ahead in the non-sync
		aio mode: hence FALSE as the first parameter */

		const page_id_t	cur_page_id(page_id.space(), i);

		if (!ibuf_bitmap_page(cur_page_id, page_size)) {

			count += buf_read_page_low(
				&err, false,
				IORequest::DO_NOT_WAKE,
				ibuf_mode, cur_page_id, page_size, false);

			if (err == DB_TABLESPACE_DELETED) {
				ib::warn() << "linear readahead trying to"
					" access page "
					<< page_id_t(page_id.space(), i)
					<< " in nonexisting or being-dropped"
					" tablespace";
			}
		}
	}

	/* In simulated aio we wake the aio handler threads only after
	queuing all aio requests, in native aio the following call does
	nothing: */

	os_aio_simulated_wake_handler_threads();

	if (count) {
		DBUG_PRINT("ib_buf", ("linear read-ahead %lu pages, "
				      UINT32PF ":" UINT32PF,
				      count,
				      page_id.space(),
				      page_id.page_no()));
	}

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
void
buf_read_ibuf_merge_pages(
/*======================*/
	bool		sync,		/*!< in: true if the caller
					wants this function to wait
					for the highest address page
					to get read in, before this
					function returns */
	const space_id_t*	space_ids,	/*!< in: array of space ids */
	const page_no_t*	page_nos,	/*!< in: array of page numbers
					to read, with the highest page
					number the last in the
					array */
	ulint		n_stored)	/*!< in: number of elements
					in the arrays */
{
#ifdef UNIV_IBUF_DEBUG
	ut_a(n_stored < UNIV_PAGE_SIZE);
#endif

	for (ulint i = 0; i < n_stored; i++) {
		const page_id_t	page_id(space_ids[i], page_nos[i]);

		buf_pool_t*	buf_pool = buf_pool_get(page_id);

		bool			found;
		const page_size_t	page_size(fil_space_get_page_size(
			space_ids[i], &found));

		if (!found) {
			/* The tablespace was not found, remove the
			entries for that page */
			ibuf_merge_or_delete_for_page(NULL, page_id,
						      NULL, FALSE);
			continue;
		}

		os_rmb;
		while (buf_pool->n_pend_reads
		       > buf_pool->curr_size / BUF_READ_AHEAD_PEND_LIMIT) {
			os_thread_sleep(500000);
		}

		dberr_t	err;

		buf_read_page_low(&err,
				  sync && (i + 1 == n_stored),
				  IORequest::IGNORE_MISSING,
				  BUF_READ_ANY_PAGE, page_id, page_size,
				  true);

		if (err == DB_TABLESPACE_DELETED) {
			/* We have deleted or are deleting the single-table
			tablespace: remove the entries for that page */
			ibuf_merge_or_delete_for_page(NULL, page_id,
						      &page_size, FALSE);
		}
	}

	os_aio_simulated_wake_handler_threads();

	if (n_stored) {
		DBUG_PRINT("ib_buf",
			   ("ibuf merge read-ahead %u pages, space %u",
			    unsigned(n_stored), unsigned(space_ids[0])));
	}
}

/** Issues read requests for pages which recovery wants to read in.
@param[in]	sync		true if the caller wants this function to wait
				for the highest address page to get read in,
				before this function returns
@param[in]	space_id	tablespace id
@param[in]	page_nos	array of page numbers to read, with the
				highest page number the last in the array
@param[in]	n_stored	number of page numbers in the array */
void
buf_read_recv_pages(
	bool			sync,
	space_id_t		space_id,
	const page_no_t*	page_nos,
	ulint			n_stored)
{
	ulint			count;
	fil_space_t*		space	= fil_space_get(space_id);

	if (space == NULL) {
		/* The tablespace is missing: do nothing */
		return;
	}

	fil_space_open_if_needed(space);

	auto	req_size = page_nos[n_stored - 1] + 1;

	/* Extend the tablespace if needed. Required only while
	recovering from cloned database. */
	if (recv_sys->is_cloned_db && space->size < req_size) {

		/* Align size to multiple of extent size */
		if (req_size > FSP_EXTENT_SIZE) {

			req_size = ut_calc_align(req_size, FSP_EXTENT_SIZE);
		}

		ib::info()
			<< "Extending tablespace : " << space->id
			<< " space name: " << space->name
			<< " from page number: " << space->size << " pages"
			<< " to " << req_size << " pages"
			<< " for page number: " << page_nos[n_stored - 1]
			<< " during recovery.";

		if(!fil_space_extend(space, req_size)) {

			ib::error()
				<< "Could not extend tablespace: " << space->id
				<< " space name: " << space->name
				<< " to " << req_size << " pages"
				<< " during recovery.";
		}
	}

	const page_size_t	page_size(space->flags);

	for (ulint i = 0; i < n_stored; i++) {
		buf_pool_t*	buf_pool;
		const page_id_t	cur_page_id(space_id, page_nos[i]);

		count = 0;

		buf_pool = buf_pool_get(cur_page_id);
		os_rmb;

		while (buf_pool->n_pend_reads >= recv_n_pool_free_frames / 2) {

			os_aio_simulated_wake_handler_threads();
			os_thread_sleep(10000);

			count++;

			if (!(count % 1000)) {

				ib::error()
					<< "Waited for " << count / 100
					<< " seconds for "
					<< buf_pool->n_pend_reads
					<< " pending reads";
			}
		}

		dberr_t		err;

		if ((i + 1 == n_stored) && sync) {
			buf_read_page_low(
				&err, true,
				0,
				BUF_READ_ANY_PAGE,
				cur_page_id, page_size, true);
		} else {
			buf_read_page_low(
				&err, false,
				IORequest::DO_NOT_WAKE,
				BUF_READ_ANY_PAGE,
				cur_page_id, page_size, true);
		}
	}

	os_aio_simulated_wake_handler_threads();

	DBUG_PRINT("ib_buf", ("recovery read-ahead (%u pages)",
			      unsigned(n_stored)));
}
