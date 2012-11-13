/*****************************************************************************

Copyright (c) 1997, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file row/row0vers.cc
Row versions

Created 2/6/1997 Heikki Tuuri
*******************************************************/

#include "row0vers.h"

#ifdef UNIV_NONINL
#include "row0vers.ic"
#endif

#include "dict0dict.h"
#include "dict0boot.h"
#include "btr0btr.h"
#include "mach0data.h"
#include "trx0rseg.h"
#include "trx0trx.h"
#include "trx0roll.h"
#include "trx0undo.h"
#include "trx0purge.h"
#include "trx0rec.h"
#include "que0que.h"
#include "row0row.h"
#include "row0upd.h"
#include "rem0cmp.h"
#include "read0read.h"
#include "lock0lock.h"

/*****************************************************************//**
Finds out if an active transaction has inserted or modified a secondary
index record.
@return 0 if committed, else the active transaction id;
NOTE that this function can return false positives but never false
negatives. The caller must confirm all positive results by calling
trx_is_active() while holding lock_sys->mutex. */
UNIV_INLINE
trx_id_t
row_vers_impl_x_locked_low(
/*=======================*/
	const rec_t*	clust_rec,	/*!< in: clustered index record */
	dict_index_t*	clust_index,	/*!< in: the clustered index */
	const rec_t*	rec,		/*!< in: secondary index record */
	dict_index_t*	index,		/*!< in: the secondary index */
	const ulint*	offsets,	/*!< in: rec_get_offsets(rec, index) */
	mtr_t*		mtr)		/*!< in/out: mini-transaction */
{
	trx_id_t	trx_id;
	ibool		corrupt;
	ulint		comp;
	ulint		rec_del;
	const rec_t*	version;
	rec_t*		prev_version = NULL;
	ulint*		clust_offsets;
	mem_heap_t*	heap;

	ut_ad(rec_offs_validate(rec, index, offsets));

	heap = mem_heap_create(1024);

	clust_offsets = rec_get_offsets(
		clust_rec, clust_index, NULL, ULINT_UNDEFINED, &heap);

	trx_id = row_get_rec_trx_id(clust_rec, clust_index, clust_offsets);
	corrupt = FALSE;

	if (!trx_rw_is_active(trx_id, &corrupt)) {
		/* The transaction that modified or inserted clust_rec is no
		longer active, or it is corrupt: no implicit lock on rec */
		if (corrupt) {
			lock_report_trx_id_insanity(
				trx_id, clust_rec, clust_index, clust_offsets,
				trx_sys_get_max_trx_id());
		}
		mem_heap_free(heap);
		return(0);
	}

	comp = page_rec_is_comp(rec);
	ut_ad(index->table == clust_index->table);
	ut_ad(!!comp == dict_table_is_comp(index->table));
	ut_ad(!comp == !page_rec_is_comp(clust_rec));

	rec_del = rec_get_deleted_flag(rec, comp);

	/* We look up if some earlier version, which was modified by
	the trx_id transaction, of the clustered index record would
	require rec to be in a different state (delete marked or
	unmarked, or have different field values, or not existing). If
	there is such a version, then rec was modified by the trx_id
	transaction, and it has an implicit x-lock on rec. Note that
	if clust_rec itself would require rec to be in a different
	state, then the trx_id transaction has not yet had time to
	modify rec, and does not necessarily have an implicit x-lock
	on rec. */

	for (version = clust_rec;; version = prev_version) {
		dberr_t		err;
		row_ext_t*	ext;
		const dtuple_t*	row;
		dtuple_t*	entry;
		ulint		vers_del;
		trx_id_t	prev_trx_id;
		mem_heap_t*	old_heap = heap;

		/* We keep the semaphore in mtr on the clust_rec page, so
		that no other transaction can update it and get an
		implicit x-lock on rec until mtr_commit(mtr). */

		heap = mem_heap_create(1024);

		err = trx_undo_prev_version_build(
			clust_rec, mtr, version, clust_index, clust_offsets,
			heap, &prev_version);

 		/* Free version and clust_offsets. */

		mem_heap_free(old_heap);

		if (prev_version == NULL) {

			/* clust_rec must be a fresh insert, because
			no previous version was found or the transaction
			has committed. The caller has to recheck as the
			synopsis of this function states, whether trx_id
			is active or not. */

			ut_a(err == DB_SUCCESS || err == DB_MISSING_HISTORY);

			break;
		}

		clust_offsets = rec_get_offsets(
			prev_version, clust_index, NULL, ULINT_UNDEFINED,
			&heap);

		vers_del = rec_get_deleted_flag(prev_version, comp);

		prev_trx_id = row_get_rec_trx_id(prev_version, clust_index,
						 clust_offsets);

		/* The stack of versions is locked by mtr.  Thus, it
		is safe to fetch the prefixes for externally stored
		columns. */

		row = row_build(ROW_COPY_POINTERS, clust_index, prev_version,
				clust_offsets,
				NULL, NULL, NULL, &ext, heap);

		entry = row_build_index_entry(row, ext, index, heap);

		/* entry may be NULL if a record was inserted in place
		of a deleted record, and the BLOB pointers of the new
		record were not initialized yet.  But in that case,
		prev_version should be NULL. */

		ut_a(entry != NULL);

		/* If we get here, we know that the trx_id transaction
		modified prev_version. Let us check if prev_version
		would require rec to be in a different state. */

		/* The previous version of clust_rec must be
		accessible, because clust_rec was not a fresh insert.
		There is no guarantee that the transaction is still
		active. */

		ut_ad(err == DB_SUCCESS);

		/* We check if entry and rec are identified in the alphabetical
		ordering */

		if (!trx_rw_is_active(trx_id, &corrupt)) {
			/* Transaction no longer active: no implicit
			x-lock. This situation should only be possible
			because we are not holding lock_sys->mutex. */
			ut_ad(!lock_mutex_own());
			if (corrupt) {
				lock_report_trx_id_insanity(
					trx_id,
					prev_version, clust_index,
					clust_offsets,
					trx_sys_get_max_trx_id());
			}
			trx_id = 0;
			break;
		} else if (0 == cmp_dtuple_rec(entry, rec, offsets)) {
			/* The delete marks of rec and prev_version should be
			equal for rec to be in the state required by
			prev_version */

			if (rec_del != vers_del) {

				break;
			}

			/* It is possible that the row was updated so that the
			secondary index record remained the same in
			alphabetical ordering, but the field values changed
			still. For example, 'abc' -> 'ABC'. Check also that. */

			dtuple_set_types_binary(
				entry, dtuple_get_n_fields(entry));

			if (0 != cmp_dtuple_rec(entry, rec, offsets)) {

				break;
			}

		} else if (!rec_del) {
			/* The delete mark should be set in rec for it to be
			in the state required by prev_version */

			break;
		}

		if (trx_id != prev_trx_id) {
			/* prev_version was the first version modified by
			the trx_id transaction: no implicit x-lock */

			trx_id = 0;
			break;
		}
	}

	mem_heap_free(heap);
	return(trx_id);
}

/*****************************************************************//**
Finds out if an active transaction has inserted or modified a secondary
index record.
@return 0 if committed, else the active transaction id;
NOTE that this function can return false positives but never false
negatives. The caller must confirm all positive results by calling
trx_is_active() while holding lock_sys->mutex. */
UNIV_INTERN
trx_id_t
row_vers_impl_x_locked(
/*===================*/
	const rec_t*	rec,	/*!< in: record in a secondary index */
	dict_index_t*	index,	/*!< in: the secondary index */
	const ulint*	offsets)/*!< in: rec_get_offsets(rec, index) */
{
	dict_index_t*	clust_index;
	const rec_t*	clust_rec;
	trx_id_t	trx_id;
	mtr_t		mtr;

	ut_ad(!lock_mutex_own());
	ut_ad(!mutex_own(&trx_sys->mutex));

	mtr_start(&mtr);

	/* Search for the clustered index record. The latch on the
	page of clust_rec locks the top of the stack of versions. The
	bottom of the version stack is not locked; oldest versions may
	disappear by the fact that transactions may be committed and
	collected by the purge. This is not a problem, because we are
	only interested in active transactions. */

	clust_rec = row_get_clust_rec(
		BTR_SEARCH_LEAF, rec, index, &clust_index, &mtr);

	if (UNIV_UNLIKELY(!clust_rec)) {
		/* In a rare case it is possible that no clust rec is found
		for a secondary index record: if in row0umod.cc
		row_undo_mod_remove_clust_low() we have already removed the
		clust rec, while purge is still cleaning and removing
		secondary index records associated with earlier versions of
		the clustered index record. In that case there cannot be
		any implicit lock on the secondary index record, because
		an active transaction which has modified the secondary index
		record has also modified the clustered index record. And in
		a rollback we always undo the modifications to secondary index
		records before the clustered index record. */

		trx_id = 0;
	} else {
		trx_id = row_vers_impl_x_locked_low(
			clust_rec, clust_index, rec, index, offsets, &mtr);
	}

	mtr_commit(&mtr);

	return(trx_id);
}

/*****************************************************************//**
Finds out if we must preserve a delete marked earlier version of a clustered
index record, because it is >= the purge view.
@return	TRUE if earlier version should be preserved */
UNIV_INTERN
ibool
row_vers_must_preserve_del_marked(
/*==============================*/
	trx_id_t	trx_id,	/*!< in: transaction id in the version */
	mtr_t*		mtr)	/*!< in: mtr holding the latch on the
				clustered index record; it will also
				hold the latch on purge_view */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(&(purge_sys->latch), RW_LOCK_SHARED));
#endif /* UNIV_SYNC_DEBUG */

	mtr_s_lock(&(purge_sys->latch), mtr);

	return(!read_view_sees_trx_id(purge_sys->view, trx_id));
}

/*****************************************************************//**
Finds out if a version of the record, where the version >= the current
purge view, should have ientry as its secondary index entry. We check
if there is any not delete marked version of the record where the trx
id >= purge view, and the secondary index entry and ientry are identified in
the alphabetical ordering; exactly in this case we return TRUE.
@return	TRUE if earlier version should have */
UNIV_INTERN
ibool
row_vers_old_has_index_entry(
/*=========================*/
	ibool		also_curr,/*!< in: TRUE if also rec is included in the
				versions to search; otherwise only versions
				prior to it are searched */
	const rec_t*	rec,	/*!< in: record in the clustered index; the
				caller must have a latch on the page */
	mtr_t*		mtr,	/*!< in: mtr holding the latch on rec; it will
				also hold the latch on purge_view */
	dict_index_t*	index,	/*!< in: the secondary index */
	const dtuple_t*	ientry)	/*!< in: the secondary index entry */
{
	const rec_t*	version;
	rec_t*		prev_version;
	dict_index_t*	clust_index;
	ulint*		clust_offsets;
	mem_heap_t*	heap;
	mem_heap_t*	heap2;
	const dtuple_t*	row;
	const dtuple_t*	entry;
	ulint		err;
	ulint		comp;

	ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX)
	      || mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(&(purge_sys->latch), RW_LOCK_SHARED));
#endif /* UNIV_SYNC_DEBUG */

	clust_index = dict_table_get_first_index(index->table);

	comp = page_rec_is_comp(rec);
	ut_ad(!dict_table_is_comp(index->table) == !comp);
	heap = mem_heap_create(1024);
	clust_offsets = rec_get_offsets(rec, clust_index, NULL,
					ULINT_UNDEFINED, &heap);

	if (also_curr && !rec_get_deleted_flag(rec, comp)) {
		row_ext_t*	ext;

		/* The top of the stack of versions is locked by the
		mtr holding a latch on the page containing the
		clustered index record. The bottom of the stack is
		locked by the fact that the purge_sys->view must
		'overtake' any read view of an active transaction.
		Thus, it is safe to fetch the prefixes for
		externally stored columns. */
		row = row_build(ROW_COPY_POINTERS, clust_index,
				rec, clust_offsets,
				NULL, NULL, NULL, &ext, heap);
		entry = row_build_index_entry(row, ext, index, heap);

		/* If entry == NULL, the record contains unset BLOB
		pointers.  This must be a freshly inserted record.  If
		this is called from
		row_purge_remove_sec_if_poss_low(), the thread will
		hold latches on the clustered index and the secondary
		index.  Because the insert works in three steps:

			(1) insert the record to clustered index
			(2) store the BLOBs and update BLOB pointers
			(3) insert records to secondary indexes

		the purge thread can safely ignore freshly inserted
		records and delete the secondary index record.  The
		thread that inserted the new record will be inserting
		the secondary index records. */

		/* NOTE that we cannot do the comparison as binary
		fields because the row is maybe being modified so that
		the clustered index record has already been updated to
		a different binary value in a char field, but the
		collation identifies the old and new value anyway! */
		if (entry && !dtuple_coll_cmp(ientry, entry)) {

			mem_heap_free(heap);

			return(TRUE);
		}
	}

	version = rec;

	for (;;) {
		heap2 = heap;
		heap = mem_heap_create(1024);
		err = trx_undo_prev_version_build(rec, mtr, version,
						  clust_index, clust_offsets,
						  heap, &prev_version);
		mem_heap_free(heap2); /* free version and clust_offsets */

		if (err != DB_SUCCESS || !prev_version) {
			/* Versions end here */

			mem_heap_free(heap);

			return(FALSE);
		}

		clust_offsets = rec_get_offsets(prev_version, clust_index,
						NULL, ULINT_UNDEFINED, &heap);

		if (!rec_get_deleted_flag(prev_version, comp)) {
			row_ext_t*	ext;

			/* The stack of versions is locked by mtr.
			Thus, it is safe to fetch the prefixes for
			externally stored columns. */
			row = row_build(ROW_COPY_POINTERS, clust_index,
					prev_version, clust_offsets,
					NULL, NULL, NULL, &ext, heap);
			entry = row_build_index_entry(row, ext, index, heap);

			/* If entry == NULL, the record contains unset
			BLOB pointers.  This must be a freshly
			inserted record that we can safely ignore.
			For the justification, see the comments after
			the previous row_build_index_entry() call. */

			/* NOTE that we cannot do the comparison as binary
			fields because maybe the secondary index record has
			already been updated to a different binary value in
			a char field, but the collation identifies the old
			and new value anyway! */

			if (entry && !dtuple_coll_cmp(ientry, entry)) {

				mem_heap_free(heap);

				return(TRUE);
			}
		}

		version = prev_version;
	}
}

/*****************************************************************//**
Constructs the version of a clustered index record which a consistent
read should see. We assume that the trx id stored in rec is such that
the consistent read should not see rec in its present version.
@return	DB_SUCCESS or DB_MISSING_HISTORY */
UNIV_INTERN
dberr_t
row_vers_build_for_consistent_read(
/*===============================*/
	const rec_t*	rec,	/*!< in: record in a clustered index; the
				caller must have a latch on the page; this
				latch locks the top of the stack of versions
				of this records */
	mtr_t*		mtr,	/*!< in: mtr holding the latch on rec */
	dict_index_t*	index,	/*!< in: the clustered index */
	ulint**		offsets,/*!< in/out: offsets returned by
				rec_get_offsets(rec, index) */
	read_view_t*	view,	/*!< in: the consistent read view */
	mem_heap_t**	offset_heap,/*!< in/out: memory heap from which
				the offsets are allocated */
	mem_heap_t*	in_heap,/*!< in: memory heap from which the memory for
				*old_vers is allocated; memory for possible
				intermediate versions is allocated and freed
				locally within the function */
	rec_t**		old_vers)/*!< out, own: old version, or NULL if the
				record does not exist in the view, that is,
				it was freshly inserted afterwards */
{
	const rec_t*	version;
	rec_t*		prev_version;
	trx_id_t	trx_id;
	mem_heap_t*	heap		= NULL;
	byte*		buf;
	dberr_t		err;

	ut_ad(dict_index_is_clust(index));
	ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX)
	      || mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(&(purge_sys->latch), RW_LOCK_SHARED));
#endif /* UNIV_SYNC_DEBUG */

	ut_ad(rec_offs_validate(rec, index, *offsets));

	trx_id = row_get_rec_trx_id(rec, index, *offsets);

	ut_ad(!read_view_sees_trx_id(view, trx_id));

	version = rec;

	for (;;) {
		mem_heap_t*	heap2	= heap;
		trx_undo_rec_t* undo_rec;
		roll_ptr_t	roll_ptr;
		undo_no_t	undo_no;
		heap = mem_heap_create(1024);

		/* If we have high-granularity consistent read view and
		creating transaction of the view is the same as trx_id in
		the record we see this record only in the case when
		undo_no of the record is < undo_no in the view. */

		if (view->type == VIEW_HIGH_GRANULARITY
		    && view->creator_trx_id == trx_id) {

			roll_ptr = row_get_rec_roll_ptr(version, index,
							*offsets);
			undo_rec = trx_undo_get_undo_rec_low(roll_ptr, heap);
			undo_no = trx_undo_rec_get_undo_no(undo_rec);
			mem_heap_empty(heap);

			if (view->undo_no > undo_no) {
				/* The view already sees this version: we can
				copy it to in_heap and return */

#if defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG
				ut_a(!rec_offs_any_null_extern(
					     version, *offsets));
#endif /* UNIV_DEBUG || UNIV_BLOB_LIGHT_DEBUG */

				buf = static_cast<byte*>(mem_heap_alloc(
					in_heap, rec_offs_size(*offsets)));

				*old_vers = rec_copy(buf, version, *offsets);
				rec_offs_make_valid(*old_vers, index,
						    *offsets);
				err = DB_SUCCESS;

				break;
			}
		}

		err = trx_undo_prev_version_build(rec, mtr, version, index,
						  *offsets, heap,
						  &prev_version);
		if (heap2) {
			mem_heap_free(heap2); /* free version */
		}

		if (err != DB_SUCCESS) {
			break;
		}

		if (prev_version == NULL) {
			/* It was a freshly inserted version */
			*old_vers = NULL;
			err = DB_SUCCESS;

			break;
		}

		*offsets = rec_get_offsets(prev_version, index, *offsets,
					   ULINT_UNDEFINED, offset_heap);

#if defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG
		ut_a(!rec_offs_any_null_extern(prev_version, *offsets));
#endif /* UNIV_DEBUG || UNIV_BLOB_LIGHT_DEBUG */

		trx_id = row_get_rec_trx_id(prev_version, index, *offsets);

		if (read_view_sees_trx_id(view, trx_id)) {

			/* The view already sees this version: we can copy
			it to in_heap and return */

			buf = static_cast<byte*>(
				mem_heap_alloc(
					in_heap, rec_offs_size(*offsets)));

			*old_vers = rec_copy(buf, prev_version, *offsets);
			rec_offs_make_valid(*old_vers, index, *offsets);
			err = DB_SUCCESS;

			break;
		}

		version = prev_version;
	}/* for (;;) */

	mem_heap_free(heap);

	return(err);
}

/*****************************************************************//**
Constructs the last committed version of a clustered index record,
which should be seen by a semi-consistent read.
@return	DB_SUCCESS or DB_MISSING_HISTORY */
UNIV_INTERN
dberr_t
row_vers_build_for_semi_consistent_read(
/*====================================*/
	const rec_t*	rec,	/*!< in: record in a clustered index; the
				caller must have a latch on the page; this
				latch locks the top of the stack of versions
				of this records */
	mtr_t*		mtr,	/*!< in: mtr holding the latch on rec */
	dict_index_t*	index,	/*!< in: the clustered index */
	ulint**		offsets,/*!< in/out: offsets returned by
				rec_get_offsets(rec, index) */
	mem_heap_t**	offset_heap,/*!< in/out: memory heap from which
				the offsets are allocated */
	mem_heap_t*	in_heap,/*!< in: memory heap from which the memory for
				*old_vers is allocated; memory for possible
				intermediate versions is allocated and freed
				locally within the function */
	const rec_t**	old_vers)/*!< out: rec, old version, or NULL if the
				record does not exist in the view, that is,
				it was freshly inserted afterwards */
{
	const rec_t*	version;
	mem_heap_t*	heap		= NULL;
	byte*		buf;
	dberr_t		err;
	trx_id_t	rec_trx_id	= 0;

	ut_ad(dict_index_is_clust(index));
	ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX)
	      || mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(&(purge_sys->latch), RW_LOCK_SHARED));
#endif /* UNIV_SYNC_DEBUG */

	ut_ad(rec_offs_validate(rec, index, *offsets));

	version = rec;

	for (;;) {
		const trx_t*	version_trx;
		mem_heap_t*	heap2;
		rec_t*		prev_version;
		trx_id_t	version_trx_id;

		version_trx_id = row_get_rec_trx_id(version, index, *offsets);
		if (rec == version) {
			rec_trx_id = version_trx_id;
		}

		mutex_enter(&trx_sys->mutex);
		version_trx = trx_get_rw_trx_by_id(version_trx_id);
		/* Because version_trx is a read-write transaction,
		its state cannot change from or to NOT_STARTED while
		we are holding the trx_sys->mutex.  It may change from
		ACTIVE to PREPARED or COMMITTED. */
		if (version_trx
		    && trx_state_eq(version_trx,
				    TRX_STATE_COMMITTED_IN_MEMORY)) {
			version_trx = NULL;
		}
		mutex_exit(&trx_sys->mutex);

		if (!version_trx) {

			/* We found a version that belongs to a
			committed transaction: return it. */

#if defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG
			ut_a(!rec_offs_any_null_extern(version, *offsets));
#endif /* UNIV_DEBUG || UNIV_BLOB_LIGHT_DEBUG */

			if (rec == version) {
				*old_vers = rec;
				err = DB_SUCCESS;
				break;
			}

			/* We assume that a rolled-back transaction stays in
			TRX_STATE_ACTIVE state until all the changes have been
			rolled back and the transaction is removed from
			the global list of transactions. */

			if (rec_trx_id == version_trx_id) {
				/* The transaction was committed while
				we searched for earlier versions.
				Return the current version as a
				semi-consistent read. */

				version = rec;
				*offsets = rec_get_offsets(version,
							   index, *offsets,
							   ULINT_UNDEFINED,
							   offset_heap);
			}

			buf = static_cast<byte*>(
				mem_heap_alloc(
					in_heap, rec_offs_size(*offsets)));

			*old_vers = rec_copy(buf, version, *offsets);
			rec_offs_make_valid(*old_vers, index, *offsets);
			err = DB_SUCCESS;

			break;
		}

		heap2 = heap;
		heap = mem_heap_create(1024);

		err = trx_undo_prev_version_build(rec, mtr, version, index,
						  *offsets, heap,
						  &prev_version);
		if (heap2) {
			mem_heap_free(heap2); /* free version */
		}

		if (UNIV_UNLIKELY(err != DB_SUCCESS)) {
			break;
		}

		if (prev_version == NULL) {
			/* It was a freshly inserted version */
			*old_vers = NULL;
			err = DB_SUCCESS;

			break;
		}

		version = prev_version;
		*offsets = rec_get_offsets(version, index, *offsets,
					   ULINT_UNDEFINED, offset_heap);
#if defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG
		ut_a(!rec_offs_any_null_extern(version, *offsets));
#endif /* UNIV_DEBUG || UNIV_BLOB_LIGHT_DEBUG */
	}/* for (;;) */

	if (heap) {
		mem_heap_free(heap);
	}

	return(err);
}
