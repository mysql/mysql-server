/*****************************************************************************

Copyright (c) 1994, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/btr0cur.h
The index tree cursor

Created 10/16/1994 Heikki Tuuri
*******************************************************/

#ifndef btr0cur_h
#define btr0cur_h

#include "univ.i"
#include "dict0dict.h"
#include "page0cur.h"
#include "btr0types.h"

/* Mode flags for btr_cur operations; these can be ORed */
#define BTR_NO_UNDO_LOG_FLAG	1	/* do no undo logging */
#define BTR_NO_LOCKING_FLAG	2	/* do no record lock checking */
#define BTR_KEEP_SYS_FLAG	4	/* sys fields will be found from the
					update vector or inserted entry */
#define BTR_KEEP_POS_FLAG	8	/* btr_cur_pessimistic_update()
					must keep cursor position when
					moving columns to big_rec */

#ifndef UNIV_HOTBACKUP
#include "que0types.h"
#include "row0types.h"
#include "ha0ha.h"

#define BTR_CUR_ADAPT
#define BTR_CUR_HASH_ADAPT

#ifdef UNIV_DEBUG
/*********************************************************//**
Returns the page cursor component of a tree cursor.
@return	pointer to page cursor component */
UNIV_INLINE
page_cur_t*
btr_cur_get_page_cur(
/*=================*/
	const btr_cur_t*	cursor);/*!< in: tree cursor */
#else /* UNIV_DEBUG */
# define btr_cur_get_page_cur(cursor) (&(cursor)->page_cur)
#endif /* UNIV_DEBUG */
/*********************************************************//**
Returns the buffer block on which the tree cursor is positioned.
@return	pointer to buffer block */
UNIV_INLINE
buf_block_t*
btr_cur_get_block(
/*==============*/
	btr_cur_t*	cursor);/*!< in: tree cursor */
/*********************************************************//**
Returns the record pointer of a tree cursor.
@return	pointer to record */
UNIV_INLINE
rec_t*
btr_cur_get_rec(
/*============*/
	btr_cur_t*	cursor);/*!< in: tree cursor */
/*********************************************************//**
Returns the compressed page on which the tree cursor is positioned.
@return	pointer to compressed page, or NULL if the page is not compressed */
UNIV_INLINE
page_zip_des_t*
btr_cur_get_page_zip(
/*=================*/
	btr_cur_t*	cursor);/*!< in: tree cursor */
/*********************************************************//**
Invalidates a tree cursor by setting record pointer to NULL. */
UNIV_INLINE
void
btr_cur_invalidate(
/*===============*/
	btr_cur_t*	cursor);/*!< in: tree cursor */
/*********************************************************//**
Returns the page of a tree cursor.
@return	pointer to page */
UNIV_INLINE
page_t*
btr_cur_get_page(
/*=============*/
	btr_cur_t*	cursor);/*!< in: tree cursor */
/*********************************************************//**
Returns the index of a cursor.
@return	index */
UNIV_INLINE
dict_index_t*
btr_cur_get_index(
/*==============*/
	btr_cur_t*	cursor);/*!< in: B-tree cursor */
/*********************************************************//**
Positions a tree cursor at a given record. */
UNIV_INLINE
void
btr_cur_position(
/*=============*/
	dict_index_t*	index,	/*!< in: index */
	rec_t*		rec,	/*!< in: record in tree */
	buf_block_t*	block,	/*!< in: buffer block of rec */
	btr_cur_t*	cursor);/*!< in: cursor */
/********************************************************************//**
Searches an index tree and positions a tree cursor on a given level.
NOTE: n_fields_cmp in tuple must be set so that it cannot be compared
to node pointer page number fields on the upper levels of the tree!
Note that if mode is PAGE_CUR_LE, which is used in inserts, then
cursor->up_match and cursor->low_match both will have sensible values.
If mode is PAGE_CUR_GE, then up_match will a have a sensible value. */
UNIV_INTERN
void
btr_cur_search_to_nth_level(
/*========================*/
	dict_index_t*	index,	/*!< in: index */
	ulint		level,	/*!< in: the tree level of search */
	const dtuple_t*	tuple,	/*!< in: data tuple; NOTE: n_fields_cmp in
				tuple must be set so that it cannot get
				compared to the node ptr page number field! */
	ulint		mode,	/*!< in: PAGE_CUR_L, ...;
				NOTE that if the search is made using a unique
				prefix of a record, mode should be PAGE_CUR_LE,
				not PAGE_CUR_GE, as the latter may end up on
				the previous page of the record! Inserts
				should always be made using PAGE_CUR_LE to
				search the position! */
	ulint		latch_mode, /*!< in: BTR_SEARCH_LEAF, ..., ORed with
				at most one of BTR_INSERT, BTR_DELETE_MARK,
				BTR_DELETE, or BTR_ESTIMATE;
				cursor->left_block is used to store a pointer
				to the left neighbor page, in the cases
				BTR_SEARCH_PREV and BTR_MODIFY_PREV;
				NOTE that if has_search_latch
				is != 0, we maybe do not have a latch set
				on the cursor page, we assume
				the caller uses his search latch
				to protect the record! */
	btr_cur_t*	cursor, /*!< in/out: tree cursor; the cursor page is
				s- or x-latched, but see also above! */
	ulint		has_search_latch,/*!< in: latch mode the caller
				currently has on btr_search_latch:
				RW_S_LATCH, or 0 */
	const char*	file,	/*!< in: file name */
	ulint		line,	/*!< in: line where called */
	mtr_t*		mtr);	/*!< in: mtr */
/*****************************************************************//**
Opens a cursor at either end of an index. */
UNIV_INTERN
void
btr_cur_open_at_index_side_func(
/*============================*/
	ibool		from_left,	/*!< in: TRUE if open to the low end,
					FALSE if to the high end */
	dict_index_t*	index,		/*!< in: index */
	ulint		latch_mode,	/*!< in: latch mode */
	btr_cur_t*	cursor,		/*!< in: cursor */
	const char*	file,		/*!< in: file name */
	ulint		line,		/*!< in: line where called */
	mtr_t*		mtr);		/*!< in: mtr */
#define btr_cur_open_at_index_side(f,i,l,c,m)				\
	btr_cur_open_at_index_side_func(f,i,l,c,__FILE__,__LINE__,m)
/**********************************************************************//**
Positions a cursor at a randomly chosen position within a B-tree. */
UNIV_INTERN
void
btr_cur_open_at_rnd_pos_func(
/*=========================*/
	dict_index_t*	index,		/*!< in: index */
	ulint		latch_mode,	/*!< in: BTR_SEARCH_LEAF, ... */
	btr_cur_t*	cursor,		/*!< in/out: B-tree cursor */
	const char*	file,		/*!< in: file name */
	ulint		line,		/*!< in: line where called */
	mtr_t*		mtr);		/*!< in: mtr */
#define btr_cur_open_at_rnd_pos(i,l,c,m)				\
	btr_cur_open_at_rnd_pos_func(i,l,c,__FILE__,__LINE__,m)
/*************************************************************//**
Tries to perform an insert to a page in an index tree, next to cursor.
It is assumed that mtr holds an x-latch on the page. The operation does
not succeed if there is too little space on the page. If there is just
one record on the page, the insert will always succeed; this is to
prevent trying to split a page with just one record.
@return	DB_SUCCESS, DB_WAIT_LOCK, DB_FAIL, or error number */
UNIV_INTERN
ulint
btr_cur_optimistic_insert(
/*======================*/
	ulint		flags,	/*!< in: undo logging and locking flags: if not
				zero, the parameters index and thr should be
				specified */
	btr_cur_t*	cursor,	/*!< in: cursor on page after which to insert;
				cursor stays valid */
	dtuple_t*	entry,	/*!< in/out: entry to insert */
	rec_t**		rec,	/*!< out: pointer to inserted record if
				succeed */
	big_rec_t**	big_rec,/*!< out: big rec vector whose fields have to
				be stored externally by the caller, or
				NULL */
	ulint		n_ext,	/*!< in: number of externally stored columns */
	que_thr_t*	thr,	/*!< in: query thread or NULL */
	mtr_t*		mtr);	/*!< in: mtr; if this function returns
				DB_SUCCESS on a leaf page of a secondary
				index in a compressed tablespace, the
				mtr must be committed before latching
				any further pages */
/*************************************************************//**
Performs an insert on a page of an index tree. It is assumed that mtr
holds an x-latch on the tree and on the cursor page. If the insert is
made on the leaf level, to avoid deadlocks, mtr must also own x-latches
to brothers of page, if those brothers exist.
@return	DB_SUCCESS or error number */
UNIV_INTERN
ulint
btr_cur_pessimistic_insert(
/*=======================*/
	ulint		flags,	/*!< in: undo logging and locking flags: if not
				zero, the parameter thr should be
				specified; if no undo logging is specified,
				then the caller must have reserved enough
				free extents in the file space so that the
				insertion will certainly succeed */
	btr_cur_t*	cursor,	/*!< in: cursor after which to insert;
				cursor stays valid */
	dtuple_t*	entry,	/*!< in/out: entry to insert */
	rec_t**		rec,	/*!< out: pointer to inserted record if
				succeed */
	big_rec_t**	big_rec,/*!< out: big rec vector whose fields have to
				be stored externally by the caller, or
				NULL */
	ulint		n_ext,	/*!< in: number of externally stored columns */
	que_thr_t*	thr,	/*!< in: query thread or NULL */
	mtr_t*		mtr);	/*!< in: mtr */
/*************************************************************//**
See if there is enough place in the page modification log to log
an update-in-place.
@return	TRUE if enough place */
UNIV_INTERN
ibool
btr_cur_update_alloc_zip(
/*=====================*/
	page_zip_des_t*	page_zip,/*!< in/out: compressed page */
	buf_block_t*	block,	/*!< in/out: buffer page */
	dict_index_t*	index,	/*!< in: the index corresponding to the block */
	ulint		length,	/*!< in: size needed */
	ibool		create,	/*!< in: TRUE=delete-and-insert,
				FALSE=update-in-place */
	mtr_t*		mtr,	/*!< in: mini-transaction */
	trx_t*		trx)	/*!< in: NULL or transaction */
    __attribute__((nonnull (1, 2, 3, 6), warn_unused_result));
/*************************************************************//**
Updates a record when the update causes no size changes in its fields.
@return	DB_SUCCESS or error number */
UNIV_INTERN
ulint
btr_cur_update_in_place(
/*====================*/
	ulint		flags,	/*!< in: undo logging and locking flags */
	btr_cur_t*	cursor,	/*!< in: cursor on the record to update;
				cursor stays valid and positioned on the
				same record */
	const upd_t*	update,	/*!< in: update vector */
	ulint		cmpl_info,/*!< in: compiler info on secondary index
				updates */
	que_thr_t*	thr,	/*!< in: query thread */
	mtr_t*		mtr);	/*!< in: mtr; must be committed before
				latching any further pages */
/*************************************************************//**
Tries to update a record on a page in an index tree. It is assumed that mtr
holds an x-latch on the page. The operation does not succeed if there is too
little space on the page or if the update would result in too empty a page,
so that tree compression is recommended.
@return DB_SUCCESS, or DB_OVERFLOW if the updated record does not fit,
DB_UNDERFLOW if the page would become too empty, or DB_ZIP_OVERFLOW if
there is not enough space left on the compressed page */
UNIV_INTERN
ulint
btr_cur_optimistic_update(
/*======================*/
	ulint		flags,	/*!< in: undo logging and locking flags */
	btr_cur_t*	cursor,	/*!< in: cursor on the record to update;
				cursor stays valid and positioned on the
				same record */
	const upd_t*	update,	/*!< in: update vector; this must also
				contain trx id and roll ptr fields */
	ulint		cmpl_info,/*!< in: compiler info on secondary index
				updates */
	que_thr_t*	thr,	/*!< in: query thread */
	mtr_t*		mtr);	/*!< in: mtr; must be committed before
				latching any further pages */
/*************************************************************//**
Performs an update of a record on a page of a tree. It is assumed
that mtr holds an x-latch on the tree and on the cursor page. If the
update is made on the leaf level, to avoid deadlocks, mtr must also
own x-latches to brothers of page, if those brothers exist.
@return	DB_SUCCESS or error code */
UNIV_INTERN
ulint
btr_cur_pessimistic_update(
/*=======================*/
	ulint		flags,	/*!< in: undo logging, locking, and rollback
				flags */
	btr_cur_t*	cursor,	/*!< in/out: cursor on the record to update;
				cursor may become invalid if *big_rec == NULL
				|| !(flags & BTR_KEEP_POS_FLAG) */
	mem_heap_t**	heap,	/*!< in/out: pointer to memory heap, or NULL */
	big_rec_t**	big_rec,/*!< out: big rec vector whose fields have to
				be stored externally by the caller, or NULL */
	const upd_t*	update,	/*!< in: update vector; this is allowed also
				contain trx id and roll ptr fields, but
				the values in update vector have no effect */
	ulint		cmpl_info,/*!< in: compiler info on secondary index
				updates */
	que_thr_t*	thr,	/*!< in: query thread */
	mtr_t*		mtr);	/*!< in: mtr; must be committed before
				latching any further pages */
/***********************************************************//**
Marks a clustered index record deleted. Writes an undo log record to
undo log on this delete marking. Writes in the trx id field the id
of the deleting transaction, and in the roll ptr field pointer to the
undo log record created.
@return	DB_SUCCESS, DB_LOCK_WAIT, or error number */
UNIV_INTERN
ulint
btr_cur_del_mark_set_clust_rec(
/*===========================*/
	ulint		flags,	/*!< in: undo logging and locking flags */
	buf_block_t*	block,	/*!< in/out: buffer block of the record */
	rec_t*		rec,	/*!< in/out: record */
	dict_index_t*	index,	/*!< in: clustered index of the record */
	const ulint*	offsets,/*!< in: rec_get_offsets(rec) */
	ibool		val,	/*!< in: value to set */
	que_thr_t*	thr,	/*!< in: query thread */
	mtr_t*		mtr)	/*!< in: mtr */
	__attribute__((nonnull));
/***********************************************************//**
Sets a secondary index record delete mark to TRUE or FALSE.
@return	DB_SUCCESS, DB_LOCK_WAIT, or error number */
UNIV_INTERN
ulint
btr_cur_del_mark_set_sec_rec(
/*=========================*/
	ulint		flags,	/*!< in: locking flag */
	btr_cur_t*	cursor,	/*!< in: cursor */
	ibool		val,	/*!< in: value to set */
	que_thr_t*	thr,	/*!< in: query thread */
	mtr_t*		mtr);	/*!< in: mtr */
/*************************************************************//**
Tries to compress a page of the tree if it seems useful. It is assumed
that mtr holds an x-latch on the tree and on the cursor page. To avoid
deadlocks, mtr must also own x-latches to brothers of page, if those
brothers exist. NOTE: it is assumed that the caller has reserved enough
free extents so that the compression will always succeed if done!
@return	TRUE if compression occurred */
UNIV_INTERN
ibool
btr_cur_compress_if_useful(
/*=======================*/
	btr_cur_t*	cursor,	/*!< in/out: cursor on the page to compress;
				cursor does not stay valid if compression
				occurs */
	ibool		adjust,	/*!< in: TRUE if should adjust the
				cursor position even if compression occurs */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
	__attribute__((nonnull));
/*******************************************************//**
Removes the record on which the tree cursor is positioned. It is assumed
that the mtr has an x-latch on the page where the cursor is positioned,
but no latch on the whole tree.
@return	TRUE if success, i.e., the page did not become too empty */
UNIV_INTERN
ibool
btr_cur_optimistic_delete(
/*======================*/
	btr_cur_t*	cursor,	/*!< in: cursor on the record to delete;
				cursor stays valid: if deletion succeeds,
				on function exit it points to the successor
				of the deleted record */
	mtr_t*		mtr);	/*!< in: mtr; if this function returns
				TRUE on a leaf page of a secondary
				index, the mtr must be committed
				before latching any further pages */
/*************************************************************//**
Removes the record on which the tree cursor is positioned. Tries
to compress the page if its fillfactor drops below a threshold
or if it is the only page on the level. It is assumed that mtr holds
an x-latch on the tree and on the cursor page. To avoid deadlocks,
mtr must also own x-latches to brothers of page, if those brothers
exist.
@return	TRUE if compression occurred */
UNIV_INTERN
ibool
btr_cur_pessimistic_delete(
/*=======================*/
	ulint*		err,	/*!< out: DB_SUCCESS or DB_OUT_OF_FILE_SPACE;
				the latter may occur because we may have
				to update node pointers on upper levels,
				and in the case of variable length keys
				these may actually grow in size */
	ibool		has_reserved_extents, /*!< in: TRUE if the
				caller has already reserved enough free
				extents so that he knows that the operation
				will succeed */
	btr_cur_t*	cursor,	/*!< in: cursor on the record to delete;
				if compression does not occur, the cursor
				stays valid: it points to successor of
				deleted record on function exit */
	enum trx_rb_ctx	rb_ctx,	/*!< in: rollback context */
	mtr_t*		mtr);	/*!< in: mtr */
#endif /* !UNIV_HOTBACKUP */
/***********************************************************//**
Parses a redo log record of updating a record in-place.
@return	end of log record or NULL */
UNIV_INTERN
byte*
btr_cur_parse_update_in_place(
/*==========================*/
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr,/*!< in: buffer end */
	page_t*		page,	/*!< in/out: page or NULL */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page, or NULL */
	dict_index_t*	index);	/*!< in: index corresponding to page */
/****************************************************************//**
Parses the redo log record for delete marking or unmarking of a clustered
index record.
@return	end of log record or NULL */
UNIV_INTERN
byte*
btr_cur_parse_del_mark_set_clust_rec(
/*=================================*/
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr,/*!< in: buffer end */
	page_t*		page,	/*!< in/out: page or NULL */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page, or NULL */
	dict_index_t*	index);	/*!< in: index corresponding to page */
/****************************************************************//**
Parses the redo log record for delete marking or unmarking of a secondary
index record.
@return	end of log record or NULL */
UNIV_INTERN
byte*
btr_cur_parse_del_mark_set_sec_rec(
/*===============================*/
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr,/*!< in: buffer end */
	page_t*		page,	/*!< in/out: page or NULL */
	page_zip_des_t*	page_zip);/*!< in/out: compressed page, or NULL */
#ifndef UNIV_HOTBACKUP
/*******************************************************************//**
Estimates the number of rows in a given index range.
@return	estimated number of rows */
UNIV_INTERN
ib_int64_t
btr_estimate_n_rows_in_range(
/*=========================*/
	dict_index_t*	index,	/*!< in: index */
	const dtuple_t*	tuple1,	/*!< in: range start, may also be empty tuple */
	ulint		mode1,	/*!< in: search mode for range start */
	const dtuple_t*	tuple2,	/*!< in: range end, may also be empty tuple */
	ulint		mode2);	/*!< in: search mode for range end */
/*******************************************************************//**
Estimates the number of different key values in a given index, for
each n-column prefix of the index where n <= dict_index_get_n_unique(index).
The estimates are stored in the array index->stat_n_diff_key_vals.
If innodb_stats_method is nulls_ignored, we also record the number of
non-null values for each prefix and stored the estimates in
array index->stat_n_non_null_key_vals. */
UNIV_INTERN
void
btr_estimate_number_of_different_key_vals(
/*======================================*/
	dict_index_t*	index);	/*!< in: index */
/*******************************************************************//**
Marks non-updated off-page fields as disowned by this record. The ownership
must be transferred to the updated record which is inserted elsewhere in the
index tree. In purge only the owner of externally stored field is allowed
to free the field. */
UNIV_INTERN
void
btr_cur_disown_inherited_fields(
/*============================*/
	page_zip_des_t*	page_zip,/*!< in/out: compressed page whose uncompressed
				part will be updated, or NULL */
	rec_t*		rec,	/*!< in/out: record in a clustered index */
	dict_index_t*	index,	/*!< in: index of the page */
	const ulint*	offsets,/*!< in: array returned by rec_get_offsets() */
	const upd_t*	update,	/*!< in: update vector */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
	__attribute__((nonnull(2,3,4,5,6)));

/** Operation code for btr_store_big_rec_extern_fields(). */
enum blob_op {
	/** Store off-page columns for a freshly inserted record */
	BTR_STORE_INSERT = 0,
	/** Store off-page columns for an insert by update */
	BTR_STORE_INSERT_UPDATE,
	/** Store off-page columns for an update */
	BTR_STORE_UPDATE
};

/*******************************************************************//**
Determine if an operation on off-page columns is an update.
@return TRUE if op != BTR_STORE_INSERT */
UNIV_INLINE
ibool
btr_blob_op_is_update(
/*==================*/
	enum blob_op	op)	/*!< in: operation */
	__attribute__((warn_unused_result));

/*******************************************************************//**
Stores the fields in big_rec_vec to the tablespace and puts pointers to
them in rec.  The extern flags in rec will have to be set beforehand.
The fields are stored on pages allocated from leaf node
file segment of the index tree.
@return	DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
UNIV_INTERN
enum db_err
btr_store_big_rec_extern_fields(
/*============================*/
	dict_index_t*	index,		/*!< in: index of rec; the index tree
					MUST be X-latched */
	buf_block_t*	rec_block,	/*!< in/out: block containing rec */
	rec_t*		rec,		/*!< in/out: record */
	const ulint*	offsets,	/*!< in: rec_get_offsets(rec, index);
					the "external storage" flags in offsets
					will not correspond to rec when
					this function returns */
	const big_rec_t*big_rec_vec,	/*!< in: vector containing fields
					to be stored externally */
	mtr_t*		btr_mtr,	/*!< in: mtr containing the
					latches to the clustered index */
	enum blob_op	op)		/*! in: operation code */
	__attribute__((nonnull, warn_unused_result));

/*******************************************************************//**
Frees the space in an externally stored field to the file space
management if the field in data is owned the externally stored field,
in a rollback we may have the additional condition that the field must
not be inherited. */
UNIV_INTERN
void
btr_free_externally_stored_field(
/*=============================*/
	dict_index_t*	index,		/*!< in: index of the data, the index
					tree MUST be X-latched; if the tree
					height is 1, then also the root page
					must be X-latched! (this is relevant
					in the case this function is called
					from purge where 'data' is located on
					an undo log page, not an index
					page) */
	byte*		field_ref,	/*!< in/out: field reference */
	const rec_t*	rec,		/*!< in: record containing field_ref, for
					page_zip_write_blob_ptr(), or NULL */
	const ulint*	offsets,	/*!< in: rec_get_offsets(rec, index),
					or NULL */
	page_zip_des_t*	page_zip,	/*!< in: compressed page corresponding
					to rec, or NULL if rec == NULL */
	ulint		i,		/*!< in: field number of field_ref;
					ignored if rec == NULL */
	enum trx_rb_ctx	rb_ctx,		/*!< in: rollback context */
	mtr_t*		local_mtr);	/*!< in: mtr containing the latch to
					data an an X-latch to the index
					tree */
/*******************************************************************//**
Copies the prefix of an externally stored field of a record.  The
clustered index record must be protected by a lock or a page latch.
@return the length of the copied field, or 0 if the column was being
or has been deleted */
UNIV_INTERN
ulint
btr_copy_externally_stored_field_prefix(
/*====================================*/
	byte*		buf,	/*!< out: the field, or a prefix of it */
	ulint		len,	/*!< in: length of buf, in bytes */
	ulint		zip_size,/*!< in: nonzero=compressed BLOB page size,
				zero for uncompressed BLOBs */
	const byte*	data,	/*!< in: 'internally' stored part of the
				field containing also the reference to
				the external part; must be protected by
				a lock or a page latch */
	ulint		local_len);/*!< in: length of data, in bytes */
/*******************************************************************//**
Copies an externally stored field of a record to mem heap.
@return	the field copied to heap, or NULL if the field is incomplete */
UNIV_INTERN
byte*
btr_rec_copy_externally_stored_field(
/*=================================*/
	const rec_t*	rec,	/*!< in: record in a clustered index;
				must be protected by a lock or a page latch */
	const ulint*	offsets,/*!< in: array returned by rec_get_offsets() */
	ulint		zip_size,/*!< in: nonzero=compressed BLOB page size,
				zero for uncompressed BLOBs */
	ulint		no,	/*!< in: field number */
	ulint*		len,	/*!< out: length of the field */
	mem_heap_t*	heap);	/*!< in: mem heap */
/*******************************************************************//**
Flags the data tuple fields that are marked as extern storage in the
update vector.  We use this function to remember which fields we must
mark as extern storage in a record inserted for an update.
@return	number of flagged external columns */
UNIV_INTERN
ulint
btr_push_update_extern_fields(
/*==========================*/
	dtuple_t*	tuple,	/*!< in/out: data tuple */
	const upd_t*	update,	/*!< in: update vector */
	mem_heap_t*	heap)	/*!< in: memory heap */
	__attribute__((nonnull));
/***********************************************************//**
Sets a secondary index record's delete mark to the given value. This
function is only used by the insert buffer merge mechanism. */
UNIV_INTERN
void
btr_cur_set_deleted_flag_for_ibuf(
/*==============================*/
	rec_t*		rec,		/*!< in/out: record */
	page_zip_des_t*	page_zip,	/*!< in/out: compressed page
					corresponding to rec, or NULL
					when the tablespace is
					uncompressed */
	ibool		val,		/*!< in: value to set */
	mtr_t*		mtr);		/*!< in/out: mini-transaction */

/***********************************************************//**
Writes a redo log record of updating a record in-place. */
UNIV_INTERN
void
btr_cur_update_in_place_log(
/*========================*/
	ulint		flags,		/*!< in: flags */
	rec_t*		rec,		/*!< in: record */
	dict_index_t*	index,		/*!< in: index where cursor positioned */
	const upd_t*	update,		/*!< in: update vector */
	trx_t*		trx,		/*!< in: transaction */
	roll_ptr_t	roll_ptr,	/*!< in: roll ptr */
	mtr_t*		mtr);		/*!< in: mtr */

/*######################################################################*/

/** In the pessimistic delete, if the page data size drops below this
limit, merging it to a neighbor is tried */
#define BTR_CUR_PAGE_COMPRESS_LIMIT	(UNIV_PAGE_SIZE / 2)

/** A slot in the path array. We store here info on a search path down the
tree. Each slot contains data on a single level of the tree. */

typedef struct btr_path_struct	btr_path_t;
struct btr_path_struct{
	ulint	nth_rec;	/*!< index of the record
				where the page cursor stopped on
				this level (index in alphabetical
				order); value ULINT_UNDEFINED
				denotes array end */
	ulint	n_recs;		/*!< number of records on the page */
	ulint	page_no;	/*!< no of the page containing the record */
	ulint	page_level;	/*!< level of the page, if later we fetch
				the page under page_no and it is no different
				level then we know that the tree has been
				reorganized */
};

#define BTR_PATH_ARRAY_N_SLOTS	250	/*!< size of path array (in slots) */

/** Values for the flag documenting the used search method */
enum btr_cur_method {
	BTR_CUR_HASH = 1,	/*!< successful shortcut using
				the hash index */
	BTR_CUR_HASH_FAIL,	/*!< failure using hash, success using
				binary search: the misleading hash
				reference is stored in the field
				hash_node, and might be necessary to
				update */
	BTR_CUR_BINARY,		/*!< success using the binary search */
	BTR_CUR_INSERT_TO_IBUF,	/*!< performed the intended insert to
				the insert buffer */
	BTR_CUR_DEL_MARK_IBUF,	/*!< performed the intended delete
				mark in the insert/delete buffer */
	BTR_CUR_DELETE_IBUF,	/*!< performed the intended delete in
				the insert/delete buffer */
	BTR_CUR_DELETE_REF	/*!< row_purge_poss_sec() failed */
};

/** The tree cursor: the definition appears here only for the compiler
to know struct size! */
struct btr_cur_struct {
	dict_index_t*	index;		/*!< index where positioned */
	page_cur_t	page_cur;	/*!< page cursor */
	purge_node_t*	purge_node;	/*!< purge node, for BTR_DELETE */
	buf_block_t*	left_block;	/*!< this field is used to store
					a pointer to the left neighbor
					page, in the cases
					BTR_SEARCH_PREV and
					BTR_MODIFY_PREV */
	/*------------------------------*/
	que_thr_t*	thr;		/*!< this field is only used
					when btr_cur_search_to_nth_level
					is called for an index entry
					insertion: the calling query
					thread is passed here to be
					used in the insert buffer */
	/*------------------------------*/
	/** The following fields are used in
	btr_cur_search_to_nth_level to pass information: */
	/* @{ */
	enum btr_cur_method	flag;	/*!< Search method used */
	ulint		tree_height;	/*!< Tree height if the search is done
					for a pessimistic insert or update
					operation */
	ulint		up_match;	/*!< If the search mode was PAGE_CUR_LE,
					the number of matched fields to the
					the first user record to the right of
					the cursor record after
					btr_cur_search_to_nth_level;
					for the mode PAGE_CUR_GE, the matched
					fields to the first user record AT THE
					CURSOR or to the right of it;
					NOTE that the up_match and low_match
					values may exceed the correct values
					for comparison to the adjacent user
					record if that record is on a
					different leaf page! (See the note in
					row_ins_duplicate_key.) */
	ulint		up_bytes;	/*!< number of matched bytes to the
					right at the time cursor positioned;
					only used internally in searches: not
					defined after the search */
	ulint		low_match;	/*!< if search mode was PAGE_CUR_LE,
					the number of matched fields to the
					first user record AT THE CURSOR or
					to the left of it after
					btr_cur_search_to_nth_level;
					NOT defined for PAGE_CUR_GE or any
					other search modes; see also the NOTE
					in up_match! */
	ulint		low_bytes;	/*!< number of matched bytes to the
					right at the time cursor positioned;
					only used internally in searches: not
					defined after the search */
	ulint		n_fields;	/*!< prefix length used in a hash
					search if hash_node != NULL */
	ulint		n_bytes;	/*!< hash prefix bytes if hash_node !=
					NULL */
	ulint		fold;		/*!< fold value used in the search if
					flag is BTR_CUR_HASH */
	/* @} */
	btr_path_t*	path_arr;	/*!< in estimating the number of
					rows in range, we store in this array
					information of the path through
					the tree */
};

/** If pessimistic delete fails because of lack of file space, there
is still a good change of success a little later.  Try this many
times. */
#define BTR_CUR_RETRY_DELETE_N_TIMES	100
/** If pessimistic delete fails because of lack of file space, there
is still a good change of success a little later.  Sleep this many
microseconds between retries. */
#define BTR_CUR_RETRY_SLEEP_TIME	50000

/** The reference in a field for which data is stored on a different page.
The reference is at the end of the 'locally' stored part of the field.
'Locally' means storage in the index record.
We store locally a long enough prefix of each column so that we can determine
the ordering parts of each index record without looking into the externally
stored part. */
/*-------------------------------------- @{ */
#define BTR_EXTERN_SPACE_ID		0	/*!< space id where stored */
#define BTR_EXTERN_PAGE_NO		4	/*!< page no where stored */
#define BTR_EXTERN_OFFSET		8	/*!< offset of BLOB header
						on that page */
#define BTR_EXTERN_LEN			12	/*!< 8 bytes containing the
						length of the externally
						stored part of the BLOB.
						The 2 highest bits are
						reserved to the flags below. */
/*-------------------------------------- @} */
/* #define BTR_EXTERN_FIELD_REF_SIZE	20 // moved to btr0types.h */

/** The most significant bit of BTR_EXTERN_LEN (i.e., the most
significant bit of the byte at smallest address) is set to 1 if this
field does not 'own' the externally stored field; only the owner field
is allowed to free the field in purge! */
#define BTR_EXTERN_OWNER_FLAG		128
/** If the second most significant bit of BTR_EXTERN_LEN (i.e., the
second most significant bit of the byte at smallest address) is 1 then
it means that the externally stored field was inherited from an
earlier version of the row.  In rollback we are not allowed to free an
inherited external field. */
#define BTR_EXTERN_INHERITED_FLAG	64

/** Number of searches down the B-tree in btr_cur_search_to_nth_level(). */
extern ulint	btr_cur_n_non_sea;
/** Number of successful adaptive hash index lookups in
btr_cur_search_to_nth_level(). */
extern ulint	btr_cur_n_sea;
/** Old value of btr_cur_n_non_sea.  Copied by
srv_refresh_innodb_monitor_stats().  Referenced by
srv_printf_innodb_monitor(). */
extern ulint	btr_cur_n_non_sea_old;
/** Old value of btr_cur_n_sea.  Copied by
srv_refresh_innodb_monitor_stats().  Referenced by
srv_printf_innodb_monitor(). */
extern ulint	btr_cur_n_sea_old;
#endif /* !UNIV_HOTBACKUP */

#ifdef UNIV_DEBUG
/* Flag to limit optimistic insert records */
extern uint	btr_cur_limit_optimistic_insert_debug;
#endif /* UNIV_DEBUG */

/*******************************************************************//**
Print information about old page and a new page on a B-tree when
we note that page types do not match.*/
void
btr_pages_info(
	page_t* old_page,	/*!< in: Page where we were */
	page_t* new_page,	/*!< in: Page where we travelsed */
	ulint	space_id,	/*!< in: space id */
	ulint	zip_size,	/*!< in: zip size */
	ulint	page_no,	/*!< in: Page id where travelsed */
	ulint	latch_mode,	/*!< in: Used latch mode */
	dict_index_t* index,	/*!< in: Used index */
	ulint	old_next_page_no, /*!< in: Next page number from old page */
	ulint	old_prev_page_no, /*!< in: Prev page number from old page */
	ulint	new_space_id,	  /*!< in: Space id of new page */
	ulint	new_zip_size,	  /*!< in: Zip size of new page */
	ulint	new_next_page_no, /*!< in: Next page number from new page */
	ulint	new_prev_page_no, /*!< in: Prev page number from new page */
	mtr_t*  mtr,		/*!< in: mini transaction */
	const char*  file,	/*!< in: file name where called */
	ulint	line);		/*!< in: line number where called */

#ifndef UNIV_NONINL
#include "btr0cur.ic"
#endif

#endif
