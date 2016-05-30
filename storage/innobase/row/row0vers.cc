/*****************************************************************************

Copyright (c) 1997, 2016, Oracle and/or its affiliates. All Rights Reserved.

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

#include "ha_prototypes.h"

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
#include "row0mysql.h"

/** Check whether all non-virtual columns in a virtual index match that of in
the cluster index
@param[in]	index		the secondary index
@param[in]	row		the cluster index row in dtuple form
@param[in]	ext		externally stored column prefix or NULL
@param[in]	ientry		the secondary index entry
@param[in,out]	heap		heap used to build virtual dtuple
@param[in,out]	n_non_v_col	number of non-virtual columns in the index
@return true if all matches, false otherwise */
static
bool
row_vers_non_vc_match(
	dict_index_t*		index,
	const dtuple_t*		row,
	const row_ext_t*	ext,
	const dtuple_t*		ientry,
	mem_heap_t*		heap,
	ulint*			n_non_v_col);
/*****************************************************************//**
Finds out if an active transaction has inserted or modified a secondary
index record.
@return 0 if committed, else the active transaction id;
NOTE that this function can return false positives but never false
negatives. The caller must confirm all positive results by calling
trx_is_active() while holding lock_sys->mutex. */
UNIV_INLINE
trx_t*
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
	dtuple_t*	ientry = NULL;
	mem_heap_t*	v_heap = NULL;
	const dtuple_t*	cur_vrow = NULL;

	DBUG_ENTER("row_vers_impl_x_locked_low");

	ut_ad(rec_offs_validate(rec, index, offsets));

	heap = mem_heap_create(1024);

	clust_offsets = rec_get_offsets(
		clust_rec, clust_index, NULL, ULINT_UNDEFINED, &heap);

	trx_id = row_get_rec_trx_id(clust_rec, clust_index, clust_offsets);
	corrupt = FALSE;

	trx_t*	trx = trx_rw_is_active(trx_id, &corrupt, true);

	if (trx == 0) {
		/* The transaction that modified or inserted clust_rec is no
		longer active, or it is corrupt: no implicit lock on rec */
		if (corrupt) {
			lock_report_trx_id_insanity(
				trx_id, clust_rec, clust_index, clust_offsets,
				trx_sys_get_max_trx_id());
		}
		mem_heap_free(heap);
		DBUG_RETURN(0);
	}

	comp = page_rec_is_comp(rec);
	ut_ad(index->table == clust_index->table);
	ut_ad(!!comp == dict_table_is_comp(index->table));
	ut_ad(!comp == !page_rec_is_comp(clust_rec));

	rec_del = rec_get_deleted_flag(rec, comp);

	if (dict_index_has_virtual(index)) {
		ulint	n_ext;
		ulint	est_size = DTUPLE_EST_ALLOC(index->n_fields);

		/* Allocate the dtuple for virtual columns extracted from undo
		log with its own heap, so to avoid it being freed as we
		iterating in the version loop below. */
		v_heap = mem_heap_create(est_size);
		ientry = row_rec_to_index_entry(
			rec, index, offsets, &n_ext, v_heap);
	}

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
		row_ext_t*	ext;
		dtuple_t*	row;
		dtuple_t*	entry;
		ulint		vers_del;
		trx_id_t	prev_trx_id;
		mem_heap_t*	old_heap = heap;
		const dtuple_t*	vrow = NULL;

		/* We keep the semaphore in mtr on the clust_rec page, so
		that no other transaction can update it and get an
		implicit x-lock on rec until mtr_commit(mtr). */

		heap = mem_heap_create(1024);

		trx_undo_prev_version_build(
			clust_rec, mtr, version, clust_index, clust_offsets,
			heap, &prev_version, NULL,
			dict_index_has_virtual(index) ? &vrow : NULL, 0);

		/* The oldest visible clustered index version must not be
		delete-marked, because we never start a transaction by
		inserting a delete-marked record. */
		ut_ad(prev_version
		      || !rec_get_deleted_flag(version, comp)
		      || !trx_rw_is_active(trx_id, NULL, false));

		/* Free version and clust_offsets. */
		mem_heap_free(old_heap);

		if (prev_version == NULL) {

			/* We reached the oldest visible version without
			finding an older version of clust_rec that would
			match the secondary index record.  If the secondary
			index record is not delete marked, then clust_rec
			is considered the correct match of the secondary
			index record and hence holds the implicit lock. */

			if (rec_del) {
				/* The secondary index record is del marked.
				So, the implicit lock holder of clust_rec
				did not modify the secondary index record yet,
				and is not holding an implicit lock on it.

				This assumes that whenever a row is inserted
				or updated, the leaf page record always is
				created with a clear delete-mark flag.
				(We never insert a delete-marked record.) */
				trx_release_reference(trx);
				trx = 0;
			}

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

		if (dict_index_has_virtual(index)) {
			if (vrow) {
				/* Keep the virtual row info for the next
				version */
				cur_vrow = dtuple_copy(vrow, v_heap);
				dtuple_dup_v_fld(cur_vrow, v_heap);
			}

			if (!cur_vrow) {
				ulint	n_non_v_col = 0;

				/* If the indexed virtual columns has changed,
				there must be log record to generate vrow.
				Otherwise, it is not changed, so no need
				to compare */
				if (row_vers_non_vc_match(
					index, row, ext, ientry, heap,
					&n_non_v_col) == 0) {
					if (rec_del != vers_del) {
						break;
					}
				} else if (!rec_del) {
					break;
				}

				goto result_check;
			} else {
				ut_ad(row->n_v_fields == cur_vrow->n_v_fields);
				dtuple_copy_v_fields(row, cur_vrow);
			}
		}

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

		/* We check if entry and rec are identified in the alphabetical
		ordering */
		if (0 == cmp_dtuple_rec(entry, rec, offsets)) {
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

result_check:
		if (trx->id != prev_trx_id) {
			/* prev_version was the first version modified by
			the trx_id transaction: no implicit x-lock */

			trx_release_reference(trx);
			trx = 0;
			break;
		}
	}

	DBUG_PRINT("info", ("Implicit lock is held by trx:" TRX_ID_FMT, trx_id));

	if (v_heap != NULL) {
		mem_heap_free(v_heap);
	}

	mem_heap_free(heap);
	DBUG_RETURN(trx);
}

/*****************************************************************//**
Finds out if an active transaction has inserted or modified a secondary
index record.
@return 0 if committed, else the active transaction id;
NOTE that this function can return false positives but never false
negatives. The caller must confirm all positive results by calling
trx_is_active() while holding lock_sys->mutex. */
trx_t*
row_vers_impl_x_locked(
/*===================*/
	const rec_t*	rec,	/*!< in: record in a secondary index */
	dict_index_t*	index,	/*!< in: the secondary index */
	const ulint*	offsets)/*!< in: rec_get_offsets(rec, index) */
{
	mtr_t		mtr;
	trx_t*		trx;
	const rec_t*	clust_rec;
	dict_index_t*	clust_index;

	ut_ad(!lock_mutex_own());
	ut_ad(!trx_sys_mutex_own());

	mtr_start(&mtr);

	/* Search for the clustered index record. The latch on the
	page of clust_rec locks the top of the stack of versions. The
	bottom of the version stack is not locked; oldest versions may
	disappear by the fact that transactions may be committed and
	collected by the purge. This is not a problem, because we are
	only interested in active transactions. */

	clust_rec = row_get_clust_rec(
		BTR_SEARCH_LEAF, rec, index, &clust_index, &mtr);

	if (!clust_rec) {
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

		trx = 0;
	} else {
		trx = row_vers_impl_x_locked_low(
			clust_rec, clust_index, rec, index, offsets, &mtr);

		ut_ad(trx == 0 || trx_is_referenced(trx));
	}

	mtr_commit(&mtr);

	return(trx);
}

/*****************************************************************//**
Finds out if we must preserve a delete marked earlier version of a clustered
index record, because it is >= the purge view.
@param[in]	trx_id		transaction id in the version
@param[in]	name		table name
@param[in,out]	mtr		mini transaction holding the latch on the
				clustered index record; it will also hold
				the latch on purge_view
@return TRUE if earlier version should be preserved */
ibool
row_vers_must_preserve_del_marked(
/*==============================*/
	trx_id_t		trx_id,
	const table_name_t&	name,
	mtr_t*			mtr)
{
	ut_ad(!rw_lock_own(&(purge_sys->latch), RW_LOCK_S));

	mtr_s_lock(&purge_sys->latch, mtr);

	return(!purge_sys->view.changes_visible(trx_id,	name));
}

/** Check whether all non-virtual columns in a virtual index match that of in
the cluster index
@param[in]	index		the secondary index
@param[in]	row		the cluster index row in dtuple form
@param[in]	ext		externally stored column prefix or NULL
@param[in]	ientry		the secondary index entry
@param[in,out]	heap		heap used to build virtual dtuple
@param[in,out]	n_non_v_col	number of non-virtual columns in the index
@return true if all matches, false otherwise */
static
bool
row_vers_non_vc_match(
	dict_index_t*		index,
	const dtuple_t*		row,
	const row_ext_t*	ext,
	const dtuple_t*		ientry,
	mem_heap_t*		heap,
	ulint*			n_non_v_col)
{
	const dfield_t* field1;
	dfield_t*	field2;
	ulint		n_fields = dtuple_get_n_fields(ientry);
	ulint		ret = true;

	*n_non_v_col = 0;

	/* Build index entry out of row */
	dtuple_t* nentry = row_build_index_entry(row, ext, index, heap);

	for (ulint i = 0; i < n_fields; i++) {
		const dict_field_t*	ind_field = dict_index_get_nth_field(
							index, i);

		const dict_col_t*	col = ind_field->col;

		/* Only check non-virtual columns */
		if (dict_col_is_virtual(col)) {
			continue;
		}

		if (ret) {
			field1  = dtuple_get_nth_field(ientry, i);
			field2  = dtuple_get_nth_field(nentry, i);

			if (cmp_dfield_dfield(field1, field2) != 0) {
				ret = false;
			}
		}

		(*n_non_v_col)++;
	}

	return(ret);
}

/** build virtual column value from current cluster index record data
@param[in,out]	row		the cluster index row in dtuple form
@param[in]	clust_index	clustered index
@param[in]	index		the secondary index
@param[in]	heap		heap used to build virtual dtuple */
static
void
row_vers_build_clust_v_col(
	dtuple_t*	row,
	dict_index_t*	clust_index,
	dict_index_t*	index,
	mem_heap_t*	heap)
{
	mem_heap_t*	local_heap = NULL;
	for (ulint i = 0; i < dict_index_get_n_fields(index); i++) {
		const dict_field_t* ind_field = dict_index_get_nth_field(
				index, i);

		if (dict_col_is_virtual(ind_field->col)) {
			const dict_v_col_t*       col;

			col = reinterpret_cast<const dict_v_col_t*>(
				ind_field->col);

			innobase_get_computed_value(
				row, col, clust_index, &local_heap,
				heap, NULL, current_thd, NULL, NULL,
				NULL, NULL);
		}
	}

	if (local_heap) {
		mem_heap_free(local_heap);
	}
}

/** Build latest virtual column data from undo log
@param[in]	in_purge	whether this is the purge thread
@param[in]	rec		clustered index record
@param[in]	clust_index	clustered index
@param[in,out]	clust_offsets	offsets on the clustered index record
@param[in]	index		the secondary index
@param[in]	roll_ptr	the rollback pointer for the purging record
@param[in]	trx_id		trx id for the purging record
@param[in,out]	v_heap		heap used to build vrow
@param[out]	v_row		dtuple holding the virtual rows
@param[in,out]	mtr		mtr holding the latch on rec */
static
void
row_vers_build_cur_vrow_low(
	bool		in_purge,
	const rec_t*	rec,
	dict_index_t*	clust_index,
	ulint*		clust_offsets,
	dict_index_t*	index,
	roll_ptr_t	roll_ptr,
	trx_id_t	trx_id,
	mem_heap_t*	v_heap,
	const dtuple_t**vrow,
	mtr_t*		mtr)
{
	const rec_t*	version;
	rec_t*		prev_version;
	mem_heap_t*	heap = NULL;
	ulint		num_v = dict_table_get_n_v_cols(index->table);
	const dfield_t* field;
	ulint		i;
	bool		all_filled = false;

	*vrow = dtuple_create_with_vcol(v_heap, 0, num_v);
	dtuple_init_v_fld(*vrow);

	for (i = 0; i < num_v; i++) {
		dfield_get_type(dtuple_get_nth_v_field(*vrow, i))->mtype
			 = DATA_MISSING;
	}

	version = rec;

	/* If this is called by purge thread, set TRX_UNDO_PREV_IN_PURGE
	bit to search the undo log until we hit the current undo log with
	roll_ptr */
	const ulint	status = in_purge
		? TRX_UNDO_PREV_IN_PURGE | TRX_UNDO_GET_OLD_V_VALUE
		: TRX_UNDO_GET_OLD_V_VALUE;

	while (!all_filled) {
		mem_heap_t*	heap2 = heap;
		heap = mem_heap_create(1024);
		roll_ptr_t	cur_roll_ptr = row_get_rec_roll_ptr(
			version, clust_index, clust_offsets);

		trx_undo_prev_version_build(
			rec, mtr, version, clust_index, clust_offsets,
			heap, &prev_version, NULL, vrow, status);

		if (heap2) {
			mem_heap_free(heap2);
		}

		if (!prev_version) {
			/* Versions end here */
			break;
		}

		clust_offsets = rec_get_offsets(prev_version, clust_index,
						NULL, ULINT_UNDEFINED, &heap);

		ulint	entry_len = dict_index_get_n_fields(index);

		all_filled = true;

		for (i = 0; i < entry_len; i++) {
			const dict_field_t*	ind_field
				 = dict_index_get_nth_field(index, i);
			const dict_col_t*	col = ind_field->col;

			if (!dict_col_is_virtual(col)) {
				continue;
			}

			const dict_v_col_t*	v_col
				= reinterpret_cast<const dict_v_col_t*>(col);
			field = dtuple_get_nth_v_field(*vrow, v_col->v_pos);

			if (dfield_get_type(field)->mtype == DATA_MISSING) {
				all_filled = false;
				break;
			}

		}

		trx_id_t	rec_trx_id = row_get_rec_trx_id(
			prev_version, clust_index, clust_offsets);

		if (rec_trx_id < trx_id || roll_ptr == cur_roll_ptr) {
			break;
		}

		version = prev_version;
	}

	mem_heap_free(heap);
}

/** Check a virtual column value index secondary virtual index matches
that of current cluster index record, which is recreated from information
stored in undo log
@param[in]	in_purge	called by purge thread
@param[in]	rec		record in the clustered index
@param[in]	row		the cluster index row in dtuple form
@param[in]	ext		externally stored column prefix or NULL
@param[in]	clust_index	cluster index
@param[in]	clust_offsets	offsets on the cluster record
@param[in]	index		the secondary index
@param[in]	ientry		the secondary index entry
@param[in]	roll_ptr	the rollback pointer for the purging record
@param[in]	trx_id		trx id for the purging record
@param[in,out]	v_heap		heap used to build virtual dtuple
@param[in,out]	v_row		dtuple holding the virtual rows (if needed)
@param[in]	mtr		mtr holding the latch on rec
@return true if matches, false otherwise */
static
bool
row_vers_vc_matches_cluster(
	bool		in_purge,
	const rec_t*	rec,
	const dtuple_t*	row,
	row_ext_t*	ext,
	dict_index_t*	clust_index,
	ulint*		clust_offsets,
	dict_index_t*	index,
	const dtuple_t* ientry,
	roll_ptr_t	roll_ptr,
	trx_id_t	trx_id,
	mem_heap_t*	v_heap,
	const dtuple_t**vrow,
	mtr_t*		mtr)
{
	const rec_t*	version;
	rec_t*          prev_version;
	mem_heap_t*	heap2;
	mem_heap_t*	heap = NULL;
	mem_heap_t*	tuple_heap;
	ulint		num_v = dict_table_get_n_v_cols(index->table);
	bool		compare[REC_MAX_N_FIELDS];
	ulint		n_fields = dtuple_get_n_fields(ientry);
	ulint		n_non_v_col = 0;
	ulint		n_cmp_v_col = 0;
	const dfield_t* field1;
	dfield_t*	field2;
	ulint		i;

	tuple_heap = mem_heap_create(1024);

	/* First compare non-virtual columns (primary keys) */
	if (!row_vers_non_vc_match(index, row, ext, ientry, tuple_heap,
				   &n_non_v_col)) {
		mem_heap_free(tuple_heap);
		return(false);
	}

	ut_ad(n_fields > n_non_v_col);

	*vrow = dtuple_create_with_vcol(v_heap ? v_heap : tuple_heap, 0, num_v);
	dtuple_init_v_fld(*vrow);

	for (i = 0; i < num_v; i++) {
		dfield_get_type(dtuple_get_nth_v_field(*vrow, i))->mtype
			 = DATA_MISSING;
		compare[i] = false;
	}

	version = rec;

	/* If this is called by purge thread, set TRX_UNDO_PREV_IN_PURGE
	bit to search the undo log until we hit the current undo log with
	roll_ptr */
	ulint	status = (in_purge ? TRX_UNDO_PREV_IN_PURGE : 0)
			 | TRX_UNDO_GET_OLD_V_VALUE;

	while (n_cmp_v_col < n_fields - n_non_v_col) {
		heap2 = heap;
		heap = mem_heap_create(1024);
		roll_ptr_t	cur_roll_ptr = row_get_rec_roll_ptr(
			version, clust_index, clust_offsets);

		ut_ad(cur_roll_ptr != 0);
		ut_ad(in_purge == (roll_ptr != 0));

		trx_undo_prev_version_build(
			rec, mtr, version, clust_index, clust_offsets,
			heap, &prev_version, NULL, vrow, status);

		if (heap2) {
			mem_heap_free(heap2);
		}

		if (!prev_version) {
			/* Versions end here */
			goto func_exit;
		}

		clust_offsets = rec_get_offsets(prev_version, clust_index,
						NULL, ULINT_UNDEFINED, &heap);

		ulint	entry_len = dict_index_get_n_fields(index);

		for (i = 0; i < entry_len; i++) {
			const dict_field_t*	ind_field
				 = dict_index_get_nth_field(index, i);
			const dict_col_t*	col = ind_field->col;
			field1 = dtuple_get_nth_field(ientry, i);

			if (!dict_col_is_virtual(col)) {
				continue;
			}

			const dict_v_col_t*     v_col
                                = reinterpret_cast<const dict_v_col_t*>(col);
			field2
				= dtuple_get_nth_v_field(*vrow, v_col->v_pos);

			if ((dfield_get_type(field2)->mtype != DATA_MISSING)
			    && (!compare[v_col->v_pos])) {

				if (ind_field->prefix_len != 0
				    && !dfield_is_null(field2)
				    && field2->len > ind_field->prefix_len) {
					field2->len = ind_field->prefix_len;
				}

				/* The index field mismatch */
				if (v_heap
				    || cmp_dfield_dfield(field2, field1) != 0) {
					if (v_heap) {
						dtuple_dup_v_fld(*vrow, v_heap);
					}

					mem_heap_free(tuple_heap);
					mem_heap_free(heap);
					return(false);
				}

				compare[v_col->v_pos] = true;
				n_cmp_v_col++;
			}
		}

		trx_id_t	rec_trx_id = row_get_rec_trx_id(
			prev_version, clust_index, clust_offsets);

		if (rec_trx_id < trx_id || roll_ptr == cur_roll_ptr) {
			break;
		}

		version = prev_version;
	}

func_exit:
	if (n_cmp_v_col == 0) {
		*vrow = NULL;
	}

	mem_heap_free(tuple_heap);
	mem_heap_free(heap);

	/* FIXME: In the case of n_cmp_v_col is not the same as
	n_fields - n_non_v_col, callback is needed to compare the rest
	columns. At the timebeing, we will need to return true */
	return (true);
}

/** Build a dtuple contains virtual column data for current cluster index
@param[in]	in_purge	called by purge thread
@param[in]	rec		cluster index rec
@param[in]	clust_index	cluster index
@param[in]	clust_offsets	cluster rec offset
@param[in]	index		secondary index
@param[in]	ientry		secondary index rec
@param[in]	roll_ptr	roll_ptr for the purge record
@param[in]	trx_id		transaction ID on the purging record
@param[in,out]	heap		heap memory
@param[in,out]	v_heap		heap memory to keep virtual colum dtuple
@param[in]	mtr		mtr holding the latch on rec
@return dtuple contains virtual column data */
static
const dtuple_t*
row_vers_build_cur_vrow(
	bool		in_purge,
	const rec_t*	rec,
	dict_index_t*	clust_index,
	ulint**		clust_offsets,
	dict_index_t*	index,
	const dtuple_t*	ientry,
	roll_ptr_t	roll_ptr,
	trx_id_t	trx_id,
	mem_heap_t*	heap,
	mem_heap_t*	v_heap,
	mtr_t*		mtr)
{
	const dtuple_t*	cur_vrow = NULL;

	roll_ptr_t t_roll_ptr = row_get_rec_roll_ptr(
		rec, clust_index, *clust_offsets);

	/* if the row is newly inserted, then the virtual
	columns need to be computed */
	if (trx_undo_roll_ptr_is_insert(t_roll_ptr)) {

		ut_ad(!rec_get_deleted_flag(rec, page_rec_is_comp(rec)));

		/* This is a newly inserted record and cannot
		be deleted, So the externally stored field
		cannot be freed yet. */
		dtuple_t* row = row_build(ROW_COPY_POINTERS, clust_index,
					  rec, *clust_offsets,
					  NULL, NULL, NULL, NULL, heap);

		row_vers_build_clust_v_col(
			row, clust_index, index, heap);
		cur_vrow = dtuple_copy(row, v_heap);
		dtuple_dup_v_fld(cur_vrow, v_heap);
	} else {
		/* Try to fetch virtual column data from undo log */
		row_vers_build_cur_vrow_low(
			in_purge, rec, clust_index, *clust_offsets,
			index, roll_ptr, trx_id, v_heap, &cur_vrow, mtr);
	}

	*clust_offsets = rec_get_offsets(rec, clust_index, NULL,
					 ULINT_UNDEFINED, &heap);
	return(cur_vrow);
}

/*****************************************************************//**
Finds out if a version of the record, where the version >= the current
purge view, should have ientry as its secondary index entry. We check
if there is any not delete marked version of the record where the trx
id >= purge view, and the secondary index entry and ientry are identified in
the alphabetical ordering; exactly in this case we return TRUE.
@return TRUE if earlier version should have */
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
	const dtuple_t*	ientry,	/*!< in: the secondary index entry */
	roll_ptr_t	roll_ptr,/*!< in: roll_ptr for the purge record */
	trx_id_t	trx_id)	/*!< in: transaction ID on the purging record */
{
	const rec_t*	version;
	rec_t*		prev_version;
	dict_index_t*	clust_index;
	ulint*		clust_offsets;
	mem_heap_t*	heap;
	mem_heap_t*	heap2;
	dtuple_t*	row;
	const dtuple_t*	entry;
	ulint		comp;
	const dtuple_t*	vrow = NULL;
	mem_heap_t*	v_heap = NULL;
	const dtuple_t*	cur_vrow = NULL;

	ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX)
	      || mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
	ut_ad(!rw_lock_own(&(purge_sys->latch), RW_LOCK_S));

	clust_index = dict_table_get_first_index(index->table);

	comp = page_rec_is_comp(rec);
	ut_ad(!dict_table_is_comp(index->table) == !comp);
	heap = mem_heap_create(1024);
	clust_offsets = rec_get_offsets(rec, clust_index, NULL,
					ULINT_UNDEFINED, &heap);

	if (dict_index_has_virtual(index)) {
		v_heap = mem_heap_create(100);
	}

	DBUG_EXECUTE_IF("ib_purge_virtual_index_crash",
			DBUG_SUICIDE(););

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

		if (dict_index_has_virtual(index)) {
#ifdef DBUG_OFF
# define dbug_v_purge false
#else /* DBUG_OFF */
                        bool    dbug_v_purge = false;
#endif /* DBUG_OFF */

			DBUG_EXECUTE_IF(
				"ib_purge_virtual_index_callback",
				dbug_v_purge = true;);

			roll_ptr_t t_roll_ptr = row_get_rec_roll_ptr(
				rec, clust_index, clust_offsets);

			/* if the row is newly inserted, then the virtual
			columns need to be computed */
			if (trx_undo_roll_ptr_is_insert(t_roll_ptr)
			    || dbug_v_purge) {
				row_vers_build_clust_v_col(
					row, clust_index, index, heap);

				entry = row_build_index_entry(
					row, ext, index, heap);
				if (entry && !dtuple_coll_cmp(ientry, entry)) {

					mem_heap_free(heap);

					if (v_heap) {
						mem_heap_free(v_heap);
					}

					return(TRUE);
				}
			} else {
				if (row_vers_vc_matches_cluster(
					also_curr, rec, row, ext, clust_index,
					clust_offsets, index, ientry, roll_ptr,
					trx_id, NULL, &vrow, mtr)) {
					mem_heap_free(heap);

					if (v_heap) {
						mem_heap_free(v_heap);
					}

					return(TRUE);
				}
			}
			clust_offsets = rec_get_offsets(rec, clust_index, NULL,
							ULINT_UNDEFINED, &heap);
		} else {

			entry = row_build_index_entry(
				row, ext, index, heap);

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

				if (v_heap) {
					mem_heap_free(v_heap);
				}
				return(TRUE);
			}
		}
	} else if (dict_index_has_virtual(index)) {
		/* The current cluster index record could be
		deleted, but the previous version of it might not. We will
		need to get the virtual column data from undo record
		associated with current cluster index */
		cur_vrow = row_vers_build_cur_vrow(
			also_curr, rec, clust_index, &clust_offsets,
			index, ientry, roll_ptr, trx_id, heap, v_heap, mtr);
	}

	version = rec;

	for (;;) {
		heap2 = heap;
		heap = mem_heap_create(1024);
		vrow = NULL;

		trx_undo_prev_version_build(rec, mtr, version,
					    clust_index, clust_offsets,
					    heap, &prev_version, NULL,
					    dict_index_has_virtual(index)
						? &vrow : NULL, 0);
		mem_heap_free(heap2); /* free version and clust_offsets */

		if (!prev_version) {
			/* Versions end here */

			mem_heap_free(heap);

			if (v_heap) {
				mem_heap_free(v_heap);
			}

			return(FALSE);
		}

		clust_offsets = rec_get_offsets(prev_version, clust_index,
						NULL, ULINT_UNDEFINED, &heap);

		if (dict_index_has_virtual(index)) {
			if (vrow) {
				/* Keep the virtual row info for the next
				version, unless it is changed */
				mem_heap_empty(v_heap);
				cur_vrow = dtuple_copy(vrow, v_heap);
				dtuple_dup_v_fld(cur_vrow, v_heap);
			}

			if (!cur_vrow) {
				/* Nothing for this index has changed,
				continue */
				version = prev_version;
				continue;
			}
		}

		if (!rec_get_deleted_flag(prev_version, comp)) {
			row_ext_t*	ext;

			/* The stack of versions is locked by mtr.
			Thus, it is safe to fetch the prefixes for
			externally stored columns. */
			row = row_build(ROW_COPY_POINTERS, clust_index,
					prev_version, clust_offsets,
					NULL, NULL, NULL, &ext, heap);

			if (dict_index_has_virtual(index)) {
				ut_ad(cur_vrow);
				ut_ad(row->n_v_fields == cur_vrow->n_v_fields);
				dtuple_copy_v_fields(row, cur_vrow);
			}

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
				if (v_heap) {
					mem_heap_free(v_heap);
				}

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
@return DB_SUCCESS or DB_MISSING_HISTORY */
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
	ReadView*	view,	/*!< in: the consistent read view */
	mem_heap_t**	offset_heap,/*!< in/out: memory heap from which
				the offsets are allocated */
	mem_heap_t*	in_heap,/*!< in: memory heap from which the memory for
				*old_vers is allocated; memory for possible
				intermediate versions is allocated and freed
				locally within the function */
	rec_t**		old_vers,/*!< out, own: old version, or NULL
				if the history is missing or the record
				does not exist in the view, that is,
				it was freshly inserted afterwards */
	const dtuple_t**vrow)	/*!< out: virtual row */
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
	ut_ad(!rw_lock_own(&(purge_sys->latch), RW_LOCK_S));

	ut_ad(rec_offs_validate(rec, index, *offsets));

	trx_id = row_get_rec_trx_id(rec, index, *offsets);

	ut_ad(!view->changes_visible(trx_id, index->table->name));

	ut_ad(!vrow || !(*vrow));

	version = rec;

	for (;;) {
		mem_heap_t*	prev_heap = heap;

		heap = mem_heap_create(1024);

		if (vrow) {
			*vrow = NULL;
		}

		/* If purge can't see the record then we can't rely on
		the UNDO log record. */

		bool	purge_sees = trx_undo_prev_version_build(
			rec, mtr, version, index, *offsets, heap,
			&prev_version, NULL, vrow, 0);

		err  = (purge_sees) ? DB_SUCCESS : DB_MISSING_HISTORY;

		if (prev_heap != NULL) {
			mem_heap_free(prev_heap);
		}

		if (prev_version == NULL) {
			/* It was a freshly inserted version */
			*old_vers = NULL;
			ut_ad(!vrow || !(*vrow));
			break;
		}

		*offsets = rec_get_offsets(
			prev_version, index, *offsets, ULINT_UNDEFINED,
			offset_heap);

#if defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG
		ut_a(!rec_offs_any_null_extern(prev_version, *offsets));
#endif /* UNIV_DEBUG || UNIV_BLOB_LIGHT_DEBUG */

		trx_id = row_get_rec_trx_id(prev_version, index, *offsets);

		if (view->changes_visible(trx_id, index->table->name)) {

			/* The view already sees this version: we can copy
			it to in_heap and return */

			buf = static_cast<byte*>(
				mem_heap_alloc(
					in_heap, rec_offs_size(*offsets)));

			*old_vers = rec_copy(buf, prev_version, *offsets);
			rec_offs_make_valid(*old_vers, index, *offsets);

			if (vrow && *vrow) {
				*vrow = dtuple_copy(*vrow, in_heap);
				dtuple_dup_v_fld(*vrow, in_heap);
			}
			break;
		}

		version = prev_version;
	}

	mem_heap_free(heap);

	return(err);
}

/*****************************************************************//**
Constructs the last committed version of a clustered index record,
which should be seen by a semi-consistent read. */
void
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
	const rec_t**	old_vers,/*!< out: rec, old version, or NULL if the
				record does not exist in the view, that is,
				it was freshly inserted afterwards */
	const dtuple_t** vrow)	/*!< out: virtual row, old version, or NULL
				if it is not updated in the view */
{
	const rec_t*	version;
	mem_heap_t*	heap		= NULL;
	byte*		buf;
	trx_id_t	rec_trx_id	= 0;

	ut_ad(dict_index_is_clust(index));
	ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX)
	      || mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
	ut_ad(!rw_lock_own(&(purge_sys->latch), RW_LOCK_S));

	ut_ad(rec_offs_validate(rec, index, *offsets));

	version = rec;
	ut_ad(!vrow || !(*vrow));

	for (;;) {
		const trx_t*	version_trx;
		mem_heap_t*	heap2;
		rec_t*		prev_version;
		trx_id_t	version_trx_id;

		version_trx_id = row_get_rec_trx_id(version, index, *offsets);
		if (rec == version) {
			rec_trx_id = version_trx_id;
		}

		trx_sys_mutex_enter();
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
		trx_sys_mutex_exit();

		if (!version_trx) {
committed_version_trx:
			/* We found a version that belongs to a
			committed transaction: return it. */

#if defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG
			ut_a(!rec_offs_any_null_extern(version, *offsets));
#endif /* UNIV_DEBUG || UNIV_BLOB_LIGHT_DEBUG */

			if (rec == version) {
				*old_vers = rec;
				if (vrow) {
					*vrow = NULL;
				}
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
			if (vrow && *vrow) {
				*vrow = dtuple_copy(*vrow, in_heap);
				dtuple_dup_v_fld(*vrow, in_heap);
			}
			break;
		}

		DEBUG_SYNC_C("after_row_vers_check_trx_active");

		heap2 = heap;
		heap = mem_heap_create(1024);

		if (!trx_undo_prev_version_build(rec, mtr, version, index,
						 *offsets, heap,
						 &prev_version,
						 in_heap, vrow, 0)) {
			mem_heap_free(heap);
			heap = heap2;
			heap2 = NULL;
			goto committed_version_trx;
		}

		if (heap2) {
			mem_heap_free(heap2); /* free version */
		}

		if (prev_version == NULL) {
			/* It was a freshly inserted version */
			*old_vers = NULL;
			ut_ad(!vrow || !(*vrow));
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
}
