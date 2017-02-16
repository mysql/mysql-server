/*****************************************************************************

Copyright (c) 1997, 2010, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 
51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

*****************************************************************************/

/**************************************************//**
@file row/row0uins.c
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
#include "trx0trx.h"
#include "trx0rec.h"
#include "row0row.h"
#include "row0upd.h"
#include "que0que.h"
#include "ibuf0ibuf.h"
#include "log0log.h"

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
@return	DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
static
ulint
row_undo_ins_remove_clust_rec(
/*==========================*/
	undo_node_t*	node)	/*!< in: undo node */
{
	btr_cur_t*	btr_cur;
	ibool		success;
	ulint		err;
	ulint		n_tries		= 0;
	mtr_t		mtr;

	mtr_start(&mtr);

	success = btr_pcur_restore_position(BTR_MODIFY_LEAF, &(node->pcur),
					    &mtr);
	ut_a(success);

	if (node->table->id == DICT_INDEXES_ID) {
		ut_ad(node->trx->dict_operation_lock_mode == RW_X_LATCH);

		/* Drop the index tree associated with the row in
		SYS_INDEXES table: */

		dict_drop_index_tree(btr_pcur_get_rec(&(node->pcur)), &mtr);

		mtr_commit(&mtr);

		mtr_start(&mtr);

		success = btr_pcur_restore_position(BTR_MODIFY_LEAF,
						    &(node->pcur), &mtr);
		ut_a(success);
	}

	btr_cur = btr_pcur_get_btr_cur(&(node->pcur));

	success = btr_cur_optimistic_delete(btr_cur, &mtr);

	btr_pcur_commit_specify_mtr(&(node->pcur), &mtr);

	if (success) {
		trx_undo_rec_release(node->trx, node->undo_no);

		return(DB_SUCCESS);
	}
retry:
	/* If did not succeed, try pessimistic descent to tree */
	mtr_start(&mtr);

	success = btr_pcur_restore_position(BTR_MODIFY_TREE,
					    &(node->pcur), &mtr);
	ut_a(success);

	btr_cur_pessimistic_delete(&err, FALSE, btr_cur,
				   trx_is_recv(node->trx)
				   ? RB_RECOVERY
				   : RB_NORMAL, &mtr);

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

	btr_pcur_commit_specify_mtr(&(node->pcur), &mtr);

	trx_undo_rec_release(node->trx, node->undo_no);

	return(err);
}

/***************************************************************//**
Removes a secondary index entry if found.
@return	DB_SUCCESS, DB_FAIL, or DB_OUT_OF_FILE_SPACE */
static
ulint
row_undo_ins_remove_sec_low(
/*========================*/
	ulint		mode,	/*!< in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE,
				depending on whether we wish optimistic or
				pessimistic descent down the index tree */
	dict_index_t*	index,	/*!< in: index */
	dtuple_t*	entry)	/*!< in: index entry to remove */
{
	btr_pcur_t		pcur;
	btr_cur_t*		btr_cur;
	ulint			err;
	mtr_t			mtr;
	enum row_search_result	search_result;

	mtr_start(&mtr);

	btr_cur = btr_pcur_get_btr_cur(&pcur);

	ut_ad(mode == BTR_MODIFY_TREE || mode == BTR_MODIFY_LEAF);

	search_result = row_search_index_entry(index, entry, mode,
					       &pcur, &mtr);

	switch (search_result) {
	case ROW_NOT_FOUND:
		err = DB_SUCCESS;
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

	if (mode == BTR_MODIFY_LEAF) {
		err = btr_cur_optimistic_delete(btr_cur, &mtr)
			? DB_SUCCESS : DB_FAIL;
	} else {
		ut_ad(mode == BTR_MODIFY_TREE);

		/* No need to distinguish RB_RECOVERY here, because we
		are deleting a secondary index record: the distinction
		between RB_NORMAL and RB_RECOVERY only matters when
		deleting a record that contains externally stored
		columns. */
		ut_ad(!dict_index_is_clust(index));
		btr_cur_pessimistic_delete(&err, FALSE, btr_cur,
					   RB_NORMAL, &mtr);
	}
func_exit:
	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	return(err);
}

/***************************************************************//**
Removes a secondary index entry from the index if found. Tries first
optimistic, then pessimistic descent down the tree.
@return	DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
static
ulint
row_undo_ins_remove_sec(
/*====================*/
	dict_index_t*	index,	/*!< in: index */
	dtuple_t*	entry)	/*!< in: index entry to insert */
{
	ulint	err;
	ulint	n_tries	= 0;

	/* Try first optimistic descent to the B-tree */

	err = row_undo_ins_remove_sec_low(BTR_MODIFY_LEAF, index, entry);

	if (err == DB_SUCCESS) {

		return(err);
	}

	/* Try then pessimistic descent to the B-tree */
retry:
	err = row_undo_ins_remove_sec_low(BTR_MODIFY_TREE, index, entry);

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
	undo_node_t*	node)	/*!< in/out: row undo node */
{
	dict_index_t*	clust_index;
	byte*		ptr;
	undo_no_t	undo_no;
	table_id_t	table_id;
	ulint		type;
	ulint		dummy;
	ibool		dummy_extern;

	ut_ad(node);

	ptr = trx_undo_rec_get_pars(node->undo_rec, &type, &dummy,
				    &dummy_extern, &undo_no, &table_id);
	ut_ad(type == TRX_UNDO_INSERT_REC);
	node->rec_type = type;

	node->update = NULL;
	node->table = dict_table_get_on_id(table_id, node->trx);

	/* Skip the UNDO if we can't find the table or the .ibd file. */
	if (UNIV_UNLIKELY(node->table == NULL)) {
	} else if (UNIV_UNLIKELY(node->table->ibd_file_missing)) {
		node->table = NULL;
	} else {
		clust_index = dict_table_get_first_index(node->table);

		if (clust_index != NULL) {
			ptr = trx_undo_rec_get_row_ref(
				ptr, clust_index, &node->ref, node->heap);
		} else {
			ut_print_timestamp(stderr);
			fprintf(stderr, "  InnoDB: table ");
			ut_print_name(stderr, node->trx, TRUE,
				      node->table->name);
			fprintf(stderr, " has no indexes, "
				"ignoring the table\n");

			node->table = NULL;
		}
	}
}

/***********************************************************//**
Undoes a fresh insert of a row to a table. A fresh insert means that
the same clustered index unique key did not have any record, even delete
marked, at the time of the insert.  InnoDB is eager in a rollback:
if it figures out that an index record will be removed in the purge
anyway, it will remove it in the rollback.
@return	DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
UNIV_INTERN
ulint
row_undo_ins(
/*=========*/
	undo_node_t*	node)	/*!< in: row undo node */
{
	ut_ad(node);
	ut_ad(node->state == UNDO_NODE_INSERT);

	row_undo_ins_parse_undo_rec(node);

	if (!node->table || !row_undo_search_clust_to_pcur(node)) {
		trx_undo_rec_release(node->trx, node->undo_no);

		return(DB_SUCCESS);
	}

	/* Iterate over all the indexes and undo the insert.*/

	/* Skip the clustered index (the first index) */
	node->index = dict_table_get_next_index(
		dict_table_get_first_index(node->table));

	dict_table_skip_corrupt_index(node->index);

	while (node->index != NULL) {
		dtuple_t*	entry;
		ulint		err;

		entry = row_build_index_entry(node->row, node->ext,
					      node->index, node->heap);
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
			log_free_check();
			err = row_undo_ins_remove_sec(node->index, entry);

			if (err != DB_SUCCESS) {

				return(err);
			}
		}

		dict_table_next_uncorrupted_index(node->index);
	}

	log_free_check();
	return(row_undo_ins_remove_clust_rec(node));
}
