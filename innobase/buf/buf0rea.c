/******************************************************
The database buffer read

(c) 1995 Innobase Oy

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

/* The size in blocks of the area where the random read-ahead algorithm counts
the accessed pages when deciding whether to read-ahead */
#define	BUF_READ_AHEAD_RANDOM_AREA	BUF_READ_AHEAD_AREA

/* There must be at least this many pages in buf_pool in the area to start
a random read-ahead */
#define BUF_READ_AHEAD_RANDOM_THRESHOLD	(5 + BUF_READ_AHEAD_RANDOM_AREA / 8)

/* The linear read-ahead area size */
#define	BUF_READ_AHEAD_LINEAR_AREA	BUF_READ_AHEAD_AREA

/* The linear read-ahead threshold */
#define BUF_READ_AHEAD_LINEAR_THRESHOLD	(3 * BUF_READ_AHEAD_LINEAR_AREA / 8)

/* If there are buf_pool->curr_size per the number below pending reads, then
read-ahead is not done: this is to prevent flooding the buffer pool with
i/o-fixed buffer blocks */
#define BUF_READ_AHEAD_PEND_LIMIT	2

/************************************************************************
Low-level function which reads a page asynchronously from a file to the
buffer buf_pool if it is not already there, in which case does nothing.
Sets the io_fix flag and sets an exclusive lock on the buffer frame. The
flag is cleared and the x-lock released by an i/o-handler thread. */
static
ulint
buf_read_page_low(
/*==============*/
			/* out: 1 if a read request was queued, 0 if the page
			already resided in buf_pool or if the page is in
			the doublewrite buffer blocks in which case it is never
			read into the pool */
	ibool	sync,	/* in: TRUE if synchronous aio is desired */
	ulint	mode,	/* in: BUF_READ_IBUF_PAGES_ONLY, ...,
			ORed to OS_AIO_SIMULATED_WAKE_LATER (see below
			at read-ahead functions) */
	ulint	space,	/* in: space id */
	ulint	offset)	/* in: page number */
{
	buf_block_t*	block;
	ulint		wake_later;

	wake_later = mode & OS_AIO_SIMULATED_WAKE_LATER;
	mode = mode & ~OS_AIO_SIMULATED_WAKE_LATER;
	
	if (trx_doublewrite && space == TRX_SYS_SPACE
		&& (   (offset >= trx_doublewrite->block1
		        && offset < trx_doublewrite->block1
		     		+ TRX_SYS_DOUBLEWRITE_BLOCK_SIZE)
		    || (offset >= trx_doublewrite->block2
		        && offset < trx_doublewrite->block2
		     		+ TRX_SYS_DOUBLEWRITE_BLOCK_SIZE))) {
		return(0);
	}

#ifdef UNIV_LOG_DEBUG
	if (space % 2 == 1) {
		/* We are updating a replicate space while holding the
		log mutex: the read must be handled before other reads
		which might incur ibuf operations and thus write to the log */

		printf("Log debug: reading replicate page in sync mode\n");

		sync = TRUE;
	}
#endif
	if (ibuf_bitmap_page(offset) || trx_sys_hdr_page(space, offset)) {

		/* Trx sys header is so low in the latching order that we play
		safe and do not leave the i/o-completion to an asynchronous
		i/o-thread. Ibuf bitmap pages must always be read with
                syncronous i/o, to make sure they do not get involved in
                thread deadlocks. */
		
		sync = TRUE;
	}

	block = buf_page_init_for_read(mode, space, offset);

	if (block != NULL) {
		fil_io(OS_FILE_READ | wake_later,
			sync, space, offset, 0, UNIV_PAGE_SIZE,
					(void*)block->frame, (void*)block);
		if (sync) {
			/* The i/o is already completed when we arrive from
			fil_read */
			buf_page_io_complete(block);
		}
		
		return(1);
	}

	return(0);
}	

/************************************************************************
Applies a random read-ahead in buf_pool if there are at least a threshold
value of accessed pages from the random read-ahead area. Does not read any
page, not even the one at the position (space, offset), if the read-ahead
mechanism is not activated. NOTE 1: the calling thread may own latches on
pages: to avoid deadlocks this function must be written such that it cannot
end up waiting for these latches! NOTE 2: the calling thread must want
access to the page given: this rule is set to prevent unintended read-aheads
performed by ibuf routines, a situation which could result in a deadlock if
the OS does not support asynchronous i/o. */
static
ulint
buf_read_ahead_random(
/*==================*/
			/* out: number of page read requests issued; NOTE
			that if we read ibuf pages, it may happen that
			the page at the given page number does not get
			read even if we return a value > 0! */
	ulint	space,	/* in: space id */
	ulint	offset)	/* in: page number of a page which the current thread
			wants to access */
{
	buf_block_t*	block;
	ulint		recent_blocks	= 0;
	ulint		count;
	ulint		LRU_recent_limit;
	ulint		ibuf_mode;
	ulint		low, high;
	ulint		i;

	if (srv_startup_is_before_trx_rollback_phase) {
	        /* No read-ahead to avoid thread deadlocks */
	        return(0);
	}

	if (ibuf_bitmap_page(offset) || trx_sys_hdr_page(space, offset)) {

		/* If it is an ibuf bitmap page or trx sys hdr, we do
                no read-ahead, as that could break the ibuf page access
                order */

		return(0);
	}

	low  = (offset / BUF_READ_AHEAD_RANDOM_AREA)
					* BUF_READ_AHEAD_RANDOM_AREA;
	high = (offset / BUF_READ_AHEAD_RANDOM_AREA + 1)
					* BUF_READ_AHEAD_RANDOM_AREA;

	if (high > fil_space_get_size(space)) {

		high = fil_space_get_size(space);
	}

	/* Get the minimum LRU_position field value for an initial segment
	of the LRU list, to determine which blocks have recently been added
	to the start of the list. */
	
	LRU_recent_limit = buf_LRU_get_recent_limit();

	mutex_enter(&(buf_pool->mutex));

	if (buf_pool->n_pend_reads >
			buf_pool->curr_size / BUF_READ_AHEAD_PEND_LIMIT) {
		mutex_exit(&(buf_pool->mutex));

		return(0);
	}	

	/* Count how many blocks in the area have been recently accessed,
	that is, reside near the start of the LRU list. */

	for (i = low; i < high; i++) {

		block = buf_page_hash_get(space, i);

		if ((block)
		    && (block->LRU_position > LRU_recent_limit)
		    && block->accessed) {

			recent_blocks++;
		}
	}

	mutex_exit(&(buf_pool->mutex));
	
	if (recent_blocks < BUF_READ_AHEAD_RANDOM_THRESHOLD) {
		/* Do nothing */

		return(0);
	}

	/* Read all the suitable blocks within the area */

	if (ibuf_inside()) {
		ibuf_mode = BUF_READ_IBUF_PAGES_ONLY;
	} else {
		ibuf_mode = BUF_READ_ANY_PAGE;
	}

	count = 0;

	for (i = low; i < high; i++) {
		/* It is only sensible to do read-ahead in the non-sync aio
		mode: hence FALSE as the first parameter */

		if (!ibuf_bitmap_page(i)) {
			
			count += buf_read_page_low(FALSE, ibuf_mode
					| OS_AIO_SIMULATED_WAKE_LATER,
								space, i);
		}
	}

	/* In simulated aio we wake the aio handler threads only after
	queuing all aio requests, in native aio the following call does
	nothing: */
	
	os_aio_simulated_wake_handler_threads();

	if (buf_debug_prints && (count > 0)) {
	
		printf("Random read-ahead space %lu offset %lu pages %lu\n",
						space, offset, count);
	}

	return(count);
}

/************************************************************************
High-level function which reads a page asynchronously from a file to the
buffer buf_pool if it is not already there. Sets the io_fix flag and sets
an exclusive lock on the buffer frame. The flag is cleared and the x-lock
released by the i/o-handler thread. Does a random read-ahead if it seems
sensible. */

ulint
buf_read_page(
/*==========*/
			/* out: number of page read requests issued: this can
			be > 1 if read-ahead occurred */
	ulint	space,	/* in: space id */
	ulint	offset)	/* in: page number */
{
	ulint	count;
	ulint	count2;

	count = buf_read_ahead_random(space, offset);

	/* We do the i/o in the synchronous aio mode to save thread
	switches: hence TRUE */

	count2 = buf_read_page_low(TRUE, BUF_READ_ANY_PAGE, space, offset);

	/* Flush pages from the end of the LRU list if necessary */
	buf_flush_free_margin();

	return(count + count2);
}

/************************************************************************
Applies linear read-ahead if in the buf_pool the page is a border page of
a linear read-ahead area and all the pages in the area have been accessed.
Does not read any page if the read-ahead mechanism is not activated. Note
that the the algorithm looks at the 'natural' adjacent successor and
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
which could result in a deadlock if the OS does not support asynchronous io. */

ulint
buf_read_ahead_linear(
/*==================*/
			/* out: number of page read requests issued */
	ulint	space,	/* in: space id */
	ulint	offset)	/* in: page number of a page; NOTE: the current thread
			must want access to this page (see NOTE 3 above) */
{
	buf_block_t*	block;
	buf_frame_t*	frame;
	buf_block_t*	pred_block	= NULL;
	ulint		pred_offset;
	ulint		succ_offset;
	ulint		count;
	int		asc_or_desc;
	ulint		new_offset;
	ulint		fail_count;
	ulint		ibuf_mode;
	ulint		low, high;
	ulint		i;
	
	if (srv_startup_is_before_trx_rollback_phase) {
	        /* No read-ahead to avoid thread deadlocks */
	        return(0);
	}

	if (ibuf_bitmap_page(offset) || trx_sys_hdr_page(space, offset)) {

		/* If it is an ibuf bitmap page or trx sys hdr, we do
                no read-ahead, as that could break the ibuf page access
                order */

		return(0);
	}

	low  = (offset / BUF_READ_AHEAD_LINEAR_AREA)
					* BUF_READ_AHEAD_LINEAR_AREA;
	high = (offset / BUF_READ_AHEAD_LINEAR_AREA + 1)
					* BUF_READ_AHEAD_LINEAR_AREA;

	if ((offset != low) && (offset != high - 1)) {
		/* This is not a border page of the area: return */

		return(0);
	}

	if (high > fil_space_get_size(space)) {
		/* The area is not whole, return */

		return(0);
	}

	mutex_enter(&(buf_pool->mutex));

	if (buf_pool->n_pend_reads >
			buf_pool->curr_size / BUF_READ_AHEAD_PEND_LIMIT) {
		mutex_exit(&(buf_pool->mutex));

		return(0);
	}	

	/* Check that almost all pages in the area have been accessed; if
	offset == low, the accesses must be in a descending order, otherwise,
	in an ascending order. */

	asc_or_desc = 1;

	if (offset == low) {
		asc_or_desc = -1;
	}

	fail_count = 0;

	for (i = low; i < high; i++) {

		block = buf_page_hash_get(space, i);
		
		if ((block == NULL) || !block->accessed) {

			/* Not accessed */
			fail_count++;

		} else if (pred_block && (ut_ulint_cmp(block->LRU_position,
				      		    pred_block->LRU_position)
			       		  != asc_or_desc)) {

			/* Accesses not in the right order */

			fail_count++;
			pred_block = block;
		}
	}

	if (fail_count > BUF_READ_AHEAD_LINEAR_AREA -
			 BUF_READ_AHEAD_LINEAR_THRESHOLD) {
		/* Too many failures: return */

		mutex_exit(&(buf_pool->mutex));

		return(0);
	}

	/* If we got this far, we know that enough pages in the area have
	been accessed in the right order: linear read-ahead can be sensible */

	block = buf_page_hash_get(space, offset);

	if (block == NULL) {
		mutex_exit(&(buf_pool->mutex));

		return(0);
	}

	frame = block->frame;
	
	/* Read the natural predecessor and successor page addresses from
	the page; NOTE that because the calling thread may have an x-latch
	on the page, we do not acquire an s-latch on the page, this is to
	prevent deadlocks. Even if we read values which are nonsense, the
	algorithm will work. */ 

	pred_offset = fil_page_get_prev(frame);
	succ_offset = fil_page_get_next(frame);

	mutex_exit(&(buf_pool->mutex));
	
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

	low  = (new_offset / BUF_READ_AHEAD_LINEAR_AREA)
					* BUF_READ_AHEAD_LINEAR_AREA;
	high = (new_offset / BUF_READ_AHEAD_LINEAR_AREA + 1)
					* BUF_READ_AHEAD_LINEAR_AREA;

	if ((new_offset != low) && (new_offset != high - 1)) {
		/* This is not a border page of the area: return */

		return(0);
	}

	if (high > fil_space_get_size(space)) {
		/* The area is not whole, return */

		return(0);
	}

	/* If we got this far, read-ahead can be sensible: do it */	    	

	if (ibuf_inside()) {
		ibuf_mode = BUF_READ_IBUF_PAGES_ONLY;
	} else {
		ibuf_mode = BUF_READ_ANY_PAGE;
	}

	count = 0;

	for (i = low; i < high; i++) {
		/* It is only sensible to do read-ahead in the non-sync
		aio mode: hence FALSE as the first parameter */

		if (!ibuf_bitmap_page(i)) {
			count += buf_read_page_low(FALSE, ibuf_mode
					| OS_AIO_SIMULATED_WAKE_LATER,
					space, i);
		}
	}

	/* In simulated aio we wake the aio handler threads only after
	queuing all aio requests, in native aio the following call does
	nothing: */
	
	os_aio_simulated_wake_handler_threads();

	/* Flush pages from the end of the LRU list if necessary */
	buf_flush_free_margin();

	if (buf_debug_prints && (count > 0)) {
		printf(
		"LINEAR read-ahead space %lu offset %lu pages %lu\n",
		space, offset, count);
	}

	return(count);
}

/************************************************************************
Issues read requests for pages which the ibuf module wants to read in, in
order to contract insert buffer trees. Technically, this function is like
a read-ahead function. */

void
buf_read_ibuf_merge_pages(
/*======================*/
	ibool	sync,		/* in: TRUE if the caller wants this function
				to wait for the highest address page to get
				read in, before this function returns */
	ulint	space,		/* in: space id */
	ulint*	page_nos,	/* in: array of page numbers to read, with the
				highest page number the last in the array */
	ulint	n_stored)	/* in: number of page numbers in the array */
{
	ulint	i;

	ut_ad(!ibuf_inside());
#ifdef UNIV_IBUF_DEBUG
	ut_a(n_stored < UNIV_PAGE_SIZE);
#endif	
	while (buf_pool->n_pend_reads >
			buf_pool->curr_size / BUF_READ_AHEAD_PEND_LIMIT) {
		os_thread_sleep(500000);
	}	

	for (i = 0; i < n_stored; i++) {
		if ((i + 1 == n_stored) && sync) {
			buf_read_page_low(TRUE, BUF_READ_ANY_PAGE, space,
								page_nos[i]);
		} else {
			buf_read_page_low(FALSE, BUF_READ_ANY_PAGE, space,
								page_nos[i]);
		}
	}
	
	/* Flush pages from the end of the LRU list if necessary */
	buf_flush_free_margin();

	if (buf_debug_prints) {
		printf("Ibuf merge read-ahead space %lu pages %lu\n",
							space, n_stored);
	}
}

/************************************************************************
Issues read requests for pages which recovery wants to read in. */

void
buf_read_recv_pages(
/*================*/
	ibool	sync,		/* in: TRUE if the caller wants this function
				to wait for the highest address page to get
				read in, before this function returns */
	ulint	space,		/* in: space id */
	ulint*	page_nos,	/* in: array of page numbers to read, with the
				highest page number the last in the array */
	ulint	n_stored)	/* in: number of page numbers in the array */
{
	ulint	i;

	for (i = 0; i < n_stored; i++) {

		while (buf_pool->n_pend_reads >= RECV_POOL_N_FREE_BLOCKS / 2) {

			os_aio_simulated_wake_handler_threads();
			os_thread_sleep(500000);
		}

		if ((i + 1 == n_stored) && sync) {
			buf_read_page_low(TRUE, BUF_READ_ANY_PAGE, space,
								page_nos[i]);
		} else {
			buf_read_page_low(FALSE, BUF_READ_ANY_PAGE
					| OS_AIO_SIMULATED_WAKE_LATER,
					space, page_nos[i]);
		}
	}
	
	os_aio_simulated_wake_handler_threads();

	/* Flush pages from the end of the LRU list if necessary */
	buf_flush_free_margin();

	if (buf_debug_prints) {
		printf("Recovery applies read-ahead pages %lu\n", n_stored);
	}
}
