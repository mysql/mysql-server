/*****************************************************************************

Copyright (c) 1995, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file buf/buf0dblwr.cc
Doublwrite buffer module

Created 2011/12/19
*******************************************************/

#include "buf0dblwr.h"

#ifdef UNIV_NONINL
#include "buf0buf.ic"
#endif

#include "buf0buf.h"
#include "buf0checksum.h"
#include "srv0start.h"
#include "srv0srv.h"
#include "page0zip.h"
#include "trx0sys.h"

#ifndef UNIV_HOTBACKUP

/** Time in milliseconds that we sleep when unable to find a slot in
the doublewrite buffer or when we have to wait for a running batch
to end. */
#define TRX_DOUBLEWRITE_BATCH_POLL_DELAY	10000

#ifdef UNIV_PFS_MUTEX
/* Key to register the mutex with performance schema */
UNIV_INTERN mysql_pfs_key_t	buf_dblwr_mutex_key;
#endif /* UNIV_PFS_RWLOCK */

/** The doublewrite buffer */
UNIV_INTERN buf_dblwr_t*	buf_dblwr = NULL;

/** Set to TRUE when the doublewrite buffer is being created */
UNIV_INTERN ibool	buf_dblwr_being_created = FALSE;

/****************************************************************//**
Determines if a page number is located inside the doublewrite buffer.
@return TRUE if the location is inside the two blocks of the
doublewrite buffer */
UNIV_INTERN
ibool
buf_dblwr_page_inside(
/*==================*/
	ulint	page_no)	/*!< in: page number */
{
	if (buf_dblwr == NULL) {

		return(FALSE);
	}

	if (page_no >= buf_dblwr->block1
	    && page_no < buf_dblwr->block1
	    + TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) {
		return(TRUE);
	}

	if (page_no >= buf_dblwr->block2
	    && page_no < buf_dblwr->block2
	    + TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) {
		return(TRUE);
	}

	return(FALSE);
}

/****************************************************************//**
Calls buf_page_get() on the TRX_SYS_PAGE and returns a pointer to the
doublewrite buffer within it.
@return	pointer to the doublewrite buffer within the filespace header
page. */
UNIV_INLINE
byte*
buf_dblwr_get(
/*==========*/
	mtr_t*	mtr)	/*!< in/out: MTR to hold the page latch */
{
	buf_block_t*	block;

	block = buf_page_get(TRX_SYS_SPACE, 0, TRX_SYS_PAGE_NO,
			     RW_X_LATCH, mtr);
	buf_block_dbg_add_level(block, SYNC_NO_ORDER_CHECK);

	return(buf_block_get_frame(block) + TRX_SYS_DOUBLEWRITE);
}


/****************************************************************//**
Creates or initialializes the doublewrite buffer at a database start. */
static
void
buf_dblwr_init(
/*===========*/
	byte*	doublewrite)	/*!< in: pointer to the doublewrite buf
				header on trx sys page */
{
	ulint	buf_size;

	buf_dblwr = static_cast<buf_dblwr_t*>(
		mem_zalloc(sizeof(buf_dblwr_t)));

	/* There are two blocks of same size in the doublewrite
	buffer. */
	buf_size = 2 * TRX_SYS_DOUBLEWRITE_BLOCK_SIZE;

	/* There must be atleast one buffer for single page writes
	and one buffer for batch writes. */
	ut_a(srv_doublewrite_batch_size > 0
	     && srv_doublewrite_batch_size < buf_size);

	mutex_create(buf_dblwr_mutex_key,
		     &buf_dblwr->mutex, SYNC_DOUBLEWRITE);

	buf_dblwr->first_free = 0;
	buf_dblwr->s_reserved = 0;
	buf_dblwr->b_reserved = 0;

	buf_dblwr->block1 = mach_read_from_4(
		doublewrite + TRX_SYS_DOUBLEWRITE_BLOCK1);
	buf_dblwr->block2 = mach_read_from_4(
		doublewrite + TRX_SYS_DOUBLEWRITE_BLOCK2);

	buf_dblwr->in_use = static_cast<ibool*>(
		mem_zalloc(buf_size * sizeof(ibool)));

	buf_dblwr->write_buf_unaligned = static_cast<byte*>(
		ut_malloc((1 + buf_size) * UNIV_PAGE_SIZE));

	buf_dblwr->write_buf = static_cast<byte*>(
		ut_align(buf_dblwr->write_buf_unaligned,
			 UNIV_PAGE_SIZE));

	buf_dblwr->buf_block_arr = static_cast<buf_page_t**>(
		mem_zalloc(buf_size * sizeof(void*)));
}

/****************************************************************//**
Creates the doublewrite buffer to a new InnoDB installation. The header of the
doublewrite buffer is placed on the trx system header page. */
UNIV_INTERN
void
buf_dblwr_create(void)
/*==================*/
{
	buf_block_t*	block2;
	buf_block_t*	new_block;
	byte*	doublewrite;
	byte*	fseg_header;
	ulint	page_no;
	ulint	prev_page_no;
	ulint	i;
	mtr_t	mtr;

	if (buf_dblwr) {
		/* Already inited */

		return;
	}

start_again:
	mtr_start(&mtr);
	buf_dblwr_being_created = TRUE;

	doublewrite = buf_dblwr_get(&mtr);

	if (mach_read_from_4(doublewrite + TRX_SYS_DOUBLEWRITE_MAGIC)
	    == TRX_SYS_DOUBLEWRITE_MAGIC_N) {
		/* The doublewrite buffer has already been created:
		just read in some numbers */

		buf_dblwr_init(doublewrite);

		mtr_commit(&mtr);
		buf_dblwr_being_created = FALSE;
		return;
	}

	ib_logf(IB_LOG_LEVEL_INFO,
		"Doublewrite buffer not found: creating new");

	if (buf_pool_get_curr_size()
	    < ((2 * TRX_SYS_DOUBLEWRITE_BLOCK_SIZE
		+ FSP_EXTENT_SIZE / 2 + 100)
	       * UNIV_PAGE_SIZE)) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Cannot create doublewrite buffer: you must "
			"increase your buffer pool size. Cannot continue "
			"operation.");

		exit(EXIT_FAILURE);
	}

	block2 = fseg_create(TRX_SYS_SPACE, TRX_SYS_PAGE_NO,
			     TRX_SYS_DOUBLEWRITE
			     + TRX_SYS_DOUBLEWRITE_FSEG, &mtr);

	/* fseg_create acquires a second latch on the page,
	therefore we must declare it: */

	buf_block_dbg_add_level(block2, SYNC_NO_ORDER_CHECK);

	if (block2 == NULL) {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Cannot create doublewrite buffer: you must "
			"increase your tablespace size. "
			"Cannot continue operation.");

		/* We exit without committing the mtr to prevent
		its modifications to the database getting to disk */

		exit(EXIT_FAILURE);
	}

	fseg_header = doublewrite + TRX_SYS_DOUBLEWRITE_FSEG;
	prev_page_no = 0;

	for (i = 0; i < 2 * TRX_SYS_DOUBLEWRITE_BLOCK_SIZE
		     + FSP_EXTENT_SIZE / 2; i++) {
		new_block = fseg_alloc_free_page(
			fseg_header, prev_page_no + 1, FSP_UP, &mtr);
		if (new_block == NULL) {
			ib_logf(IB_LOG_LEVEL_ERROR,
				"Cannot create doublewrite buffer: you must "
				"increase your tablespace size. "
				"Cannot continue operation.");

			exit(EXIT_FAILURE);
		}

		/* We read the allocated pages to the buffer pool;
		when they are written to disk in a flush, the space
		id and page number fields are also written to the
		pages. When we at database startup read pages
		from the doublewrite buffer, we know that if the
		space id and page number in them are the same as
		the page position in the tablespace, then the page
		has not been written to in doublewrite. */

		ut_ad(rw_lock_get_x_lock_count(&new_block->lock) == 1);
		page_no = buf_block_get_page_no(new_block);

		if (i == FSP_EXTENT_SIZE / 2) {
			ut_a(page_no == FSP_EXTENT_SIZE);
			mlog_write_ulint(doublewrite
					 + TRX_SYS_DOUBLEWRITE_BLOCK1,
					 page_no, MLOG_4BYTES, &mtr);
			mlog_write_ulint(doublewrite
					 + TRX_SYS_DOUBLEWRITE_REPEAT
					 + TRX_SYS_DOUBLEWRITE_BLOCK1,
					 page_no, MLOG_4BYTES, &mtr);

		} else if (i == FSP_EXTENT_SIZE / 2
			   + TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) {
			ut_a(page_no == 2 * FSP_EXTENT_SIZE);
			mlog_write_ulint(doublewrite
					 + TRX_SYS_DOUBLEWRITE_BLOCK2,
					 page_no, MLOG_4BYTES, &mtr);
			mlog_write_ulint(doublewrite
					 + TRX_SYS_DOUBLEWRITE_REPEAT
					 + TRX_SYS_DOUBLEWRITE_BLOCK2,
					 page_no, MLOG_4BYTES, &mtr);

		} else if (i > FSP_EXTENT_SIZE / 2) {
			ut_a(page_no == prev_page_no + 1);
		}

		if (((i + 1) & 15) == 0) {
			/* rw_locks can only be recursively x-locked
			2048 times. (on 32 bit platforms,
			(lint) 0 - (X_LOCK_DECR * 2049)
			is no longer a negative number, and thus
			lock_word becomes like a shared lock).
			For 4k page size this loop will
			lock the fseg header too many times. Since
			this code is not done while any other threads
			are active, restart the MTR occasionally. */
			mtr_commit(&mtr);
			mtr_start(&mtr);
			doublewrite = buf_dblwr_get(&mtr);
			fseg_header = doublewrite
				      + TRX_SYS_DOUBLEWRITE_FSEG;
		}

		prev_page_no = page_no;
	}

	mlog_write_ulint(doublewrite + TRX_SYS_DOUBLEWRITE_MAGIC,
			 TRX_SYS_DOUBLEWRITE_MAGIC_N,
			 MLOG_4BYTES, &mtr);
	mlog_write_ulint(doublewrite + TRX_SYS_DOUBLEWRITE_MAGIC
			 + TRX_SYS_DOUBLEWRITE_REPEAT,
			 TRX_SYS_DOUBLEWRITE_MAGIC_N,
			 MLOG_4BYTES, &mtr);

	mlog_write_ulint(doublewrite
			 + TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED,
			 TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED_N,
			 MLOG_4BYTES, &mtr);
	mtr_commit(&mtr);

	/* Flush the modified pages to disk and make a checkpoint */
	log_make_checkpoint_at(LSN_MAX, TRUE);

	/* Remove doublewrite pages from LRU */
	buf_pool_invalidate();

	ib_logf(IB_LOG_LEVEL_INFO, "Doublewrite buffer created");

	goto start_again;
}

/****************************************************************//**
At a database startup initializes the doublewrite buffer memory structure if
we already have a doublewrite buffer created in the data files. If we are
upgrading to an InnoDB version which supports multiple tablespaces, then this
function performs the necessary update operations. If we are in a crash
recovery, this function uses a possible doublewrite buffer to restore
half-written pages in the data files. */
UNIV_INTERN
void
buf_dblwr_init_or_restore_pages(
/*============================*/
	ibool	restore_corrupt_pages)	/*!< in: TRUE=restore pages */
{
	byte*	buf;
	byte*	read_buf;
	byte*	unaligned_read_buf;
	ulint	block1;
	ulint	block2;
	byte*	page;
	ibool	reset_space_ids = FALSE;
	byte*	doublewrite;
	ulint	space_id;
	ulint	page_no;
	ulint	i;

	/* We do the file i/o past the buffer pool */

	unaligned_read_buf = static_cast<byte*>(ut_malloc(2 * UNIV_PAGE_SIZE));

	read_buf = static_cast<byte*>(
		ut_align(unaligned_read_buf, UNIV_PAGE_SIZE));

	/* Read the trx sys header to check if we are using the doublewrite
	buffer */

	fil_io(OS_FILE_READ, TRUE, TRX_SYS_SPACE, 0, TRX_SYS_PAGE_NO, 0,
	       UNIV_PAGE_SIZE, read_buf, NULL);
	doublewrite = read_buf + TRX_SYS_DOUBLEWRITE;

	if (mach_read_from_4(doublewrite + TRX_SYS_DOUBLEWRITE_MAGIC)
	    == TRX_SYS_DOUBLEWRITE_MAGIC_N) {
		/* The doublewrite buffer has been created */

		buf_dblwr_init(doublewrite);

		block1 = buf_dblwr->block1;
		block2 = buf_dblwr->block2;

		buf = buf_dblwr->write_buf;
	} else {
		goto leave_func;
	}

	if (mach_read_from_4(doublewrite + TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED)
	    != TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED_N) {

		/* We are upgrading from a version < 4.1.x to a version where
		multiple tablespaces are supported. We must reset the space id
		field in the pages in the doublewrite buffer because starting
		from this version the space id is stored to
		FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID. */

		reset_space_ids = TRUE;

		ib_logf(IB_LOG_LEVEL_INFO,
			"Resetting space id's in the doublewrite buffer");
	}

	/* Read the pages from the doublewrite buffer to memory */

	fil_io(OS_FILE_READ, TRUE, TRX_SYS_SPACE, 0, block1, 0,
	       TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * UNIV_PAGE_SIZE,
	       buf, NULL);
	fil_io(OS_FILE_READ, TRUE, TRX_SYS_SPACE, 0, block2, 0,
	       TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * UNIV_PAGE_SIZE,
	       buf + TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * UNIV_PAGE_SIZE,
	       NULL);
	/* Check if any of these pages is half-written in data files, in the
	intended position */

	page = buf;

	for (i = 0; i < TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * 2; i++) {

		ulint source_page_no;
		page_no = mach_read_from_4(page + FIL_PAGE_OFFSET);

		if (reset_space_ids) {

			space_id = 0;
			mach_write_to_4(page
					+ FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, 0);
			/* We do not need to calculate new checksums for the
			pages because the field .._SPACE_ID does not affect
			them. Write the page back to where we read it from. */

			if (i < TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) {
				source_page_no = block1 + i;
			} else {
				source_page_no = block2
					+ i - TRX_SYS_DOUBLEWRITE_BLOCK_SIZE;
			}

			fil_io(OS_FILE_WRITE, TRUE, 0, 0, source_page_no, 0,
			       UNIV_PAGE_SIZE, page, NULL);
		} else {

			space_id = mach_read_from_4(
				page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
		}

		if (!restore_corrupt_pages) {
			/* The database was shut down gracefully: no need to
			restore pages */

		} else if (!fil_tablespace_exists_in_mem(space_id)) {
			/* Maybe we have dropped the single-table tablespace
			and this page once belonged to it: do nothing */

		} else if (!fil_check_adress_in_tablespace(space_id,
							   page_no)) {
			ib_logf(IB_LOG_LEVEL_WARN,
				"A page in the doublewrite buffer is not "
				"within space bounds; space id %lu "
				"page number %lu, page %lu in "
				"doublewrite buf.",
				(ulong) space_id, (ulong) page_no, (ulong) i);

		} else if (space_id == TRX_SYS_SPACE
			   && ((page_no >= block1
				&& page_no
				< block1 + TRX_SYS_DOUBLEWRITE_BLOCK_SIZE)
			       || (page_no >= block2
				   && page_no
				   < (block2
				      + TRX_SYS_DOUBLEWRITE_BLOCK_SIZE)))) {

			/* It is an unwritten doublewrite buffer page:
			do nothing */
		} else {
			ulint	zip_size = fil_space_get_zip_size(space_id);

			/* Read in the actual page from the file */
			fil_io(OS_FILE_READ, TRUE, space_id, zip_size,
			       page_no, 0,
			       zip_size ? zip_size : UNIV_PAGE_SIZE,
			       read_buf, NULL);

			/* Check if the page is corrupt */

			if (buf_page_is_corrupted(true, read_buf, zip_size)) {

				fprintf(stderr,
					"InnoDB: Warning: database page"
					" corruption or a failed\n"
					"InnoDB: file read of"
					" space %lu page %lu.\n"
					"InnoDB: Trying to recover it from"
					" the doublewrite buffer.\n",
					(ulong) space_id, (ulong) page_no);

				if (buf_page_is_corrupted(true,
							  page, zip_size)) {
					fprintf(stderr,
						"InnoDB: Dump of the page:\n");
					buf_page_print(
						read_buf, zip_size,
						BUF_PAGE_PRINT_NO_CRASH);
					fprintf(stderr,
						"InnoDB: Dump of"
						" corresponding page"
						" in doublewrite buffer:\n");
					buf_page_print(
						page, zip_size,
						BUF_PAGE_PRINT_NO_CRASH);

					fprintf(stderr,
						"InnoDB: Also the page in the"
						" doublewrite buffer"
						" is corrupt.\n"
						"InnoDB: Cannot continue"
						" operation.\n"
						"InnoDB: You can try to"
						" recover the database"
						" with the my.cnf\n"
						"InnoDB: option:\n"
						"InnoDB:"
						" innodb_force_recovery=6\n");
					ut_error;
				}

				/* Write the good page from the
				doublewrite buffer to the intended
				position */

				fil_io(OS_FILE_WRITE, TRUE, space_id,
				       zip_size, page_no, 0,
				       zip_size ? zip_size : UNIV_PAGE_SIZE,
				       page, NULL);

				ib_logf(IB_LOG_LEVEL_INFO,
					"Recovered the page from"
					" the doublewrite buffer.");
			}
		}

		page += UNIV_PAGE_SIZE;
	}

	fil_flush_file_spaces(FIL_TABLESPACE);

leave_func:
	ut_free(unaligned_read_buf);
}

/****************************************************************//**
Frees doublewrite buffer. */
UNIV_INTERN
void
buf_dblwr_free(void)
/*================*/
{
	/* Free the double write data structures. */
	ut_a(buf_dblwr != NULL);
	ut_ad(buf_dblwr->s_reserved == 0);
	ut_ad(buf_dblwr->b_reserved == 0);

	ut_free(buf_dblwr->write_buf_unaligned);
	buf_dblwr->write_buf_unaligned = NULL;

	mem_free(buf_dblwr->buf_block_arr);
	buf_dblwr->buf_block_arr = NULL;

	mem_free(buf_dblwr->in_use);
	buf_dblwr->in_use = NULL;

	mutex_free(&buf_dblwr->mutex);
	mem_free(buf_dblwr);
	buf_dblwr = NULL;
}

/********************************************************************//**
Updates the doublewrite buffer when an IO request that is part of an
LRU or flush batch is completed. */
UNIV_INTERN
void
buf_dblwr_update(void)
/*==================*/
{
	if (!srv_use_doublewrite_buf || buf_dblwr == NULL) {
		return;
	}

	mutex_enter(&buf_dblwr->mutex);

	ut_ad(buf_dblwr->batch_running);
	ut_ad(buf_dblwr->b_reserved > 0);

	buf_dblwr->b_reserved--;
	if (buf_dblwr->b_reserved == 0) {

		mutex_exit(&buf_dblwr->mutex);
		/* This will finish the batch. Sync data files
		to the disk. */
		fil_flush_file_spaces(FIL_TABLESPACE);
		mutex_enter(&buf_dblwr->mutex);

		/* We can now reuse the doublewrite memory buffer: */
		buf_dblwr->first_free = 0;
		buf_dblwr->batch_running = FALSE;
	}

	mutex_exit(&buf_dblwr->mutex);
}

/********************************************************************//**
Check the LSN values on the page. */
static
void
buf_dblwr_check_page_lsn(
/*=====================*/
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
buf_dblwr_assert_on_corrupt_block(
/*==============================*/
	const buf_block_t*	block)	/*!< in: block to check */
{
	buf_page_print(block->frame, 0, BUF_PAGE_PRINT_NO_CRASH);

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
buf_dblwr_check_block(
/*==================*/
	const buf_block_t*	block)	/*!< in: block to check */
{
	if (buf_block_get_state(block) != BUF_BLOCK_FILE_PAGE
	    || block->page.zip.data) {
		/* No simple validate for compressed pages exists. */
		return;
	}

	buf_dblwr_check_page_lsn(block->frame);

	if (!block->check_index_page_at_flush) {
		return;
	}

	if (page_is_comp(block->frame)) {
		if (!page_simple_validate_new(block->frame)) {
			buf_dblwr_assert_on_corrupt_block(block);
		}
	} else if (!page_simple_validate_old(block->frame)) {

		buf_dblwr_assert_on_corrupt_block(block);
	}
}

/********************************************************************//**
Writes a page that has already been written to the doublewrite buffer
to the datafile. It is the job of the caller to sync the datafile. */
static
void
buf_dblwr_write_block_to_datafile(
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
		       (void*) block->page.zip.data,
		       (void*) block);

		goto exit;
	}

	ut_a(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);
	buf_dblwr_check_page_lsn(block->frame);

	fil_io(OS_FILE_WRITE | OS_AIO_SIMULATED_WAKE_LATER,
	       FALSE, buf_block_get_space(block), 0,
	       buf_block_get_page_no(block), 0, UNIV_PAGE_SIZE,
	       (void*) block->frame, (void*) block);

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
UNIV_INTERN
void
buf_dblwr_flush_buffered_writes(void)
/*=================================*/
{
	byte*		write_buf;
	ulint		len;
	ulint		len2;
	ulint		i;

	if (!srv_use_doublewrite_buf || buf_dblwr == NULL) {
		/* Sync the writes to the disk. */
		buf_flush_sync_datafiles();
		return;
	}

try_again:
	mutex_enter(&(buf_dblwr->mutex));

	/* Write first to doublewrite buffer blocks. We use synchronous
	aio and thus know that file write has been completed when the
	control returns. */

	if (buf_dblwr->first_free == 0) {

		mutex_exit(&(buf_dblwr->mutex));

		return;
	}

	if (buf_dblwr->batch_running) {
		mutex_exit(&buf_dblwr->mutex);

		/* Another thread is running the batch right now. Wait
		for it to finish. */
		os_thread_sleep(TRX_DOUBLEWRITE_BATCH_POLL_DELAY);
		goto try_again;
	}

	ut_a(!buf_dblwr->batch_running);

	/* Disallow anyone else to post to doublewrite buffer or to
	start another batch of flushing. */
	buf_dblwr->batch_running = TRUE;

	/* Now safe to release the mutex. Note that though no other
	thread is allowed to post to the doublewrite batch flushing
	but any threads working on single page flushes are allowed
	to proceed. */
	mutex_exit(&buf_dblwr->mutex);

	write_buf = buf_dblwr->write_buf;

	for (len2 = 0, i = 0;
	     i < buf_dblwr->first_free;
	     len2 += UNIV_PAGE_SIZE, i++) {

		const buf_block_t*	block;

		block = (buf_block_t*) buf_dblwr->buf_block_arr[i];

		if (buf_block_get_state(block) != BUF_BLOCK_FILE_PAGE
		    || block->page.zip.data) {
			/* No simple validate for compressed
			pages exists. */
			continue;
		}

		/* Check that the actual page in the buffer pool is
		not corrupt and the LSN values are sane. */
		buf_dblwr_check_block(block);

		/* Check that the page as written to the doublewrite
		buffer has sane LSN values. */
		buf_dblwr_check_page_lsn(write_buf + len2);
	}

	/* Write out the first block of the doublewrite buffer */
	len = ut_min(TRX_SYS_DOUBLEWRITE_BLOCK_SIZE,
		     buf_dblwr->first_free) * UNIV_PAGE_SIZE;

	fil_io(OS_FILE_WRITE, TRUE, TRX_SYS_SPACE, 0,
	       buf_dblwr->block1, 0, len,
	       (void*) write_buf, NULL);

	if (buf_dblwr->first_free <= TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) {
		/* No unwritten pages in the second block. */
		goto flush;
	}

	/* Write out the second block of the doublewrite buffer. */
	len = (buf_dblwr->first_free - TRX_SYS_DOUBLEWRITE_BLOCK_SIZE)
	       * UNIV_PAGE_SIZE;

	write_buf = buf_dblwr->write_buf
		    + TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * UNIV_PAGE_SIZE;

	fil_io(OS_FILE_WRITE, TRUE, TRX_SYS_SPACE, 0,
	       buf_dblwr->block2, 0, len,
	       (void*) write_buf, NULL);

flush:
	/* increment the doublewrite flushed pages counter */
	srv_stats.dblwr_pages_written.add(buf_dblwr->first_free);
	srv_stats.dblwr_writes.inc();

	/* Now flush the doublewrite buffer data to disk */
	fil_flush(TRX_SYS_SPACE);

	/* We know that the writes have been flushed to disk now
	and in recovery we will find them in the doublewrite buffer
	blocks. Next do the writes to the intended positions. */

	for (i = 0; i < buf_dblwr->first_free; i++) {
		const buf_block_t* block = (buf_block_t*)
			buf_dblwr->buf_block_arr[i];

		buf_dblwr_write_block_to_datafile(block);
	}

	/* Wake possible simulated aio thread to actually post the
	writes to the operating system. We don't flush the files
	at this point. We leave it to the IO helper thread to flush
	datafiles when the whole batch has been processed. */
	os_aio_simulated_wake_handler_threads();
}

/********************************************************************//**
Posts a buffer page for writing. If the doublewrite memory buffer is
full, calls buf_dblwr_flush_buffered_writes and waits for for free
space to appear. */
UNIV_INTERN
void
buf_dblwr_add_to_batch(
/*====================*/
	buf_page_t*	bpage)	/*!< in: buffer block to write */
{
	ulint	zip_size;

	ut_a(buf_page_in_file(bpage));

try_again:
	mutex_enter(&(buf_dblwr->mutex));

	ut_a(buf_dblwr->first_free <= srv_doublewrite_batch_size);

	if (buf_dblwr->batch_running) {
		mutex_exit(&buf_dblwr->mutex);

		/* This not nearly as bad as it looks. There is only
		page_cleaner thread which does background flushing
		in batches therefore it is unlikely to be a contention
		point. The only exception is when a user thread is
		forced to do a flush batch because of a sync
		checkpoint. */
		os_thread_sleep(TRX_DOUBLEWRITE_BATCH_POLL_DELAY);
		goto try_again;
	}

	if (buf_dblwr->first_free == srv_doublewrite_batch_size) {
		mutex_exit(&(buf_dblwr->mutex));

		buf_dblwr_flush_buffered_writes();

		goto try_again;
	}

	zip_size = buf_page_get_zip_size(bpage);

	if (zip_size) {
		UNIV_MEM_ASSERT_RW(bpage->zip.data, zip_size);
		/* Copy the compressed page and clear the rest. */
		memcpy(buf_dblwr->write_buf
		       + UNIV_PAGE_SIZE * buf_dblwr->first_free,
		       bpage->zip.data, zip_size);
		memset(buf_dblwr->write_buf
		       + UNIV_PAGE_SIZE * buf_dblwr->first_free
		       + zip_size, 0, UNIV_PAGE_SIZE - zip_size);
	} else {
		ut_a(buf_page_get_state(bpage) == BUF_BLOCK_FILE_PAGE);
		UNIV_MEM_ASSERT_RW(((buf_block_t*) bpage)->frame,
				   UNIV_PAGE_SIZE);

		memcpy(buf_dblwr->write_buf
		       + UNIV_PAGE_SIZE * buf_dblwr->first_free,
		       ((buf_block_t*) bpage)->frame, UNIV_PAGE_SIZE);
	}

	buf_dblwr->buf_block_arr[buf_dblwr->first_free] = bpage;

	buf_dblwr->first_free++;
	buf_dblwr->b_reserved++;

	ut_ad(buf_dblwr->b_reserved <= srv_doublewrite_batch_size);

	if (buf_dblwr->first_free == srv_doublewrite_batch_size) {
		mutex_exit(&(buf_dblwr->mutex));

		buf_dblwr_flush_buffered_writes();

		return;
	}

	mutex_exit(&(buf_dblwr->mutex));
}

/********************************************************************//**
Writes a page to the doublewrite buffer on disk, sync it, then write
the page to the datafile and sync the datafile. This function is used
for single page flushes. If all the buffers allocated for single page
flushes in the doublewrite buffer are in use we wait here for one to
become free. We are guaranteed that a slot will become free because any
thread that is using a slot must also release the slot before leaving
this function. */
UNIV_INTERN
void
buf_dblwr_write_single_page(
/*========================*/
	buf_page_t*	bpage)	/*!< in: buffer block to write */
{
	ulint		n_slots;
	ulint		size;
	ulint		zip_size;
	ulint		offset;
	ulint		i;

	ut_a(buf_page_in_file(bpage));
	ut_a(srv_use_doublewrite_buf);
	ut_a(buf_dblwr != NULL);

	/* total number of slots available for single page flushes
	starts from srv_doublewrite_batch_size to the end of the
	buffer. */
	size = 2 * TRX_SYS_DOUBLEWRITE_BLOCK_SIZE;
	ut_a(size > srv_doublewrite_batch_size);
	n_slots = size - srv_doublewrite_batch_size;

	if (buf_page_get_state(bpage) == BUF_BLOCK_FILE_PAGE) {

		/* Check that the actual page in the buffer pool is
		not corrupt and the LSN values are sane. */
		buf_dblwr_check_block((buf_block_t*) bpage);

		/* Check that the page as written to the doublewrite
		buffer has sane LSN values. */
		if (!bpage->zip.data) {
			buf_dblwr_check_page_lsn(
				((buf_block_t*) bpage)->frame);
		}
	}

retry:
	mutex_enter(&buf_dblwr->mutex);
	if (buf_dblwr->s_reserved == n_slots) {

		mutex_exit(&buf_dblwr->mutex);
		/* All slots are reserved. Since it involves two IOs
		during the processing a sleep of 10ms should be
		enough. */
		os_thread_sleep(TRX_DOUBLEWRITE_BATCH_POLL_DELAY);
		goto retry;
	}

	for (i = srv_doublewrite_batch_size; i < size; ++i) {

		if (!buf_dblwr->in_use[i]) {
			break;
		}
	}

	/* We are guaranteed to find a slot. */
	ut_a(i < size);
	buf_dblwr->in_use[i] = TRUE;
	buf_dblwr->s_reserved++;
	buf_dblwr->buf_block_arr[i] = bpage;
	mutex_exit(&buf_dblwr->mutex);

	/* Lets see if we are going to write in the first or second
	block of the doublewrite buffer. */
	if (i < TRX_SYS_DOUBLEWRITE_BLOCK_SIZE) {
		offset = buf_dblwr->block1 + i;
	} else {
		offset = buf_dblwr->block2 + i
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
		memcpy(buf_dblwr->write_buf + UNIV_PAGE_SIZE * i,
		       bpage->zip.data, zip_size);
		memset(buf_dblwr->write_buf + UNIV_PAGE_SIZE * i
		       + zip_size, 0, UNIV_PAGE_SIZE - zip_size);

		fil_io(OS_FILE_WRITE, TRUE, TRX_SYS_SPACE, 0,
		       offset, 0, UNIV_PAGE_SIZE,
		       (void*) (buf_dblwr->write_buf
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
	buf_dblwr_write_block_to_datafile((buf_block_t*) bpage);

	/* Sync the writes to the disk. */
	buf_flush_sync_datafiles();

	mutex_enter(&buf_dblwr->mutex);

	buf_dblwr->s_reserved--;
	buf_dblwr->buf_block_arr[i] = NULL;
	buf_dblwr->in_use[i] = FALSE;

	/* increment the doublewrite flushed pages counter */
	srv_stats.dblwr_pages_written.add(buf_dblwr->first_free);
	srv_stats.dblwr_writes.inc();

	mutex_exit(&(buf_dblwr->mutex));

}
#endif /* !UNIV_HOTBACKUP */
