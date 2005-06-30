/******************************************************
Mini-transaction buffer

(c) 1995 Innobase Oy

Created 11/26/1995 Heikki Tuuri
*******************************************************/

#include "mtr0mtr.h"

#ifdef UNIV_NONINL
#include "mtr0mtr.ic"
#endif

#include "buf0buf.h"
#include "page0types.h"
#include "mtr0log.h"
#include "log0log.h"

/*******************************************************************
Starts a mini-transaction and creates a mini-transaction handle 
and buffer in the memory buffer given by the caller. */

mtr_t*
mtr_start_noninline(
/*================*/
			/* out: mtr buffer which also acts as
			the mtr handle */
	mtr_t*	mtr)	/* in: memory buffer for the mtr buffer */
{
	return(mtr_start(mtr));
}

/*********************************************************************
Releases the item in the slot given. */
UNIV_INLINE
void
mtr_memo_slot_release(
/*==================*/
	mtr_t*			mtr,	/* in: mtr */
	mtr_memo_slot_t*	slot)	/* in: memo slot */
{
	void*	object;
	ulint	type;

	ut_ad(mtr && slot);

	object = slot->object;
	type = slot->type;

	if (UNIV_LIKELY(object != NULL)) {
		if (type <= MTR_MEMO_BUF_FIX) {
			buf_page_release((buf_block_t*)object, type, mtr);
		} else if (type == MTR_MEMO_S_LOCK) {
			rw_lock_s_unlock((rw_lock_t*)object);
#ifdef UNIV_DEBUG
		} else if (type == MTR_MEMO_X_LOCK) {
			rw_lock_x_unlock((rw_lock_t*)object);
		} else {
			ut_ad(type == MTR_MEMO_MODIFY);
			ut_ad(mtr_memo_contains(mtr, object,
						MTR_MEMO_PAGE_X_FIX));
#else
		} else {
			rw_lock_x_unlock((rw_lock_t*)object);
#endif
		}
	}

	slot->object = NULL;
}

/**************************************************************
Releases the mlocks and other objects stored in an mtr memo. They are released
in the order opposite to which they were pushed to the memo. NOTE! It is
essential that the x-rw-lock on a modified buffer page is not released before
buf_page_note_modification is called for that page! Otherwise, some thread
might race to modify it, and the flush list sort order on lsn would be
destroyed. */
UNIV_INLINE
void
mtr_memo_pop_all(
/*=============*/
	mtr_t*	mtr)	/* in: mtr */
{
	mtr_memo_slot_t* slot;
	dyn_array_t*	memo;
	ulint		offset;

	ut_ad(mtr);
	ut_ad(mtr->magic_n == MTR_MAGIC_N);
	ut_ad(mtr->state == MTR_COMMITTING); /* Currently only used in
								commit */
	memo = &(mtr->memo);

	offset = dyn_array_get_data_size(memo);
	
	while (offset > 0) {
		offset -= sizeof(mtr_memo_slot_t);
		slot = dyn_array_get_element(memo, offset);
		
		mtr_memo_slot_release(mtr, slot);
	}
}

/****************************************************************
Writes the contents of a mini-transaction log, if any, to the database log. */
static
void
mtr_log_reserve_and_write(
/*======================*/
	mtr_t*	mtr)	/* in: mtr */
{
	dyn_array_t*	mlog;
	dyn_block_t*	block;
	ulint		data_size;
	ibool		success;
	byte*		first_data;

	ut_ad(mtr);

	mlog = &(mtr->log);

	first_data = dyn_block_get_data(mlog);

	if (mtr->n_log_recs > 1) {
		mlog_catenate_ulint(mtr, MLOG_MULTI_REC_END, MLOG_1BYTE);
	} else {
		*first_data = (byte)((ulint)*first_data | MLOG_SINGLE_REC_FLAG);
	}
	
	if (mlog->heap == NULL) {
		mtr->end_lsn = log_reserve_and_write_fast(first_data,
						dyn_block_get_used(mlog),
						&(mtr->start_lsn), &success);
		if (success) {

			return;
		}
	}

	data_size = dyn_array_get_data_size(mlog);

	/* Open the database log for log_write_low */
	mtr->start_lsn = log_reserve_and_open(data_size); 

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
}

/*******************************************************************
Commits a mini-transaction. */

void
mtr_commit(
/*=======*/
	mtr_t*	mtr)	/* in: mini-transaction */
{
	ut_ad(mtr);
	ut_ad(mtr->magic_n == MTR_MAGIC_N);
	ut_ad(mtr->state == MTR_ACTIVE);
#ifdef UNIV_DEBUG
	mtr->state = MTR_COMMITTING;
#endif
	if (mtr->modifications) {
		mtr_log_reserve_and_write(mtr);
	}

	/* We first update the modification info to buffer pages, and only
	after that release the log mutex: this guarantees that when the log
	mutex is free, all buffer pages contain an up-to-date info of their
	modifications. This fact is used in making a checkpoint when we look
	at the oldest modification of any page in the buffer pool. It is also
	required when we insert modified buffer pages in to the flush list
	which must be sorted on oldest_modification. */
	
	mtr_memo_pop_all(mtr);

	if (mtr->modifications) {
		log_release();
	}

#ifdef UNIV_DEBUG
	mtr->state = MTR_COMMITTED;
#endif
	dyn_array_free(&(mtr->memo));
	dyn_array_free(&(mtr->log));
}		

/**************************************************************
Releases the latches stored in an mtr memo down to a savepoint.
NOTE! The mtr must not have made changes to buffer pages after the
savepoint, as these can be handled only by mtr_commit. */

void
mtr_rollback_to_savepoint(
/*======================*/
	mtr_t*	mtr,		/* in: mtr */
	ulint	savepoint)	/* in: savepoint */
{
	mtr_memo_slot_t* slot;
	dyn_array_t*	memo;
	ulint		offset;

	ut_ad(mtr);
	ut_ad(mtr->magic_n == MTR_MAGIC_N);
	ut_ad(mtr->state == MTR_ACTIVE);

	memo = &(mtr->memo);

	offset = dyn_array_get_data_size(memo);
	ut_ad(offset >= savepoint);
	
	while (offset > savepoint) {
		offset -= sizeof(mtr_memo_slot_t);

		slot = dyn_array_get_element(memo, offset);

		ut_ad(slot->type != MTR_MEMO_MODIFY);
		mtr_memo_slot_release(mtr, slot);
	}
}

/*******************************************************
Releases an object in the memo stack. */

void
mtr_memo_release(
/*=============*/
	mtr_t*	mtr,	/* in: mtr */
	void*	object,	/* in: object */
	ulint	type)	/* in: object type: MTR_MEMO_S_LOCK, ... */
{
	mtr_memo_slot_t* slot;
	dyn_array_t*	memo;
	ulint		offset;

	ut_ad(mtr);
	ut_ad(mtr->magic_n == MTR_MAGIC_N);
	ut_ad(mtr->state == MTR_ACTIVE);

	memo = &(mtr->memo);

 	offset = dyn_array_get_data_size(memo);

	while (offset > 0) {
		offset -= sizeof(mtr_memo_slot_t);

		slot = dyn_array_get_element(memo, offset);

		if ((object == slot->object) && (type == slot->type)) {

			mtr_memo_slot_release(mtr, slot);

			break;
		}
	}
}

/************************************************************
Reads 1 - 4 bytes from a file page buffered in the buffer pool. */

ulint
mtr_read_ulint(
/*===========*/
				/* out: value read */
	byte*		ptr,	/* in: pointer from where to read */
	ulint		type,	/* in: MLOG_1BYTE, MLOG_2BYTES, MLOG_4BYTES */
	mtr_t*		mtr __attribute__((unused)))
                                /* in: mini-transaction handle */
{
	ut_ad(mtr->state == MTR_ACTIVE);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(ptr), 
						MTR_MEMO_PAGE_S_FIX) ||
	      mtr_memo_contains(mtr, buf_block_align(ptr), 
						MTR_MEMO_PAGE_X_FIX));
	if (type == MLOG_1BYTE) {
		return(mach_read_from_1(ptr));
	} else if (type == MLOG_2BYTES) {
		return(mach_read_from_2(ptr));
	} else {
		ut_ad(type == MLOG_4BYTES);
		return(mach_read_from_4(ptr));
	}
}

/************************************************************
Reads 8 bytes from a file page buffered in the buffer pool. */

dulint
mtr_read_dulint(
/*===========*/
				/* out: value read */
	byte*		ptr,	/* in: pointer from where to read */
	mtr_t*		mtr __attribute__((unused)))
                                /* in: mini-transaction handle */
{
	ut_ad(mtr->state == MTR_ACTIVE);
	ut_ad(ptr && mtr);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(ptr), 
						MTR_MEMO_PAGE_S_FIX) ||
	      mtr_memo_contains(mtr, buf_block_align(ptr), 
						MTR_MEMO_PAGE_X_FIX));
	return(mach_read_from_8(ptr));
}

/*************************************************************
Prints info of an mtr handle. */

void
mtr_print(
/*======*/
	mtr_t*	mtr)	/* in: mtr */
{
	fprintf(stderr,
	"Mini-transaction handle: memo size %lu bytes log size %lu bytes\n",
		(ulong) dyn_array_get_data_size(&(mtr->memo)),
		(ulong) dyn_array_get_data_size(&(mtr->log)));
}
