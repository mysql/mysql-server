/*****************************************************************************

Copyright (c) 1995, 2013, Oracle and/or its affiliates. All Rights Reserved.

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
@file mtr/mtr0mtr.c
Mini-transaction buffer

Created 11/26/1995 Heikki Tuuri
*******************************************************/

#include "mtr0mtr.h"

#ifdef UNIV_NONINL
#include "mtr0mtr.ic"
#endif

#include "buf0buf.h"
#include "buf0flu.h"
#include "page0types.h"
#include "mtr0log.h"
#include "log0log.h"
#include "buf0flu.h"

#ifndef UNIV_HOTBACKUP
# include "log0recv.h"

/***************************************************//**
Checks if a mini-transaction is dirtying a clean page.
@return TRUE if the mtr is dirtying a clean page. */
UNIV_INTERN
ibool
mtr_block_dirtied(
/*==============*/
	const buf_block_t*	block)	/*!< in: block being x-fixed */
{
	ut_ad(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);
	ut_ad(block->page.buf_fix_count > 0);

	/* It is OK to read oldest_modification because no
	other thread can be performing a write of it and it
	is only during write that the value is reset to 0. */
	return(block->page.oldest_modification == 0);
}

/*****************************************************************//**
Releases the item in the slot given. */
static __attribute__((nonnull))
void
mtr_memo_slot_release_func(
/*=======================*/
#ifdef UNIV_DEBUG
 	mtr_t*			mtr,	/*!< in/out: mini-transaction */
#endif /* UNIV_DEBUG */
	mtr_memo_slot_t*	slot)	/*!< in: memo slot */
{
	void*	object = slot->object;
	slot->object = NULL;

	/* slot release is a local operation for the current mtr.
	We must not be holding the flush_order mutex while
	doing this. */
	ut_ad(!log_flush_order_mutex_own());

	switch (slot->type) {
	case MTR_MEMO_PAGE_S_FIX:
	case MTR_MEMO_PAGE_X_FIX:
	case MTR_MEMO_BUF_FIX:
		buf_page_release((buf_block_t*) object, slot->type);
		break;
	case MTR_MEMO_S_LOCK:
		rw_lock_s_unlock((rw_lock_t*) object);
		break;
	case MTR_MEMO_X_LOCK:
		rw_lock_x_unlock((rw_lock_t*) object);
		break;
#ifdef UNIV_DEBUG
	default:
		ut_ad(slot->type == MTR_MEMO_MODIFY);
		ut_ad(mtr_memo_contains(mtr, object, MTR_MEMO_PAGE_X_FIX));
#endif /* UNIV_DEBUG */
	}
}

#ifdef UNIV_DEBUG
# define mtr_memo_slot_release(mtr, slot) mtr_memo_slot_release_func(mtr, slot)
#else /* UNIV_DEBUG */
# define mtr_memo_slot_release(mtr, slot) mtr_memo_slot_release_func(slot)
#endif /* UNIV_DEBUG */

/**********************************************************//**
Releases the mlocks and other objects stored in an mtr memo.
They are released in the order opposite to which they were pushed
to the memo. */
static __attribute__((nonnull))
void
mtr_memo_pop_all(
/*=============*/
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
{
	const dyn_block_t*	block;

	ut_ad(mtr->magic_n == MTR_MAGIC_N);
	ut_ad(mtr->state == MTR_COMMITTING); /* Currently only used in
					     commit */

	for (block = dyn_array_get_last_block(&mtr->memo);
	     block;
	     block = dyn_array_get_prev_block(&mtr->memo, block)) {
		const mtr_memo_slot_t*	start
			= (mtr_memo_slot_t*) dyn_block_get_data(block);
		mtr_memo_slot_t*	slot
			= (mtr_memo_slot_t*) (dyn_block_get_data(block)
					      + dyn_block_get_used(block));

		ut_ad(!(dyn_block_get_used(block) % sizeof(mtr_memo_slot_t)));

		while (slot-- != start) {
			if (slot->object != NULL) {
				mtr_memo_slot_release(mtr, slot);
			}
		}
	}
}

/*****************************************************************//**
Releases the item in the slot given. */
static
void
mtr_memo_slot_note_modification(
/*============================*/
	mtr_t*			mtr,	/*!< in: mtr */
	mtr_memo_slot_t*	slot)	/*!< in: memo slot */
{
	ut_ad(mtr);
	ut_ad(mtr->magic_n == MTR_MAGIC_N);
	ut_ad(mtr->modifications);

	if (slot->object != NULL && slot->type == MTR_MEMO_PAGE_X_FIX) {
		buf_block_t*	block = (buf_block_t*) slot->object;

#ifdef UNIV_DEBUG
		ut_ad(!mtr->made_dirty || log_flush_order_mutex_own());
#endif /* UNIV_DEBUG */
		buf_flush_note_modification(block, mtr);
	}
}

/**********************************************************//**
Add the modified pages to the buffer flush list. They are released
in the order opposite to which they were pushed to the memo. NOTE! It is
essential that the x-rw-lock on a modified buffer page is not released
before buf_page_note_modification is called for that page! Otherwise,
some thread might race to modify it, and the flush list sort order on
lsn would be destroyed. */
static
void
mtr_memo_note_modifications(
/*========================*/
	mtr_t*	mtr)	/*!< in: mtr */
{
	dyn_array_t*	memo;
	ulint		offset;

	ut_ad(mtr);
	ut_ad(mtr->magic_n == MTR_MAGIC_N);
	ut_ad(mtr->state == MTR_COMMITTING); /* Currently only used in
					     commit */
	memo = &mtr->memo;

	offset = dyn_array_get_data_size(memo);

	while (offset > 0) {
		mtr_memo_slot_t* slot;

		offset -= sizeof(mtr_memo_slot_t);
		slot = dyn_array_get_element(memo, offset);

		mtr_memo_slot_note_modification(mtr, slot);
	}
}

/************************************************************//**
Writes the contents of a mini-transaction log, if any, to the database log. */
static
void
mtr_log_reserve_and_write(
/*======================*/
	mtr_t*	mtr)	/*!< in: mtr */
{
	dyn_array_t*	mlog;
	dyn_block_t*	block;
	ulint		data_size;
	byte*		first_data;

	ut_ad(mtr);

	mlog = &(mtr->log);

	first_data = dyn_block_get_data(mlog);

	if (mtr->n_log_recs > 1) {
		mlog_catenate_ulint(mtr, MLOG_MULTI_REC_END, MLOG_1BYTE);
	} else {
		*first_data = (byte)((ulint)*first_data
				     | MLOG_SINGLE_REC_FLAG);
	}

	if (mlog->heap == NULL) {
		mtr->end_lsn = log_reserve_and_write_fast(
			first_data, dyn_block_get_used(mlog),
			&mtr->start_lsn);
		if (mtr->end_lsn) {

			/* Success. We have the log mutex.
			Add pages to flush list and exit */
			goto func_exit;
		}
	} else {
		mutex_enter(&log_sys->mutex);
	}

	data_size = dyn_array_get_data_size(mlog);

	/* Open the database log for log_write_low */
	mtr->start_lsn = log_open(data_size);

	if (mtr->log_mode == MTR_LOG_ALL) {

		block = mlog;

		while (block != NULL) {
			log_write_low(dyn_block_get_data(block),
				      dyn_block_get_used(block));
			block = dyn_array_get_next_block(mlog, block);
		}
	} else {
		ut_ad(mtr->log_mode == MTR_LOG_NONE);
		/* Do nothing */
	}

	mtr->end_lsn = log_close();

func_exit:

	/* No need to acquire log_flush_order_mutex if this mtr has
	not dirtied a clean page. log_flush_order_mutex is used to
	ensure ordered insertions in the flush_list. We need to
	insert in the flush_list iff the page in question was clean
	before modifications. */
	if (mtr->made_dirty) {
		log_flush_order_mutex_enter();
	}

	/* It is now safe to release the log mutex because the
	flush_order mutex will ensure that we are the first one
	to insert into the flush list. */
	log_release();

	if (mtr->modifications) {
		mtr_memo_note_modifications(mtr);
	}

	if (mtr->made_dirty) {
		log_flush_order_mutex_exit();
	}
}
#endif /* !UNIV_HOTBACKUP */

/***************************************************************//**
Commits a mini-transaction. */
UNIV_INTERN
void
mtr_commit(
/*=======*/
	mtr_t*	mtr)	/*!< in: mini-transaction */
{
	ut_ad(mtr);
	ut_ad(mtr->magic_n == MTR_MAGIC_N);
	ut_ad(mtr->state == MTR_ACTIVE);
	ut_ad(!mtr->inside_ibuf);
	ut_d(mtr->state = MTR_COMMITTING);

#ifndef UNIV_HOTBACKUP
	/* This is a dirty read, for debugging. */
	ut_ad(!recv_no_log_write);

	if (mtr->modifications && mtr->n_log_recs) {
		mtr_log_reserve_and_write(mtr);
	}

	mtr_memo_pop_all(mtr);
#endif /* !UNIV_HOTBACKUP */

	dyn_array_free(&(mtr->memo));
	dyn_array_free(&(mtr->log));
#ifdef UNIV_DEBUG_VALGRIND
	/* Declare everything uninitialized except
	mtr->start_lsn, mtr->end_lsn and mtr->state. */
	{
		ib_uint64_t	start_lsn	= mtr->start_lsn;
		ib_uint64_t	end_lsn		= mtr->end_lsn;
		UNIV_MEM_INVALID(mtr, sizeof *mtr);
		mtr->start_lsn = start_lsn;
		mtr->end_lsn = end_lsn;
	}
#endif /* UNIV_DEBUG_VALGRIND */
	ut_d(mtr->state = MTR_COMMITTED);
}

#ifndef UNIV_HOTBACKUP
/***************************************************//**
Releases an object in the memo stack. */
UNIV_INTERN
void
mtr_memo_release(
/*=============*/
	mtr_t*	mtr,	/*!< in/out: mini-transaction */
	void*	object,	/*!< in: object */
	ulint	type)	/*!< in: object type: MTR_MEMO_S_LOCK, ... */
{
	const dyn_block_t*	block;

	ut_ad(mtr->magic_n == MTR_MAGIC_N);
	ut_ad(mtr->state == MTR_ACTIVE);
	/* We cannot release a page that has been written to in the
	middle of a mini-transaction. */
	ut_ad(!mtr->modifications || type != MTR_MEMO_PAGE_X_FIX);

	for (block = dyn_array_get_last_block(&mtr->memo);
	     block;
	     block = dyn_array_get_prev_block(&mtr->memo, block)) {
		const mtr_memo_slot_t*	start
			= (mtr_memo_slot_t*) dyn_block_get_data(block);
		mtr_memo_slot_t*	slot
			= (mtr_memo_slot_t*) (dyn_block_get_data(block)
					      + dyn_block_get_used(block));

		ut_ad(!(dyn_block_get_used(block) % sizeof(mtr_memo_slot_t)));

		while (slot-- != start) {
			if (object == slot->object && type == slot->type) {
				mtr_memo_slot_release(mtr, slot);
				return;
			}
		}
	}
}
#endif /* !UNIV_HOTBACKUP */

/********************************************************//**
Reads 1 - 4 bytes from a file page buffered in the buffer pool.
@return	value read */
UNIV_INTERN
ulint
mtr_read_ulint(
/*===========*/
	const byte*	ptr,	/*!< in: pointer from where to read */
	ulint		type,	/*!< in: MLOG_1BYTE, MLOG_2BYTES, MLOG_4BYTES */
	mtr_t*		mtr __attribute__((unused)))
				/*!< in: mini-transaction handle */
{
	ut_ad(mtr->state == MTR_ACTIVE);
	ut_ad(mtr_memo_contains_page(mtr, ptr, MTR_MEMO_PAGE_S_FIX)
	      || mtr_memo_contains_page(mtr, ptr, MTR_MEMO_PAGE_X_FIX));
	if (type == MLOG_1BYTE) {
		return(mach_read_from_1(ptr));
	} else if (type == MLOG_2BYTES) {
		return(mach_read_from_2(ptr));
	} else {
		ut_ad(type == MLOG_4BYTES);
		return(mach_read_from_4(ptr));
	}
}

#ifdef UNIV_DEBUG
# ifndef UNIV_HOTBACKUP
/**********************************************************//**
Checks if memo contains the given page.
@return	TRUE if contains */
UNIV_INTERN
ibool
mtr_memo_contains_page(
/*===================*/
	mtr_t*		mtr,	/*!< in: mtr */
	const byte*	ptr,	/*!< in: pointer to buffer frame */
	ulint		type)	/*!< in: type of object */
{
	return(mtr_memo_contains(mtr, buf_block_align(ptr), type));
}

/*********************************************************//**
Prints info of an mtr handle. */
UNIV_INTERN
void
mtr_print(
/*======*/
	mtr_t*	mtr)	/*!< in: mtr */
{
	fprintf(stderr,
		"Mini-transaction handle: memo size %lu bytes"
		" log size %lu bytes\n",
		(ulong) dyn_array_get_data_size(&(mtr->memo)),
		(ulong) dyn_array_get_data_size(&(mtr->log)));
}
# endif /* !UNIV_HOTBACKUP */
#endif /* UNIV_DEBUG */
