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
Modification log for online index creation and online table rebuild

Created 2011-05-26 Marko Makela
*******************************************************/

#include "row0log.h"

#ifdef UNIV_NONINL
#include "row0log.ic"
#endif

#include "row0row.h"
#include "row0ins.h"
#include "row0upd.h"
#include "row0merge.h"
#include "row0ext.h"
#include "data0data.h"
#include "que0que.h"
#include "handler0alter.h"

#include<set>

/** Table row modification operations during online table rebuild.
Delete-marked records are not copied to the rebuilt table. */
enum row_tab_op {
	/** Insert a record */
	ROW_T_INSERT = 0x41,
	/** Update a record in place */
	ROW_T_UPDATE,
	/** Delete (purge) a record */
	ROW_T_DELETE
};

/** Index record modification operations during online index creation */
enum row_op {
	/** Insert a record */
	ROW_OP_INSERT = 0x61,
	/** Delete a record */
	ROW_OP_DELETE
};

#ifdef UNIV_DEBUG
/** Write information about the applied record to the error log */
# define ROW_LOG_APPLY_PRINT
#endif /* UNIV_DEBUG */

#ifdef ROW_LOG_APPLY_PRINT
/** When set, write information about the applied record to the error log */
static bool row_log_apply_print;
#endif /* ROW_LOG_APPLY_PRINT */

/** Size of the modification log entry header, in bytes */
#define ROW_LOG_HEADER_SIZE 2/*op, extra_size*/

/** Log block for modifications during online index creation */
struct row_log_buf_t {
	byte*		block;	/*!< file block buffer */
	mrec_buf_t	buf;	/*!< buffer for accessing a record
				that spans two blocks */
	ulint		blocks; /*!< current position in blocks */
	ulint		bytes;	/*!< current position within buf */
};

/** Set of transactions that rolled back inserts of BLOBs during
online table rebuild */
typedef std::set<trx_id_t> trx_id_set;

/** @brief Buffer for logging modifications during online index creation

All modifications to an index that is being created will be logged by
row_log_online_op() to this buffer.

All modifications to a table that is being rebuilt will be logged by
row_log_table_delete(), row_log_table_update(), row_log_table_insert()
to this buffer.

When head.blocks == tail.blocks, the reader will access tail.block
directly. When also head.bytes == tail.bytes, both counts will be
reset to 0 and the file will be truncated. */
struct row_log_t {
	int		fd;	/*!< file descriptor */
	ib_mutex_t	mutex;	/*!< mutex protecting trx_log, error,
				max_trx and tail */
	trx_id_set*	trx_rb;	/*!< set of transactions that rolled back
				inserts of BLOBs during online table rebuild;
				protected by mutex */
	dict_table_t*	table;	/*!< table that is being rebuilt,
				or NULL when this is a secondary
				index that is being created online */
	bool		same_pk;/*!< whether the definition of the PRIMARY KEY
				has remained the same */
	const dtuple_t*	add_cols;
				/*!< default values of added columns, or NULL */
	const ulint*	col_map;/*!< mapping of old column numbers to
				new ones, or NULL if !table */
	dberr_t		error;	/*!< error that occurred during online
				table rebuild */
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
};

/******************************************************//**
Logs an operation to a secondary index that is (or was) being created. */
UNIV_INTERN
void
row_log_online_op(
/*==============*/
	dict_index_t*	index,	/*!< in/out: index, S or X latched */
	const dtuple_t* tuple,	/*!< in: index tuple */
	trx_id_t	trx_id)	/*!< in: transaction ID for insert,
				or 0 for delete */
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
	      || rw_lock_own(dict_index_get_lock(index), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	if (dict_index_is_corrupted(index)) {
		return;
	}

	ut_ad(dict_index_is_online_ddl(index));

	/* Compute the size of the record. This differs from
	row_merge_buf_encode(), because here we do not encode
	extra_size+1 (and reserve 0 as the end-of-chunk marker). */

	size = rec_get_converted_size_temp(
		index, tuple->fields, tuple->n_fields, &extra_size);
	ut_ad(size >= extra_size);
	ut_ad(size <= sizeof log->tail.buf);

	mrec_size = ROW_LOG_HEADER_SIZE
		+ (extra_size >= 0x80) + size
		+ (trx_id ? DATA_TRX_ID_LEN : 0);

	log = index->online_log;
	mutex_enter(&log->mutex);

	if (trx_id > log->max_trx) {
		log->max_trx = trx_id;
	}

	UNIV_MEM_INVALID(log->tail.buf, sizeof log->tail.buf);

	ut_ad(log->tail.bytes < srv_sort_buf_size);
	avail_size = srv_sort_buf_size - log->tail.bytes;

	if (mrec_size > avail_size) {
		b = log->tail.buf;
	} else {
		b = log->tail.block + log->tail.bytes;
	}

	if (trx_id != 0) {
		*b++ = ROW_OP_INSERT;
		trx_write_trx_id(b, trx_id);
		b += DATA_TRX_ID_LEN;
	} else {
		*b++ = ROW_OP_DELETE;
	}

	if (extra_size < 0x80) {
		*b++ = (byte) extra_size;
	} else {
		ut_ad(extra_size < 0x8000);
		*b++ = (byte) (0x80 | (extra_size >> 8));
		*b++ = (byte) extra_size;
	}

	rec_convert_dtuple_to_temp(
		b + extra_size, index, tuple->fields, tuple->n_fields);
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
			OS_FILE_FROM_FD(log->fd),
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
	mutex_exit(&log->mutex);
}

/******************************************************//**
Gets the error status of the online index rebuild log.
@return DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
row_log_table_get_error(
/*====================*/
	const dict_index_t*	index)	/*!< in: clustered index of a table
					that is being rebuilt online */
{
	ut_ad(dict_index_is_clust(index));
	ut_ad(dict_index_is_online_ddl(index));
	return(index->online_log->error);
}

/******************************************************//**
Starts logging an operation to a table that is being rebuilt.
@return pointer to log, or NULL if no logging is necessary */
static __attribute__((nonnull, warn_unused_result))
byte*
row_log_table_open(
/*===============*/
	row_log_t*	log,	/*!< in/out: online rebuild log */
	ulint		size,	/*!< in: size of log record */
	ulint*		avail)	/*!< out: available size for log record */
{
	mutex_enter(&log->mutex);

	UNIV_MEM_INVALID(log->tail.buf, sizeof log->tail.buf);

	if (log->error != DB_SUCCESS) {
		mutex_exit(&log->mutex);
		return(NULL);
	}

	ut_ad(log->tail.bytes < srv_sort_buf_size);
	*avail = srv_sort_buf_size - log->tail.bytes;

	if (size > *avail) {
		return(log->tail.buf);
	} else {
		return(log->tail.block + log->tail.bytes);
	}
}

/******************************************************//**
Stops logging an operation to a table that is being rebuilt. */
static __attribute__((nonnull))
void
row_log_table_close_func(
/*=====================*/
	row_log_t*	log,	/*!< in/out: online rebuild log */
#ifdef UNIV_DEBUG
	const byte*	b,	/*!< in: end of log record */
#endif /* UNIV_DEBUG */
	ulint		size,	/*!< in: size of log record */
	ulint		avail)	/*!< in: available size for log record */
{
	ut_ad(mutex_own(&log->mutex));

	if (size >= avail) {
		const os_offset_t	byte_offset
			= (os_offset_t) log->tail.blocks
			* srv_sort_buf_size;
		ibool			ret;

		if (byte_offset + srv_sort_buf_size >= srv_online_max_size) {
			goto write_failed;
		}

		if (size == avail) {
			ut_ad(b == &log->tail.block[srv_sort_buf_size]);
		} else {
			ut_ad(b == log->tail.buf + size);
			memcpy(log->tail.block + log->tail.bytes,
			       log->tail.buf, avail);
		}
		UNIV_MEM_ASSERT_RW(log->tail.block, srv_sort_buf_size);
		ret = os_file_write(
			"(modification log)",
			OS_FILE_FROM_FD(log->fd),
			log->tail.block, byte_offset, srv_sort_buf_size);
		log->tail.blocks++;
		if (!ret) {
write_failed:
			log->error = DB_ONLINE_LOG_TOO_BIG;
		}
		UNIV_MEM_INVALID(log->tail.block, srv_sort_buf_size);
		memcpy(log->tail.block, log->tail.buf + avail, size - avail);
		log->tail.bytes = size - avail;
	} else {
		log->tail.bytes += size;
		ut_ad(b == log->tail.block + log->tail.bytes);
	}

	UNIV_MEM_INVALID(log->tail.buf, sizeof log->tail.buf);
	mutex_exit(&log->mutex);
}

#ifdef UNIV_DEBUG
# define row_log_table_close(log, b, size, avail)	\
	row_log_table_close_func(log, b, size, avail)
#else /* UNIV_DEBUG */
# define row_log_table_close(log, b, size, avail)	\
	row_log_table_close_func(log, size, avail)
#endif /* UNIV_DEBUG */

/******************************************************//**
Logs a delete operation to a table that is being rebuilt.
This will be merged in row_log_table_apply_delete(). */
UNIV_INTERN
void
row_log_table_delete(
/*=================*/
	const rec_t*	rec,	/*!< in: clustered index leaf page record,
				page X-latched */
	dict_index_t*	index,	/*!< in/out: clustered index, S-latched
				or X-latched */
	const ulint*	offsets,/*!< in: rec_get_offsets(rec,index) */
	trx_id_t	trx_id)	/*!< in: DB_TRX_ID of the record before
				it was deleted */
{
	ulint		old_pk_extra_size;
	ulint		old_pk_size;
	ulint		ext_size = 0;
	ulint		mrec_size;
	ulint		avail_size;
	mem_heap_t*	heap		= NULL;
	const dtuple_t*	old_pk;
	row_ext_t*	ext;

	ut_ad(dict_index_is_clust(index));
	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_ad(rec_offs_n_fields(offsets) == dict_index_get_n_fields(index));
	ut_ad(rec_offs_size(offsets) <= sizeof index->online_log->tail.buf);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&index->lock, RW_LOCK_SHARED)
	      || rw_lock_own(&index->lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	if (dict_index_is_corrupted(index)
	    || !dict_index_is_online_ddl(index)
	    || index->online_log->error != DB_SUCCESS) {
		return;
	}

	dict_table_t* new_table = index->online_log->table;
	dict_index_t* new_index = dict_table_get_first_index(new_table);

	ut_ad(dict_index_is_clust(new_index));
	ut_ad(!dict_index_is_online_ddl(new_index));

	/* Create the tuple PRIMARY KEY, DB_TRX_ID in the new_table. */
	if (index->online_log->same_pk) {
		byte*		db_trx_id;
		dtuple_t*	tuple;
		ut_ad(new_index->n_uniq == index->n_uniq);

		/* The PRIMARY KEY and DB_TRX_ID are in the first
		fields of the record. */
		heap = mem_heap_create(
			DATA_TRX_ID_LEN
			+ DTUPLE_EST_ALLOC(new_index->n_uniq + 1));
		old_pk = tuple = dtuple_create(heap, new_index->n_uniq + 1);
		dict_index_copy_types(tuple, new_index, tuple->n_fields);
		dtuple_set_n_fields_cmp(tuple, new_index->n_uniq);

		for (ulint i = 0; i < new_index->n_uniq; i++) {
			ulint		len;
			const void*	field	= rec_get_nth_field(
				rec, offsets, i, &len);
			dfield_t*	dfield	= dtuple_get_nth_field(
				tuple, i);
			ut_ad(len != UNIV_SQL_NULL);
			ut_ad(!rec_offs_nth_extern(offsets, i));
			dfield_set_data(dfield, field, len);
		}

		db_trx_id = static_cast<byte*>(
			mem_heap_alloc(heap, DATA_TRX_ID_LEN));
		trx_write_trx_id(db_trx_id, trx_id);

		dfield_set_data(dtuple_get_nth_field(tuple, new_index->n_uniq),
				db_trx_id, DATA_TRX_ID_LEN);
	} else {
		/* The PRIMARY KEY has changed. Translate the tuple. */
		dfield_t*	dfield;

		old_pk = row_log_table_get_pk(rec, index, offsets, &heap);

		if (!old_pk) {
			ut_ad(index->online_log->error != DB_SUCCESS);
			return;
		}

		/* Remove DB_ROLL_PTR. */
		ut_ad(dtuple_get_n_fields_cmp(old_pk)
		      == dict_index_get_n_unique(new_index));
		ut_ad(dtuple_get_n_fields(old_pk)
		      == dict_index_get_n_unique(new_index) + 2);
		const_cast<ulint&>(old_pk->n_fields)--;

		/* Overwrite DB_TRX_ID with the old trx_id. */
		dfield = dtuple_get_nth_field(old_pk, new_index->n_uniq);
		ut_ad(dfield_get_type(dfield)->mtype == DATA_SYS);
		ut_ad(dfield_get_type(dfield)->prtype
		      == (DATA_NOT_NULL | DATA_TRX_ID));
		ut_ad(dfield_get_len(dfield) == DATA_TRX_ID_LEN);
		trx_write_trx_id(static_cast<byte*>(dfield->data), trx_id);
	}

	ut_ad(dtuple_get_n_fields(old_pk) > 1);
	ut_ad(DATA_TRX_ID_LEN == dtuple_get_nth_field(
		      old_pk, old_pk->n_fields - 1)->len);
	old_pk_size = rec_get_converted_size_temp(
		new_index, old_pk->fields, old_pk->n_fields,
		&old_pk_extra_size);
	ut_ad(old_pk_extra_size < 0x100);

	mrec_size = 4 + old_pk_size;

	/* If the row is marked as rollback, we will need to
	log the enough prefix of the BLOB unless both the
	old and new table are in COMPACT or REDUNDANT format */
	if ((dict_table_get_format(index->table) >= UNIV_FORMAT_B
	     || dict_table_get_format(new_table) >= UNIV_FORMAT_B)
	    && row_log_table_is_rollback(index, trx_id)) {
		if (rec_offs_any_extern(offsets)) {
			/* Build a cache of those off-page column
			prefixes that are referenced by secondary
			indexes. It can be that none of the off-page
			columns are needed. */
			row_build(ROW_COPY_DATA, index, rec,
				  offsets, NULL, NULL, NULL, &ext, heap);
			if (ext) {
				/* Log the row_ext_t, ext->ext and ext->buf */
				ext_size = ext->n_ext * ext->max_len
					+ sizeof(*ext)
					+ ext->n_ext * sizeof(ulint)
					+ (ext->n_ext - 1) * sizeof ext->len;
				mrec_size += ext_size;
			}
		}
	}

	if (byte* b = row_log_table_open(index->online_log,
					 mrec_size, &avail_size)) {
		*b++ = ROW_T_DELETE;
		*b++ = static_cast<byte>(old_pk_extra_size);

		/* Log the size of external prefix we saved */
		mach_write_to_2(b, ext_size);
		b += 2;

		rec_convert_dtuple_to_temp(
			b + old_pk_extra_size, new_index,
			old_pk->fields, old_pk->n_fields);

		b += old_pk_size;

		if (ext_size) {
			ulint	cur_ext_size = sizeof(*ext)
				+ (ext->n_ext - 1) * sizeof ext->len;

			memcpy(b, ext, cur_ext_size);
			b += cur_ext_size;

			/* Check if we need to col_map to adjust the column
			number. If columns were added/removed/reordered,
			adjust the column number. */
			if (const ulint* col_map =
				index->online_log->col_map) {
				for (ulint i = 0; i < ext->n_ext; i++) {
					const_cast<ulint&>(ext->ext[i]) =
						col_map[ext->ext[i]];
				}
			}

			memcpy(b, ext->ext, ext->n_ext * sizeof(*ext->ext));
			b += ext->n_ext * sizeof(*ext->ext);

			ext_size -= cur_ext_size
				 + ext->n_ext * sizeof(*ext->ext);
			memcpy(b, ext->buf, ext_size);
			b += ext_size;
		}

		row_log_table_close(
			index->online_log, b, mrec_size, avail_size);
	}

	mem_heap_free(heap);
}

/******************************************************//**
Logs an insert or update to a table that is being rebuilt. */
static __attribute__((nonnull(1,2,3)))
void
row_log_table_low_redundant(
/*========================*/
	const rec_t*		rec,	/*!< in: clustered index leaf
					page record in ROW_FORMAT=REDUNDANT,
					page X-latched */
	dict_index_t*		index,	/*!< in/out: clustered index, S-latched
					or X-latched */
	const ulint*		offsets,/*!< in: rec_get_offsets(rec,index) */
	bool			insert,	/*!< in: true if insert,
					false if update */
	const dtuple_t*		old_pk,	/*!< in: old PRIMARY KEY value
					(if !insert and a PRIMARY KEY
					is being created) */
	const dict_index_t*	new_index)
					/*!< in: clustered index of the
					new table, not latched */
{
	ulint		old_pk_size;
	ulint		old_pk_extra_size;
	ulint		size;
	ulint		extra_size;
	ulint		mrec_size;
	ulint		avail_size;
	mem_heap_t*	heap		= NULL;
	dtuple_t*	tuple;

	ut_ad(!page_is_comp(page_align(rec)));
	ut_ad(dict_index_get_n_fields(index) == rec_get_n_fields_old(rec));

	heap = mem_heap_create(DTUPLE_EST_ALLOC(index->n_fields));
	tuple = dtuple_create(heap, index->n_fields);
	dict_index_copy_types(tuple, index, index->n_fields);
	dtuple_set_n_fields_cmp(tuple, dict_index_get_n_unique(index));

	if (rec_get_1byte_offs_flag(rec)) {
		for (ulint i = 0; i < index->n_fields; i++) {
			dfield_t*	dfield;
			ulint		len;
			const void*	field;

			dfield = dtuple_get_nth_field(tuple, i);
			field = rec_get_nth_field_old(rec, i, &len);

			dfield_set_data(dfield, field, len);
		}
	} else {
		for (ulint i = 0; i < index->n_fields; i++) {
			dfield_t*	dfield;
			ulint		len;
			const void*	field;

			dfield = dtuple_get_nth_field(tuple, i);
			field = rec_get_nth_field_old(rec, i, &len);

			dfield_set_data(dfield, field, len);

			if (rec_2_is_field_extern(rec, i)) {
				dfield_set_ext(dfield);
			}
		}
	}

	size = rec_get_converted_size_temp(
		index, tuple->fields, tuple->n_fields, &extra_size);

	mrec_size = ROW_LOG_HEADER_SIZE + size + (extra_size >= 0x80);

	if (insert || index->online_log->same_pk) {
		ut_ad(!old_pk);
		old_pk_extra_size = old_pk_size = 0;
	} else {
		ut_ad(old_pk);
		ut_ad(old_pk->n_fields == 2 + old_pk->n_fields_cmp);
		ut_ad(DATA_TRX_ID_LEN == dtuple_get_nth_field(
			      old_pk, old_pk->n_fields - 2)->len);
		ut_ad(DATA_ROLL_PTR_LEN == dtuple_get_nth_field(
			      old_pk, old_pk->n_fields - 1)->len);

		old_pk_size = rec_get_converted_size_temp(
			new_index, old_pk->fields, old_pk->n_fields,
			&old_pk_extra_size);
		ut_ad(old_pk_extra_size < 0x100);
		mrec_size += 1/*old_pk_extra_size*/ + old_pk_size;
	}

	if (byte* b = row_log_table_open(index->online_log,
					 mrec_size, &avail_size)) {
		*b++ = insert ? ROW_T_INSERT : ROW_T_UPDATE;

		if (old_pk_size) {
			*b++ = static_cast<byte>(old_pk_extra_size);

			rec_convert_dtuple_to_temp(
				b + old_pk_extra_size, new_index,
				old_pk->fields, old_pk->n_fields);
			b += old_pk_size;
		}

		if (extra_size < 0x80) {
			*b++ = static_cast<byte>(extra_size);
		} else {
			ut_ad(extra_size < 0x8000);
			*b++ = static_cast<byte>(0x80 | (extra_size >> 8));
			*b++ = static_cast<byte>(extra_size);
		}

		rec_convert_dtuple_to_temp(
			b + extra_size, index, tuple->fields, tuple->n_fields);
		b += size;

		row_log_table_close(
			index->online_log, b, mrec_size, avail_size);
	}

	mem_heap_free(heap);
}

/******************************************************//**
Logs an insert or update to a table that is being rebuilt. */
static __attribute__((nonnull(1,2,3)))
void
row_log_table_low(
/*==============*/
	const rec_t*	rec,	/*!< in: clustered index leaf page record,
				page X-latched */
	dict_index_t*	index,	/*!< in/out: clustered index, S-latched
				or X-latched */
	const ulint*	offsets,/*!< in: rec_get_offsets(rec,index) */
	bool		insert,	/*!< in: true if insert, false if update */
	const dtuple_t*	old_pk)	/*!< in: old PRIMARY KEY value (if !insert
				and a PRIMARY KEY is being created) */
{
	ulint			omit_size;
	ulint			old_pk_size;
	ulint			old_pk_extra_size;
	ulint			extra_size;
	ulint			mrec_size;
	ulint			avail_size;
	const dict_index_t*	new_index = dict_table_get_first_index(
		index->online_log->table);
	ut_ad(dict_index_is_clust(index));
	ut_ad(dict_index_is_clust(new_index));
	ut_ad(!dict_index_is_online_ddl(new_index));
	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_ad(rec_offs_n_fields(offsets) == dict_index_get_n_fields(index));
	ut_ad(rec_offs_size(offsets) <= sizeof index->online_log->tail.buf);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&index->lock, RW_LOCK_SHARED)
	      || rw_lock_own(&index->lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(fil_page_get_type(page_align(rec)) == FIL_PAGE_INDEX);
	ut_ad(page_is_leaf(page_align(rec)));
	ut_ad(!page_is_comp(page_align(rec)) == !rec_offs_comp(offsets));

	if (dict_index_is_corrupted(index)
	    || !dict_index_is_online_ddl(index)
	    || index->online_log->error != DB_SUCCESS) {
		return;
	}

	if (!rec_offs_comp(offsets)) {
		row_log_table_low_redundant(
			rec, index, offsets, insert, old_pk, new_index);
		return;
	}

	ut_ad(page_is_comp(page_align(rec)));
	ut_ad(rec_get_status(rec) == REC_STATUS_ORDINARY);

	omit_size = REC_N_NEW_EXTRA_BYTES;

	extra_size = rec_offs_extra_size(offsets) - omit_size;

	mrec_size = rec_offs_size(offsets) - omit_size
		+ ROW_LOG_HEADER_SIZE + (extra_size >= 0x80);

	if (insert || index->online_log->same_pk) {
		ut_ad(!old_pk);
		old_pk_extra_size = old_pk_size = 0;
	} else {
		ut_ad(old_pk);
		ut_ad(old_pk->n_fields == 2 + old_pk->n_fields_cmp);
		ut_ad(DATA_TRX_ID_LEN == dtuple_get_nth_field(
			      old_pk, old_pk->n_fields - 2)->len);
		ut_ad(DATA_ROLL_PTR_LEN == dtuple_get_nth_field(
			      old_pk, old_pk->n_fields - 1)->len);

		old_pk_size = rec_get_converted_size_temp(
			new_index, old_pk->fields, old_pk->n_fields,
			&old_pk_extra_size);
		ut_ad(old_pk_extra_size < 0x100);
		mrec_size += 1/*old_pk_extra_size*/ + old_pk_size;
	}

	if (byte* b = row_log_table_open(index->online_log,
					 mrec_size, &avail_size)) {
		*b++ = insert ? ROW_T_INSERT : ROW_T_UPDATE;

		if (old_pk_size) {
			*b++ = static_cast<byte>(old_pk_extra_size);

			rec_convert_dtuple_to_temp(
				b + old_pk_extra_size, new_index,
				old_pk->fields, old_pk->n_fields);
			b += old_pk_size;
		}

		if (extra_size < 0x80) {
			*b++ = static_cast<byte>(extra_size);
		} else {
			ut_ad(extra_size < 0x8000);
			*b++ = static_cast<byte>(0x80 | (extra_size >> 8));
			*b++ = static_cast<byte>(extra_size);
		}

		memcpy(b, rec - rec_offs_extra_size(offsets), extra_size);
		b += extra_size;
		memcpy(b, rec, rec_offs_data_size(offsets));
		b += rec_offs_data_size(offsets);

		row_log_table_close(
			index->online_log, b, mrec_size, avail_size);
	}
}

/******************************************************//**
Logs an update to a table that is being rebuilt.
This will be merged in row_log_table_apply_update(). */
UNIV_INTERN
void
row_log_table_update(
/*=================*/
	const rec_t*	rec,	/*!< in: clustered index leaf page record,
				page X-latched */
	dict_index_t*	index,	/*!< in/out: clustered index, S-latched
				or X-latched */
	const ulint*	offsets,/*!< in: rec_get_offsets(rec,index) */
	const dtuple_t*	old_pk)	/*!< in: row_log_table_get_pk()
				before the update */
{
	row_log_table_low(rec, index, offsets, false, old_pk);
}

/******************************************************//**
Constructs the old PRIMARY KEY and DB_TRX_ID,DB_ROLL_PTR
of a table that is being rebuilt.
@return tuple of PRIMARY KEY,DB_TRX_ID,DB_ROLL_PTR in the rebuilt table,
or NULL if the PRIMARY KEY definition does not change */
UNIV_INTERN
const dtuple_t*
row_log_table_get_pk(
/*=================*/
	const rec_t*	rec,	/*!< in: clustered index leaf page record,
				page X-latched */
	dict_index_t*	index,	/*!< in/out: clustered index, S-latched
				or X-latched */
	const ulint*	offsets,/*!< in: rec_get_offsets(rec,index) */
	mem_heap_t**	heap)	/*!< in/out: memory heap where allocated */
{
	dtuple_t*	tuple	= NULL;
	row_log_t*	log	= index->online_log;

	ut_ad(dict_index_is_clust(index));
	ut_ad(dict_index_is_online_ddl(index));
	ut_ad(!offsets || rec_offs_validate(rec, index, offsets));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&index->lock, RW_LOCK_SHARED)
	      || rw_lock_own(&index->lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	ut_ad(log);
	ut_ad(log->table);

	if (log->same_pk) {
		/* The PRIMARY KEY columns are unchanged. */
		return(NULL);
	}

	mutex_enter(&log->mutex);

	/* log->error is protected by log->mutex. */
	if (log->error == DB_SUCCESS) {
		dict_table_t*	new_table	= log->table;
		dict_index_t*	new_index
			= dict_table_get_first_index(new_table);
		const ulint	new_n_uniq
			= dict_index_get_n_unique(new_index);

		if (!*heap) {
			ulint	size = 0;

			if (!offsets) {
				size += (1 + REC_OFFS_HEADER_SIZE
					 + index->n_fields)
					* sizeof *offsets;
			}

			for (ulint i = 0; i < new_n_uniq; i++) {
				size += dict_col_get_min_size(
					dict_index_get_nth_col(new_index, i));
			}

			*heap = mem_heap_create(
				DTUPLE_EST_ALLOC(new_n_uniq + 2) + size);
		}

		if (!offsets) {
			offsets = rec_get_offsets(rec, index, NULL,
						  ULINT_UNDEFINED, heap);
		}

		tuple = dtuple_create(*heap, new_n_uniq + 2);
		dict_index_copy_types(tuple, new_index, tuple->n_fields);
		dtuple_set_n_fields_cmp(tuple, new_n_uniq);

		for (ulint new_i = 0; new_i < new_n_uniq; new_i++) {
			dict_field_t*		ifield;
			dfield_t*		dfield;
			const dict_col_t*	new_col;
			const dict_col_t*	col;
			ulint			col_no;
			ulint			i;
			ulint			len;
			const byte*		field;

			ifield = dict_index_get_nth_field(new_index, new_i);
			dfield = dtuple_get_nth_field(tuple, new_i);
			new_col = dict_field_get_col(ifield);
			col_no = new_col->ind;

			for (ulint old_i = 0; old_i < index->table->n_cols;
			     old_i++) {
				if (col_no == log->col_map[old_i]) {
					col_no = old_i;
					goto copy_col;
				}
			}

			/* No matching column was found in the old
			table, so this must be an added column.
			Copy the default value. */
			ut_ad(log->add_cols);
			dfield_copy(dfield,
				    dtuple_get_nth_field(
					    log->add_cols, col_no));
			continue;

copy_col:
			col = dict_table_get_nth_col(index->table, col_no);

			i = dict_col_get_clust_pos(col, index);

			if (i == ULINT_UNDEFINED) {
				ut_ad(0);
				log->error = DB_CORRUPTION;
				tuple = NULL;
				goto func_exit;
			}

			field = rec_get_nth_field(rec, offsets, i, &len);

			if (len == UNIV_SQL_NULL) {
				log->error = DB_INVALID_NULL;
				tuple = NULL;
				goto func_exit;
			}

			if (rec_offs_nth_extern(offsets, i)) {
				ulint		field_len = ifield->prefix_len;
				byte*		blob_field;
				const ulint	max_len =
					DICT_MAX_FIELD_LEN_BY_FORMAT(
						new_table);

				if (!field_len) {
					field_len = ifield->fixed_len;
					if (!field_len) {
						field_len = max_len + 1;
					}
				}

				blob_field = static_cast<byte*>(
					mem_heap_alloc(*heap, field_len));

				len = btr_copy_externally_stored_field_prefix(
					blob_field, field_len,
					dict_table_zip_size(index->table),
					field, len);
				if (len == max_len + 1) {
					log->error = DB_TOO_BIG_INDEX_COL;
					tuple = NULL;
					goto func_exit;
				}

				dfield_set_data(dfield, blob_field, len);
			} else {
				if (ifield->prefix_len
				    && ifield->prefix_len < len) {
					len = ifield->prefix_len;
				}

				dfield_set_data(
					dfield,
					mem_heap_dup(*heap, field, len), len);
			}
		}

		const byte* trx_roll = rec
			+ row_get_trx_id_offset(index, offsets);

		dfield_set_data(dtuple_get_nth_field(tuple, new_n_uniq),
				trx_roll, DATA_TRX_ID_LEN);
		dfield_set_data(dtuple_get_nth_field(tuple, new_n_uniq + 1),
				trx_roll + DATA_TRX_ID_LEN, DATA_ROLL_PTR_LEN);
	}

func_exit:
	mutex_exit(&log->mutex);
	return(tuple);
}

/******************************************************//**
Logs an insert to a table that is being rebuilt.
This will be merged in row_log_table_apply_insert(). */
UNIV_INTERN
void
row_log_table_insert(
/*=================*/
	const rec_t*	rec,	/*!< in: clustered index leaf page record,
				page X-latched */
	dict_index_t*	index,	/*!< in/out: clustered index, S-latched
				or X-latched */
	const ulint*	offsets)/*!< in: rec_get_offsets(rec,index) */
{
	row_log_table_low(rec, index, offsets, true, NULL);
}

/******************************************************//**
Notes that a transaction is being rolled back. */
UNIV_INTERN
void
row_log_table_rollback(
/*===================*/
	dict_index_t*	index,	/*!< in/out: clustered index */
	trx_id_t	trx_id)	/*!< in: transaction being rolled back */
{
	ut_ad(dict_index_is_clust(index));
#ifdef UNIV_DEBUG
	ibool	corrupt	= FALSE;
	ut_ad(trx_rw_is_active(trx_id, &corrupt));
	ut_ad(!corrupt);
#endif /* UNIV_DEBUG */

	/* Protect transitions of index->online_status and access to
	index->online_log. */
	rw_lock_s_lock(&index->lock);

	if (dict_index_is_online_ddl(index)) {
		ut_ad(index->online_log);
		ut_ad(index->online_log->table);
		mutex_enter(&index->online_log->mutex);
		trx_id_set*	trxs = index->online_log->trx_rb;

		if (!trxs) {
			index->online_log->trx_rb = trxs = new trx_id_set();
		}

		trxs->insert(trx_id);

		mutex_exit(&index->online_log->mutex);
	}

	rw_lock_s_unlock(&index->lock);
}

/******************************************************//**
Check if a transaction rollback has been initiated.
@return true if inserts of this transaction were rolled back */
UNIV_INTERN
bool
row_log_table_is_rollback(
/*======================*/
	const dict_index_t*	index,	/*!< in: clustered index */
	trx_id_t		trx_id)	/*!< in: transaction id */
{
	ut_ad(dict_index_is_clust(index));
	ut_ad(dict_index_is_online_ddl(index));
	ut_ad(index->online_log);

	if (const trx_id_set* trxs = index->online_log->trx_rb) {
		mutex_enter(&index->online_log->mutex);
		bool is_rollback = trxs->find(trx_id) != trxs->end();
		mutex_exit(&index->online_log->mutex);

		return(is_rollback);
	}

	return(false);
}

/******************************************************//**
Converts a log record to a table row.
@return converted row, or NULL if the conversion fails
or the transaction has been rolled back */
static __attribute__((nonnull, warn_unused_result))
const dtuple_t*
row_log_table_apply_convert_mrec(
/*=============================*/
	const mrec_t*		mrec,		/*!< in: merge record */
	dict_index_t*		index,		/*!< in: index of mrec */
	const ulint*		offsets,	/*!< in: offsets of mrec */
	const row_log_t*	log,		/*!< in: rebuild context */
	mem_heap_t*		heap,		/*!< in/out: memory heap */
	trx_id_t		trx_id,		/*!< in: DB_TRX_ID of mrec */
	dberr_t*		error)		/*!< out: DB_SUCCESS or
						reason of failure */
{
	dtuple_t*	row;

#ifdef UNIV_SYNC_DEBUG
	/* This prevents BLOBs from being freed, in case an insert
	transaction rollback starts after row_log_table_is_rollback(). */
	ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	if (row_log_table_is_rollback(index, trx_id)) {
		row = NULL;
		goto func_exit;
	}

	/* This is based on row_build(). */
	if (log->add_cols) {
		row = dtuple_copy(log->add_cols, heap);
		/* dict_table_copy_types() would set the fields to NULL */
		for (ulint i = 0; i < dict_table_get_n_cols(log->table); i++) {
			dict_col_copy_type(
				dict_table_get_nth_col(log->table, i),
				dfield_get_type(dtuple_get_nth_field(row, i)));
		}
	} else {
		row = dtuple_create(heap, dict_table_get_n_cols(log->table));
		dict_table_copy_types(row, log->table);
	}

	for (ulint i = 0; i < rec_offs_n_fields(offsets); i++) {
		const dict_field_t*	ind_field
			= dict_index_get_nth_field(index, i);

		if (ind_field->prefix_len) {
			/* Column prefixes can only occur in key
			fields, which cannot be stored externally. For
			a column prefix, there should also be the full
			field in the clustered index tuple. The row
			tuple comprises full fields, not prefixes. */
			ut_ad(!rec_offs_nth_extern(offsets, i));
			continue;
		}

		const dict_col_t*	col
			= dict_field_get_col(ind_field);
		ulint			col_no
			= log->col_map[dict_col_get_no(col)];

		if (col_no == ULINT_UNDEFINED) {
			/* dropped column */
			continue;
		}

		dfield_t*		dfield
			= dtuple_get_nth_field(row, col_no);
		ulint			len;
		const void*		data;

		if (rec_offs_nth_extern(offsets, i)) {
			ut_ad(rec_offs_any_extern(offsets));
			data = btr_rec_copy_externally_stored_field(
				mrec, offsets,
				dict_table_zip_size(index->table),
				i, &len, heap);
			ut_a(data);
		} else {
			data = rec_get_nth_field(mrec, offsets, i, &len);
		}

		dfield_set_data(dfield, data, len);

		/* See if any columns were changed to NULL or NOT NULL. */
		const dict_col_t*	new_col
			= dict_table_get_nth_col(log->table, col_no);
		ut_ad(new_col->mtype == col->mtype);

		/* Assert that prtype matches except for nullability. */
		ut_ad(!((new_col->prtype ^ col->prtype) & ~DATA_NOT_NULL));
		ut_ad(!((new_col->prtype ^ dfield_get_type(dfield)->prtype)
			& ~DATA_NOT_NULL));

		if (new_col->prtype == col->prtype) {
			continue;
		}

		if ((new_col->prtype & DATA_NOT_NULL)
		    && dfield_is_null(dfield)) {
			/* We got a NULL value for a NOT NULL column. */
			*error = DB_INVALID_NULL;
			return(NULL);
		}

		/* Adjust the DATA_NOT_NULL flag in the parsed row. */
		dfield_get_type(dfield)->prtype = new_col->prtype;

		ut_ad(dict_col_type_assert_equal(new_col,
						 dfield_get_type(dfield)));
	}

func_exit:
	*error = DB_SUCCESS;
	return(row);
}

/******************************************************//**
Replays an insert operation on a table that was rebuilt.
@return DB_SUCCESS or error code */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_log_table_apply_insert_low(
/*===========================*/
	que_thr_t*		thr,		/*!< in: query graph */
	const dtuple_t*		row,		/*!< in: table row
						in the old table definition */
	trx_id_t		trx_id,		/*!< in: trx_id of the row */
	mem_heap_t*		offsets_heap,	/*!< in/out: memory heap
						that can be emptied */
	mem_heap_t*		heap,		/*!< in/out: memory heap */
	row_merge_dup_t*	dup)		/*!< in/out: for reporting
						duplicate key errors */
{
	dberr_t		error;
	dtuple_t*	entry;
	const row_log_t*log	= dup->index->online_log;
	dict_index_t*	index	= dict_table_get_first_index(log->table);

	ut_ad(dtuple_validate(row));
	ut_ad(trx_id);

#ifdef ROW_LOG_APPLY_PRINT
	if (row_log_apply_print) {
		fprintf(stderr, "table apply insert "
			IB_ID_FMT " " IB_ID_FMT "\n",
			index->table->id, index->id);
		dtuple_print(stderr, row);
	}
#endif /* ROW_LOG_APPLY_PRINT */

	static const ulint	flags
		= (BTR_CREATE_FLAG
		   | BTR_NO_LOCKING_FLAG
		   | BTR_NO_UNDO_LOG_FLAG
		   | BTR_KEEP_SYS_FLAG);

	entry = row_build_index_entry(row, NULL, index, heap);

	error = row_ins_clust_index_entry_low(
		flags, BTR_MODIFY_TREE, index, index->n_uniq, entry, 0, thr);

	switch (error) {
	case DB_SUCCESS:
		break;
	case DB_SUCCESS_LOCKED_REC:
		/* The row had already been copied to the table. */
		return(DB_SUCCESS);
	default:
		return(error);
	}

	do {
		if (!(index = dict_table_get_next_index(index))) {
			break;
		}

		if (index->type & DICT_FTS) {
			continue;
		}

		entry = row_build_index_entry(row, NULL, index, heap);
		error = row_ins_sec_index_entry_low(
			flags, BTR_MODIFY_TREE,
			index, offsets_heap, heap, entry, trx_id, thr);
	} while (error == DB_SUCCESS);

	return(error);
}

/******************************************************//**
Replays an insert operation on a table that was rebuilt.
@return DB_SUCCESS or error code */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_log_table_apply_insert(
/*=======================*/
	que_thr_t*		thr,		/*!< in: query graph */
	const mrec_t*		mrec,		/*!< in: record to insert */
	const ulint*		offsets,	/*!< in: offsets of mrec */
	mem_heap_t*		offsets_heap,	/*!< in/out: memory heap
						that can be emptied */
	mem_heap_t*		heap,		/*!< in/out: memory heap */
	row_merge_dup_t*	dup,		/*!< in/out: for reporting
						duplicate key errors */
	trx_id_t		trx_id)		/*!< in: DB_TRX_ID of mrec */
{
	const row_log_t*log	= dup->index->online_log;
	dberr_t		error;
	const dtuple_t*	row	= row_log_table_apply_convert_mrec(
		mrec, dup->index, offsets, log, heap, trx_id, &error);

	ut_ad(error == DB_SUCCESS || !row);
	/* Handling of duplicate key error requires storing
	of offending key in a record buffer. */
	ut_ad(error != DB_DUPLICATE_KEY);

	if (error != DB_SUCCESS)
		return(error);

	if (row) {
		error = row_log_table_apply_insert_low(
			thr, row, trx_id, offsets_heap, heap, dup);
		if (error != DB_SUCCESS) {
			/* Report the erroneous row using the new
			version of the table. */
			innobase_row_to_mysql(dup->table, log->table, row);
		}
	}
	return(error);
}

/******************************************************//**
Deletes a record from a table that is being rebuilt.
@return DB_SUCCESS or error code */
static __attribute__((nonnull(1, 2, 4, 5), warn_unused_result))
dberr_t
row_log_table_apply_delete_low(
/*===========================*/
	btr_pcur_t*		pcur,		/*!< in/out: B-tree cursor,
						will be trashed */
	const ulint*		offsets,	/*!< in: offsets on pcur */
	const row_ext_t*	save_ext,	/*!< in: saved external field
						info, or NULL */
	mem_heap_t*		heap,		/*!< in/out: memory heap */
	mtr_t*			mtr)		/*!< in/out: mini-transaction,
						will be committed */
{
	dberr_t		error;
	row_ext_t*	ext;
	dtuple_t*	row;
	dict_index_t*	index	= btr_pcur_get_btr_cur(pcur)->index;

	ut_ad(dict_index_is_clust(index));

#ifdef ROW_LOG_APPLY_PRINT
	if (row_log_apply_print) {
		fprintf(stderr, "table apply delete "
			IB_ID_FMT " " IB_ID_FMT "\n",
			index->table->id, index->id);
		rec_print_new(stderr, btr_pcur_get_rec(pcur), offsets);
	}
#endif /* ROW_LOG_APPLY_PRINT */
	if (dict_table_get_next_index(index)) {
		/* Build a row template for purging secondary index entries. */
		row = row_build(
			ROW_COPY_DATA, index, btr_pcur_get_rec(pcur),
			offsets, NULL, NULL, NULL,
			save_ext ? NULL : &ext, heap);
		if (!save_ext) {
			save_ext = ext;
		}
	} else {
		row = NULL;
	}

	btr_cur_pessimistic_delete(&error, FALSE, btr_pcur_get_btr_cur(pcur),
				   BTR_CREATE_FLAG, RB_NONE, mtr);
	mtr_commit(mtr);

	if (error != DB_SUCCESS) {
		return(error);
	}

	while ((index = dict_table_get_next_index(index)) != NULL) {
		if (index->type & DICT_FTS) {
			continue;
		}

		const dtuple_t*	entry = row_build_index_entry(
			row, save_ext, index, heap);
		mtr_start(mtr);
		btr_pcur_open(index, entry, PAGE_CUR_LE,
			      BTR_MODIFY_TREE, pcur, mtr);
#ifdef UNIV_DEBUG
		switch (btr_pcur_get_btr_cur(pcur)->flag) {
		case BTR_CUR_DELETE_REF:
		case BTR_CUR_DEL_MARK_IBUF:
		case BTR_CUR_DELETE_IBUF:
		case BTR_CUR_INSERT_TO_IBUF:
			/* We did not request buffering. */
			break;
		case BTR_CUR_HASH:
		case BTR_CUR_HASH_FAIL:
		case BTR_CUR_BINARY:
			goto flag_ok;
		}
		ut_ad(0);
flag_ok:
#endif /* UNIV_DEBUG */

		if (page_rec_is_infimum(btr_pcur_get_rec(pcur))
		    || btr_pcur_get_low_match(pcur) < index->n_uniq) {
			/* All secondary index entries should be
			found, because new_table is being modified by
			this thread only, and all indexes should be
			updated in sync. */
			mtr_commit(mtr);
			return(DB_INDEX_CORRUPT);
		}

		btr_cur_pessimistic_delete(&error, FALSE,
					   btr_pcur_get_btr_cur(pcur),
					   BTR_CREATE_FLAG, RB_NONE, mtr);
		mtr_commit(mtr);
	}

	return(error);
}

/******************************************************//**
Replays a delete operation on a table that was rebuilt.
@return DB_SUCCESS or error code */
static __attribute__((nonnull(1, 3, 4, 5, 6, 7), warn_unused_result))
dberr_t
row_log_table_apply_delete(
/*=======================*/
	que_thr_t*		thr,		/*!< in: query graph */
	ulint			trx_id_col,	/*!< in: position of
						DB_TRX_ID in the new
						clustered index */
	const mrec_t*		mrec,		/*!< in: merge record */
	const ulint*		moffsets,	/*!< in: offsets of mrec */
	mem_heap_t*		offsets_heap,	/*!< in/out: memory heap
						that can be emptied */
	mem_heap_t*		heap,		/*!< in/out: memory heap */
	dict_table_t*		new_table,	/*!< in: rebuilt table */
	const row_ext_t*	save_ext)	/*!< in: saved external field
						info, or NULL */
{
	dict_index_t*	index = dict_table_get_first_index(new_table);
	dtuple_t*	old_pk;
	mtr_t		mtr;
	btr_pcur_t	pcur;
	ulint*		offsets;

	ut_ad(rec_offs_n_fields(moffsets)
	      == dict_index_get_n_unique(index) + 1);
	ut_ad(!rec_offs_any_extern(moffsets));

	/* Convert the row to a search tuple. */
	old_pk = dtuple_create(heap, index->n_uniq + 1);
	dict_index_copy_types(old_pk, index, old_pk->n_fields);
	dtuple_set_n_fields_cmp(old_pk, index->n_uniq);

	for (ulint i = 0; i <= index->n_uniq; i++) {
		ulint		len;
		const void*	field;
		field = rec_get_nth_field(mrec, moffsets, i, &len);
		ut_ad(len != UNIV_SQL_NULL);
		dfield_set_data(dtuple_get_nth_field(old_pk, i),
				field, len);
	}

	mtr_start(&mtr);
	btr_pcur_open(index, old_pk, PAGE_CUR_LE,
		      BTR_MODIFY_TREE, &pcur, &mtr);
#ifdef UNIV_DEBUG
	switch (btr_pcur_get_btr_cur(&pcur)->flag) {
	case BTR_CUR_DELETE_REF:
	case BTR_CUR_DEL_MARK_IBUF:
	case BTR_CUR_DELETE_IBUF:
	case BTR_CUR_INSERT_TO_IBUF:
		/* We did not request buffering. */
		break;
	case BTR_CUR_HASH:
	case BTR_CUR_HASH_FAIL:
	case BTR_CUR_BINARY:
		goto flag_ok;
	}
	ut_ad(0);
flag_ok:
#endif /* UNIV_DEBUG */

	if (page_rec_is_infimum(btr_pcur_get_rec(&pcur))
	    || btr_pcur_get_low_match(&pcur) < index->n_uniq) {
all_done:
		mtr_commit(&mtr);
		/* The record was not found. All done. */
		return(DB_SUCCESS);
	}

	offsets = rec_get_offsets(btr_pcur_get_rec(&pcur), index, NULL,
				  ULINT_UNDEFINED, &offsets_heap);
#if defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG
	ut_a(!rec_offs_any_null_extern(btr_pcur_get_rec(&pcur), offsets));
#endif /* UNIV_DEBUG || UNIV_BLOB_LIGHT_DEBUG */

	/* Only remove the record if DB_TRX_ID matches what was
	buffered. */

	{
		ulint		len;
		const void*	mrec_trx_id
			= rec_get_nth_field(mrec, moffsets, trx_id_col, &len);
		ut_ad(len == DATA_TRX_ID_LEN);
		const void*	rec_trx_id
			= rec_get_nth_field(btr_pcur_get_rec(&pcur), offsets,
					    trx_id_col, &len);
		ut_ad(len == DATA_TRX_ID_LEN);
		if (memcmp(mrec_trx_id, rec_trx_id, DATA_TRX_ID_LEN)) {
			goto all_done;
		}
	}

	return(row_log_table_apply_delete_low(&pcur, offsets, save_ext,
					      heap, &mtr));
}

/******************************************************//**
Replays an update operation on a table that was rebuilt.
@return DB_SUCCESS or error code */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_log_table_apply_update(
/*=======================*/
	que_thr_t*		thr,		/*!< in: query graph */
	ulint			trx_id_col,	/*!< in: position of
						DB_TRX_ID in the
						old clustered index */
	ulint			new_trx_id_col,	/*!< in: position of
						DB_TRX_ID in the new
						clustered index */
	const mrec_t*		mrec,		/*!< in: new value */
	const ulint*		offsets,	/*!< in: offsets of mrec */
	mem_heap_t*		offsets_heap,	/*!< in/out: memory heap
						that can be emptied */
	mem_heap_t*		heap,		/*!< in/out: memory heap */
	row_merge_dup_t*	dup,		/*!< in/out: for reporting
						duplicate key errors */
	trx_id_t		trx_id,		/*!< in: DB_TRX_ID of mrec */
	const dtuple_t*		old_pk)		/*!< in: PRIMARY KEY and
						DB_TRX_ID,DB_ROLL_PTR
						of the old value,
						or PRIMARY KEY if same_pk */
{
	const row_log_t*log	= dup->index->online_log;
	const dtuple_t*	row;
	dict_index_t*	index	= dict_table_get_first_index(log->table);
	mtr_t		mtr;
	btr_pcur_t	pcur;
	dberr_t		error;

	ut_ad(dtuple_get_n_fields_cmp(old_pk)
	      == dict_index_get_n_unique(index));
	ut_ad(dtuple_get_n_fields(old_pk)
	      == dict_index_get_n_unique(index)
	      + (dup->index->online_log->same_pk ? 0 : 2));

	row = row_log_table_apply_convert_mrec(
		mrec, dup->index, offsets, log, heap, trx_id, &error);

	ut_ad(error == DB_SUCCESS || !row);
	/* Handling of duplicate key error requires storing
	of offending key in a record buffer. */
	ut_ad(error != DB_DUPLICATE_KEY);

	if (!row) {
		return(error);
	}

	mtr_start(&mtr);
	btr_pcur_open(index, old_pk, PAGE_CUR_LE,
		      BTR_MODIFY_TREE, &pcur, &mtr);
#ifdef UNIV_DEBUG
	switch (btr_pcur_get_btr_cur(&pcur)->flag) {
	case BTR_CUR_DELETE_REF:
	case BTR_CUR_DEL_MARK_IBUF:
	case BTR_CUR_DELETE_IBUF:
	case BTR_CUR_INSERT_TO_IBUF:
		ut_ad(0);/* We did not request buffering. */
	case BTR_CUR_HASH:
	case BTR_CUR_HASH_FAIL:
	case BTR_CUR_BINARY:
		break;
	}
#endif /* UNIV_DEBUG */

	if (page_rec_is_infimum(btr_pcur_get_rec(&pcur))
	    || btr_pcur_get_low_match(&pcur) < index->n_uniq) {
		mtr_commit(&mtr);
insert:
		ut_ad(mtr.state == MTR_COMMITTED);
		/* The row was not found. Insert it. */
		error = row_log_table_apply_insert_low(
			thr, row, trx_id, offsets_heap, heap, dup);
		if (error != DB_SUCCESS) {
err_exit:
			/* Report the erroneous row using the new
			version of the table. */
			innobase_row_to_mysql(dup->table, log->table, row);
		}

		return(error);
	}

	/* Update the record. */
	ulint*		cur_offsets	= rec_get_offsets(
		btr_pcur_get_rec(&pcur),
		index, NULL, ULINT_UNDEFINED, &offsets_heap);

	dtuple_t*	entry	= row_build_index_entry(
		row, NULL, index, heap);
	const upd_t*	update	= row_upd_build_difference_binary(
		index, entry, btr_pcur_get_rec(&pcur), cur_offsets,
		false, NULL, heap);

	error = DB_SUCCESS;

	if (!update->n_fields) {
		/* Nothing to do. */
		goto func_exit;
	}

	if (rec_offs_any_extern(cur_offsets)) {
		/* If the record contains any externally stored
		columns, perform the update by delete and insert,
		because we will not write any undo log that would
		allow purge to free any orphaned externally stored
		columns. */
delete_insert:
		error = row_log_table_apply_delete_low(
			&pcur, cur_offsets, NULL, heap, &mtr);
		ut_ad(mtr.state == MTR_COMMITTED);

		if (error != DB_SUCCESS) {
			goto err_exit;
		}

		goto insert;
	}

	if (upd_get_nth_field(update, 0)->field_no < new_trx_id_col) {
		if (dup->index->online_log->same_pk) {
			/* The ROW_T_UPDATE log record should only be
			written when the PRIMARY KEY fields of the
			record did not change in the old table.  We
			can only get a change of PRIMARY KEY columns
			in the rebuilt table if the PRIMARY KEY was
			redefined (!same_pk). */
			ut_ad(0);
			error = DB_CORRUPTION;
			goto func_exit;
		}

		/* The PRIMARY KEY columns have changed.
		Delete the record with the old PRIMARY KEY value,
		provided that it carries the same
		DB_TRX_ID,DB_ROLL_PTR. Then, insert the new row. */
		ulint		len;
		const byte*	cur_trx_roll	= rec_get_nth_field(
			mrec, offsets, trx_id_col, &len);
		ut_ad(len == DATA_TRX_ID_LEN);
		const dfield_t*	new_trx_roll	= dtuple_get_nth_field(
			old_pk, new_trx_id_col);
		/* We assume that DB_TRX_ID,DB_ROLL_PTR are stored
		in one contiguous block. */
		ut_ad(rec_get_nth_field(mrec, offsets, trx_id_col + 1, &len)
		      == cur_trx_roll + DATA_TRX_ID_LEN);
		ut_ad(len == DATA_ROLL_PTR_LEN);
		ut_ad(new_trx_roll->len == DATA_TRX_ID_LEN);
		ut_ad(dtuple_get_nth_field(old_pk, new_trx_id_col + 1)
		      -> len == DATA_ROLL_PTR_LEN);
		ut_ad(static_cast<const byte*>(
			      dtuple_get_nth_field(old_pk, new_trx_id_col + 1)
			      ->data)
		      == static_cast<const byte*>(new_trx_roll->data)
		      + DATA_TRX_ID_LEN);

		if (!memcmp(cur_trx_roll, new_trx_roll->data,
			    DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN)) {
			/* The old row exists. Remove it. */
			goto delete_insert;
		}

		/* Unless we called row_log_table_apply_delete_low(),
		this will likely cause a duplicate key error. */
		mtr_commit(&mtr);
		goto insert;
	}

	dtuple_t*	old_row;
	row_ext_t*	old_ext;

	if (dict_table_get_next_index(index)) {
		/* Construct the row corresponding to the old value of
		the record. */
		old_row = row_build(
			ROW_COPY_DATA, index, btr_pcur_get_rec(&pcur),
			cur_offsets, NULL, NULL, NULL, &old_ext, heap);
		ut_ad(old_row);
#ifdef ROW_LOG_APPLY_PRINT
		if (row_log_apply_print) {
			fprintf(stderr, "table apply update "
				IB_ID_FMT " " IB_ID_FMT "\n",
				index->table->id, index->id);
			dtuple_print(stderr, old_row);
			dtuple_print(stderr, row);
		}
#endif /* ROW_LOG_APPLY_PRINT */
	} else {
		old_row = NULL;
		old_ext = NULL;
	}

	big_rec_t*	big_rec;

	error = btr_cur_pessimistic_update(
		BTR_CREATE_FLAG | BTR_NO_LOCKING_FLAG
		| BTR_NO_UNDO_LOG_FLAG | BTR_KEEP_SYS_FLAG
		| BTR_KEEP_POS_FLAG,
		btr_pcur_get_btr_cur(&pcur),
		&cur_offsets, &offsets_heap, heap, &big_rec,
		update, 0, NULL, 0, &mtr);

	if (big_rec) {
		if (error == DB_SUCCESS) {
			error = btr_store_big_rec_extern_fields(
				index, btr_pcur_get_block(&pcur),
				btr_pcur_get_rec(&pcur), cur_offsets,
				big_rec, &mtr, BTR_STORE_UPDATE);
		}

		dtuple_big_rec_free(big_rec);
	}

	while ((index = dict_table_get_next_index(index)) != NULL) {
		if (error != DB_SUCCESS) {
			break;
		}

		if (index->type & DICT_FTS) {
			continue;
		}

		if (!row_upd_changes_ord_field_binary(
			    index, update, thr, old_row, NULL)) {
			continue;
		}

		mtr_commit(&mtr);

		entry = row_build_index_entry(old_row, old_ext, index, heap);
		if (!entry) {
			ut_ad(0);
			return(DB_CORRUPTION);
		}

		mtr_start(&mtr);

		if (ROW_FOUND != row_search_index_entry(
			    index, entry, BTR_MODIFY_TREE, &pcur, &mtr)) {
			ut_ad(0);
			error = DB_CORRUPTION;
			break;
		}

		btr_cur_pessimistic_delete(
			&error, FALSE, btr_pcur_get_btr_cur(&pcur),
			BTR_CREATE_FLAG, RB_NONE, &mtr);

		if (error != DB_SUCCESS) {
			break;
		}

		mtr_commit(&mtr);

		entry = row_build_index_entry(row, NULL, index, heap);
		error = row_ins_sec_index_entry_low(
			BTR_CREATE_FLAG | BTR_NO_LOCKING_FLAG
			| BTR_NO_UNDO_LOG_FLAG | BTR_KEEP_SYS_FLAG,
			BTR_MODIFY_TREE, index, offsets_heap, heap,
			entry, trx_id, thr);

		mtr_start(&mtr);
	}

func_exit:
	mtr_commit(&mtr);
	if (error != DB_SUCCESS) {
		goto err_exit;
	}

	return(error);
}

/******************************************************//**
Applies an operation to a table that was rebuilt.
@return NULL on failure (mrec corruption) or when out of data;
pointer to next record on success */
static __attribute__((nonnull, warn_unused_result))
const mrec_t*
row_log_table_apply_op(
/*===================*/
	que_thr_t*		thr,		/*!< in: query graph */
	ulint			trx_id_col,	/*!< in: position of
						DB_TRX_ID in old index */
	ulint			new_trx_id_col,	/*!< in: position of
						DB_TRX_ID in new index */
	row_merge_dup_t*	dup,		/*!< in/out: for reporting
						duplicate key errors */
	dberr_t*		error,		/*!< out: DB_SUCCESS
						or error code */
	mem_heap_t*		offsets_heap,	/*!< in/out: memory heap
						that can be emptied */
	mem_heap_t*		heap,		/*!< in/out: memory heap */
	const mrec_t*		mrec,		/*!< in: merge record */
	const mrec_t*		mrec_end,	/*!< in: end of buffer */
	ulint*			offsets)	/*!< in/out: work area
						for parsing mrec */
{
	const row_log_t*log	= dup->index->online_log;
	dict_index_t*	new_index = dict_table_get_first_index(log->table);
	ulint		extra_size;
	const mrec_t*	next_mrec;
	dtuple_t*	old_pk;
	row_ext_t*	ext;
	ulint		ext_size;

	ut_ad(dict_index_is_clust(dup->index));
	ut_ad(dup->index->table != log->table);

	*error = DB_SUCCESS;

	/* 3 = 1 (op type) + 1 (ext_size) + at least 1 byte payload */
	if (mrec + 3 >= mrec_end) {
		return(NULL);
	}

	switch (*mrec++) {
	default:
		ut_ad(0);
		*error = DB_CORRUPTION;
		return(NULL);
	case ROW_T_INSERT:
		extra_size = *mrec++;

		if (extra_size >= 0x80) {
			/* Read another byte of extra_size. */

			extra_size = (extra_size & 0x7f) << 8;
			extra_size |= *mrec++;
		}

		mrec += extra_size;

		if (mrec > mrec_end) {
			return(NULL);
		}

		rec_offs_set_n_fields(offsets, dup->index->n_fields);
		rec_init_offsets_temp(mrec, dup->index, offsets);

		next_mrec = mrec + rec_offs_data_size(offsets);

		if (next_mrec > mrec_end) {
			return(NULL);
		} else {
			ulint		len;
			const byte*	db_trx_id
				= rec_get_nth_field(
					mrec, offsets, trx_id_col, &len);
			ut_ad(len == DATA_TRX_ID_LEN);
			*error = row_log_table_apply_insert(
				thr, mrec, offsets, offsets_heap,
				heap, dup, trx_read_trx_id(db_trx_id));
		}
		break;

	case ROW_T_DELETE:
		/* 1 (extra_size) + 2 (ext_size) + at least 1 (payload) */
		if (mrec + 4 >= mrec_end) {
			return(NULL);
		}

		extra_size = *mrec++;
		ext_size = mach_read_from_2(mrec);
		mrec += 2;
		ut_ad(mrec < mrec_end);

		/* We assume extra_size < 0x100 for the PRIMARY KEY prefix.
		For fixed-length PRIMARY key columns, it is 0. */
		mrec += extra_size;

		rec_offs_set_n_fields(offsets, new_index->n_uniq + 1);
		rec_init_offsets_temp(mrec, new_index, offsets);
		next_mrec = mrec + rec_offs_data_size(offsets) + ext_size;
		if (next_mrec > mrec_end) {
			return(NULL);
		}

		/* If there are external fields, retrieve those logged
		prefix info and reconstruct the row_ext_t */
		if (ext_size) {
			/* We use memcpy to avoid unaligned
			access on some non-x86 platforms.*/
			ext = static_cast<row_ext_t*>(
				mem_heap_dup(heap,
					     mrec + rec_offs_data_size(offsets),
					     ext_size));

			byte*	ext_start = reinterpret_cast<byte*>(ext);

			ulint	ext_len = sizeof(*ext)
				+ (ext->n_ext - 1) * sizeof ext->len;

			ext->ext = reinterpret_cast<ulint*>(ext_start + ext_len);
			ext_len += ext->n_ext * sizeof(*ext->ext);

			ext->buf = static_cast<byte*>(ext_start + ext_len);
		} else {
			ext = NULL;
		}

		*error = row_log_table_apply_delete(
			thr, new_trx_id_col,
			mrec, offsets, offsets_heap, heap,
			log->table, ext);
		break;

	case ROW_T_UPDATE:
		/* Logically, the log entry consists of the
		(PRIMARY KEY,DB_TRX_ID) of the old value (converted
		to the new primary key definition) followed by
		the new value in the old table definition. If the
		definition of the columns belonging to PRIMARY KEY
		is not changed, the log will only contain
		DB_TRX_ID,new_row. */

		if (dup->index->online_log->same_pk) {
			ut_ad(new_index->n_uniq == dup->index->n_uniq);

			extra_size = *mrec++;

			if (extra_size >= 0x80) {
				/* Read another byte of extra_size. */

				extra_size = (extra_size & 0x7f) << 8;
				extra_size |= *mrec++;
			}

			mrec += extra_size;

			if (mrec > mrec_end) {
				return(NULL);
			}

			rec_offs_set_n_fields(offsets, dup->index->n_fields);
			rec_init_offsets_temp(mrec, dup->index, offsets);

			next_mrec = mrec + rec_offs_data_size(offsets);

			if (next_mrec > mrec_end) {
				return(NULL);
			}

			old_pk = dtuple_create(heap, new_index->n_uniq);
			dict_index_copy_types(
				old_pk, new_index, old_pk->n_fields);

			/* Copy the PRIMARY KEY fields from mrec to old_pk. */
			for (ulint i = 0; i < new_index->n_uniq; i++) {
				const void*	field;
				ulint		len;
				dfield_t*	dfield;

				ut_ad(!rec_offs_nth_extern(offsets, i));

				field = rec_get_nth_field(
					mrec, offsets, i, &len);
				ut_ad(len != UNIV_SQL_NULL);

				dfield = dtuple_get_nth_field(old_pk, i);
				dfield_set_data(dfield, field, len);
			}
		} else {
			/* We assume extra_size < 0x100
			for the PRIMARY KEY prefix. */
			mrec += *mrec + 1;

			if (mrec > mrec_end) {
				return(NULL);
			}

			/* Get offsets for PRIMARY KEY,
			DB_TRX_ID, DB_ROLL_PTR. */
			rec_offs_set_n_fields(offsets, new_index->n_uniq + 2);
			rec_init_offsets_temp(mrec, new_index, offsets);

			next_mrec = mrec + rec_offs_data_size(offsets);
			if (next_mrec + 2 > mrec_end) {
				return(NULL);
			}

			/* Copy the PRIMARY KEY fields and
			DB_TRX_ID, DB_ROLL_PTR from mrec to old_pk. */
			old_pk = dtuple_create(heap, new_index->n_uniq + 2);
			dict_index_copy_types(old_pk, new_index,
					      old_pk->n_fields);

			for (ulint i = 0;
			     i < dict_index_get_n_unique(new_index) + 2;
			     i++) {
				const void*	field;
				ulint		len;
				dfield_t*	dfield;

				ut_ad(!rec_offs_nth_extern(offsets, i));

				field = rec_get_nth_field(
					mrec, offsets, i, &len);
				ut_ad(len != UNIV_SQL_NULL);

				dfield = dtuple_get_nth_field(old_pk, i);
				dfield_set_data(dfield, field, len);
			}

			mrec = next_mrec;

			/* Fetch the new value of the row as it was
			in the old table definition. */
			extra_size = *mrec++;

			if (extra_size >= 0x80) {
				/* Read another byte of extra_size. */

				extra_size = (extra_size & 0x7f) << 8;
				extra_size |= *mrec++;
			}

			mrec += extra_size;

			if (mrec > mrec_end) {
				return(NULL);
			}

			rec_offs_set_n_fields(offsets, dup->index->n_fields);
			rec_init_offsets_temp(mrec, dup->index, offsets);

			next_mrec = mrec + rec_offs_data_size(offsets);

			if (next_mrec > mrec_end) {
				return(NULL);
			}
		}

		ut_ad(next_mrec <= mrec_end);
		dtuple_set_n_fields_cmp(old_pk, new_index->n_uniq);

		{
			ulint		len;
			const byte*	db_trx_id
				= rec_get_nth_field(
					mrec, offsets, trx_id_col, &len);
			ut_ad(len == DATA_TRX_ID_LEN);
			*error = row_log_table_apply_update(
				thr, trx_id_col, new_trx_id_col,
				mrec, offsets, offsets_heap,
				heap, dup, trx_read_trx_id(db_trx_id), old_pk);
		}

		break;
	}

	mem_heap_empty(offsets_heap);
	mem_heap_empty(heap);
	return(next_mrec);
}

/******************************************************//**
Applies operations to a table was rebuilt.
@return DB_SUCCESS, or error code on failure */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_log_table_apply_ops(
/*====================*/
	que_thr_t*	thr,	/*!< in: query graph */
	row_merge_dup_t*dup)	/*!< in/out: for reporting duplicate key
				errors */
{
	dberr_t		error;
	const mrec_t*	mrec		= NULL;
	const mrec_t*	next_mrec;
	const mrec_t*	mrec_end	= NULL; /* silence bogus warning */
	const mrec_t*	next_mrec_end;
	mem_heap_t*	heap;
	mem_heap_t*	offsets_heap;
	ulint*		offsets;
	bool		has_index_lock;
	dict_index_t*	index		= const_cast<dict_index_t*>(
		dup->index);
	dict_table_t*	new_table	= index->online_log->table;
	dict_index_t*	new_index	= dict_table_get_first_index(
		new_table);
	const ulint	i		= 1 + REC_OFFS_HEADER_SIZE
		+ ut_max(dict_index_get_n_fields(index),
			 dict_index_get_n_unique(new_index) + 2);
	const ulint	trx_id_col	= dict_col_get_clust_pos(
		dict_table_get_sys_col(index->table, DATA_TRX_ID), index);
	const ulint	new_trx_id_col	= dict_col_get_clust_pos(
		dict_table_get_sys_col(new_table, DATA_TRX_ID), new_index);
	trx_t*		trx		= thr_get_trx(thr);

	ut_ad(dict_index_is_clust(index));
	ut_ad(dict_index_is_online_ddl(index));
	ut_ad(trx->mysql_thd);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(!dict_index_is_online_ddl(new_index));
	ut_ad(trx_id_col > 0);
	ut_ad(trx_id_col != ULINT_UNDEFINED);
	ut_ad(new_trx_id_col > 0);
	ut_ad(new_trx_id_col != ULINT_UNDEFINED);

	UNIV_MEM_INVALID(&mrec_end, sizeof mrec_end);

	offsets = static_cast<ulint*>(ut_malloc(i * sizeof *offsets));
	offsets[0] = i;
	offsets[1] = dict_index_get_n_fields(index);

	heap = mem_heap_create(UNIV_PAGE_SIZE);
	offsets_heap = mem_heap_create(UNIV_PAGE_SIZE);
	has_index_lock = true;

next_block:
	ut_ad(has_index_lock);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(index->online_log->head.bytes == 0);

	if (trx_is_interrupted(trx)) {
		goto interrupted;
	}

	if (dict_index_is_corrupted(index)) {
		error = DB_INDEX_CORRUPT;
		goto func_exit;
	}

	ut_ad(dict_index_is_online_ddl(index));

	error = index->online_log->error;

	if (error != DB_SUCCESS) {
		goto func_exit;
	}

	if (UNIV_UNLIKELY(index->online_log->head.blocks
			  > index->online_log->tail.blocks)) {
unexpected_eof:
		fprintf(stderr, "InnoDB: unexpected end of temporary file"
			" for table %s\n", index->table_name);
corruption:
		error = DB_CORRUPTION;
		goto func_exit;
	}

	if (index->online_log->head.blocks
	    == index->online_log->tail.blocks) {
		if (index->online_log->head.blocks) {
#ifdef HAVE_FTRUNCATE
			/* Truncate the file in order to save space. */
			ftruncate(index->online_log->fd, 0);
#endif /* HAVE_FTRUNCATE */
			index->online_log->head.blocks
				= index->online_log->tail.blocks = 0;
		}

		next_mrec = index->online_log->tail.block;
		next_mrec_end = next_mrec + index->online_log->tail.bytes;

		if (next_mrec_end == next_mrec) {
			/* End of log reached. */
all_done:
			ut_ad(has_index_lock);
			ut_ad(index->online_log->head.blocks == 0);
			ut_ad(index->online_log->tail.blocks == 0);
			index->online_log->head.bytes = 0;
			index->online_log->tail.bytes = 0;
			error = DB_SUCCESS;
			goto func_exit;
		}
	} else {
		os_offset_t	ofs;
		ibool		success;

		ofs = (os_offset_t) index->online_log->head.blocks
			* srv_sort_buf_size;

		ut_ad(has_index_lock);
		has_index_lock = false;
		rw_lock_x_unlock(dict_index_get_lock(index));

		log_free_check();

		ut_ad(dict_index_is_online_ddl(index));

		success = os_file_read_no_error_handling(
			OS_FILE_FROM_FD(index->online_log->fd),
			index->online_log->head.block, ofs,
			srv_sort_buf_size);

		if (!success) {
			fprintf(stderr, "InnoDB: unable to read temporary file"
				" for table %s\n", index->table_name);
			goto corruption;
		}

#ifdef POSIX_FADV_DONTNEED
		/* Each block is read exactly once.  Free up the file cache. */
		posix_fadvise(index->online_log->fd,
			      ofs, srv_sort_buf_size, POSIX_FADV_DONTNEED);
#endif /* POSIX_FADV_DONTNEED */
#ifdef FALLOC_FL_PUNCH_HOLE
		/* Try to deallocate the space for the file on disk.
		This should work on ext4 on Linux 2.6.39 and later,
		and be ignored when the operation is unsupported. */
		fallocate(index->online_log->fd,
			  FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
			  ofs, srv_buf_size);
#endif /* FALLOC_FL_PUNCH_HOLE */

		next_mrec = index->online_log->head.block;
		next_mrec_end = next_mrec + srv_sort_buf_size;
	}

	/* This read is not protected by index->online_log->mutex for
	performance reasons. We will eventually notice any error that
	was flagged by a DML thread. */
	error = index->online_log->error;

	if (error != DB_SUCCESS) {
		goto func_exit;
	}

	if (mrec) {
		/* A partial record was read from the previous block.
		Copy the temporary buffer full, as we do not know the
		length of the record. Parse subsequent records from
		the bigger buffer index->online_log->head.block
		or index->online_log->tail.block. */

		ut_ad(mrec == index->online_log->head.buf);
		ut_ad(mrec_end > mrec);
		ut_ad(mrec_end < (&index->online_log->head.buf)[1]);

		memcpy((mrec_t*) mrec_end, next_mrec,
		       (&index->online_log->head.buf)[1] - mrec_end);
		mrec = row_log_table_apply_op(
			thr, trx_id_col, new_trx_id_col,
			dup, &error, offsets_heap, heap,
			index->online_log->head.buf,
			(&index->online_log->head.buf)[1], offsets);
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

		index->online_log->head.bytes = mrec - mrec_end;
		next_mrec += index->online_log->head.bytes;
	}

	ut_ad(next_mrec <= next_mrec_end);
	/* The following loop must not be parsing the temporary
	buffer, but head.block or tail.block. */

	/* mrec!=NULL means that the next record starts from the
	middle of the block */
	ut_ad((mrec == NULL) == (index->online_log->head.bytes == 0));

#ifdef UNIV_DEBUG
	if (next_mrec_end == index->online_log->head.block
	    + srv_sort_buf_size) {
		/* If tail.bytes == 0, next_mrec_end can also be at
		the end of tail.block. */
		if (index->online_log->tail.bytes == 0) {
			ut_ad(next_mrec == next_mrec_end);
			ut_ad(index->online_log->tail.blocks == 0);
			ut_ad(index->online_log->head.blocks == 0);
			ut_ad(index->online_log->head.bytes == 0);
		} else {
			ut_ad(next_mrec == index->online_log->head.block
			      + index->online_log->head.bytes);
			ut_ad(index->online_log->tail.blocks
			      > index->online_log->head.blocks);
		}
	} else if (next_mrec_end == index->online_log->tail.block
		   + index->online_log->tail.bytes) {
		ut_ad(next_mrec == index->online_log->tail.block
		      + index->online_log->head.bytes);
		ut_ad(index->online_log->tail.blocks == 0);
		ut_ad(index->online_log->head.blocks == 0);
		ut_ad(index->online_log->head.bytes
		      <= index->online_log->tail.bytes);
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
			We do not hold index->lock in order to
			allow other threads to concurrently buffer
			modifications. */
			ut_ad(mrec >= index->online_log->head.block);
			ut_ad(mrec_end == index->online_log->head.block
			      + srv_sort_buf_size);
			ut_ad(index->online_log->head.bytes
			      < srv_sort_buf_size);

			/* Take the opportunity to do a redo log
			checkpoint if needed. */
			log_free_check();
		} else {
			/* We are applying operations from the last block.
			Do not allow other threads to buffer anything,
			so that we can finally catch up and synchronize. */
			ut_ad(index->online_log->head.blocks == 0);
			ut_ad(index->online_log->tail.blocks == 0);
			ut_ad(mrec_end == index->online_log->tail.block
			      + index->online_log->tail.bytes);
			ut_ad(mrec >= index->online_log->tail.block);
		}

		/* This read is not protected by index->online_log->mutex
		for performance reasons. We will eventually notice any
		error that was flagged by a DML thread. */
		error = index->online_log->error;

		if (error != DB_SUCCESS) {
			goto func_exit;
		}

		next_mrec = row_log_table_apply_op(
			thr, trx_id_col, new_trx_id_col,
			dup, &error, offsets_heap, heap,
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
			has_index_lock = true;

			index->online_log->head.bytes = 0;
			index->online_log->head.blocks++;
			goto next_block;
		} else if (next_mrec != NULL) {
			ut_ad(next_mrec < next_mrec_end);
			index->online_log->head.bytes += next_mrec - mrec;
		} else if (has_index_lock) {
			/* When mrec is within tail.block, it should
			be a complete record, because we are holding
			index->lock and thus excluding the writer. */
			ut_ad(index->online_log->tail.blocks == 0);
			ut_ad(mrec_end == index->online_log->tail.block
			      + index->online_log->tail.bytes);
			ut_ad(0);
			goto unexpected_eof;
		} else {
			memcpy(index->online_log->head.buf, mrec,
			       mrec_end - mrec);
			mrec_end += index->online_log->head.buf - mrec;
			mrec = index->online_log->head.buf;
			goto process_next_block;
		}
	}

interrupted:
	error = DB_INTERRUPTED;
func_exit:
	if (!has_index_lock) {
		rw_lock_x_lock(dict_index_get_lock(index));
	}

	mem_heap_free(offsets_heap);
	mem_heap_free(heap);
	ut_free(offsets);
	return(error);
}

/******************************************************//**
Apply the row_log_table log to a table upon completing rebuild.
@return DB_SUCCESS, or error code on failure */
UNIV_INTERN
dberr_t
row_log_table_apply(
/*================*/
	que_thr_t*	thr,	/*!< in: query graph */
	dict_table_t*	old_table,
				/*!< in: old table */
	struct TABLE*	table)	/*!< in/out: MySQL table
				(for reporting duplicates) */
{
	dberr_t		error;
	dict_index_t*	clust_index;

	thr_get_trx(thr)->error_key_num = 0;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(&dict_operation_lock, RW_LOCK_SHARED));
#endif /* UNIV_SYNC_DEBUG */
	clust_index = dict_table_get_first_index(old_table);

	rw_lock_x_lock(dict_index_get_lock(clust_index));

	if (!clust_index->online_log) {
		ut_ad(dict_index_get_online_status(clust_index)
		      == ONLINE_INDEX_COMPLETE);
		/* This function should not be called unless
		rebuilding a table online. Build in some fault
		tolerance. */
		ut_ad(0);
		error = DB_ERROR;
	} else {
		row_merge_dup_t	dup = {
			clust_index, table,
			clust_index->online_log->col_map, 0
		};

		error = row_log_table_apply_ops(thr, &dup);
	}

	rw_lock_x_unlock(dict_index_get_lock(clust_index));
	return(error);
}

/******************************************************//**
Allocate the row log for an index and flag the index
for online creation.
@retval true if success, false if not */
UNIV_INTERN
bool
row_log_allocate(
/*=============*/
	dict_index_t*	index,	/*!< in/out: index */
	dict_table_t*	table,	/*!< in/out: new table being rebuilt,
				or NULL when creating a secondary index */
	bool		same_pk,/*!< in: whether the definition of the
				PRIMARY KEY has remained the same */
	const dtuple_t*	add_cols,
				/*!< in: default values of
				added columns, or NULL */
	const ulint*	col_map)/*!< in: mapping of old column
				numbers to new ones, or NULL if !table */
{
	byte*		buf;
	row_log_t*	log;
	ulint		size;

	ut_ad(!dict_index_is_online_ddl(index));
	ut_ad(dict_index_is_clust(index) == !!table);
	ut_ad(!table || index->table != table);
	ut_ad(same_pk || table);
	ut_ad(!table || col_map);
	ut_ad(!add_cols || col_map);
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
	if (log->fd < 0) {
		os_mem_free_large(buf, size);
		return(false);
	}
	mutex_create(index_online_log_key, &log->mutex,
		     SYNC_INDEX_ONLINE_LOG);
	log->trx_rb = NULL;
	log->table = table;
	log->same_pk = same_pk;
	log->add_cols = add_cols;
	log->col_map = col_map;
	log->error = DB_SUCCESS;
	log->max_trx = 0;
	log->head.block = buf;
	log->tail.block = buf + srv_sort_buf_size;
	log->tail.blocks = log->tail.bytes = 0;
	log->head.blocks = log->head.bytes = 0;
	dict_index_set_online_status(index, ONLINE_INDEX_CREATION);
	index->online_log = log;

	/* While we might be holding an exclusive data dictionary lock
	here, in row_log_abort_sec() we will not always be holding it. Use
	atomic operations in both cases. */
	MONITOR_ATOMIC_INC(MONITOR_ONLINE_CREATE_INDEX);

	return(true);
}

/******************************************************//**
Free the row log for an index that was being created online. */
UNIV_INTERN
void
row_log_free(
/*=========*/
	row_log_t*&	log)	/*!< in,own: row log */
{
	MONITOR_ATOMIC_DEC(MONITOR_ONLINE_CREATE_INDEX);

	delete log->trx_rb;
	row_merge_file_destroy_low(log->fd);
	mutex_free(&log->mutex);
	os_mem_free_large(log->head.block, log->size);
	log = 0;
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
	       && mutex_own(&index->online_log->mutex))
	      || rw_lock_own(dict_index_get_lock(index), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	return(index->online_log->max_trx);
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
	dberr_t*	error,		/*!< out: DB_SUCCESS or error code */
	mem_heap_t*	offsets_heap,	/*!< in/out: memory heap for
					allocating offsets; can be emptied */
	bool		has_index_lock, /*!< in: true if holding index->lock
					in exclusive mode */
	enum row_op	op,		/*!< in: operation being applied */
	trx_id_t	trx_id,		/*!< in: transaction identifier */
	const dtuple_t*	entry)		/*!< in: row */
{
	mtr_t		mtr;
	btr_cur_t	cursor;
	ulint*		offsets = NULL;

	ut_ad(!dict_index_is_clust(index));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_EX)
	      == has_index_lock);
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(!dict_index_is_corrupted(index));
	ut_ad(trx_id != 0 || op == ROW_OP_DELETE);

	mtr_start(&mtr);

	/* We perform the pessimistic variant of the operations if we
	already hold index->lock exclusively. First, search the
	record. The operation may already have been performed,
	depending on when the row in the clustered index was
	scanned. */
	btr_cur_search_to_nth_level(index, 0, entry, PAGE_CUR_LE,
				    has_index_lock
				    ? BTR_MODIFY_TREE
				    : BTR_MODIFY_LEAF,
				    &cursor, 0, __FILE__, __LINE__,
				    &mtr);

	ut_ad(dict_index_get_n_unique(index) > 0);
	/* This test is somewhat similar to row_ins_must_modify_rec(),
	but not identical for unique secondary indexes. */
	if (cursor.low_match >= dict_index_get_n_unique(index)
	    && !page_rec_is_infimum(btr_cur_get_rec(&cursor))) {
		/* We have a matching record. */
		bool	exists	= (cursor.low_match
				   == dict_index_get_n_fields(index));
#ifdef UNIV_DEBUG
		rec_t*	rec	= btr_cur_get_rec(&cursor);
		ut_ad(page_rec_is_user_rec(rec));
		ut_ad(!rec_get_deleted_flag(rec, page_rec_is_comp(rec)));
#endif /* UNIV_DEBUG */

		ut_ad(exists || dict_index_is_unique(index));

		switch (op) {
		case ROW_OP_DELETE:
			if (!exists) {
				/* The record was already deleted. */
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
					BTR_MODIFY_TREE, &cursor, 0,
					__FILE__, __LINE__, &mtr);

				/* No other thread than the current one
				is allowed to modify the index tree.
				Thus, the record should still exist. */
				ut_ad(cursor.low_match
				      >= dict_index_get_n_fields(index));
				ut_ad(page_rec_is_user_rec(
					      btr_cur_get_rec(&cursor)));
			}

			/* As there are no externally stored fields in
			a secondary index record, the parameter
			rb_ctx = RB_NONE will be ignored. */

			btr_cur_pessimistic_delete(
				error, FALSE, &cursor,
				BTR_CREATE_FLAG, RB_NONE, &mtr);
			break;
		case ROW_OP_INSERT:
			if (exists) {
				/* The record already exists. There
				is nothing to be inserted. */
				goto func_exit;
			}

			if (dtuple_contains_null(entry)) {
				/* The UNIQUE KEY columns match, but
				there is a NULL value in the key, and
				NULL!=NULL. */
				goto insert_the_rec;
			}

			/* Duplicate key error */
			ut_ad(dict_index_is_unique(index));
			row_merge_dup_report(dup, entry->fields);
			goto func_exit;
		}
	} else {
		switch (op) {
			rec_t*		rec;
			big_rec_t*	big_rec;
		case ROW_OP_DELETE:
			/* The record does not exist. */
			goto func_exit;
		case ROW_OP_INSERT:
			if (dict_index_is_unique(index)
			    && (cursor.up_match
				>= dict_index_get_n_unique(index)
				|| cursor.low_match
				>= dict_index_get_n_unique(index))
			    && (!index->n_nullable
				|| !dtuple_contains_null(entry))) {
				/* Duplicate key */
				row_merge_dup_report(dup, entry->fields);
				goto func_exit;
			}
insert_the_rec:
			/* Insert the record. As we are inserting into
			a secondary index, there cannot be externally
			stored columns (!big_rec). */
			*error = btr_cur_optimistic_insert(
				BTR_NO_UNDO_LOG_FLAG
				| BTR_NO_LOCKING_FLAG
				| BTR_CREATE_FLAG,
				&cursor, &offsets, &offsets_heap,
				const_cast<dtuple_t*>(entry),
				&rec, &big_rec, 0, NULL, &mtr);
			ut_ad(!big_rec);
			if (*error != DB_FAIL) {
				break;
			}

			if (!has_index_lock) {
				/* This needs a pessimistic operation.
				Lock the index tree exclusively. */
				mtr_commit(&mtr);
				mtr_start(&mtr);
				btr_cur_search_to_nth_level(
					index, 0, entry, PAGE_CUR_LE,
					BTR_MODIFY_TREE, &cursor, 0,
					__FILE__, __LINE__, &mtr);
			}

			/* We already determined that the
			record did not exist. No other thread
			than the current one is allowed to
			modify the index tree. Thus, the
			record should still not exist. */

			*error = btr_cur_pessimistic_insert(
				BTR_NO_UNDO_LOG_FLAG
				| BTR_NO_LOCKING_FLAG
				| BTR_CREATE_FLAG,
				&cursor, &offsets, &offsets_heap,
				const_cast<dtuple_t*>(entry),
				&rec, &big_rec,
				0, NULL, &mtr);
			ut_ad(!big_rec);
			break;
		}
		mem_heap_empty(offsets_heap);
	}

	if (*error == DB_SUCCESS && trx_id) {
		page_update_max_trx_id(btr_cur_get_block(&cursor),
				       btr_cur_get_page_zip(&cursor),
				       trx_id, &mtr);
	}

func_exit:
	mtr_commit(&mtr);
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
	dberr_t*	error,		/*!< out: DB_SUCCESS or error code */
	mem_heap_t*	offsets_heap,	/*!< in/out: memory heap for
					allocating offsets; can be emptied */
	mem_heap_t*	heap,		/*!< in/out: memory heap for
					allocating data tuples */
	bool		has_index_lock, /*!< in: true if holding index->lock
					in exclusive mode */
	const mrec_t*	mrec,		/*!< in: merge record */
	const mrec_t*	mrec_end,	/*!< in: end of buffer */
	ulint*		offsets)	/*!< in/out: work area for
					rec_init_offsets_temp() */

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
		if (ROW_LOG_HEADER_SIZE + DATA_TRX_ID_LEN + mrec >= mrec_end) {
			return(NULL);
		}

		op = static_cast<enum row_op>(*mrec++);
		trx_id = trx_read_trx_id(mrec);
		mrec += DATA_TRX_ID_LEN;
		break;
	case ROW_OP_DELETE:
		op = static_cast<enum row_op>(*mrec++);
		trx_id = 0;
		break;
	default:
corrupted:
		ut_ad(0);
		*error = DB_CORRUPTION;
		return(NULL);
	}

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

	rec_init_offsets_temp(mrec, index, offsets);

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
	row_log_apply_op_low(index, dup, error, offsets_heap,
			     has_index_lock, op, trx_id, entry);
	return(mrec);
}

/******************************************************//**
Applies operations to a secondary index that was being created.
@return DB_SUCCESS, or error code on failure */
static __attribute__((nonnull))
dberr_t
row_log_apply_ops(
/*==============*/
	trx_t*		trx,	/*!< in: transaction (for checking if
				the operation was interrupted) */
	dict_index_t*	index,	/*!< in/out: index */
	row_merge_dup_t*dup)	/*!< in/out: for reporting duplicate key
				errors */
{
	dberr_t		error;
	const mrec_t*	mrec	= NULL;
	const mrec_t*	next_mrec;
	const mrec_t*	mrec_end= NULL; /* silence bogus warning */
	const mrec_t*	next_mrec_end;
	mem_heap_t*	offsets_heap;
	mem_heap_t*	heap;
	ulint*		offsets;
	bool		has_index_lock;
	const ulint	i	= 1 + REC_OFFS_HEADER_SIZE
		+ dict_index_get_n_fields(index);

	ut_ad(dict_index_is_online_ddl(index));
	ut_ad(*index->name == TEMP_INDEX_PREFIX);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(index->online_log);
	UNIV_MEM_INVALID(&mrec_end, sizeof mrec_end);

	offsets = static_cast<ulint*>(ut_malloc(i * sizeof *offsets));
	offsets[0] = i;
	offsets[1] = dict_index_get_n_fields(index);

	offsets_heap = mem_heap_create(UNIV_PAGE_SIZE);
	heap = mem_heap_create(UNIV_PAGE_SIZE);
	has_index_lock = true;

next_block:
	ut_ad(has_index_lock);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(dict_index_get_lock(index), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(index->online_log->head.bytes == 0);

	if (trx_is_interrupted(trx)) {
		goto interrupted;
	}

	if (dict_index_is_corrupted(index)) {
		error = DB_INDEX_CORRUPT;
		goto func_exit;
	}

	if (UNIV_UNLIKELY(index->online_log->head.blocks
			  > index->online_log->tail.blocks)) {
unexpected_eof:
		fprintf(stderr, "InnoDB: unexpected end of temporary file"
			" for index %s\n", index->name + 1);
corruption:
		error = DB_CORRUPTION;
		goto func_exit;
	}

	if (index->online_log->head.blocks
	    == index->online_log->tail.blocks) {
		if (index->online_log->head.blocks) {
#ifdef HAVE_FTRUNCATE
			/* Truncate the file in order to save space. */
			ftruncate(index->online_log->fd, 0);
#endif /* HAVE_FTRUNCATE */
			index->online_log->head.blocks
				= index->online_log->tail.blocks = 0;
		}

		next_mrec = index->online_log->tail.block;
		next_mrec_end = next_mrec + index->online_log->tail.bytes;

		if (next_mrec_end == next_mrec) {
			/* End of log reached. */
all_done:
			ut_ad(has_index_lock);
			ut_ad(index->online_log->head.blocks == 0);
			ut_ad(index->online_log->tail.blocks == 0);
			error = DB_SUCCESS;
			goto func_exit;
		}
	} else {
		os_offset_t	ofs;
		ibool		success;

		ofs = (os_offset_t) index->online_log->head.blocks
			* srv_sort_buf_size;

		ut_ad(has_index_lock);
		has_index_lock = false;
		rw_lock_x_unlock(dict_index_get_lock(index));

		log_free_check();

		success = os_file_read_no_error_handling(
			OS_FILE_FROM_FD(index->online_log->fd),
			index->online_log->head.block, ofs,
			srv_sort_buf_size);

		if (!success) {
			fprintf(stderr, "InnoDB: unable to read temporary file"
				" for index %s\n", index->name + 1);
			goto corruption;
		}

#ifdef POSIX_FADV_DONTNEED
		/* Each block is read exactly once.  Free up the file cache. */
		posix_fadvise(index->online_log->fd,
			      ofs, srv_sort_buf_size, POSIX_FADV_DONTNEED);
#endif /* POSIX_FADV_DONTNEED */
#ifdef FALLOC_FL_PUNCH_HOLE
		/* Try to deallocate the space for the file on disk.
		This should work on ext4 on Linux 2.6.39 and later,
		and be ignored when the operation is unsupported. */
		fallocate(index->online_log->fd,
			  FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
			  ofs, srv_buf_size);
#endif /* FALLOC_FL_PUNCH_HOLE */

		next_mrec = index->online_log->head.block;
		next_mrec_end = next_mrec + srv_sort_buf_size;
	}

	if (mrec) {
		/* A partial record was read from the previous block.
		Copy the temporary buffer full, as we do not know the
		length of the record. Parse subsequent records from
		the bigger buffer index->online_log->head.block
		or index->online_log->tail.block. */

		ut_ad(mrec == index->online_log->head.buf);
		ut_ad(mrec_end > mrec);
		ut_ad(mrec_end < (&index->online_log->head.buf)[1]);

		memcpy((mrec_t*) mrec_end, next_mrec,
		       (&index->online_log->head.buf)[1] - mrec_end);
		mrec = row_log_apply_op(
			index, dup, &error, offsets_heap, heap,
			has_index_lock, index->online_log->head.buf,
			(&index->online_log->head.buf)[1], offsets);
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

		index->online_log->head.bytes = mrec - mrec_end;
		next_mrec += index->online_log->head.bytes;
	}

	ut_ad(next_mrec <= next_mrec_end);
	/* The following loop must not be parsing the temporary
	buffer, but head.block or tail.block. */

	/* mrec!=NULL means that the next record starts from the
	middle of the block */
	ut_ad((mrec == NULL) == (index->online_log->head.bytes == 0));

#ifdef UNIV_DEBUG
	if (next_mrec_end == index->online_log->head.block
	    + srv_sort_buf_size) {
		/* If tail.bytes == 0, next_mrec_end can also be at
		the end of tail.block. */
		if (index->online_log->tail.bytes == 0) {
			ut_ad(next_mrec == next_mrec_end);
			ut_ad(index->online_log->tail.blocks == 0);
			ut_ad(index->online_log->head.blocks == 0);
			ut_ad(index->online_log->head.bytes == 0);
		} else {
			ut_ad(next_mrec == index->online_log->head.block
			      + index->online_log->head.bytes);
			ut_ad(index->online_log->tail.blocks
			      > index->online_log->head.blocks);
		}
	} else if (next_mrec_end == index->online_log->tail.block
		   + index->online_log->tail.bytes) {
		ut_ad(next_mrec == index->online_log->tail.block
		      + index->online_log->head.bytes);
		ut_ad(index->online_log->tail.blocks == 0);
		ut_ad(index->online_log->head.blocks == 0);
		ut_ad(index->online_log->head.bytes
		      <= index->online_log->tail.bytes);
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
			We do not hold index->lock in order to
			allow other threads to concurrently buffer
			modifications. */
			ut_ad(mrec >= index->online_log->head.block);
			ut_ad(mrec_end == index->online_log->head.block
			      + srv_sort_buf_size);
			ut_ad(index->online_log->head.bytes
			      < srv_sort_buf_size);

			/* Take the opportunity to do a redo log
			checkpoint if needed. */
			log_free_check();
		} else {
			/* We are applying operations from the last block.
			Do not allow other threads to buffer anything,
			so that we can finally catch up and synchronize. */
			ut_ad(index->online_log->head.blocks == 0);
			ut_ad(index->online_log->tail.blocks == 0);
			ut_ad(mrec_end == index->online_log->tail.block
			      + index->online_log->tail.bytes);
			ut_ad(mrec >= index->online_log->tail.block);
		}

		next_mrec = row_log_apply_op(
			index, dup, &error, offsets_heap, heap,
			has_index_lock, mrec, mrec_end, offsets);

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
			has_index_lock = true;

			index->online_log->head.bytes = 0;
			index->online_log->head.blocks++;
			goto next_block;
		} else if (next_mrec != NULL) {
			ut_ad(next_mrec < next_mrec_end);
			index->online_log->head.bytes += next_mrec - mrec;
		} else if (has_index_lock) {
			/* When mrec is within tail.block, it should
			be a complete record, because we are holding
			index->lock and thus excluding the writer. */
			ut_ad(index->online_log->tail.blocks == 0);
			ut_ad(mrec_end == index->online_log->tail.block
			      + index->online_log->tail.bytes);
			ut_ad(0);
			goto unexpected_eof;
		} else {
			memcpy(index->online_log->head.buf, mrec,
			       mrec_end - mrec);
			mrec_end += index->online_log->head.buf - mrec;
			mrec = index->online_log->head.buf;
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
		if (((os_offset_t) index->online_log->tail.blocks + 1)
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
	mem_heap_free(offsets_heap);
	ut_free(offsets);
	return(error);
}

/******************************************************//**
Apply the row log to the index upon completing index creation.
@return DB_SUCCESS, or error code on failure */
UNIV_INTERN
dberr_t
row_log_apply(
/*==========*/
	trx_t*		trx,	/*!< in: transaction (for checking if
				the operation was interrupted) */
	dict_index_t*	index,	/*!< in/out: secondary index */
	struct TABLE*	table)	/*!< in/out: MySQL table
				(for reporting duplicates) */
{
	dberr_t		error;
	row_log_t*	log;
	row_merge_dup_t	dup = { index, table, NULL, 0 };

	ut_ad(dict_index_is_online_ddl(index));
	ut_ad(!dict_index_is_clust(index));

	log_free_check();

	rw_lock_x_lock(dict_index_get_lock(index));

	if (!dict_table_is_corrupted(index->table)) {
		error = row_log_apply_ops(trx, index, &dup);
	} else {
		error = DB_SUCCESS;
	}

	if (error != DB_SUCCESS || dup.n_dup) {
		ut_a(!dict_table_is_discarded(index->table));
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

	log = index->online_log;
	index->online_log = NULL;
	/* We could remove the TEMP_INDEX_PREFIX and update the data
	dictionary to say that this index is complete, if we had
	access to the .frm file here.  If the server crashes before
	all requested indexes have been created, this completed index
	will be dropped. */
	rw_lock_x_unlock(dict_index_get_lock(index));

	row_log_free(log);

	return(error);
}
