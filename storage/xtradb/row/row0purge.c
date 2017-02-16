/*****************************************************************************

Copyright (c) 1997, 2015, Oracle and/or its affiliates. All Rights Reserved.

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
@file row/row0purge.c
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
#include "log0log.h"
#include "rem0cmp.h"

/*************************************************************************
IMPORTANT NOTE: Any operation that generates redo MUST check that there
is enough space in the redo log before for that operation. This is
done by calling log_free_check(). The reason for checking the
availability of the redo log space before the start of the operation is
that we MUST not hold any synchonization objects when performing the
check.
If you make a change in this module make sure that no codepath is
introduced where a call to log_free_check() is bypassed. */

/********************************************************************//**
Creates a purge node to a query graph.
@return	own: purge node */
UNIV_INTERN
purge_node_t*
row_purge_node_create(
/*==================*/
	que_thr_t*	parent,	/*!< in: parent node, i.e., a thr node */
	mem_heap_t*	heap)	/*!< in: memory heap where created */
{
	purge_node_t*	node;

	ut_ad(parent && heap);

	node = mem_heap_alloc(heap, sizeof(purge_node_t));

	node->common.type = QUE_NODE_PURGE;
	node->common.parent = parent;

	node->heap = mem_heap_create(256);

	return(node);
}

/***********************************************************//**
Repositions the pcur in the purge node on the clustered index record,
if found. If the record is not found, close pcur.
@return	TRUE if the record was found */
static
ibool
row_purge_reposition_pcur(
/*======================*/
	ulint		mode,	/*!< in: latching mode */
	purge_node_t*	node,	/*!< in: row purge node */
	mtr_t*		mtr)	/*!< in: mtr */
{
	if (node->found_clust) {
		ut_ad(row_purge_validate_pcur(node));

		node->found_clust = btr_pcur_restore_position(
			mode, &(node->pcur), mtr);

	} else {

		node->found_clust = row_search_on_row_ref(
			&(node->pcur), mode, node->table, node->ref, mtr);

		if (node->found_clust) {
			btr_pcur_store_position(&(node->pcur), mtr);
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
@return TRUE if success, or if not found, or if modified after the
delete marking */
static
ibool
row_purge_remove_clust_if_poss_low(
/*===============================*/
	purge_node_t*	node,	/*!< in: row purge node */
	ulint		mode)	/*!< in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE */
{
	dict_index_t*	index;
	btr_pcur_t*	pcur;
	btr_cur_t*	btr_cur;
	ibool		success;
	ulint		err;
	mtr_t		mtr;
	rec_t*		rec;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	rec_offs_init(offsets_);

	index = dict_table_get_first_index(node->table);

	pcur = &(node->pcur);
	btr_cur = btr_pcur_get_btr_cur(pcur);

	log_free_check();
	mtr_start(&mtr);

	success = row_purge_reposition_pcur(mode, node, &mtr);

	if (!success) {
		/* The record is already removed */
		/* Persistent cursor is closed if reposition fails. */
		mtr_commit(&mtr);

		return(TRUE);
	}

	rec = btr_pcur_get_rec(pcur);

	if (node->roll_ptr != row_get_rec_roll_ptr(
		    rec, index, rec_get_offsets(rec, index, offsets_,
						ULINT_UNDEFINED, &heap))) {
		if (UNIV_LIKELY_NULL(heap)) {
			mem_heap_free(heap);
		}
		/* Someone else has modified the record later: do not remove */
		btr_pcur_commit_specify_mtr(pcur, &mtr);

		return(TRUE);
	}

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}

	if (mode == BTR_MODIFY_LEAF) {
		success = btr_cur_optimistic_delete(btr_cur, &mtr);
	} else {
		ut_ad(mode == BTR_MODIFY_TREE);
		btr_cur_pessimistic_delete(&err, FALSE, btr_cur,
					   RB_NONE, &mtr);

		if (err == DB_SUCCESS) {
			success = TRUE;
		} else if (err == DB_OUT_OF_FILE_SPACE) {
			success = FALSE;
		} else {
			ut_error;
		}
	}

	btr_pcur_commit_specify_mtr(pcur, &mtr);

	return(success);
}

/***********************************************************//**
Removes a clustered index record if it has not been modified after the delete
marking. */
static
void
row_purge_remove_clust_if_poss(
/*===========================*/
	purge_node_t*	node)	/*!< in: row purge node */
{
	ibool	success;
	ulint	n_tries	= 0;

	/*	fputs("Purge: Removing clustered record\n", stderr); */

	success = row_purge_remove_clust_if_poss_low(node, BTR_MODIFY_LEAF);
	if (success) {

		return;
	}
retry:
	success = row_purge_remove_clust_if_poss_low(node, BTR_MODIFY_TREE);
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

/***********************************************************//**
Determines if it is possible to remove a secondary index entry.
Removal is possible if the secondary index entry does not refer to any
not delete marked version of a clustered index record where DB_TRX_ID
is newer than the purge view.

NOTE: This function should only be called by the purge thread, only
while holding a latch on the leaf page of the secondary index entry
(or keeping the buffer pool watch on the page).  It is possible that
this function first returns TRUE and then FALSE, if a user transaction
inserts a record that the secondary index entry would refer to.
However, in that case, the user transaction would also re-insert the
secondary index entry after purge has removed it and released the leaf
page latch.
@return	TRUE if the secondary index record can be purged */
UNIV_INTERN
ibool
row_purge_poss_sec(
/*===============*/
	purge_node_t*	node,	/*!< in/out: row purge node */
	dict_index_t*	index,	/*!< in: secondary index */
	const dtuple_t*	entry)	/*!< in: secondary index entry */
{
	ibool	can_delete;
	mtr_t	mtr;

	ut_ad(!dict_index_is_clust(index));
	mtr_start(&mtr);

	can_delete = !row_purge_reposition_pcur(BTR_SEARCH_LEAF, node, &mtr)
		|| !row_vers_old_has_index_entry(TRUE,
						 btr_pcur_get_rec(&node->pcur),
						 &mtr, index, entry);

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
@return	TRUE if success or if not found */
static
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
	ulint			err;
	mtr_t			mtr;
	enum row_search_result	search_result;

	log_free_check();
	mtr_start(&mtr);

	search_result = row_search_index_entry(index, entry, BTR_MODIFY_TREE,
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
		ut_ad(REC_INFO_DELETED_FLAG
		      & rec_get_info_bits(btr_cur_get_rec(btr_cur),
					  dict_table_is_comp(index->table)));

		btr_cur_pessimistic_delete(&err, FALSE, btr_cur,
					   RB_NONE, &mtr);
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
	mtr_commit(&mtr);

	return(success);
}

/***************************************************************
Removes a secondary index entry without modifying the index tree,
if possible.
@return	TRUE if success or if not found */
static
ibool
row_purge_remove_sec_if_poss_leaf(
/*==============================*/
	purge_node_t*	node,	/*!< in: row purge node */
	dict_index_t*	index,	/*!< in: index */
	const dtuple_t*	entry)	/*!< in: index entry */
{
	mtr_t			mtr;
	btr_pcur_t		pcur;
	enum row_search_result	search_result;

	log_free_check();

	mtr_start(&mtr);

	/* Set the purge node for the call to row_purge_poss_sec(). */
	pcur.btr_cur.purge_node = node;
	/* Set the query thread, so that ibuf_insert_low() will be
	able to invoke thd_get_trx(). */
	pcur.btr_cur.thr = que_node_get_parent(node);

	search_result = row_search_index_entry(
		index, entry, BTR_MODIFY_LEAF | BTR_DELETE, &pcur, &mtr);

	switch (search_result) {
		ibool	success;
	case ROW_FOUND:
		/* Before attempting to purge a record, check
		if it is safe to do so. */
		if (row_purge_poss_sec(node, index, entry)) {
			btr_cur_t* btr_cur = btr_pcur_get_btr_cur(&pcur);

			/* Only delete-marked records should be purged. */
			ut_ad(REC_INFO_DELETED_FLAG
			      & rec_get_info_bits(
				      btr_cur_get_rec(btr_cur),
				      dict_table_is_comp(index->table)));

			if (!btr_cur_optimistic_delete(btr_cur, &mtr)) {

				/* The index entry could not be deleted. */
				success = FALSE;
				goto func_exit;
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
		success = TRUE;
	func_exit:
		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		return(success);
	}

	ut_error;
	return(FALSE);
}

/***********************************************************//**
Removes a secondary index entry if possible. */
UNIV_INLINE
void
row_purge_remove_sec_if_poss(
/*=========================*/
	purge_node_t*	node,	/*!< in: row purge node */
	dict_index_t*	index,	/*!< in: index */
	dtuple_t*	entry)	/*!< in: index entry */
{
	ibool	success;
	ulint	n_tries		= 0;

	/*	fputs("Purge: Removing secondary record\n", stderr); */

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

/***********************************************************//**
Purges a delete marking of a record. */
static
void
row_purge_del_mark(
/*===============*/
	purge_node_t*	node)	/*!< in: row purge node */
{
	mem_heap_t*	heap;
	dtuple_t*	entry;
	dict_index_t*	index;

	ut_ad(node);

	heap = mem_heap_create(1024);

	while (node->index != NULL) {
		/* skip corrupted secondary index */
		dict_table_skip_corrupt_index(node->index);

		if (!node->index) {
			break;
		}

		index = node->index;

		/* Build the index entry */
		entry = row_build_index_entry(node->row, NULL, index, heap);
		ut_a(entry);
		row_purge_remove_sec_if_poss(node, index, entry);

		node->index = dict_table_get_next_index(node->index);
	}

	mem_heap_free(heap);

	row_purge_remove_clust_if_poss(node);
}

/***********************************************************//**
Purges an update of an existing record. Also purges an update of a delete
marked record if that record contained an externally stored field. */
static
void
row_purge_upd_exist_or_extern_func(
/*===============================*/
#ifdef UNIV_DEBUG
	const que_thr_t*thr,	/*!< in: query thread */
#endif /* UNIV_DEBUG */
	purge_node_t*	node)	/*!< in: row purge node */
{
	mem_heap_t*	heap;
	dtuple_t*	entry;
	dict_index_t*	index;
	ibool		is_insert;
	ulint		rseg_id;
	ulint		page_no;
	ulint		offset;
	ulint		i;
	mtr_t		mtr;

	ut_ad(node);

	if (node->rec_type == TRX_UNDO_UPD_DEL_REC
	    || (node->cmpl_info & UPD_NODE_NO_ORD_CHANGE)) {

		goto skip_secondaries;
	}

	heap = mem_heap_create(1024);

	while (node->index != NULL) {
		dict_table_skip_corrupt_index(node->index);

		if (!node->index) {
			break;
		}

		index = node->index;

		if (row_upd_changes_ord_field_binary(node->index, node->update,
						     thr, NULL, NULL)) {
			/* Build the older version of the index entry */
			entry = row_build_index_entry(node->row, NULL,
						      index, heap);
			ut_a(entry);
			row_purge_remove_sec_if_poss(node, index, entry);
		}

		node->index = dict_table_get_next_index(node->index);
	}

	mem_heap_free(heap);

skip_secondaries:
	/* Free possible externally stored fields */
	for (i = 0; i < upd_get_n_fields(node->update); i++) {

		const upd_field_t*	ufield
			= upd_get_nth_field(node->update, i);

		if (dfield_is_ext(&ufield->new_val)) {
			buf_block_t*	block;
			ulint		internal_offset;
			byte*		data_field;

			/* We use the fact that new_val points to
			node->undo_rec and get thus the offset of
			dfield data inside the undo record. Then we
			can calculate from node->roll_ptr the file
			address of the new_val data */

			internal_offset
				= ((const byte*)
				   dfield_get_data(&ufield->new_val))
				- node->undo_rec;

			ut_a(internal_offset < UNIV_PAGE_SIZE);

			trx_undo_decode_roll_ptr(node->roll_ptr,
						 &is_insert, &rseg_id,
						 &page_no, &offset);
			mtr_start(&mtr);

			/* We have to acquire an X-latch to the clustered
			index tree */

			index = dict_table_get_first_index(node->table);

			mtr_x_lock(dict_index_get_lock(index), &mtr);

			/* NOTE: we must also acquire an X-latch to the
			root page of the tree. We will need it when we
			free pages from the tree. If the tree is of height 1,
			the tree X-latch does NOT protect the root page,
			because it is also a leaf page. Since we will have a
			latch on an undo log page, we would break the
			latching order if we would only later latch the
			root page of such a tree! */

			btr_root_get(index, &mtr);

			/* We assume in purge of externally stored fields
			that the space id of the undo log record is 0! */

			block = buf_page_get(0, 0, page_no, RW_X_LATCH, &mtr);
			buf_block_dbg_add_level(block, SYNC_TRX_UNDO_PAGE);

			data_field = buf_block_get_frame(block)
				+ offset + internal_offset;

			ut_a(dfield_get_len(&ufield->new_val)
			     >= BTR_EXTERN_FIELD_REF_SIZE);
			btr_free_externally_stored_field(
				index,
				data_field + dfield_get_len(&ufield->new_val)
				- BTR_EXTERN_FIELD_REF_SIZE,
				NULL, NULL, NULL, 0, RB_NONE, &mtr);
			mtr_commit(&mtr);
		}
	}
}

#ifdef UNIV_DEBUG
# define row_purge_upd_exist_or_extern(thr,node)	\
	row_purge_upd_exist_or_extern_func(thr,node)
#else /* UNIV_DEBUG */
# define row_purge_upd_exist_or_extern(thr,node)	\
	row_purge_upd_exist_or_extern_func(node)
#endif /* UNIV_DEBUG */

/***********************************************************//**
Parses the row reference and other info in a modify undo log record.
@return TRUE if purge operation required: NOTE that then the CALLER
must unfreeze data dictionary! */
static
ibool
row_purge_parse_undo_rec(
/*=====================*/
	purge_node_t*	node,	/*!< in: row undo node */
	ibool*		updated_extern,
				/*!< out: TRUE if an externally stored field
				was updated */
	que_thr_t*	thr)	/*!< in: query thread */
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

	ut_ad(node && thr);

	trx = thr_get_trx(thr);

	ptr = trx_undo_rec_get_pars(
		node->undo_rec, &type, &node->cmpl_info,
		updated_extern, &undo_no, &table_id);
	node->rec_type = type;

	if (type == TRX_UNDO_UPD_DEL_REC && !(*updated_extern)) {

		return(FALSE);
	}

	ptr = trx_undo_update_rec_get_sys_cols(ptr, &trx_id, &roll_ptr,
					       &info_bits);
	node->table = NULL;

	if (type == TRX_UNDO_UPD_EXIST_REC
	    && node->cmpl_info & UPD_NODE_NO_ORD_CHANGE
	    && !(*updated_extern)) {

		/* Purge requires no changes to indexes: we may return */

		return(FALSE);
	}

	/* Prevent DROP TABLE etc. from running when we are doing the purge
	for this row */

	row_mysql_freeze_data_dictionary(trx);

	mutex_enter(&(dict_sys->mutex));

	node->table = dict_table_get_on_id_low(table_id);

	mutex_exit(&(dict_sys->mutex));

	if (node->table == NULL) {
		/* The table has been dropped: no need to do purge */
err_exit:
		row_mysql_unfreeze_data_dictionary(trx);
		return(FALSE);
	}

	if (node->table->ibd_file_missing) {
		/* We skip purge of missing .ibd files */

		node->table = NULL;

		goto err_exit;
	}

	clust_index = dict_table_get_first_index(node->table);

	if (clust_index == NULL) {
		/* The table was corrupt in the data dictionary */

		goto err_exit;
	}

	ptr = trx_undo_rec_get_row_ref(ptr, clust_index, &(node->ref),
				       node->heap);

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

	return(TRUE);
}

/***********************************************************//**
Fetches an undo log record and does the purge for the recorded operation.
If none left, or the current purge completed, returns the control to the
parent node, which is always a query thread node. */
static __attribute__((nonnull))
void
row_purge(
/*======*/
	purge_node_t*	node,	/*!< in: row purge node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	ibool		updated_extern;

	ut_ad(node);
	ut_ad(thr);

	node->undo_rec = trx_purge_fetch_next_rec(&node->roll_ptr,
						  &node->reservation,
						  node->heap);
	if (!node->undo_rec) {
		/* Purge completed for this query thread */

		thr->run_node = que_node_get_parent(node);

		return;
	}

	if (node->undo_rec != &trx_purge_dummy_rec
	    && row_purge_parse_undo_rec(node, &updated_extern, thr)) {
		node->found_clust = FALSE;

		node->index = dict_table_get_next_index(
			dict_table_get_first_index(node->table));

		if (node->rec_type == TRX_UNDO_DEL_MARK_REC) {
			row_purge_del_mark(node);

		} else if (updated_extern
			   || node->rec_type == TRX_UNDO_UPD_EXIST_REC) {

			row_purge_upd_exist_or_extern(thr, node);
		}

		if (node->found_clust) {
			btr_pcur_close(&(node->pcur));
		}

		row_mysql_unfreeze_data_dictionary(thr_get_trx(thr));
	}

	/* Do some cleanup */
	trx_purge_rec_release(node->reservation);
	mem_heap_empty(node->heap);

	thr->run_node = node;
}

/***********************************************************//**
Does the purge operation for a single undo log record. This is a high-level
function used in an SQL execution graph.
@return	query thread to run next or NULL */
UNIV_INTERN
que_thr_t*
row_purge_step(
/*===========*/
	que_thr_t*	thr)	/*!< in: query thread */
{
	purge_node_t*	node;

	ut_ad(thr);

	node = thr->run_node;

	ut_ad(que_node_get_type(node) == QUE_NODE_PURGE);

	row_purge(node, thr);

	return(thr);
}

#ifdef UNIV_DEBUG
/***********************************************************//**
Validate the persisent cursor in the purge node. The purge node has two
references to the clustered index record - one via the ref member, and the
other via the persistent cursor.  These two references must match each
other if the found_clust flag is set.
@return true if the stored copy of persistent cursor is consistent
with the ref member.*/
UNIV_INTERN
ibool
row_purge_validate_pcur(
	purge_node_t*	node)
{
	dict_index_t*	clust_index;
	ulint*		offsets;
	int		st;

	if (!node->found_clust) {
		return(TRUE);
	}

	if (node->index == NULL) {
		return(TRUE);
	}

	if (node->pcur.old_stored != BTR_PCUR_OLD_STORED) {
		return(TRUE);
	}

	clust_index = node->pcur.btr_cur.index;

	offsets = rec_get_offsets(node->pcur.old_rec, clust_index, NULL,
				  node->pcur.old_n_fields, &node->heap);

	/* Here we are comparing the purge ref record and the stored initial
	part in persistent cursor. Both cases we store n_uniq fields of the
	cluster index and so it is fine to do the comparison. We note this
	dependency here as pcur and ref belong to different modules. */
	st = cmp_dtuple_rec(node->ref, node->pcur.old_rec, offsets);

	if (st != 0) {
		fprintf(stderr, "Purge node pcur validation failed\n");
		dtuple_print(stderr, node->ref);
		rec_print(stderr, node->pcur.old_rec, clust_index);
		return(FALSE);
	}

	return(TRUE);
}
#endif /* UNIV_DEBUG */
