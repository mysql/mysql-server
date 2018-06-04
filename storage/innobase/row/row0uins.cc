/*****************************************************************************

Copyright (c) 1997, 2018, Oracle and/or its affiliates. All Rights Reserved.

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
@file row/row0uins.cc
Fresh insert undo

Created 2/25/1997 Heikki Tuuri
*******************************************************/

#include "row0uins.h"

#ifdef UNIV_NONINL
#include "row0uins.ic"
#endif

#include "dict0dict.h"
#include "dict0boot.h"
#include "dict0crea.h"
#include "trx0undo.h"
#include "trx0roll.h"
#include "btr0btr.h"
#include "mach0data.h"
#include "row0undo.h"
#include "row0vers.h"
#include "row0log.h"
#include "trx0trx.h"
#include "trx0rec.h"
#include "row0row.h"
#include "row0upd.h"
#include "que0que.h"
#include "ibuf0ibuf.h"
#include "log0log.h"
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

/***************************************************************//**
Removes a clustered index record. The pcur in node was positioned on the
record, now it is detached.
@return DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
static  MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_undo_ins_remove_clust_rec(
/*==========================*/
	undo_node_t*	node)	/*!< in: undo node */
{
	btr_cur_t*	btr_cur;
	ibool		success;
	dberr_t		err;
	ulint		n_tries	= 0;
	mtr_t		mtr;
	dict_index_t*	index	= node->pcur.btr_cur.index;
	bool		online;

	ut_ad(dict_index_is_clust(index));
	ut_ad(node->trx->in_rollback);

	mtr_start(&mtr);
	mtr.set_named_space(index->space);
	dict_disable_redo_if_temporary(index->table, &mtr);

	/* This is similar to row_undo_mod_clust(). The DDL thread may
	already have copied this row from the log to the new table.
	We must log the removal, so that the row will be correctly
	purged. However, we can log the removal out of sync with the
	B-tree modification. */

	online = dict_index_is_online_ddl(index);
	if (online) {
		ut_ad(node->trx->dict_operation_lock_mode
		      != RW_X_LATCH);
		ut_ad(node->table->id != DICT_INDEXES_ID);
		mtr_s_lock(dict_index_get_lock(index), &mtr);
	}

	success = btr_pcur_restore_position(
		online
		? BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED
		: BTR_MODIFY_LEAF, &node->pcur, &mtr);
	ut_a(success);

	btr_cur = btr_pcur_get_btr_cur(&node->pcur);

	ut_ad(rec_get_trx_id(btr_cur_get_rec(btr_cur), btr_cur->index)
	      == node->trx->id);
	ut_ad(!rec_get_deleted_flag(
		      btr_cur_get_rec(btr_cur),
		      dict_table_is_comp(btr_cur->index->table)));

	if (online && dict_index_is_online_ddl(index)) {
		const rec_t*	rec	= btr_cur_get_rec(btr_cur);
		mem_heap_t*	heap	= NULL;
		const ulint*	offsets	= rec_get_offsets(
			rec, index, NULL, ULINT_UNDEFINED, &heap);
		row_log_table_delete(rec, node->row, index, offsets, NULL);
		mem_heap_free(heap);
	}

	if (node->table->id == DICT_INDEXES_ID) {

		ut_ad(!online);
		ut_ad(node->trx->dict_operation_lock_mode == RW_X_LATCH);

		dict_drop_index_tree(
			btr_pcur_get_rec(&node->pcur), &(node->pcur), &mtr);

		mtr_commit(&mtr);

		mtr_start(&mtr);

		success = btr_pcur_restore_position(
			BTR_MODIFY_LEAF, &node->pcur, &mtr);
		ut_a(success);
	}

	if (btr_cur_optimistic_delete(btr_cur, 0, &mtr)) {
		err = DB_SUCCESS;
		goto func_exit;
	}

	btr_pcur_commit_specify_mtr(&node->pcur, &mtr);
retry:
	/* If did not succeed, try pessimistic descent to tree */
	mtr_start(&mtr);
	mtr.set_named_space(index->space);
	dict_disable_redo_if_temporary(index->table, &mtr);

	success = btr_pcur_restore_position(
			BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE,
			&node->pcur, &mtr);
	ut_a(success);

	btr_cur_pessimistic_delete(&err, FALSE, btr_cur, 0, true, &mtr);

	/* The delete operation may fail if we have little
	file space left: TODO: easiest to crash the database
	and restart with more file space */

	if (err == DB_OUT_OF_FILE_SPACE
	    && n_tries < BTR_CUR_RETRY_DELETE_N_TIMES) {

		btr_pcur_commit_specify_mtr(&(node->pcur), &mtr);

		n_tries++;

		os_thread_sleep(BTR_CUR_RETRY_SLEEP_TIME);

		goto retry;
	}

func_exit:
	btr_pcur_commit_specify_mtr(&node->pcur, &mtr);

	return(err);
}

/***************************************************************//**
Removes a secondary index entry if found.
@return DB_SUCCESS, DB_FAIL, or DB_OUT_OF_FILE_SPACE */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_undo_ins_remove_sec_low(
/*========================*/
	ulint		mode,	/*!< in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE,
				depending on whether we wish optimistic or
				pessimistic descent down the index tree */
	dict_index_t*	index,	/*!< in: index */
	dtuple_t*	entry,	/*!< in: index entry to remove */
	que_thr_t*	thr)	/*!< in: query thread */
{
	btr_pcur_t		pcur;
	btr_cur_t*		btr_cur;
	dberr_t			err	= DB_SUCCESS;
	mtr_t			mtr;
	enum row_search_result	search_result;
	ibool			modify_leaf = false;

	log_free_check();

	mtr_start(&mtr);
	mtr.set_named_space(index->space);
	dict_disable_redo_if_temporary(index->table, &mtr);

	if (mode == BTR_MODIFY_LEAF) {
		mode = BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED;
		mtr_s_lock(dict_index_get_lock(index), &mtr);
		modify_leaf = true;
	} else {
		ut_ad(mode == (BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE));
		mtr_sx_lock(dict_index_get_lock(index), &mtr);
	}

	if (row_log_online_op_try(index, entry, 0)) {
		goto func_exit_no_pcur;
	}

	if (dict_index_is_spatial(index)) {
		if (mode & BTR_MODIFY_LEAF) {
			mode |= BTR_RTREE_DELETE_MARK;
		}
		btr_pcur_get_btr_cur(&pcur)->thr = thr;
		mode |= BTR_RTREE_UNDO_INS;
	}

	search_result = row_search_index_entry(index, entry, mode,
					       &pcur, &mtr);

	switch (search_result) {
	case ROW_NOT_FOUND:
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

	if (search_result == ROW_FOUND && dict_index_is_spatial(index)) {
		rec_t*	rec = btr_pcur_get_rec(&pcur);
		if (rec_get_deleted_flag(rec,
					 dict_table_is_comp(index->table))) {
			ib::error() << "Record found in index " << index->name
				<< " is deleted marked on insert rollback.";
		}
	}

	btr_cur = btr_pcur_get_btr_cur(&pcur);

	if (modify_leaf) {
		err = btr_cur_optimistic_delete(btr_cur, 0, &mtr)
			? DB_SUCCESS : DB_FAIL;
	} else {
		/* Passing rollback=false here, because we are
		deleting a secondary index record: the distinction
		only matters when deleting a record that contains
		externally stored columns. */
		ut_ad(!dict_index_is_clust(index));
		btr_cur_pessimistic_delete(&err, FALSE, btr_cur, 0,
					   false, &mtr);
	}
func_exit:
	btr_pcur_close(&pcur);
func_exit_no_pcur:
	mtr_commit(&mtr);

	return(err);
}

/***************************************************************//**
Removes a secondary index entry from the index if found. Tries first
optimistic, then pessimistic descent down the tree.
@return DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_undo_ins_remove_sec(
/*====================*/
	dict_index_t*	index,	/*!< in: index */
	dtuple_t*	entry,	/*!< in: index entry to insert */
	que_thr_t*	thr)	/*!< in: query thread */
{
	dberr_t	err;
	ulint	n_tries	= 0;

	/* Try first optimistic descent to the B-tree */

	err = row_undo_ins_remove_sec_low(BTR_MODIFY_LEAF, index, entry, thr);

	if (err == DB_SUCCESS) {

		return(err);
	}

	/* Try then pessimistic descent to the B-tree */
retry:
	err = row_undo_ins_remove_sec_low(
		BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE,
		index, entry, thr);

	/* The delete operation may fail if we have little
	file space left: TODO: easiest to crash the database
	and restart with more file space */

	if (err != DB_SUCCESS && n_tries < BTR_CUR_RETRY_DELETE_N_TIMES) {

		n_tries++;

		os_thread_sleep(BTR_CUR_RETRY_SLEEP_TIME);

		goto retry;
	}

	return(err);
}

/***********************************************************//**
Parses the row reference and other info in a fresh insert undo record. */
static
void
row_undo_ins_parse_undo_rec(
/*========================*/
	undo_node_t*	node,		/*!< in/out: row undo node */
	ibool		dict_locked)	/*!< in: TRUE if own dict_sys->mutex */
{
	dict_index_t*	clust_index;
	byte*		ptr;
	undo_no_t	undo_no;
	table_id_t	table_id;
	ulint		type;
	ulint		dummy;
	bool		dummy_extern;

	ut_ad(node);

	ptr = trx_undo_rec_get_pars(node->undo_rec, &type, &dummy,
				    &dummy_extern, &undo_no, &table_id);
	ut_ad(type == TRX_UNDO_INSERT_REC);
	node->rec_type = type;

	node->update = NULL;
	node->table = dict_table_open_on_id(
		table_id, dict_locked, DICT_TABLE_OP_NORMAL);

	/* Skip the UNDO if we can't find the table or the .ibd file. */
	if (UNIV_UNLIKELY(node->table == NULL)) {
	} else if (UNIV_UNLIKELY(node->table->ibd_file_missing)) {
close_table:
		dict_table_close(node->table, dict_locked, FALSE);
		node->table = NULL;
	} else if (fil_space_is_being_truncated(node->table->space)) {

		dict_table_close(node->table, dict_locked, FALSE);
		node->table = NULL;
	} else {
		clust_index = dict_table_get_first_index(node->table);

		if (clust_index != NULL) {
			ptr = trx_undo_rec_get_row_ref(
				ptr, clust_index, &node->ref, node->heap);

			if (!row_undo_search_clust_to_pcur(node)) {
				goto close_table;
			}
			if (node->table->n_v_cols) {
				trx_undo_read_v_cols(node->table, ptr,
						     node->row, false, NULL);
			}

		} else {
			ib::warn() << "Table " << node->table->name
				 << " has no indexes,"
				" ignoring the table";
			goto close_table;
		}
	}
}

/***************************************************************//**
Removes secondary index records.
@return DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_undo_ins_remove_sec_rec(
/*========================*/
	undo_node_t*	node,	/*!< in/out: row undo node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	dberr_t		err	= DB_SUCCESS;
	dict_index_t*	index	= node->index;
	mem_heap_t*	heap;

	heap = mem_heap_create(1024);

	while (index != NULL) {
		dtuple_t*	entry;

		if (index->type & DICT_FTS) {
			dict_table_next_uncorrupted_index(index);
			continue;
		}

		/* An insert undo record TRX_UNDO_INSERT_REC will
		always contain all fields of the index. It does not
		matter if any indexes were created afterwards; all
		index entries can be reconstructed from the row. */
		entry = row_build_index_entry(
			node->row, node->ext, index, heap);
		if (UNIV_UNLIKELY(!entry)) {
			/* The database must have crashed after
			inserting a clustered index record but before
			writing all the externally stored columns of
			that record, or a statement is being rolled
			back because an error occurred while storing
			off-page columns.

			Because secondary index entries are inserted
			after the clustered index record, we may
			assume that the secondary index record does
			not exist. */
		} else {
			err = row_undo_ins_remove_sec(index, entry, thr);

			if (UNIV_UNLIKELY(err != DB_SUCCESS)) {
				goto func_exit;
			}
		}

		mem_heap_empty(heap);
		dict_table_next_uncorrupted_index(index);
	}

func_exit:
	node->index = index;
	mem_heap_free(heap);
	return(err);
}

/***********************************************************//**
Undoes a fresh insert of a row to a table. A fresh insert means that
the same clustered index unique key did not have any record, even delete
marked, at the time of the insert.  InnoDB is eager in a rollback:
if it figures out that an index record will be removed in the purge
anyway, it will remove it in the rollback.
@return DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
dberr_t
row_undo_ins(
/*=========*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	dberr_t	err;
	ibool	dict_locked;

	ut_ad(node->state == UNDO_NODE_INSERT);
	ut_ad(node->trx->in_rollback);
	ut_ad(trx_undo_roll_ptr_is_insert(node->roll_ptr));

	dict_locked = node->trx->dict_operation_lock_mode == RW_X_LATCH;

	row_undo_ins_parse_undo_rec(node, dict_locked);

	if (node->table == NULL) {
		return(DB_SUCCESS);
	}

	/* Iterate over all the indexes and undo the insert.*/

	node->index = dict_table_get_first_index(node->table);
	ut_ad(dict_index_is_clust(node->index));
	/* Skip the clustered index (the first index) */
	node->index = dict_table_get_next_index(node->index);

	dict_table_skip_corrupt_index(node->index);

	err = row_undo_ins_remove_sec_rec(node, thr);

	if (err == DB_SUCCESS) {

		log_free_check();

		if (node->table->id == DICT_INDEXES_ID) {

			if (!dict_locked) {
				mutex_enter(&dict_sys->mutex);
			}
		}

		// FIXME: We need to update the dict_index_t::space and
		// page number fields too.
		err = row_undo_ins_remove_clust_rec(node);

		if (node->table->id == DICT_INDEXES_ID
		    && !dict_locked) {

			mutex_exit(&dict_sys->mutex);
		}
	}

	dict_table_close(node->table, dict_locked, FALSE);

	node->table = NULL;

	return(err);
}
