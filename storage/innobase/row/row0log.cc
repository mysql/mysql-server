/*****************************************************************************

Copyright (c) 2011, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file row/row0log.cc
Modification log for online index creation

Created 2011-05-26 Marko Makela
*******************************************************/

#include "row0log.h"
#include "row0row.h"
#include "row0ins.h"
#include "row0upd.h"
#include "row0merge.h"
#include "data0data.h"

#ifdef UNIV_DEBUG
/** Write information about the applied record to the error log */
# define ROW_LOG_APPLY_PRINT
#endif /* UNIV_DEBUG */

#ifdef ROW_LOG_APPLY_PRINT
/** When set, write information about the applied record to the error log */
static bool row_log_apply_print;
#endif /* ROW_LOG_APPLY_PRINT */

/** Size of the modification log entry header, in bytes */
#define ROW_LOG_HEADER_SIZE (2 + DATA_TRX_ID_LEN)/*op, trx_id, extra_size*/

/** Log block for modifications during online index creation */
struct row_log_buf_struct {
	byte*		block;	/*!< file block buffer */
	mrec_buf_t	buf;	/*!< buffer for accessing a record
				that spans two blocks */
	ulint		blocks; /*!< current position in blocks */
	ulint		bytes;	/*!< current position within buf */
};

/** Log block for modifications during online index creation */
typedef struct row_log_buf_struct row_log_buf_t;

/** @brief Buffer for logging modifications during online index creation

All modifications to an index that is being created will be logged by
row_log_online_op() to this buffer.

When head.blocks == tail.blocks, the reader will access tail.block
directly. When also head.bytes == tail.bytes, both counts will be
reset to 0 and the file will be truncated. */
struct row_log_struct {
	int		fd;	/*!< file descriptor */
	mutex_t		mutex;	/*!< mutex protecting max_trx and tail */
	trx_id_t	max_trx;/*!< biggest observed trx_id in
				row_log_online_op();
				protected by mutex and index->lock S-latch,
				or by index->lock X-latch only */
	row_log_buf_t	tail;	/*!< writer context;
				protected by mutex and index->lock S-latch,
				or by index->lock X-latch only */
	row_log_buf_t	head;	/*!< reader context; protected by MDL only;
				modifiable by row_log_apply_ops() */
	ulint		size;	/*!< allocated size */
	btr_search_t*	search; /*!< old content of index->info.search */
};

/******************************************************//**
Logs an operation to a secondary index that is (or was) being created. */
UNIV_INTERN
void
row_log_online_op(
/*==============*/
	dict_index_t*	index,	/*!< in/out: index, S-latched */
	const dtuple_t* tuple,	/*!< in: index tuple */
	trx_id_t	trx_id, /*!< in: transaction ID or 0 if not known */
	enum row_op	op)	/*!< in: operation */
{
	byte*		b;
	ulint		extra_size;
	ulint		size;
	ulint		mrec_size;
	ulint		avail_size;
	row_log_t*	log;

	ut_ad(dtuple_validate(tuple));
	ut_ad(dtuple_get_n_fields(tuple) == dict_index_get_n_fields(index));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_SHARED)
	      == (op != ROW_OP_PURGE));
	ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_EX)
	      == (op == ROW_OP_PURGE));
#endif /* UNIV_SYNC_DEBUG */

#ifdef UNIV_DEBUG
	switch (op) {
	case ROW_OP_INSERT:
	case ROW_OP_DELETE_MARK:
	case ROW_OP_DELETE_UNMARK:
	case ROW_OP_DELETE_PURGE:
		ut_ad(trx_id);
		/* fall through */
	case ROW_OP_PURGE:
		goto op_ok;
	}
op_ok:
#endif /* UNIV_DEBUG */
	if (dict_index_is_corrupted(index)) {
		return;
	}

	ut_ad(dict_index_is_online_ddl(index));
	mutex_enter(&index->info.online_log->mutex);

	if (trx_id > index->info.online_log->max_trx) {
		index->info.online_log->max_trx = trx_id;
	}

	log = index->info.online_log;
	UNIV_MEM_INVALID(log->tail.buf, sizeof log->tail.buf);

	/* Compute the size of the record. This differs from
	row_merge_buf_encode(), because here we do not encode
	extra_size+1 (and reserve 0 as the end-of-chunk marker). */

	size = rec_get_converted_size_comp(
		index, REC_STATUS_ORDINARY,
		tuple->fields, tuple->n_fields, &extra_size);
	ut_ad(size >= extra_size);
	ut_ad(extra_size >= REC_N_NEW_EXTRA_BYTES);
	extra_size -= REC_N_NEW_EXTRA_BYTES;
	size -= REC_N_NEW_EXTRA_BYTES;
	mrec_size = size + ROW_LOG_HEADER_SIZE + (extra_size >= 0x80);

	ut_ad(size <= sizeof log->tail.buf);
	ut_ad(log->tail.bytes < srv_sort_buf_size);
	avail_size = srv_sort_buf_size - log->tail.bytes;

	if (mrec_size > avail_size) {
		b = log->tail.buf;
	} else {
		b = log->tail.block + log->tail.bytes;
	}

	*b++ = op;
	trx_write_trx_id(b, trx_id);
	b += DATA_TRX_ID_LEN;

	if (extra_size < 0x80) {
		*b++ = (byte) extra_size;
	} else {
		ut_ad(extra_size < 0x8000);
		*b++ = (byte) (0x80 | (extra_size >> 8));
		*b++ = (byte) extra_size;
	}

	rec_convert_dtuple_to_rec_comp(
		b + extra_size, 0, index,
		REC_STATUS_ORDINARY, tuple->fields, tuple->n_fields);
	b += size;

	if (mrec_size >= avail_size) {
		const os_offset_t	byte_offset
			= (os_offset_t) log->tail.blocks
			* srv_sort_buf_size;
		ibool			ret;

		if (byte_offset + srv_sort_buf_size >= srv_online_max_size) {
			goto write_failed;
		}

		if (mrec_size == avail_size) {
			ut_ad(b == &log->tail.block[srv_sort_buf_size]);
		} else {
			ut_ad(b == log->tail.buf + mrec_size);
			memcpy(log->tail.block + log->tail.bytes,
			       log->tail.buf, avail_size);
		}
		UNIV_MEM_ASSERT_RW(log->tail.block, srv_sort_buf_size);
		ret = os_file_write(
			"(modification log)",
			OS_FILE_FROM_FD(index->info.online_log->fd),
			log->tail.block, byte_offset, srv_sort_buf_size);
		log->tail.blocks++;
		if (!ret) {
write_failed:
			/* We set the flag directly instead of invoking
			dict_set_corrupted_index_cache_only(index) here,
			because the index is not "public" yet. */
			index->type |= DICT_CORRUPT;
		}
		UNIV_MEM_INVALID(log->tail.block, srv_sort_buf_size);
		memcpy(log->tail.block, log->tail.buf + avail_size,
		       mrec_size - avail_size);
		log->tail.bytes = mrec_size - avail_size;
	} else {
		log->tail.bytes += mrec_size;
		ut_ad(b == log->tail.block + log->tail.bytes);
	}

	UNIV_MEM_INVALID(log->tail.buf, sizeof log->tail.buf);
	mutex_exit(&index->info.online_log->mutex);
}

/******************************************************//**
Allocate the row log for an index and flag the index
for online creation.
@retval true if success, false if not */
UNIV_INTERN
bool
row_log_allocate(
/*=============*/
	dict_index_t*	index)	/*!< in/out: index */
{
	byte*		buf;
	row_log_t*	log;
	ulint		size;

	ut_ad(!dict_index_is_online_ddl(index));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	size = 2 * srv_sort_buf_size + sizeof *log;
	buf = (byte*) os_mem_alloc_large(&size);
	if (!buf) {
		return(false);
	}

	log = (row_log_t*) &buf[2 * srv_sort_buf_size];
	log->size = size;
	log->fd = row_merge_file_create_low();
	mutex_create(index_online_log_key, &log->mutex,
		     SYNC_INDEX_ONLINE_LOG);
	log->max_trx = 0;
	log->head.block = buf;
	log->tail.block = buf + srv_sort_buf_size;
	log->tail.blocks = log->tail.bytes = 0;
	log->head.blocks = log->head.bytes = 0;
	log->search = index->info.search;
	dict_index_set_online_status(index, ONLINE_INDEX_CREATION);
	index->info.online_log = log;

	/* While we might be holding an exclusive data dictionary lock
	here, in row_log_free() we will not always be holding it. Use
	atomic operations in both cases. */
	MONITOR_ATOMIC_INC(MONITOR_ONLINE_CREATE_INDEX);

	return(true);
}

/******************************************************//**
Free the row log for an index that was being created online. */
static
void
row_log_free_low(
/*=============*/
	row_log_t*	log)	/*!< in,own: row log */
{
	MONITOR_ATOMIC_DEC(MONITOR_ONLINE_CREATE_INDEX);

	row_merge_file_destroy_low(log->fd);
	mutex_free(&log->mutex);
	os_mem_free_large(log->head.block, log->size);
}

/******************************************************//**
Free the row log for an index on which online creation was aborted. */
UNIV_INTERN
void
row_log_free(
/*=========*/
	dict_index_t*	index)	/*!< in/out: index (x-latched) */
{
	row_log_t*	log;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	dict_index_set_online_status(index, ONLINE_INDEX_ABORTED);
	log = index->info.online_log;
	index->info.search = log->search;
	row_log_free_low(log);
}

/******************************************************//**
Get the latest transaction ID that has invoked row_log_online_op()
during online creation.
@return latest transaction ID, or 0 if nothing was logged */
UNIV_INTERN
trx_id_t
row_log_get_max_trx(
/*================*/
	dict_index_t*	index)	/*!< in: index, must be locked */
{
	ut_ad(dict_index_get_online_status(index) == ONLINE_INDEX_CREATION);
#ifdef UNIV_SYNC_DEBUG
	ut_ad((rw_lock_own(dict_index_get_lock(index), RW_LOCK_SHARED)
	       && mutex_own(&index->info.online_log->mutex))
	      || rw_lock_own(dict_index_get_lock(index), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	return(index->info.online_log->max_trx);
}

/******************************************************//**
Applies an operation to a secondary index that was being created. */
static __attribute__((nonnull))
void
row_log_apply_op_low(
/*=================*/
	dict_index_t*	index,		/*!< in/out: index */
	row_merge_dup_t*dup,		/*!< in/out: for reporting
					duplicate key errors */
	ulint*		error,		/*!< out: DB_SUCCESS or error code */
	mem_heap_t*	heap,		/*!< in/out: memory heap for
					allocating data tuples */
	ibool		has_index_lock, /*!< in: TRUE if holding index->lock
					in exclusive mode */
	enum row_op	op,		/*!< in: operation being applied */
	trx_id_t	trx_id,		/*!< in: transaction identifier */
	const dtuple_t*	entry)		/*!< in: row */
{
	mtr_t		mtr;
	btr_cur_t	cursor;

	ut_ad(!dict_index_is_clust(index));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_EX)
	      == has_index_lock);
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(!dict_index_is_corrupted(index));
	ut_ad(trx_id || op == ROW_OP_PURGE);

	mtr_start(&mtr);

	/* We perform the pessimistic variant of the operations if we
	already hold index->lock exclusively. First, search the
	record. The operation may already have been performed,
	depending on when the row in the clustered index was
	scanned. */
	btr_cur_search_to_nth_level(index, 0, entry, PAGE_CUR_LE,
				    has_index_lock
				    ? BTR_MODIFY_TREE_APPLY_LOG
				    : BTR_MODIFY_LEAF_APPLY_LOG,
				    &cursor, 0, __FILE__, __LINE__,
				    &mtr);

	/* This test is somewhat similar to row_ins_must_modify_rec(),
	but not identical for unique secondary indexes. */
	if (cursor.low_match >= dict_index_get_n_unique(index)
	    && !page_rec_is_infimum(btr_cur_get_rec(&cursor))) {
		/* We have a matching record. */
		rec_t*		rec	= btr_cur_get_rec(&cursor);
		ulint		deleted	= rec_get_deleted_flag(
			rec, page_rec_is_comp(rec));
		const ulint*	offsets;
		upd_t*		update;
		big_rec_t*	big_rec;

		ut_ad(page_rec_is_user_rec(rec));

		offsets = rec_get_offsets(rec, index, NULL,
					  ULINT_UNDEFINED, &heap);
		update = row_upd_build_sec_rec_difference_binary(
			rec, index, offsets, entry, heap);

		switch (op) {
		case ROW_OP_PURGE:
			if (!deleted) {
				/** The record is not delete-marked.
				It should not be a byte-for-byte equal
				record. */
				ut_ad(update->n_fields > 0);
				goto func_exit;
			}
			/* fall through */
		case ROW_OP_DELETE_PURGE:
			if (update->n_fields > 0) {
				/* This was not byte-for-byte equal to
				the record. The record that we were
				interested in was apparently already
				purged. */
				goto func_exit;
			}

			if (btr_cur_optimistic_delete(
				    &cursor, BTR_CREATE_FLAG, &mtr)) {
				*error = DB_SUCCESS;
				break;
			}

			if (!has_index_lock) {
				/* This needs a pessimistic operation.
				Lock the index tree exclusively. */
				mtr_commit(&mtr);
				mtr_start(&mtr);
				btr_cur_search_to_nth_level(
					index, 0, entry, PAGE_CUR_LE,
					BTR_MODIFY_TREE_APPLY_LOG,
					&cursor, 0, __FILE__, __LINE__,
					&mtr);

				/* No other thread than the current one
				is allowed to modify the index tree.
				Thus, the record should still exist. */
				ut_ad(cursor.low_match
				      >= dict_index_get_n_fields(index));
				ut_ad(page_rec_is_user_rec(
					      btr_cur_get_rec(&cursor)));
			}

			/* As there are no externally stored fields in
			the record, the parameter rb_ctx = RB_NONE
			will be ignored. */
			ut_ad(!rec_offs_any_extern(offsets));

			btr_cur_pessimistic_delete(
				error, FALSE, &cursor,
				BTR_CREATE_FLAG, RB_NONE, &mtr);
			break;
		case ROW_OP_DELETE_MARK:
		case ROW_OP_DELETE_UNMARK:
update_the_rec:
			ut_ad(!(entry->info_bits & ~REC_INFO_DELETED_FLAG));

			if (update->n_fields == 0) {
				/* Update the delete-mark flag only. */
				*error = btr_cur_del_mark_set_sec_rec(
					BTR_NO_UNDO_LOG_FLAG
					| BTR_NO_LOCKING_FLAG
					| BTR_CREATE_FLAG,
					&cursor, op == ROW_OP_DELETE_MARK,
					NULL, &mtr);
				break;
			}

			/* No byte-for-byte equal record was found. */
			if (cursor.low_match
			    < dict_index_get_n_fields(index)) {
				if (!dict_index_is_unique(index)) {
					goto insert_the_rec;
				}

				/* Duplicate key found. Complain if
				the record was not delete-marked or we
				are trying to insert a non-matching
				delete-marked record. */
				if (!deleted || entry->info_bits) {
					row_merge_dup_report(
						dup, entry->fields);
					goto func_exit;
				}
			}

			update->info_bits =
				(rec_get_info_bits(rec, page_rec_is_comp(rec))
				 & ~REC_INFO_DELETED_FLAG)
				| entry->info_bits;

			if (!has_index_lock) {
				*error = btr_cur_optimistic_update(
					BTR_NO_UNDO_LOG_FLAG
					| BTR_NO_LOCKING_FLAG
					| BTR_CREATE_FLAG
					| BTR_KEEP_SYS_FLAG,
					&cursor, update, 0, NULL,
					trx_id, &mtr);

				if (*error != DB_FAIL) {
					break;
				}

				/* This needs a pessimistic operation.
				Lock the index tree exclusively. */
#ifdef UNIV_DEBUG
				ulint	low_match = cursor.low_match;
#endif /* UNIV_DEBUG */

				mtr_commit(&mtr);
				mtr_start(&mtr);
				btr_cur_search_to_nth_level(
					index, 0, entry, PAGE_CUR_LE,
					BTR_MODIFY_TREE_APPLY_LOG,
					&cursor, 0,
					__FILE__, __LINE__, &mtr);
				/* No other thread than the
				current one is allowed to
				modify the index tree. Thus,
				the record should still exist. */
				ut_ad(low_match == cursor.low_match);
			}

			*error = btr_cur_pessimistic_update(
				BTR_NO_UNDO_LOG_FLAG
				| BTR_NO_LOCKING_FLAG
				| BTR_CREATE_FLAG
				| BTR_KEEP_SYS_FLAG,
				&cursor, &heap, &big_rec,
				update, 0, NULL, trx_id, &mtr);
			ut_ad(!big_rec);
			break;

		case ROW_OP_INSERT:
			/* If the matching record is delete-marked,
			perform the insert by updating the record. */
			if (deleted) {
				goto update_the_rec;
			}

			if (update->n_fields > 0
			    && cursor.low_match
			    < dict_index_get_n_fields(index)) {
				/* Duplicate key error */
				ut_ad(dict_index_is_unique(index));
				row_merge_dup_report(dup, entry->fields);
			}

			goto func_exit;
		}
	} else {
		switch (op) {
			rec_t*		rec;
			big_rec_t*	big_rec;
		case ROW_OP_DELETE_PURGE:
		case ROW_OP_PURGE:
			/* The record was apparently purged already when
			row_merge_read_clustered_index() got that far. */
			goto func_exit;
		case ROW_OP_DELETE_MARK:
		case ROW_OP_DELETE_UNMARK:
			/* The record was already delete-marked and
			possibly purged. Insert it. */
		case ROW_OP_INSERT:
insert_the_rec:
			/* Insert the record */
			if (!has_index_lock) {
				*error = btr_cur_optimistic_insert(
					BTR_NO_UNDO_LOG_FLAG
					| BTR_NO_LOCKING_FLAG
					| BTR_CREATE_FLAG,
					&cursor, const_cast<dtuple_t*>(entry),
					&rec, &big_rec,
					0, NULL, &mtr);
				ut_ad(!big_rec);
				if (*error != DB_FAIL) {
					break;
				}
				/* This needs a pessimistic operation.
				Lock the index tree exclusively. */
				mtr_commit(&mtr);
				mtr_start(&mtr);
				btr_cur_search_to_nth_level(
					index, 0, entry, PAGE_CUR_LE,
					BTR_MODIFY_TREE_APPLY_LOG,
					&cursor, 0,
					__FILE__, __LINE__, &mtr);
				/* We already determined that the
				record did not exist. No other thread
				than the current one is allowed to
				modify the index tree. Thus, the
				record should still not exist. */
			}

			*error = btr_cur_pessimistic_insert(
				BTR_NO_UNDO_LOG_FLAG
				| BTR_NO_LOCKING_FLAG
				| BTR_CREATE_FLAG,
				&cursor, const_cast<dtuple_t*>(entry),
				&rec, &big_rec,
				0, NULL, &mtr);
			ut_ad(!big_rec);
			break;
		}
	}

	if (*error == DB_SUCCESS && trx_id) {
		page_update_max_trx_id(btr_cur_get_block(&cursor),
				       btr_cur_get_page_zip(&cursor),
				       trx_id, &mtr);
	}

func_exit:
	mtr_commit(&mtr);
	mem_heap_empty(heap);
}

/******************************************************//**
Applies an operation to a secondary index that was being created.
@return NULL on failure (mrec corruption) or when out of data;
pointer to next record on success */
static __attribute__((nonnull, warn_unused_result))
const mrec_t*
row_log_apply_op(
/*=============*/
	dict_index_t*	index,		/*!< in/out: index */
	row_merge_dup_t*dup,		/*!< in/out: for reporting
					duplicate key errors */
	ulint*		error,		/*!< out: DB_SUCCESS or error code */
	mem_heap_t*	heap,		/*!< in/out: memory heap for
					allocating data tuples */
	ibool		has_index_lock, /*!< in: TRUE if holding index->lock
					in exclusive mode */
	const mrec_t*	mrec,		/*!< in: merge record */
	const mrec_t*	mrec_end,	/*!< in: end of buffer */
	ulint*		offsets)	/*!< in/out: work area for
					rec_init_offsets_comp_ordinary() */

{
	enum row_op	op;
	ulint		extra_size;
	ulint		data_size;
	ulint		n_ext;
	dtuple_t*	entry;
	trx_id_t	trx_id;

	/* Online index creation is only used for secondary indexes. */
	ut_ad(!dict_index_is_clust(index));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_EX)
	      == has_index_lock);
#endif /* UNIV_SYNC_DEBUG */

	if (dict_index_is_corrupted(index)) {
		*error = DB_INDEX_CORRUPT;
		return(NULL);
	}

	*error = DB_SUCCESS;

	if (mrec + ROW_LOG_HEADER_SIZE >= mrec_end) {
		return(NULL);
	}

	switch (*mrec) {
	case ROW_OP_INSERT:
	case ROW_OP_DELETE_MARK:
	case ROW_OP_DELETE_UNMARK:
	case ROW_OP_PURGE:
	case ROW_OP_DELETE_PURGE:
		op = static_cast<enum row_op>(*mrec++);
		break;
	default:
corrupted:
		ut_ad(0);
		*error = DB_CORRUPTION;
		return(NULL);
	}

	trx_id = trx_read_trx_id(mrec);
	mrec += DATA_TRX_ID_LEN;

	extra_size = *mrec++;

	ut_ad(mrec < mrec_end);

	if (extra_size >= 0x80) {
		/* Read another byte of extra_size. */

		extra_size = (extra_size & 0x7f) << 8;
		extra_size |= *mrec++;
	}

	mrec += extra_size;

	if (mrec > mrec_end) {
		return(NULL);
	}

	rec_init_offsets_comp_ordinary(mrec, 0, index, offsets);

	if (rec_offs_any_extern(offsets)) {
		/* There should never be any externally stored fields
		in a secondary index, which is what online index
		creation is used for. Therefore, the log file must be
		corrupted. */
		goto corrupted;
	}

	data_size = rec_offs_data_size(offsets);

	mrec += data_size;

	if (mrec > mrec_end) {
		return(NULL);
	}

	entry = row_rec_to_index_entry_low(
		mrec - data_size, index, offsets, &n_ext, heap);
	/* Online index creation is only implemented for secondary
	indexes, which never contain off-page columns. */
	ut_ad(n_ext == 0);
	entry->info_bits = (op == ROW_OP_DELETE_MARK)
		? REC_INFO_DELETED_FLAG
		: 0;
#ifdef ROW_LOG_APPLY_PRINT
	if (row_log_apply_print) {
		fprintf(stderr, "apply " IB_ID_FMT " " TRX_ID_FMT " %u %u ",
			index->id, trx_id,
			unsigned (op), unsigned (has_index_lock));
		for (const byte* m = mrec - data_size; m < mrec; m++) {
			fprintf(stderr, "%02x", *m);
		}
		putc('\n', stderr);
	}
#endif /* ROW_LOG_APPLY_PRINT */
	row_log_apply_op_low(index, dup, error, heap, has_index_lock,
			     op, trx_id, entry);
	return(mrec);
}

/******************************************************//**
Applies operations to a secondary index that was being created.
@return DB_SUCCESS, or error code on failure */
static __attribute__((nonnull))
ulint
row_log_apply_ops(
/*==============*/
	trx_t*		trx,	/*!< in: transaction (for checking if
				the operation was interrupted) */
	dict_index_t*	index,	/*!< in/out: index */
	row_merge_dup_t*dup)	/*!< in/out: for reporting duplicate key
				errors */
{
	ulint		error;
	const mrec_t*	mrec	= NULL;
	const mrec_t*	next_mrec;
	const mrec_t*	mrec_end= NULL; /* silence bogus warning */
	const mrec_t*	next_mrec_end;
	mem_heap_t*	heap;
	ulint*		offsets;
	ibool		has_index_lock;
	const ulint	i	= 1 + REC_OFFS_HEADER_SIZE
		+ dict_index_get_n_fields(index);

	ut_ad(dict_index_is_online_ddl(index));
	ut_ad(*index->name == TEMP_INDEX_PREFIX);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(index->info.online_log);
	UNIV_MEM_INVALID(&mrec_end, sizeof mrec_end);

	offsets = static_cast<ulint*>(ut_malloc(i * sizeof *offsets));
	offsets[0] = i;
	offsets[1] = dict_index_get_n_fields(index);

	heap = mem_heap_create(UNIV_PAGE_SIZE);
	has_index_lock = TRUE;

next_block:
	ut_ad(has_index_lock);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(index->info.online_log->head.bytes == 0);

	if (trx_is_interrupted(trx)) {
		goto interrupted;
	}

	if (dict_index_is_corrupted(index)) {
		error = DB_INDEX_CORRUPT;
		goto func_exit;
	}

	if (UNIV_UNLIKELY(index->info.online_log->head.blocks
			  > index->info.online_log->tail.blocks)) {
unexpected_eof:
		fprintf(stderr, "InnoDB: unexpected end of temporary file"
			" for index %s\n", index->name + 1);
corruption:
		error = DB_CORRUPTION;
		goto func_exit;
	}

	if (index->info.online_log->head.blocks
	    == index->info.online_log->tail.blocks) {
		if (index->info.online_log->head.blocks) {
#ifdef HAVE_FTRUNCATE
			/* Truncate the file in order to save space. */
			ftruncate(index->info.online_log->fd, 0);
#endif /* HAVE_FTRUNCATE */
			index->info.online_log->head.blocks
				= index->info.online_log->tail.blocks = 0;
		}

		next_mrec = index->info.online_log->tail.block;
		next_mrec_end = next_mrec + index->info.online_log->tail.bytes;

		if (next_mrec_end == next_mrec) {
			/* End of log reached. */
all_done:
			ut_ad(has_index_lock);
			ut_ad(index->info.online_log->head.blocks == 0);
			ut_ad(index->info.online_log->tail.blocks == 0);
			error = DB_SUCCESS;
			goto func_exit;
		}
	} else {
		os_offset_t	ofs;
		ibool		success;

		ofs = (os_offset_t) index->info.online_log->head.blocks
			* srv_sort_buf_size;

		if (has_index_lock) {
			has_index_lock = FALSE;
			rw_lock_x_unlock(dict_index_get_lock(index));
		}

		log_free_check();

		success = os_file_read_no_error_handling(
			OS_FILE_FROM_FD(index->info.online_log->fd),
			index->info.online_log->head.block, ofs,
			srv_sort_buf_size);

		if (!success) {
			fprintf(stderr, "InnoDB: unable to read temporary file"
				" for index %s\n", index->name + 1);
			goto corruption;
		}

#ifdef POSIX_FADV_DONTNEED
		/* Each block is read exactly once.  Free up the file cache. */
		posix_fadvise(index->info.online_log->fd,
			      ofs, srv_sort_buf_size, POSIX_FADV_DONTNEED);
#endif /* POSIX_FADV_DONTNEED */
#ifdef FALLOC_FL_PUNCH_HOLE
		/* Try to deallocate the space for the file on disk.
		This should work on ext4 on Linux 2.6.39 and later,
		and be ignored when the operation is unsupported. */
		fallocate(index->info.online_log->fd,
			  FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
			  ofs, srv_buf_size);
#endif /* FALLOC_FL_PUNCH_HOLE */

		next_mrec = index->info.online_log->head.block;
		next_mrec_end = next_mrec + srv_sort_buf_size;
	}

	if (mrec) {
		/* A partial record was read from the previous block.
		Copy the temporary buffer full, as we do not know the
		length of the record. Parse subsequent records from
		the bigger buffer index->info.online_log->head.block
		or index->info.online_log->tail.block. */

		ut_ad(mrec == index->info.online_log->head.buf);
		ut_ad(mrec_end > mrec);
		ut_ad(mrec_end < (&index->info.online_log->head.buf)[1]);

		memcpy((mrec_t*) mrec_end, next_mrec,
		       (&index->info.online_log->head.buf)[1] - mrec_end);
		mrec = row_log_apply_op(
			index, dup, &error, heap, has_index_lock,
			index->info.online_log->head.buf,
			(&index->info.online_log->head.buf)[1], offsets);
		if (error != DB_SUCCESS) {
			goto func_exit;
		} else if (UNIV_UNLIKELY(mrec == NULL)) {
			/* The record was not reassembled properly. */
			goto corruption;
		}
		/* The record was previously found out to be
		truncated. Now that the parse buffer was extended,
		it should proceed beyond the old end of the buffer. */
		ut_a(mrec > mrec_end);

		index->info.online_log->head.bytes = mrec - mrec_end;
		next_mrec += index->info.online_log->head.bytes;
	}

	ut_ad(next_mrec <= next_mrec_end);
	/* The following loop must not be parsing the temporary
	buffer, but head.block or tail.block. */

	/* mrec!=NULL means that the next record starts from the
	middle of the block */
	ut_ad((mrec == NULL) == (index->info.online_log->head.bytes == 0));

#ifdef UNIV_DEBUG
	if (next_mrec_end == index->info.online_log->head.block
	    + srv_sort_buf_size) {
		/* If tail.bytes == 0, next_mrec_end can also be at
		the end of tail.block. */
		if (index->info.online_log->tail.bytes == 0) {
			ut_ad(next_mrec == next_mrec_end);
			ut_ad(index->info.online_log->tail.blocks == 0);
			ut_ad(index->info.online_log->head.blocks == 0);
			ut_ad(index->info.online_log->head.bytes == 0);
		} else {
			ut_ad(next_mrec == index->info.online_log->head.block
			      + index->info.online_log->head.bytes);
			ut_ad(index->info.online_log->tail.blocks
			      > index->info.online_log->head.blocks);
		}
	} else if (next_mrec_end == index->info.online_log->tail.block
		   + index->info.online_log->tail.bytes) {
		ut_ad(next_mrec == index->info.online_log->tail.block
		      + index->info.online_log->head.bytes);
		ut_ad(index->info.online_log->tail.blocks == 0);
		ut_ad(index->info.online_log->head.blocks == 0);
		ut_ad(index->info.online_log->head.bytes
		      <= index->info.online_log->tail.bytes);
	} else {
		ut_error;
	}
#endif /* UNIV_DEBUG */

	mrec_end = next_mrec_end;

	while (!trx_is_interrupted(trx)) {
		mrec = next_mrec;
		ut_ad(mrec < mrec_end);

		if (!has_index_lock) {
			/* We are applying operations from a different
			block than the one that is being written to.
			Release and reacquire index->lock in order to
			allow other threads to concurrently buffer
			modifications. */
			ut_ad(mrec >= index->info.online_log->head.block);
			ut_ad(mrec_end == index->info.online_log->head.block
			      + srv_sort_buf_size);
			ut_ad(index->info.online_log->head.bytes
			      < srv_sort_buf_size);

			/* Take the opportunity to do a redo log
			checkpoint if needed. */
			log_free_check();
		} else {
			/* We are applying operations from the last block.
			Do not allow other threads to buffer anything,
			so that we can finally catch up and synchronize. */
			ut_ad(index->info.online_log->head.blocks == 0);
			ut_ad(index->info.online_log->tail.blocks == 0);
			ut_ad(mrec_end == index->info.online_log->tail.block
			      + index->info.online_log->tail.bytes);
			ut_ad(mrec >= index->info.online_log->tail.block);
		}

		next_mrec = row_log_apply_op(
			index, dup, &error, heap, has_index_lock,
			mrec, mrec_end, offsets);

		if (error != DB_SUCCESS) {
			goto func_exit;
		} else if (next_mrec == next_mrec_end) {
			/* The record happened to end on a block boundary.
			Do we have more blocks left? */
			if (has_index_lock) {
				/* The index will be locked while
				applying the last block. */
				goto all_done;
			}

			mrec = NULL;
process_next_block:
			rw_lock_x_lock(dict_index_get_lock(index));
			has_index_lock = TRUE;

			index->info.online_log->head.bytes = 0;
			index->info.online_log->head.blocks++;
			goto next_block;
		} else if (next_mrec != NULL) {
			ut_ad(next_mrec < next_mrec_end);
			index->info.online_log->head.bytes += next_mrec - mrec;
		} else if (has_index_lock) {
			/* When mrec is within tail.block, it should
			be a complete record, because we are holding
			index->lock and thus excluding the writer. */
			ut_ad(index->info.online_log->tail.blocks == 0);
			ut_ad(mrec_end == index->info.online_log->tail.block
			      + index->info.online_log->tail.bytes);
			ut_ad(0);
			goto unexpected_eof;
		} else {
			memcpy(index->info.online_log->head.buf, mrec,
			       mrec_end - mrec);
			mrec_end += index->info.online_log->head.buf - mrec;
			mrec = index->info.online_log->head.buf;
			goto process_next_block;
		}
	}

interrupted:
	error = DB_INTERRUPTED;
func_exit:
	if (!has_index_lock) {
		rw_lock_x_lock(dict_index_get_lock(index));
	}

	switch (error) {
	case DB_SUCCESS:
		break;
	case DB_INDEX_CORRUPT:
		if (((os_offset_t) index->info.online_log->tail.blocks + 1)
		    * srv_sort_buf_size >= srv_online_max_size) {
			/* The log file grew too big. */
			error = DB_ONLINE_LOG_TOO_BIG;
		}
		/* fall through */
	default:
		/* We set the flag directly instead of invoking
		dict_set_corrupted_index_cache_only(index) here,
		because the index is not "public" yet. */
		index->type |= DICT_CORRUPT;
	}

	mem_heap_free(heap);
	ut_free(offsets);
	return(error);
}

/******************************************************//**
Apply the row log to the index upon completing index creation.
@return DB_SUCCESS, or error code on failure */
UNIV_INTERN
ulint
row_log_apply(
/*==========*/
	trx_t*		trx,	/*!< in: transaction (for checking if
				the operation was interrupted) */
	dict_index_t*	index,	/*!< in/out: index */
	struct TABLE*	table)	/*!< in/out: MySQL table
				(for reporting duplicates) */
{
	ulint		error;
	row_log_t*	log;
	row_merge_dup_t dup;
	dup.index = index;
	dup.table = table;
	dup.n_dup = 0;

	ut_ad(dict_index_is_online_ddl(index));

	log_free_check();

	rw_lock_x_lock(dict_index_get_lock(index));

	error = row_log_apply_ops(trx, index, &dup);

	if (error != DB_SUCCESS || dup.n_dup) {
		/* We set the flag directly instead of invoking
		dict_set_corrupted_index_cache_only(index) here,
		because the index is not "public" yet. */
		index->type |= DICT_CORRUPT;
		index->table->drop_aborted = TRUE;

		if (error == DB_SUCCESS) {
			error = DB_DUPLICATE_KEY;
		}

		dict_index_set_online_status(index, ONLINE_INDEX_ABORTED);
	} else {
		dict_index_set_online_status(index, ONLINE_INDEX_COMPLETE);
	}

	log = index->info.online_log;
	index->info.search = log->search;
	/* We could remove the TEMP_INDEX_PREFIX and update the data
	dictionary to say that this index is complete, if we had
	access to the .frm file here.  If the server crashes before
	all requested indexes have been created, this completed index
	will be dropped. */
	rw_lock_x_unlock(dict_index_get_lock(index));

	row_log_free_low(log);

	return(error);
}
