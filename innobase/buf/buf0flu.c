/******************************************************
The database buffer buf_pool flush algorithm

(c) 1995-2001 Innobase Oy

Created 11/11/1995 Heikki Tuuri
*******************************************************/

#include "buf0flu.h"

#ifdef UNIV_NONINL
#include "buf0flu.ic"
#endif

#include "ut0byte.h"
#include "ut0lst.h"
#include "fil0fil.h"
#include "buf0buf.h"
#include "buf0lru.h"
#include "buf0rea.h"
#include "ibuf0ibuf.h"
#include "log0log.h"
#include "os0file.h"

/* When flushed, dirty blocks are searched in neigborhoods of this size, and
flushed along with the original page. */

#define BUF_FLUSH_AREA		ut_min(BUF_READ_AHEAD_AREA,\
						buf_pool->curr_size / 16)

/**********************************************************************
Validates the flush list. */
static
ibool
buf_flush_validate_low(void);
/*========================*/
		/* out: TRUE if ok */

/************************************************************************
Inserts a modified block into the flush list. */

void
buf_flush_insert_into_flush_list(
/*=============================*/
	buf_block_t*	block)	/* in: block which is modified */
{
	ut_ad(mutex_own(&(buf_pool->mutex)));

	ut_ad((UT_LIST_GET_FIRST(buf_pool->flush_list) == NULL)
	      || (ut_dulint_cmp(
			(UT_LIST_GET_FIRST(buf_pool->flush_list))
						->oldest_modification,
			block->oldest_modification) <= 0));

	UT_LIST_ADD_FIRST(flush_list, buf_pool->flush_list, block);

	ut_ad(buf_flush_validate_low());
}

/************************************************************************
Inserts a modified block into the flush list in the right sorted position.
This function is used by recovery, because there the modifications do not
necessarily come in the order of lsn's. */

void
buf_flush_insert_sorted_into_flush_list(
/*====================================*/
	buf_block_t*	block)	/* in: block which is modified */
{
	buf_block_t*	prev_b;
	buf_block_t*	b;
	
	ut_ad(mutex_own(&(buf_pool->mutex)));

	prev_b = NULL;
	b = UT_LIST_GET_FIRST(buf_pool->flush_list);

	while (b && (ut_dulint_cmp(b->oldest_modification,
					block->oldest_modification) > 0)) {
		prev_b = b;
		b = UT_LIST_GET_NEXT(flush_list, b);
	}

	if (prev_b == NULL) {
		UT_LIST_ADD_FIRST(flush_list, buf_pool->flush_list, block);
	} else {
		UT_LIST_INSERT_AFTER(flush_list, buf_pool->flush_list, prev_b,
								block);
	}

	ut_ad(buf_flush_validate_low());
}

/************************************************************************
Returns TRUE if the file page block is immediately suitable for replacement,
i.e., the transition FILE_PAGE => NOT_USED allowed. */

ibool
buf_flush_ready_for_replace(
/*========================*/
				/* out: TRUE if can replace immediately */
	buf_block_t*	block)	/* in: buffer control block, must be in state
				BUF_BLOCK_FILE_PAGE and in the LRU list*/
{
	ut_ad(mutex_own(&(buf_pool->mutex)));
	ut_ad(block->state == BUF_BLOCK_FILE_PAGE);

	if ((ut_dulint_cmp(block->oldest_modification, ut_dulint_zero) > 0)
	    || (block->buf_fix_count != 0)
	    || (block->io_fix != 0)) {

		return(FALSE);
	}
	
	return(TRUE);
}

/************************************************************************
Returns TRUE if the block is modified and ready for flushing. */
UNIV_INLINE
ibool
buf_flush_ready_for_flush(
/*======================*/
				/* out: TRUE if can flush immediately */
	buf_block_t*	block,	/* in: buffer control block, must be in state
				BUF_BLOCK_FILE_PAGE */
	ulint		flush_type)/* in: BUF_FLUSH_LRU or BUF_FLUSH_LIST */
{
	ut_ad(mutex_own(&(buf_pool->mutex)));
	ut_ad(block->state == BUF_BLOCK_FILE_PAGE);

	if ((ut_dulint_cmp(block->oldest_modification, ut_dulint_zero) > 0)
	    					&& (block->io_fix == 0)) {

	    	if (flush_type != BUF_FLUSH_LRU) {

			return(TRUE);

		} else if ((block->old || (UT_LIST_GET_LEN(buf_pool->LRU)
							< BUF_LRU_OLD_MIN_LEN))
			   && (block->buf_fix_count == 0)) {
 
			/* If we are flushing the LRU list, to avoid deadlocks
			we require the block not to be bufferfixed, and hence
			not latched. Since LRU flushed blocks are soon moved
			to the free list, it is good to flush only old blocks
			from the end of the LRU list. */

			return(TRUE);
		}
	}
	
	return(FALSE);
}

/************************************************************************
Updates the flush system data structures when a write is completed. */

void
buf_flush_write_complete(
/*=====================*/
	buf_block_t*	block)	/* in: pointer to the block in question */
{
	ut_ad(block);
	ut_ad(mutex_own(&(buf_pool->mutex)));

	block->oldest_modification = ut_dulint_zero;

	UT_LIST_REMOVE(flush_list, buf_pool->flush_list, block);

	ut_d(UT_LIST_VALIDATE(flush_list, buf_block_t, buf_pool->flush_list));

	(buf_pool->n_flush[block->flush_type])--;

	if (block->flush_type == BUF_FLUSH_LRU) {
		/* Put the block to the end of the LRU list to wait to be
		moved to the free list */

		buf_LRU_make_block_old(block);

		buf_pool->LRU_flush_ended++;
	}

	/* 	printf("n pending flush %lu\n",
		buf_pool->n_flush[block->flush_type]); */

	if ((buf_pool->n_flush[block->flush_type] == 0)
	    && (buf_pool->init_flush[block->flush_type] == FALSE)) {

		/* The running flush batch has ended */

		os_event_set(buf_pool->no_flush[block->flush_type]);
	}
}

/************************************************************************
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
	buf_block_t*	block;
	ulint		len;
	ulint		i;

	if (trx_doublewrite == NULL) {
		os_aio_simulated_wake_handler_threads();

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

	if (trx_doublewrite->first_free > TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) {
		len = TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * UNIV_PAGE_SIZE;
	} else {
		len = trx_doublewrite->first_free * UNIV_PAGE_SIZE;
	}
	
	fil_io(OS_FILE_WRITE,
		TRUE, TRX_SYS_SPACE,
		trx_doublewrite->block1, 0, len,
		 	(void*)trx_doublewrite->write_buf, NULL);
	
	if (trx_doublewrite->first_free > TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) {
		len = (trx_doublewrite->first_free
			- TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) * UNIV_PAGE_SIZE;
	
		fil_io(OS_FILE_WRITE,
			TRUE, TRX_SYS_SPACE,
			trx_doublewrite->block2, 0, len,
		 	(void*)(trx_doublewrite->write_buf
		 	+ TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * UNIV_PAGE_SIZE),
			NULL);
	}

	/* Now flush the doublewrite buffer data to disk */

	fil_flush(TRX_SYS_SPACE);

	/* We know that the writes have been flushed to disk now
	and in recovery we will find them in the doublewrite buffer
	blocks. Next do the writes to the intended positions. */

	for (i = 0; i < trx_doublewrite->first_free; i++) {
		block = trx_doublewrite->buf_block_arr[i];

		fil_io(OS_FILE_WRITE | OS_AIO_SIMULATED_WAKE_LATER,
			FALSE, block->space, block->offset, 0, UNIV_PAGE_SIZE,
		 			(void*)block->frame, (void*)block);
	}
	
	/* Wake possible simulated aio thread to actually post the
	writes to the operating system */

	os_aio_simulated_wake_handler_threads();

	/* Wait that all async writes to tablespaces have been posted to
	the OS */	
	
	os_aio_wait_until_no_pending_writes();

	/* Now we flush the data to disk (for example, with fsync) */

	fil_flush_file_spaces(FIL_TABLESPACE);

	/* We can now reuse the doublewrite memory buffer: */

	trx_doublewrite->first_free = 0;

	mutex_exit(&(trx_doublewrite->mutex));	
}

/************************************************************************
Posts a buffer page for writing. If the doublewrite memory buffer is
full, calls buf_flush_buffered_writes and waits for for free space to
appear. */
static
void
buf_flush_post_to_doublewrite_buf(
/*==============================*/
	buf_block_t*	block)	/* in: buffer block to write */
{
try_again:
	mutex_enter(&(trx_doublewrite->mutex));

	if (trx_doublewrite->first_free
				>= 2 * TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) {
		mutex_exit(&(trx_doublewrite->mutex));

		buf_flush_buffered_writes();

		goto try_again;
	}

	ut_memcpy(trx_doublewrite->write_buf
				+ UNIV_PAGE_SIZE * trx_doublewrite->first_free,
			block->frame, UNIV_PAGE_SIZE);

	trx_doublewrite->buf_block_arr[trx_doublewrite->first_free] = block;

	trx_doublewrite->first_free++;

	if (trx_doublewrite->first_free
				>= 2 * TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) {
		mutex_exit(&(trx_doublewrite->mutex));

		buf_flush_buffered_writes();

		return;
	}

	mutex_exit(&(trx_doublewrite->mutex));
}

/************************************************************************
Does an asynchronous write of a buffer page. NOTE: in simulated aio and
also when the doublewrite buffer is used, we must call
buf_flush_buffered_writes after we have posted a batch of writes! */
static
void
buf_flush_write_block_low(
/*======================*/
	buf_block_t*	block)	/* in: buffer block to write */
{
#ifdef UNIV_IBUF_DEBUG
	ut_a(ibuf_count_get(block->space, block->offset) == 0);
#endif
	ut_ad(!ut_dulint_is_zero(block->newest_modification));

#ifdef UNIV_LOG_DEBUG
	printf(
	"Warning: cannot force log to disk in the log debug version!\n");
#else
	/* Force the log to the disk before writing the modified block */
	log_flush_up_to(block->newest_modification, LOG_WAIT_ALL_GROUPS);
#endif	
	/* Write the newest modification lsn to the page */
	mach_write_to_8(block->frame + FIL_PAGE_LSN,
						block->newest_modification);
	mach_write_to_8(block->frame + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN,
						block->newest_modification);

	/* Write to the page the space id and page number */

	mach_write_to_4(block->frame + FIL_PAGE_SPACE, block->space);
	mach_write_to_4(block->frame + FIL_PAGE_OFFSET, block->offset);

	/* We overwrite the first 4 bytes of the end lsn field to store
	a page checksum */

	mach_write_to_4(block->frame + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN,
			buf_calc_page_checksum(block->frame));

	if (!trx_doublewrite) {
		fil_io(OS_FILE_WRITE | OS_AIO_SIMULATED_WAKE_LATER,
			FALSE, block->space, block->offset, 0, UNIV_PAGE_SIZE,
		 			(void*)block->frame, (void*)block);
	} else {
		buf_flush_post_to_doublewrite_buf(block);
	}
}

/************************************************************************
Writes a page asynchronously from the buffer buf_pool to a file, if it can be
found in the buf_pool and it is in a flushable state. NOTE: in simulated aio
we must call os_aio_simulated_wake_handler_threads after we have posted a batch
of writes! */
static
ulint
buf_flush_try_page(
/*===============*/
				/* out: 1 if a page was flushed, 0 otherwise */
	ulint	space,		/* in: space id */
	ulint	offset,		/* in: page offset */
	ulint	flush_type)	/* in: BUF_FLUSH_LRU, BUF_FLUSH_LIST, or
				BUF_FLUSH_SINGLE_PAGE */
{
	buf_block_t*	block;
	ibool		locked;
	
	ut_ad(flush_type == BUF_FLUSH_LRU || flush_type == BUF_FLUSH_LIST
				|| flush_type == BUF_FLUSH_SINGLE_PAGE);

	mutex_enter(&(buf_pool->mutex));

	block = buf_page_hash_get(space, offset);

	if (flush_type == BUF_FLUSH_LIST
	    && block && buf_flush_ready_for_flush(block, flush_type)) {
	
		block->io_fix = BUF_IO_WRITE;
		block->flush_type = flush_type;

		if (buf_pool->n_flush[block->flush_type] == 0) {

			os_event_reset(buf_pool->no_flush[block->flush_type]);
		}

		(buf_pool->n_flush[flush_type])++;

		locked = FALSE;
		
		/* If the simulated aio thread is not running, we must
		not wait for any latch, as we may end up in a deadlock:
		if buf_fix_count == 0, then we know we need not wait */

		if (block->buf_fix_count == 0) {
			rw_lock_s_lock_gen(&(block->lock), BUF_IO_WRITE);

			locked = TRUE;
		}

		mutex_exit(&(buf_pool->mutex));

		if (!locked) {
			buf_flush_buffered_writes();

			rw_lock_s_lock_gen(&(block->lock), BUF_IO_WRITE);
		}

		if (buf_debug_prints) {
			printf("Flushing page space %lu, page no %lu \n",
					block->space, block->offset);
		}

		buf_flush_write_block_low(block);
		
		return(1);

	} else if (flush_type == BUF_FLUSH_LRU && block
			&& buf_flush_ready_for_flush(block, flush_type)) {

		/* VERY IMPORTANT:
		Because any thread may call the LRU flush, even when owning
		locks on pages, to avoid deadlocks, we must make sure that the
		s-lock is acquired on the page without waiting: this is
		accomplished because in the if-condition above we require
		the page not to be bufferfixed (in function
		..._ready_for_flush). */

		block->io_fix = BUF_IO_WRITE;
		block->flush_type = flush_type;

		(buf_pool->n_flush[flush_type])++;

		rw_lock_s_lock_gen(&(block->lock), BUF_IO_WRITE);

		/* Note that the s-latch is acquired before releasing the
		buf_pool mutex: this ensures that the latch is acquired
		immediately. */
		
		mutex_exit(&(buf_pool->mutex));

		buf_flush_write_block_low(block);

		return(1);

	} else if (flush_type == BUF_FLUSH_SINGLE_PAGE && block
			&& buf_flush_ready_for_flush(block, flush_type)) {
	
		block->io_fix = BUF_IO_WRITE;
		block->flush_type = flush_type;

		if (buf_pool->n_flush[block->flush_type] == 0) {

			os_event_reset(buf_pool->no_flush[block->flush_type]);
		}

		(buf_pool->n_flush[flush_type])++;

		mutex_exit(&(buf_pool->mutex));

		rw_lock_s_lock_gen(&(block->lock), BUF_IO_WRITE);

		if (buf_debug_prints) {
			printf("Flushing single page space %lu, page no %lu \n",
						block->space, block->offset);
		}

		buf_flush_write_block_low(block);
		
		return(1);
	} else {
		mutex_exit(&(buf_pool->mutex));

		return(0);
	}		
}

/***************************************************************
Flushes to disk all flushable pages within the flush area. */
static
ulint
buf_flush_try_neighbors(
/*====================*/
				/* out: number of pages flushed */
	ulint	space,		/* in: space id */
	ulint	offset,		/* in: page offset */
	ulint	flush_type)	/* in: BUF_FLUSH_LRU or BUF_FLUSH_LIST */
{
	buf_block_t*	block;
	ulint		low, high;
	ulint		count		= 0;
	ulint		i;

	ut_ad(flush_type == BUF_FLUSH_LRU || flush_type == BUF_FLUSH_LIST);

	low = (offset / BUF_FLUSH_AREA) * BUF_FLUSH_AREA;
	high = (offset / BUF_FLUSH_AREA + 1) * BUF_FLUSH_AREA;

	if (UT_LIST_GET_LEN(buf_pool->LRU) < BUF_LRU_OLD_MIN_LEN) {
		/* If there is little space, it is better not to flush any
		block except from the end of the LRU list */
	
		low = offset;
		high = offset + 1;
	} else if (flush_type == BUF_FLUSH_LIST) {
		/* Since semaphore waits require us to flush the
		doublewrite buffer to disk, it is best that the
		search area is just the page itself, to minimize
		chances for semaphore waits */

		low = offset;
		high = offset + 1;
	}		

	/* printf("Flush area: low %lu high %lu\n", low, high); */
	
	if (high > fil_space_get_size(space)) {
		high = fil_space_get_size(space);
	}

	mutex_enter(&(buf_pool->mutex));

	for (i = low; i < high; i++) {

		block = buf_page_hash_get(space, i);

		if (block && buf_flush_ready_for_flush(block, flush_type)) {

			mutex_exit(&(buf_pool->mutex));

			/* Note: as we release the buf_pool mutex above, in
			buf_flush_try_page we cannot be sure the page is still
			in a flushable state: therefore we check it again
			inside that function. */

			count += buf_flush_try_page(space, i, flush_type);

			mutex_enter(&(buf_pool->mutex));
		}
	}
				
	mutex_exit(&(buf_pool->mutex));

	return(count);
}

/***********************************************************************
This utility flushes dirty blocks from the end of the LRU list or flush_list.
NOTE 1: in the case of an LRU flush the calling thread may own latches to
pages: to avoid deadlocks, this function must be written so that it cannot
end up waiting for these latches! NOTE 2: in the case of a flush list flush,
the calling thread is not allowed to own any latches on pages! */

ulint
buf_flush_batch(
/*============*/
				/* out: number of blocks for which the write
				request was queued; ULINT_UNDEFINED if there
				was a flush of the same type already running */
	ulint	flush_type,	/* in: BUF_FLUSH_LRU or BUF_FLUSH_LIST; if
				BUF_FLUSH_LIST, then the caller must not own
				any latches on pages */
	ulint	min_n,		/* in: wished minimum mumber of blocks flushed
				(it is not guaranteed that the actual number
				is that big, though) */
	dulint	lsn_limit)	/* in the case BUF_FLUSH_LIST all blocks whose
				oldest_modification is smaller than this
				should be flushed (if their number does not
				exceed min_n), otherwise ignored */
{
	buf_block_t*	block;
	ulint		page_count 	= 0;
	ulint		old_page_count;
	ulint		space;
	ulint		offset;
	ibool		found;
	
	ut_ad((flush_type == BUF_FLUSH_LRU) || (flush_type == BUF_FLUSH_LIST)); 
	ut_ad((flush_type != BUF_FLUSH_LIST) ||
					sync_thread_levels_empty_gen(TRUE));
	      
	mutex_enter(&(buf_pool->mutex));

	if ((buf_pool->n_flush[flush_type] > 0)
	    || (buf_pool->init_flush[flush_type] == TRUE)) {

		/* There is already a flush batch of the same type running */
		
		mutex_exit(&(buf_pool->mutex));

		return(ULINT_UNDEFINED);
	}

	(buf_pool->init_flush)[flush_type] = TRUE;
	
	for (;;) {
		/* If we have flushed enough, leave the loop */
		if (page_count >= min_n) {

			break;
		}
	
		/* Start from the end of the list looking for a suitable
		block to be flushed. */
		
	    	if (flush_type == BUF_FLUSH_LRU) {
			block = UT_LIST_GET_LAST(buf_pool->LRU);
	    	} else {
			ut_ad(flush_type == BUF_FLUSH_LIST);

			block = UT_LIST_GET_LAST(buf_pool->flush_list);

			if (!block
			    || (ut_dulint_cmp(block->oldest_modification,
			    				lsn_limit) >= 0)) {
				/* We have flushed enough */

				break;
			}
	    	}
	    	
	    	found = FALSE;
	
		/* Note that after finding a single flushable page, we try to
		flush also all its neighbors, and after that start from the
		END of the LRU list or flush list again: the list may change
		during the flushing and we cannot safely preserve within this
		function a pointer to a block in the list! */

	    	while ((block != NULL) && !found) {

			if (buf_flush_ready_for_flush(block, flush_type)) {

				found = TRUE;
				space = block->space;
				offset = block->offset;
	    
				mutex_exit(&(buf_pool->mutex));

				old_page_count = page_count;
				
				/* Try to flush also all the neighbors */
				page_count +=
					buf_flush_try_neighbors(space, offset,
								flush_type);

				/* printf(
				"Flush type %lu, page no %lu, neighb %lu\n",
				flush_type, offset,
				page_count - old_page_count); */

				mutex_enter(&(buf_pool->mutex));

			} else if (flush_type == BUF_FLUSH_LRU) {

				block = UT_LIST_GET_PREV(LRU, block);

			} else {
				ut_ad(flush_type == BUF_FLUSH_LIST);

				block = UT_LIST_GET_PREV(flush_list, block);
			}
	    	}

	    	/* If we could not find anything to flush, leave the loop */

	    	if (!found) {
	    		break;
	    	}
	}

	(buf_pool->init_flush)[flush_type] = FALSE;

	if ((buf_pool->n_flush[flush_type] == 0)
	    && (buf_pool->init_flush[flush_type] == FALSE)) {

		/* The running flush batch has ended */

		os_event_set(buf_pool->no_flush[flush_type]);
	}

	mutex_exit(&(buf_pool->mutex));

	buf_flush_buffered_writes();

	if (buf_debug_prints && page_count > 0) {
		if (flush_type == BUF_FLUSH_LRU) {
			printf("Flushed %lu pages in LRU flush\n",
						page_count);
		} else if (flush_type == BUF_FLUSH_LIST) {
			printf("Flushed %lu pages in flush list flush\n",
						page_count);
		} else {
			ut_error;
		}
	}
	
	return(page_count);
}

/**********************************************************************
Waits until a flush batch of the given type ends */

void
buf_flush_wait_batch_end(
/*=====================*/
	ulint	type)	/* in: BUF_FLUSH_LRU or BUF_FLUSH_LIST */
{
	ut_ad((type == BUF_FLUSH_LRU) || (type == BUF_FLUSH_LIST));
	
	os_event_wait(buf_pool->no_flush[type]);
}	

/**********************************************************************
Gives a recommendation of how many blocks should be flushed to establish
a big enough margin of replaceable blocks near the end of the LRU list
and in the free list. */
static
ulint
buf_flush_LRU_recommendation(void)
/*==============================*/
			/* out: number of blocks which should be flushed
			from the end of the LRU list */
{
	buf_block_t*	block;
	ulint		n_replaceable;
	ulint		distance	= 0;
	
	mutex_enter(&(buf_pool->mutex));

	n_replaceable = UT_LIST_GET_LEN(buf_pool->free);

	block = UT_LIST_GET_LAST(buf_pool->LRU);

	while ((block != NULL)
	       && (n_replaceable < BUF_FLUSH_FREE_BLOCK_MARGIN
	       				+ BUF_FLUSH_EXTRA_MARGIN)
	       && (distance < BUF_LRU_FREE_SEARCH_LEN)) {

		if (buf_flush_ready_for_replace(block)) {
			n_replaceable++;
		}

		distance++;
			
		block = UT_LIST_GET_PREV(LRU, block);
	}
	
	mutex_exit(&(buf_pool->mutex));

	if (n_replaceable >= BUF_FLUSH_FREE_BLOCK_MARGIN) {

		return(0);
	}
	
	return(BUF_FLUSH_FREE_BLOCK_MARGIN + BUF_FLUSH_EXTRA_MARGIN
							- n_replaceable);
}

/*************************************************************************
Flushes pages from the end of the LRU list if there is too small a margin
of replaceable pages there or in the free list. VERY IMPORTANT: this function
is called also by threads which have locks on pages. To avoid deadlocks, we
flush only pages such that the s-lock required for flushing can be acquired
immediately, without waiting. */ 

void
buf_flush_free_margin(void)
/*=======================*/
{
	ulint	n_to_flush;

	n_to_flush = buf_flush_LRU_recommendation();
	
	if (n_to_flush > 0) {
		buf_flush_batch(BUF_FLUSH_LRU, n_to_flush, ut_dulint_zero);
	}
}

/**********************************************************************
Validates the flush list. */
static
ibool
buf_flush_validate_low(void)
/*========================*/
		/* out: TRUE if ok */
{
	buf_block_t*	block;
	dulint		om;
	
	UT_LIST_VALIDATE(flush_list, buf_block_t, buf_pool->flush_list);

	block = UT_LIST_GET_FIRST(buf_pool->flush_list);

	while (block != NULL) {
		om = block->oldest_modification;
		ut_a(block->state == BUF_BLOCK_FILE_PAGE);
		ut_a(ut_dulint_cmp(om, ut_dulint_zero) > 0);
		
		block = UT_LIST_GET_NEXT(flush_list, block);

		if (block) {
			ut_a(ut_dulint_cmp(om, block->oldest_modification)
									>= 0);
		}
	}

	return(TRUE);
}

/**********************************************************************
Validates the flush list. */

ibool
buf_flush_validate(void)
/*====================*/
		/* out: TRUE if ok */
{
	ibool	ret;
	
	mutex_enter(&(buf_pool->mutex));

	ret = buf_flush_validate_low();
	
	mutex_exit(&(buf_pool->mutex));

	return(ret);
}
