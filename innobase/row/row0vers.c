/******************************************************
Row versions

(c) 1997 Innobase Oy

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

/*********************************************************************
Finds out if an active transaction has inserted or modified a secondary
index record. NOTE: the kernel mutex is temporarily released in this
function! */

trx_t*
row_vers_impl_x_locked_off_kernel(
/*==============================*/
				/* out: NULL if committed, else the active
				transaction; NOTE that the kernel mutex is
				temporarily released! */
	rec_t*		rec,	/* in: record in a secondary index */
	dict_index_t*	index,	/* in: the secondary index */
	const ulint*	offsets)/* in: rec_get_offsets(rec, index) */
{
	dict_index_t*	clust_index;
	rec_t*		clust_rec;
	ulint*		clust_offsets;
	rec_t*		version;
	rec_t*		prev_version;
	dulint		trx_id;
	dulint		prev_trx_id;
	mem_heap_t*	heap;
	mem_heap_t*	heap2;
	dtuple_t*	row;
	dtuple_t*	entry	= NULL; /* assignment to eliminate compiler
					warning */
	trx_t*		trx;
	ibool		vers_del;
	ibool		rec_del;
	ulint		err;
	mtr_t		mtr;
	ibool		comp;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
	ut_ad(!rw_lock_own(&(purge_sys->latch), RW_LOCK_SHARED));
#endif /* UNIV_SYNC_DEBUG */

	mutex_exit(&kernel_mutex);

	mtr_start(&mtr);
	
	/* Search for the clustered index record: this is a time-consuming
	operation: therefore we release the kernel mutex; also, the release
	is required by the latching order convention. The latch on the
	clustered index locks the top of the stack of versions. We also
	reserve purge_latch to lock the bottom of the version stack. */	

	clust_rec = row_get_clust_rec(BTR_SEARCH_LEAF, rec, index,
							&clust_index, &mtr);
	if (!clust_rec) {
		/* In a rare case it is possible that no clust rec is found
		for a secondary index record: if in row0umod.c
		row_undo_mod_remove_clust_low() we have already removed the
		clust rec, while purge is still cleaning and removing
		secondary index records associated with earlier versions of
		the clustered index record. In that case there cannot be
		any implicit lock on the secondary index record, because
		an active transaction which has modified the secondary index
		record has also modified the clustered index record. And in
		a rollback we always undo the modifications to secondary index
		records before the clustered index record. */

		mutex_enter(&kernel_mutex);
		mtr_commit(&mtr);

	        return(NULL);
	}

	heap = mem_heap_create(1024);
	clust_offsets = rec_get_offsets(clust_rec, clust_index,
					ULINT_UNDEFINED, heap);
	trx_id = row_get_rec_trx_id(clust_rec, clust_index, clust_offsets);

	mtr_s_lock(&(purge_sys->latch), &mtr);

	mutex_enter(&kernel_mutex);
	
	if (!trx_is_active(trx_id)) {
		/* The transaction that modified or inserted clust_rec is no
		longer active: no implicit lock on rec */
		
		mem_heap_free(heap);
		mtr_commit(&mtr);

		return(NULL);
	}

	if (!lock_check_trx_id_sanity(trx_id, clust_rec, clust_index,
					clust_offsets, TRUE)) {
		/* Corruption noticed: try to avoid a crash by returning */
		
		mem_heap_free(heap);
		mtr_commit(&mtr);

		return(NULL);
	}

	comp = index->table->comp;
	ut_ad(index->table == clust_index->table);
	ut_ad(comp == page_is_comp(buf_frame_align(rec)));
	ut_ad(comp == page_is_comp(buf_frame_align(clust_rec)));

	/* We look up if some earlier version, which was modified by the trx_id
	transaction, of the clustered index record would require rec to be in
	a different state (delete marked or unmarked, or have different field
	values, or not existing). If there is such a version, then rec was
	modified by the trx_id transaction, and it has an implicit x-lock on
	rec. Note that if clust_rec itself would require rec to be in a
	different state, then the trx_id transaction has not yet had time to
	modify rec, and does not necessarily have an implicit x-lock on rec. */

	rec_del = rec_get_deleted_flag(rec, comp);
	trx = NULL;

	version = clust_rec;

	for (;;) {
		mutex_exit(&kernel_mutex);

		/* While we retrieve an earlier version of clust_rec, we
		release the kernel mutex, because it may take time to access
		the disk. After the release, we have to check if the trx_id
		transaction is still active. We keep the semaphore in mtr on
		the clust_rec page, so that no other transaction can update
		it and get an implicit x-lock on rec. */

		heap2 = heap;
		heap = mem_heap_create(1024);
		err = trx_undo_prev_version_build(clust_rec, &mtr, version,
					clust_index, clust_offsets, heap,
					&prev_version);
		mem_heap_free(heap2); /* free version and clust_offsets */

		if (prev_version) {
			clust_offsets = rec_get_offsets(prev_version,
					clust_index, ULINT_UNDEFINED, heap);
			row = row_build(ROW_COPY_POINTERS, clust_index,
					prev_version, clust_offsets, heap);
			entry = row_build_index_entry(row, index, heap);
		}

		mutex_enter(&kernel_mutex);

		if (!trx_is_active(trx_id)) {
			/* Transaction no longer active: no implicit x-lock */

			break;
		}

		/* If the transaction is still active, the previous version
		of clust_rec must be accessible if not a fresh insert; we
		may assert the following: */

		ut_ad(err == DB_SUCCESS);
						
		if (prev_version == NULL) {
			/* It was a freshly inserted version: there is an
			implicit x-lock on rec */

			trx = trx_get_on_id(trx_id);

			break;
		}

		/* If we get here, we know that the trx_id transaction is
		still active and it has modified prev_version. Let us check
		if prev_version would require rec to be in a different
		state. */

		vers_del = rec_get_deleted_flag(prev_version, comp);

		/* We check if entry and rec are identified in the alphabetical
		ordering */
		if (0 == cmp_dtuple_rec(entry, rec, offsets)) {
			/* The delete marks of rec and prev_version should be
			equal for rec to be in the state required by
			prev_version */

			if (rec_del != vers_del) {
				trx = trx_get_on_id(trx_id);

				break;
			}

			/* It is possible that the row was updated so that the
			secondary index record remained the same in
			alphabetical ordering, but the field values changed
			still. For example, 'abc' -> 'ABC'. Check also that. */

			dtuple_set_types_binary(entry,
						dtuple_get_n_fields(entry));
			if (0 != cmp_dtuple_rec(entry, rec, offsets)) {

				trx = trx_get_on_id(trx_id);

				break;
			}
		} else if (!rec_del) {
			/* The delete mark should be set in rec for it to be
			in the state required by prev_version */

			trx = trx_get_on_id(trx_id);

			break;
		}

		prev_trx_id = row_get_rec_trx_id(prev_version, clust_index,
								clust_offsets);

		if (0 != ut_dulint_cmp(trx_id, prev_trx_id)) {
			/* The versions modified by the trx_id transaction end
			to prev_version: no implicit x-lock */

			break;
		}

		version = prev_version;
	}/* for (;;) */

	mtr_commit(&mtr);
	mem_heap_free(heap);

	return(trx);
}

/*********************************************************************
Finds out if we must preserve a delete marked earlier version of a clustered
index record, because it is >= the purge view. */

ibool
row_vers_must_preserve_del_marked(
/*==============================*/
			/* out: TRUE if earlier version should be preserved */
	dulint	trx_id,	/* in: transaction id in the version */
	mtr_t*	mtr)	/* in: mtr holding the latch on the clustered index
			record; it will also hold the latch on purge_view */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(&(purge_sys->latch), RW_LOCK_SHARED));
#endif /* UNIV_SYNC_DEBUG */

	mtr_s_lock(&(purge_sys->latch), mtr);

	if (trx_purge_update_undo_must_exist(trx_id)) {

		/* A purge operation is not yet allowed to remove this
		delete marked record */
			
		return(TRUE);
	}

	return(FALSE);
}

/*********************************************************************
Finds out if a version of the record, where the version >= the current
purge view, should have ientry as its secondary index entry. We check
if there is any not delete marked version of the record where the trx
id >= purge view, and the secondary index entry and ientry are identified in
the alphabetical ordering; exactly in this case we return TRUE. */

ibool
row_vers_old_has_index_entry(
/*=========================*/
				/* out: TRUE if earlier version should have */
	ibool		also_curr,/* in: TRUE if also rec is included in the
				versions to search; otherwise only versions
				prior to it are searched */
	rec_t*		rec,	/* in: record in the clustered index; the
				caller must have a latch on the page */
	mtr_t*		mtr,	/* in: mtr holding the latch on rec; it will
				also hold the latch on purge_view */
	dict_index_t*	index,	/* in: the secondary index */
	dtuple_t*	ientry)	/* in: the secondary index entry */
{
	rec_t*		version;
	rec_t*		prev_version;
	dict_index_t*	clust_index;
	ulint*		clust_offsets;
	mem_heap_t*	heap;
	mem_heap_t*	heap2;
	dtuple_t*	row;
	dtuple_t*	entry;
	ulint		err;
	ibool		comp;

	ut_ad(mtr_memo_contains(mtr, buf_block_align(rec), MTR_MEMO_PAGE_X_FIX)
	   	|| mtr_memo_contains(mtr, buf_block_align(rec),
						MTR_MEMO_PAGE_S_FIX));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(&(purge_sys->latch), RW_LOCK_SHARED));
#endif /* UNIV_SYNC_DEBUG */
	mtr_s_lock(&(purge_sys->latch), mtr);

	clust_index = dict_table_get_first_index(index->table);

	comp = index->table->comp;
	ut_ad(comp == page_is_comp(buf_frame_align(rec)));
	heap = mem_heap_create(1024);
	clust_offsets = rec_get_offsets(rec, clust_index,
					ULINT_UNDEFINED, heap);

	if (also_curr && !rec_get_deleted_flag(rec, comp)) {
		row = row_build(ROW_COPY_POINTERS, clust_index,
						rec, clust_offsets, heap);
		entry = row_build_index_entry(row, index, heap);

 		/* NOTE that we cannot do the comparison as binary
		fields because the row is maybe being modified so that
		the clustered index record has already been updated
		to a different binary value in a char field, but the
		collation identifies the old and new value anyway! */

		if (dtuple_datas_are_ordering_equal(ientry, entry)) {

			mem_heap_free(heap);

			return(TRUE);
		}
	}

	version = rec;

	for (;;) {
		heap2 = heap;
		heap = mem_heap_create(1024);
		err = trx_undo_prev_version_build(rec, mtr, version,
					clust_index, clust_offsets, heap,
					&prev_version);
		mem_heap_free(heap2); /* free version and clust_offsets */

		if (err != DB_SUCCESS || !prev_version) {
			/* Versions end here */

			mem_heap_free(heap);

			return(FALSE);
		}

		clust_offsets = rec_get_offsets(prev_version, clust_index,
						ULINT_UNDEFINED, heap);

		if (!rec_get_deleted_flag(prev_version, comp)) {
			row = row_build(ROW_COPY_POINTERS, clust_index,
					prev_version, clust_offsets, heap);
			entry = row_build_index_entry(row, index, heap);

 			/* NOTE that we cannot do the comparison as binary
			fields because maybe the secondary index record has
			already been updated to a different binary value in
			a char field, but the collation identifies the old
			and new value anyway! */

			if (dtuple_datas_are_ordering_equal(ientry, entry)) {

				mem_heap_free(heap);

				return(TRUE);
			}
		}

		version = prev_version;
	}
}

/*********************************************************************
Constructs the version of a clustered index record which a consistent
read should see. We assume that the trx id stored in rec is such that
the consistent read should not see rec in its present version. */

ulint
row_vers_build_for_consistent_read(
/*===============================*/
				/* out: DB_SUCCESS or DB_MISSING_HISTORY */
	rec_t*		rec,	/* in: record in a clustered index; the
				caller must have a latch on the page; this
				latch locks the top of the stack of versions
				of this records */
	mtr_t*		mtr,	/* in: mtr holding the latch on rec */
	dict_index_t*	index,	/* in: the clustered index */
	read_view_t*	view,	/* in: the consistent read view */
	mem_heap_t*	in_heap,/* in: memory heap from which the memory for
				old_vers is allocated; memory for possible
				intermediate versions is allocated and freed
				locally within the function */
	rec_t**		old_vers)/* out, own: old version, or NULL if the
				record does not exist in the view, that is,
				it was freshly inserted afterwards */
{
	rec_t*		version;
	rec_t*		prev_version;
	dulint		prev_trx_id;
	mem_heap_t*	heap;
	mem_heap_t*	heap2;
	byte*		buf;
	ulint		err;
	ulint*		offsets;

	ut_ad(index->type & DICT_CLUSTERED);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(rec), MTR_MEMO_PAGE_X_FIX)
	   	|| mtr_memo_contains(mtr, buf_block_align(rec),
						MTR_MEMO_PAGE_S_FIX));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(&(purge_sys->latch), RW_LOCK_SHARED));
#endif /* UNIV_SYNC_DEBUG */

	heap = mem_heap_create(1024);
	offsets = rec_get_offsets(rec, index, ULINT_UNDEFINED, heap);

	ut_ad(!read_view_sees_trx_id(view,
				row_get_rec_trx_id(rec, index, offsets)));

	rw_lock_s_lock(&(purge_sys->latch));
	version = rec;

	for (;;) {
		heap2 = heap;
		heap = mem_heap_create(1024);

		err = trx_undo_prev_version_build(rec, mtr, version, index,
						offsets, heap, &prev_version);
		mem_heap_free(heap2); /* free version and offsets */

		if (err != DB_SUCCESS) {
			break;
		}

		if (prev_version == NULL) {
			/* It was a freshly inserted version */
			*old_vers = NULL;
			err = DB_SUCCESS;

			break;
		}

		offsets = rec_get_offsets(prev_version, index,
					ULINT_UNDEFINED, heap);
		prev_trx_id = row_get_rec_trx_id(prev_version, index, offsets);

		if (read_view_sees_trx_id(view, prev_trx_id)) {

			/* The view already sees this version: we can copy
			it to in_heap and return */

			buf = mem_heap_alloc(in_heap, rec_offs_size(offsets));
			*old_vers = rec_copy(buf, prev_version, offsets);
			err = DB_SUCCESS;

			break;
		}

		version = prev_version;
	}/* for (;;) */

	mem_heap_free(heap);
	rw_lock_s_unlock(&(purge_sys->latch));

	return(err);
}
