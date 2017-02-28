/*****************************************************************************

Copyright (c) 1997, 2017, Oracle and/or its affiliates. All Rights Reserved.

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
@file row/row0purge.cc
Purge obsolete records

Created 3/14/1997 Heikki Tuuri
*******************************************************/

#include "row0purge.h"

#ifdef UNIV_NONINL
#include "row0purge.ic"
#endif

#include "fsp0fsp.h"
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
#include "row0vers.h"
#include "row0mysql.h"
#include "row0log.h"
#include "log0log.h"
#include "srv0mon.h"
#include "srv0start.h"
#include "handler.h"
#include "ha_innodb.h"
#include "fil0fil.h"

/*************************************************************************
IMPORTANT NOTE: Any operation that generates redo MUST check that there
is enough space in the redo log before for that operation. This is
done by calling log_free_check(). The reason for checking the
availability of the redo log space before the start of the operation is
that we MUST not hold any synchonization objects when performing the
check.
If you make a change in this module make sure that no codepath is
introduced where a call to log_free_check() is bypassed. */

/** Create a purge node to a query graph.
@param[in]	parent	parent node, i.e., a thr node
@param[in]	heap	memory heap where created
@return own: purge node */
purge_node_t*
row_purge_node_create(
	que_thr_t*	parent,
	mem_heap_t*	heap)
{
	purge_node_t*	node;

	ut_ad(parent != NULL);
	ut_ad(heap != NULL);

	node = static_cast<purge_node_t*>(
		mem_heap_zalloc(heap, sizeof(*node)));

	node->common.type = QUE_NODE_PURGE;
	node->common.parent = parent;
	node->done = TRUE;
	node->heap = mem_heap_create(256);

	return(node);
}

/***********************************************************//**
Repositions the pcur in the purge node on the clustered index record,
if found. If the record is not found, close pcur.
@return TRUE if the record was found */
static
ibool
row_purge_reposition_pcur(
/*======================*/
	ulint		mode,	/*!< in: latching mode */
	purge_node_t*	node,	/*!< in: row purge node */
	mtr_t*		mtr)	/*!< in: mtr */
{
	if (node->found_clust) {
		ut_ad(node->validate_pcur());

		node->found_clust = btr_pcur_restore_position(mode, &node->pcur, mtr);

	} else {
		node->found_clust = row_search_on_row_ref(
			&node->pcur, mode, node->table, node->ref, mtr);

		if (node->found_clust) {
			btr_pcur_store_position(&node->pcur, mtr);
		}
	}

	/* Close the current cursor if we fail to position it correctly. */
	if (!node->found_clust) {
		btr_pcur_close(&node->pcur);
	}

	return(node->found_clust);
}

/***********************************************************//**
Removes a delete marked clustered index record if possible.
@retval true if the row was not found, or it was successfully removed
@retval false if the row was modified after the delete marking */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
bool
row_purge_remove_clust_if_poss_low(
/*===============================*/
	purge_node_t*	node,	/*!< in/out: row purge node */
	ulint		mode)	/*!< in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE */
{
	dict_index_t*		index;
	bool			success		= true;
	mtr_t			mtr;
	rec_t*			rec;
	mem_heap_t*		heap		= NULL;
	ulint*			offsets;
	ulint			offsets_[REC_OFFS_NORMAL_SIZE];
	rec_offs_init(offsets_);

	ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_S));

	index = dict_table_get_first_index(node->table);

	log_free_check();
	mtr_start(&mtr);
	mtr.set_named_space(index->space);

	if (!row_purge_reposition_pcur(mode, node, &mtr)) {
		/* The record was already removed. */
		goto func_exit;
	}

	rec = btr_pcur_get_rec(&node->pcur);

	offsets = rec_get_offsets(
		rec, index, offsets_, ULINT_UNDEFINED, &heap);

	if (node->roll_ptr != row_get_rec_roll_ptr(rec, index, offsets)) {
		/* Someone else has modified the record later: do not remove */
		goto func_exit;
	}

	ut_ad(rec_get_deleted_flag(rec, rec_offs_comp(offsets)));

	if (mode == BTR_MODIFY_LEAF) {
		success = btr_cur_optimistic_delete(
			btr_pcur_get_btr_cur(&node->pcur), 0, &mtr);
	} else {
		dberr_t	err;
		ut_ad(mode == (BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE));
		btr_cur_pessimistic_delete(
			&err, FALSE, btr_pcur_get_btr_cur(&node->pcur), 0,
			false, &mtr);

		switch (err) {
		case DB_SUCCESS:
			break;
		case DB_OUT_OF_FILE_SPACE:
			success = false;
			break;
		default:
			ut_error;
		}
	}

func_exit:
	if (heap) {
		mem_heap_free(heap);
	}

	/* Persistent cursor is closed if reposition fails. */
	if (node->found_clust) {
		btr_pcur_commit_specify_mtr(&node->pcur, &mtr);
	} else {
		mtr_commit(&mtr);
	}

	return(success);
}

/***********************************************************//**
Removes a clustered index record if it has not been modified after the delete
marking.
@retval true if the row was not found, or it was successfully removed
@retval false the purge needs to be suspended because of running out
of file space. */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
bool
row_purge_remove_clust_if_poss(
/*===========================*/
	purge_node_t*	node)	/*!< in/out: row purge node */
{
	if (row_purge_remove_clust_if_poss_low(node, BTR_MODIFY_LEAF)) {
		return(true);
	}

	for (ulint n_tries = 0;
	     n_tries < BTR_CUR_RETRY_DELETE_N_TIMES;
	     n_tries++) {
		if (row_purge_remove_clust_if_poss_low(
			    node, BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE)) {
			return(true);
		}

		os_thread_sleep(BTR_CUR_RETRY_SLEEP_TIME);
	}

	return(false);
}

/***********************************************************//**
Determines if it is possible to remove a secondary index entry.
Removal is possible if the secondary index entry does not refer to any
not delete marked version of a clustered index record where DB_TRX_ID
is newer than the purge view.

NOTE: This function should only be called by the purge thread, only
while holding a latch on the leaf page of the secondary index entry
(or keeping the buffer pool watch on the page).  It is possible that
this function first returns true and then false, if a user transaction
inserts a record that the secondary index entry would refer to.
However, in that case, the user transaction would also re-insert the
secondary index entry after purge has removed it and released the leaf
page latch.
@return true if the secondary index record can be purged */
bool
row_purge_poss_sec(
/*===============*/
	purge_node_t*	node,	/*!< in/out: row purge node */
	dict_index_t*	index,	/*!< in: secondary index */
	const dtuple_t*	entry)	/*!< in: secondary index entry */
{
	bool	can_delete;
	mtr_t	mtr;

	ut_ad(!dict_index_is_clust(index));
	mtr_start(&mtr);

	can_delete = !row_purge_reposition_pcur(BTR_SEARCH_LEAF, node, &mtr)
		|| !row_vers_old_has_index_entry(TRUE,
						 btr_pcur_get_rec(&node->pcur),
						 &mtr, index, entry,
						 node->roll_ptr, node->trx_id);

	/* Persistent cursor is closed if reposition fails. */
	if (node->found_clust) {
		btr_pcur_commit_specify_mtr(&node->pcur, &mtr);
	} else {
		mtr_commit(&mtr);
	}

	return(can_delete);
}

/***************************************************************
Removes a secondary index entry if possible, by modifying the
index tree.  Does not try to buffer the delete.
@return TRUE if success or if not found */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
ibool
row_purge_remove_sec_if_poss_tree(
/*==============================*/
	purge_node_t*	node,	/*!< in: row purge node */
	dict_index_t*	index,	/*!< in: index */
	const dtuple_t*	entry)	/*!< in: index entry */
{
	btr_pcur_t		pcur;
	btr_cur_t*		btr_cur;
	ibool			success	= TRUE;
	dberr_t			err;
	mtr_t			mtr;
	enum row_search_result	search_result;

	log_free_check();
	mtr_start(&mtr);
	mtr.set_named_space(index->space);

	if (!index->is_committed()) {
		/* The index->online_status may change if the index is
		or was being created online, but not committed yet. It
		is protected by index->lock. */
		mtr_sx_lock(dict_index_get_lock(index), &mtr);

		if (dict_index_is_online_ddl(index)) {
			/* Online secondary index creation will not
			copy any delete-marked records. Therefore
			there is nothing to be purged. We must also
			skip the purge when a completed index is
			dropped by rollback_inplace_alter_table(). */
			goto func_exit_no_pcur;
		}
	} else {
		/* For secondary indexes,
		index->online_status==ONLINE_INDEX_COMPLETE if
		index->is_committed(). */
		ut_ad(!dict_index_is_online_ddl(index));
	}

	search_result = row_search_index_entry(
				index, entry,
				BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE,
				&pcur, &mtr);

	switch (search_result) {
	case ROW_NOT_FOUND:
		/* Not found.  This is a legitimate condition.  In a
		rollback, InnoDB will remove secondary recs that would
		be purged anyway.  Then the actual purge will not find
		the secondary index record.  Also, the purge itself is
		eager: if it comes to consider a secondary index
		record, and notices it does not need to exist in the
		index, it will remove it.  Then if/when the purge
		comes to consider the secondary index record a second
		time, it will not exist any more in the index. */

		/* fputs("PURGE:........sec entry not found\n", stderr); */
		/* dtuple_print(stderr, entry); */
		goto func_exit;
	case ROW_FOUND:
		break;
	case ROW_BUFFERED:
	case ROW_NOT_DELETED_REF:
		/* These are invalid outcomes, because the mode passed
		to row_search_index_entry() did not include any of the
		flags BTR_INSERT, BTR_DELETE, or BTR_DELETE_MARK. */
		ut_error;
	}

	btr_cur = btr_pcur_get_btr_cur(&pcur);

	/* We should remove the index record if no later version of the row,
	which cannot be purged yet, requires its existence. If some requires,
	we should do nothing. */

	if (row_purge_poss_sec(node, index, entry)) {
		/* Remove the index record, which should have been
		marked for deletion. */
		if (!rec_get_deleted_flag(btr_cur_get_rec(btr_cur),
					  dict_table_is_comp(index->table))) {
			ib::error()
				<< "tried to purge non-delete-marked record"
				" in index " << index->name
				<< " of table " << index->table->name
				<< ": tuple: " << *entry
				<< ", record: " << rec_index_print(
					btr_cur_get_rec(btr_cur), index);

			ut_ad(0);

			goto func_exit;
		}

		btr_cur_pessimistic_delete(&err, FALSE, btr_cur, 0,
					   false, &mtr);
		switch (UNIV_EXPECT(err, DB_SUCCESS)) {
		case DB_SUCCESS:
			break;
		case DB_OUT_OF_FILE_SPACE:
			success = FALSE;
			break;
		default:
			ut_error;
		}
	}

func_exit:
	btr_pcur_close(&pcur);
func_exit_no_pcur:
	mtr_commit(&mtr);

	return(success);
}

/***************************************************************
Removes a secondary index entry without modifying the index tree,
if possible.
@retval true if success or if not found
@retval false if row_purge_remove_sec_if_poss_tree() should be invoked */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
bool
row_purge_remove_sec_if_poss_leaf(
/*==============================*/
	purge_node_t*	node,	/*!< in: row purge node */
	dict_index_t*	index,	/*!< in: index */
	const dtuple_t*	entry)	/*!< in: index entry */
{
	mtr_t			mtr;
	btr_pcur_t		pcur;
	ulint			mode;
	enum row_search_result	search_result;
	bool			success	= true;

	log_free_check();

	mtr_start(&mtr);
	mtr.set_named_space(index->space);

	if (!index->is_committed()) {
		/* For uncommitted spatial index, we also skip the purge. */
		if (dict_index_is_spatial(index)) {
			goto func_exit_no_pcur;
		}

		/* The index->online_status may change if the the
		index is or was being created online, but not
		committed yet. It is protected by index->lock. */
		mtr_s_lock(dict_index_get_lock(index), &mtr);

		if (dict_index_is_online_ddl(index)) {
			/* Online secondary index creation will not
			copy any delete-marked records. Therefore
			there is nothing to be purged. We must also
			skip the purge when a completed index is
			dropped by rollback_inplace_alter_table(). */
			goto func_exit_no_pcur;
		}

		/* Change buffering is disabled for temporary tables. */
		mode = (dict_table_is_temporary(index->table))
			? BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED
			: BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED
			| BTR_DELETE;
	} else {
		/* For secondary indexes,
		index->online_status==ONLINE_INDEX_COMPLETE if
		index->is_committed(). */
		ut_ad(!dict_index_is_online_ddl(index));

		/* Change buffering is disabled for temporary tables
		and spatial index. */
		mode = (dict_table_is_temporary(index->table)
			|| dict_index_is_spatial(index))
			? BTR_MODIFY_LEAF
			: BTR_MODIFY_LEAF | BTR_DELETE;
	}

	/* Set the purge node for the call to row_purge_poss_sec(). */
	pcur.btr_cur.purge_node = node;
	if (dict_index_is_spatial(index)) {
		rw_lock_sx_lock(dict_index_get_lock(index));
		pcur.btr_cur.thr = NULL;
	} else {
		/* Set the query thread, so that ibuf_insert_low() will be
		able to invoke thd_get_trx(). */
		pcur.btr_cur.thr = static_cast<que_thr_t*>(
			que_node_get_parent(node));
	}

	search_result = row_search_index_entry(
		index, entry, mode, &pcur, &mtr);

	if (dict_index_is_spatial(index)) {
		rw_lock_sx_unlock(dict_index_get_lock(index));
	}

	switch (search_result) {
	case ROW_FOUND:
		/* Before attempting to purge a record, check
		if it is safe to do so. */
		if (row_purge_poss_sec(node, index, entry)) {
			btr_cur_t* btr_cur = btr_pcur_get_btr_cur(&pcur);

			/* Only delete-marked records should be purged. */
			if (!rec_get_deleted_flag(
				btr_cur_get_rec(btr_cur),
				dict_table_is_comp(index->table))) {

				ib::error()
					<< "tried to purge non-delete-marked"
					" record" " in index " << index->name
					<< " of table " << index->table->name
					<< ": tuple: " << *entry
					<< ", record: "
					<< rec_index_print(
						btr_cur_get_rec(btr_cur),
						index);
				ut_ad(0);

				btr_pcur_close(&pcur);

				goto func_exit_no_pcur;
			}

			if (dict_index_is_spatial(index)) {
				const page_t*   page;
				const trx_t*	trx = NULL;

				if (btr_cur->rtr_info != NULL
				    && btr_cur->rtr_info->thr != NULL) {
					trx = thr_get_trx(
						btr_cur->rtr_info->thr);
				}

				page = btr_cur_get_page(btr_cur);

				if (!lock_test_prdt_page_lock(
					trx,
					page_get_space_id(page),
					page_get_page_no(page))
				     && page_get_n_recs(page) < 2
				     && page_get_page_no(page) !=
					dict_index_get_page(index)) {
					/* this is the last record on page,
					and it has a "page" lock on it,
					which mean search is still depending
					on it, so do not delete */
#ifdef UNIV_DEBUG
					ib::info() << "skip purging last"
						" record on page "
						<< page_get_page_no(page)
						<< ".";
#endif /* UNIV_DEBUG */

					btr_pcur_close(&pcur);
					mtr_commit(&mtr);
					return(success);
				}
			}

			if (!btr_cur_optimistic_delete(btr_cur, 0, &mtr)) {

				/* The index entry could not be deleted. */
				success = false;
			}
		}
		/* fall through (the index entry is still needed,
		or the deletion succeeded) */
	case ROW_NOT_DELETED_REF:
		/* The index entry is still needed. */
	case ROW_BUFFERED:
		/* The deletion was buffered. */
	case ROW_NOT_FOUND:
		/* The index entry does not exist, nothing to do. */
		btr_pcur_close(&pcur);
func_exit_no_pcur:
		mtr_commit(&mtr);
		return(success);
	}

	ut_error;
	return(false);
}

/***********************************************************//**
Removes a secondary index entry if possible. */
UNIV_INLINE MY_ATTRIBUTE((nonnull(1,2)))
void
row_purge_remove_sec_if_poss(
/*=========================*/
	purge_node_t*	node,	/*!< in: row purge node */
	dict_index_t*	index,	/*!< in: index */
	const dtuple_t*	entry)	/*!< in: index entry */
{
	ibool	success;
	ulint	n_tries		= 0;

	/*	fputs("Purge: Removing secondary record\n", stderr); */

	if (!entry) {
		/* The node->row must have lacked some fields of this
		index. This is possible when the undo log record was
		written before this index was created. */
		return;
	}

	if (row_purge_remove_sec_if_poss_leaf(node, index, entry)) {

		return;
	}
retry:
	success = row_purge_remove_sec_if_poss_tree(node, index, entry);
	/* The delete operation may fail if we have little
	file space left: TODO: easiest to crash the database
	and restart with more file space */

	if (!success && n_tries < BTR_CUR_RETRY_DELETE_N_TIMES) {

		n_tries++;

		os_thread_sleep(BTR_CUR_RETRY_SLEEP_TIME);

		goto retry;
	}

	ut_a(success);
}

/** Skip uncommitted virtual indexes on newly added virtual column.
@param[in,out]	index	dict index object */
static
inline
void
row_purge_skip_uncommitted_virtual_index(
	dict_index_t*&	index)
{
	/* We need to skip virtual indexes which is not
	committed yet. It's safe because these indexes are
	newly created by alter table, and because we do
	not support LOCK=NONE when adding an index on newly
	added virtual column.*/
	while (index != NULL && dict_index_has_virtual(index)
	       && !index->is_committed() && index->has_new_v_col) {
		index = dict_table_get_next_index(index);
	}
}

/***********************************************************//**
Purges a delete marking of a record.
@retval true if the row was not found, or it was successfully removed
@retval false the purge needs to be suspended because of
running out of file space */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
bool
row_purge_del_mark(
/*===============*/
	purge_node_t*	node)	/*!< in/out: row purge node */
{
	mem_heap_t*	heap;

	heap = mem_heap_create(1024);

	while (node->index != NULL) {
		/* skip corrupted secondary index */
		dict_table_skip_corrupt_index(node->index);

		row_purge_skip_uncommitted_virtual_index(node->index);

		if (!node->index) {
			break;
		}

		if (node->index->type != DICT_FTS) {
			dtuple_t*	entry = row_build_index_entry_low(
				node->row, NULL, node->index,
				heap, ROW_BUILD_FOR_PURGE);
			row_purge_remove_sec_if_poss(node, node->index, entry);
			mem_heap_empty(heap);
		}

		node->index = dict_table_get_next_index(node->index);
	}

	mem_heap_free(heap);

	return(row_purge_remove_clust_if_poss(node));
}

/***********************************************************//**
Purges an update of an existing record. Also purges an update of a delete
marked record if that record contained an externally stored field. */
static
void
row_purge_upd_exist_or_extern_func(
/*===============================*/
#ifdef UNIV_DEBUG
	const que_thr_t*thr,		/*!< in: query thread */
#endif /* UNIV_DEBUG */
	purge_node_t*	node,		/*!< in: row purge node */
	trx_undo_rec_t*	undo_rec)	/*!< in: record to purge */
{
	mem_heap_t*	heap;

	ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_S));

	if (node->rec_type == TRX_UNDO_UPD_DEL_REC
	    || (node->cmpl_info & UPD_NODE_NO_ORD_CHANGE)) {

		goto skip_secondaries;
	}

	heap = mem_heap_create(1024);

	while (node->index != NULL) {
		dict_table_skip_corrupt_index(node->index);

		row_purge_skip_uncommitted_virtual_index(node->index);

		if (!node->index) {
			break;
		}

		if (row_upd_changes_ord_field_binary(node->index, node->update,
						     thr, NULL, NULL)) {
			/* Build the older version of the index entry */
			dtuple_t*	entry = row_build_index_entry_low(
				node->row, NULL, node->index,
				heap, ROW_BUILD_FOR_PURGE);
			row_purge_remove_sec_if_poss(node, node->index, entry);
			mem_heap_empty(heap);
		}

		node->index = dict_table_get_next_index(node->index);
	}

	mem_heap_free(heap);

skip_secondaries:
	/* Free possible externally stored fields */
	for (ulint i = 0; i < upd_get_n_fields(node->update); i++) {

		const upd_field_t*	ufield
			= upd_get_nth_field(node->update, i);

		if (dfield_is_ext(&ufield->new_val)) {
			trx_rseg_t*	rseg;
			buf_block_t*	block;
			ulint		internal_offset;
			byte*		data_field;
			dict_index_t*	index;
			ibool		is_insert;
			ulint		rseg_id;
			ulint		page_no;
			ulint		offset;
			mtr_t		mtr;

			/* We use the fact that new_val points to
			undo_rec and get thus the offset of
			dfield data inside the undo record. Then we
			can calculate from node->roll_ptr the file
			address of the new_val data */

			internal_offset
				= ((const byte*)
				   dfield_get_data(&ufield->new_val))
				- undo_rec;

			ut_a(internal_offset < UNIV_PAGE_SIZE);

			trx_undo_decode_roll_ptr(node->roll_ptr,
						 &is_insert, &rseg_id,
						 &page_no, &offset);

			/* If table is temp then it can't have its undo log
			residing in rollback segment with REDO log enabled. */
			bool is_redo_rseg =
				dict_table_is_temporary(node->table)
				? false : true;
			rseg = trx_sys_get_nth_rseg(
				trx_sys, rseg_id, is_redo_rseg);

			ut_a(rseg != NULL);
			ut_a(rseg->id == rseg_id);

			mtr_start(&mtr);

			/* We have to acquire an SX-latch to the clustered
			index tree (exclude other tree changes) */

			index = dict_table_get_first_index(node->table);
			mtr_sx_lock(dict_index_get_lock(index), &mtr);

			mtr.set_named_space(index->space);

			/* NOTE: we must also acquire an X-latch to the
			root page of the tree. We will need it when we
			free pages from the tree. If the tree is of height 1,
			the tree X-latch does NOT protect the root page,
			because it is also a leaf page. Since we will have a
			latch on an undo log page, we would break the
			latching order if we would only later latch the
			root page of such a tree! */

			btr_root_get(index, &mtr);

			block = buf_page_get(
				page_id_t(rseg->space, page_no),
				univ_page_size, RW_X_LATCH, &mtr);

			buf_block_dbg_add_level(block, SYNC_TRX_UNDO_PAGE);

			data_field = buf_block_get_frame(block)
				+ offset + internal_offset;

			ut_a(dfield_get_len(&ufield->new_val)
			     >= BTR_EXTERN_FIELD_REF_SIZE);
			btr_free_externally_stored_field(
				index,
				data_field + dfield_get_len(&ufield->new_val)
				- BTR_EXTERN_FIELD_REF_SIZE,
				NULL, NULL, NULL, 0, false, &mtr);
			mtr_commit(&mtr);
		}
	}
}

#ifdef UNIV_DEBUG
# define row_purge_upd_exist_or_extern(thr,node,undo_rec)	\
	row_purge_upd_exist_or_extern_func(thr,node,undo_rec)
#else /* UNIV_DEBUG */
# define row_purge_upd_exist_or_extern(thr,node,undo_rec)	\
	row_purge_upd_exist_or_extern_func(node,undo_rec)
#endif /* UNIV_DEBUG */

/***********************************************************//**
Parses the row reference and other info in a modify undo log record.
@return true if purge operation required */
static
bool
row_purge_parse_undo_rec(
/*=====================*/
	purge_node_t*		node,		/*!< in: row undo node */
	trx_undo_rec_t*		undo_rec,	/*!< in: record to purge */
	bool*			updated_extern, /*!< out: true if an externally
						stored field was updated */
	que_thr_t*		thr)		/*!< in: query thread */
{
	dict_index_t*	clust_index;
	byte*		ptr;
	trx_t*		trx;
	undo_no_t	undo_no;
	table_id_t	table_id;
	trx_id_t	trx_id;
	roll_ptr_t	roll_ptr;
	ulint		info_bits;
	ulint		type;

	ut_ad(node != NULL);
	ut_ad(thr != NULL);

	ptr = trx_undo_rec_get_pars(
		undo_rec, &type, &node->cmpl_info,
		updated_extern, &undo_no, &table_id);

	node->rec_type = type;

	if (type == TRX_UNDO_UPD_DEL_REC && !*updated_extern) {

		return(false);
	}

	ptr = trx_undo_update_rec_get_sys_cols(ptr, &trx_id, &roll_ptr,
					       &info_bits);
	node->table = NULL;
	node->trx_id = trx_id;

	/* Prevent DROP TABLE etc. from running when we are doing the purge
	for this row */

try_again:
	rw_lock_s_lock_inline(dict_operation_lock, 0, __FILE__, __LINE__);

	node->table = dict_table_open_on_id(
		table_id, FALSE, DICT_TABLE_OP_NORMAL);

	if (node->table == NULL) {
		/* The table has been dropped: no need to do purge */
		goto err_exit;
	}

	if (fil_space_is_being_truncated(node->table->space)) {

#if UNIV_DEBUG
		ib::info() << "Record with space id "
			   << node->table->space
			   << " belongs to table which is being truncated"
			   << " therefore skipping this undo record.";
#endif
		ut_ad(dict_table_is_file_per_table(node->table));
		dict_table_close(node->table, FALSE, FALSE);
		node->table = NULL;
		goto err_exit;
	}

	if (node->table->n_v_cols && !node->table->vc_templ
	    && dict_table_has_indexed_v_cols(node->table)) {
		/* Need server fully up for virtual column computation */
		if (!mysqld_server_started) {

			dict_table_close(node->table, FALSE, FALSE);
			rw_lock_s_unlock(dict_operation_lock);
			if (srv_shutdown_state != SRV_SHUTDOWN_NONE) {
				return(false);
			}
			os_thread_sleep(1000000);
			goto try_again;
		}

		/* Initialize the template for the table */
		innobase_init_vc_templ(node->table);
	}

	/* Disable purging for temp-tables as they are short-lived
	and no point in re-organzing such short lived tables */
	if (dict_table_is_temporary(node->table)) {
		goto close_exit;
	}

	if (node->table->ibd_file_missing) {
		/* We skip purge of missing .ibd files */

		dict_table_close(node->table, FALSE, FALSE);

		node->table = NULL;

		goto err_exit;
	}

	clust_index = dict_table_get_first_index(node->table);

	if (clust_index == NULL
	    || dict_index_is_corrupted(clust_index)) {
		/* The table was corrupt in the data dictionary.
		dict_set_corrupted() works on an index, and
		we do not have an index to call it with. */
close_exit:
		dict_table_close(node->table, FALSE, FALSE);
err_exit:
		rw_lock_s_unlock(dict_operation_lock);
		return(false);
	}

	if (type == TRX_UNDO_UPD_EXIST_REC
	    && (node->cmpl_info & UPD_NODE_NO_ORD_CHANGE)
	    && !*updated_extern) {

		/* Purge requires no changes to indexes: we may return */
		goto close_exit;
	}

	ptr = trx_undo_rec_get_row_ref(ptr, clust_index, &(node->ref),
				       node->heap);

	trx = thr_get_trx(thr);

	ptr = trx_undo_update_rec_get_update(ptr, clust_index, type, trx_id,
					     roll_ptr, info_bits, trx,
					     node->heap, &(node->update));

	/* Read to the partial row the fields that occur in indexes */

	if (!(node->cmpl_info & UPD_NODE_NO_ORD_CHANGE)) {
		ptr = trx_undo_rec_get_partial_row(
			ptr, clust_index, &node->row,
			type == TRX_UNDO_UPD_DEL_REC,
			node->heap);
	}

	return(true);
}

/***********************************************************//**
Purges the parsed record.
@return true if purged, false if skipped */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
bool
row_purge_record_func(
/*==================*/
	purge_node_t*	node,		/*!< in: row purge node */
	trx_undo_rec_t*	undo_rec,	/*!< in: record to purge */
#ifdef UNIV_DEBUG
	const que_thr_t*thr,		/*!< in: query thread */
#endif /* UNIV_DEBUG */
	bool		updated_extern)	/*!< in: whether external columns
					were updated */
{
	dict_index_t*	clust_index;
	bool		purged		= true;

	ut_ad(!node->found_clust);

	clust_index = dict_table_get_first_index(node->table);

	node->index = dict_table_get_next_index(clust_index);
	ut_ad(!trx_undo_roll_ptr_is_insert(node->roll_ptr));

	switch (node->rec_type) {
	case TRX_UNDO_DEL_MARK_REC:
		purged = row_purge_del_mark(node);
		if (!purged) {
			break;
		}
		MONITOR_INC(MONITOR_N_DEL_ROW_PURGE);
		break;
	default:
		if (!updated_extern) {
			break;
		}
		/* fall through */
	case TRX_UNDO_UPD_EXIST_REC:
		row_purge_upd_exist_or_extern(thr, node, undo_rec);
		MONITOR_INC(MONITOR_N_UPD_EXIST_EXTERN);
		break;
	}

	if (node->found_clust) {
		btr_pcur_close(&node->pcur);
		node->found_clust = FALSE;
	}

	if (node->table != NULL) {
		dict_table_close(node->table, FALSE, FALSE);
		node->table = NULL;
	}

	return(purged);
}

#ifdef UNIV_DEBUG
# define row_purge_record(node,undo_rec,thr,updated_extern)	\
	row_purge_record_func(node,undo_rec,thr,updated_extern)
#else /* UNIV_DEBUG */
# define row_purge_record(node,undo_rec,thr,updated_extern)	\
	row_purge_record_func(node,undo_rec,updated_extern)
#endif /* UNIV_DEBUG */

/***********************************************************//**
Fetches an undo log record and does the purge for the recorded operation.
If none left, or the current purge completed, returns the control to the
parent node, which is always a query thread node. */
static MY_ATTRIBUTE((nonnull))
void
row_purge(
/*======*/
	purge_node_t*	node,		/*!< in: row purge node */
	trx_undo_rec_t*	undo_rec,	/*!< in: record to purge */
	que_thr_t*	thr)		/*!< in: query thread */
{
	if (undo_rec != &trx_purge_dummy_rec) {
		bool	updated_extern;

		while (row_purge_parse_undo_rec(
			       node, undo_rec, &updated_extern, thr)) {

			bool purged = row_purge_record(
				node, undo_rec, thr, updated_extern);

			rw_lock_s_unlock(dict_operation_lock);

			if (purged
			    || srv_shutdown_state != SRV_SHUTDOWN_NONE) {
				return;
			}

			/* Retry the purge in a second. */
			os_thread_sleep(1000000);
		}
	}
}

/***********************************************************//**
Reset the purge query thread. */
UNIV_INLINE
void
row_purge_end(
/*==========*/
	que_thr_t*	thr)	/*!< in: query thread */
{
	purge_node_t*	node;

	ut_ad(thr);

	node = static_cast<purge_node_t*>(thr->run_node);

	ut_ad(que_node_get_type(node) == QUE_NODE_PURGE);

	thr->run_node = que_node_get_parent(node);

	node->undo_recs = NULL;

	node->done = TRUE;

	ut_a(thr->run_node != NULL);

	mem_heap_empty(node->heap);
}

/***********************************************************//**
Does the purge operation for a single undo log record. This is a high-level
function used in an SQL execution graph.
@return query thread to run next or NULL */
que_thr_t*
row_purge_step(
/*===========*/
	que_thr_t*	thr)	/*!< in: query thread */
{
	purge_node_t*	node;

	ut_ad(thr);

	node = static_cast<purge_node_t*>(thr->run_node);

	node->table = NULL;
	node->row = NULL;
	node->ref = NULL;
	node->index = NULL;
	node->update = NULL;
	node->found_clust = FALSE;
	node->rec_type = ULINT_UNDEFINED;
	node->cmpl_info = ULINT_UNDEFINED;

	ut_a(!node->done);

	ut_ad(que_node_get_type(node) == QUE_NODE_PURGE);

	if (!(node->undo_recs == NULL || ib_vector_is_empty(node->undo_recs))) {
		trx_purge_rec_t*purge_rec;

		purge_rec = static_cast<trx_purge_rec_t*>(
			ib_vector_pop(node->undo_recs));

		node->roll_ptr = purge_rec->roll_ptr;

		row_purge(node, purge_rec->undo_rec, thr);

		if (ib_vector_is_empty(node->undo_recs)) {
			row_purge_end(thr);
		} else {
			thr->run_node = node;
		}
	} else {
		row_purge_end(thr);
	}

	return(thr);
}

#ifdef UNIV_DEBUG
/***********************************************************//**
Validate the persisent cursor. The purge node has two references
to the clustered index record - one via the ref member, and the
other via the persistent cursor.  These two references must match
each other if the found_clust flag is set.
@return true if the stored copy of persistent cursor is consistent
with the ref member.*/
bool
purge_node_t::validate_pcur()
{
	if (!found_clust) {
		return(true);
	}

	if (index == NULL) {
		return(true);
	}

	if (index->type == DICT_FTS) {
		return(true);
	}

	if (!pcur.old_stored) {
		return(true);
	}

	dict_index_t*	clust_index = pcur.btr_cur.index;

	ulint*	offsets = rec_get_offsets(
		pcur.old_rec, clust_index, NULL, pcur.old_n_fields, &heap);

	/* Here we are comparing the purge ref record and the stored initial
	part in persistent cursor. Both cases we store n_uniq fields of the
	cluster index and so it is fine to do the comparison. We note this
	dependency here as pcur and ref belong to different modules. */
	int st = cmp_dtuple_rec(ref, pcur.old_rec, offsets);

	if (st != 0) {
		ib::error() << "Purge node pcur validation failed";
		ib::error() << rec_printer(ref).str();
		ib::error() << rec_printer(pcur.old_rec, offsets).str();
		return(false);
	}

	return(true);
}
#endif /* UNIV_DEBUG */
