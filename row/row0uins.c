/******************************************************
Fresh insert undo

(c) 1996 Innobase Oy

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
#include "row0merge.h"

/*******************************************************************
Removes a clustered index record. The pcur in node was positioned on the
record, now it is detached. */
static
ulint
row_undo_ins_remove_clust_rec(
/*==========================*/
				/* out: DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
	undo_node_t*	node)	/* in: undo node */
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

	if (ut_dulint_cmp(node->table->id, DICT_INDEXES_ID) == 0) {
		trx_t*	trx;
		ibool	thawed_dictionary = FALSE;
		ibool	locked_dictionary = FALSE;

		trx = node->trx;

		if (trx->dict_operation_lock_mode == RW_S_LATCH) {
			row_mysql_unfreeze_data_dictionary(trx);

			thawed_dictionary = TRUE;
		}

		if (trx->dict_operation_lock_mode == 0
		    || trx->dict_operation_lock_mode != RW_X_LATCH) {

			row_mysql_lock_data_dictionary(trx);

			locked_dictionary = TRUE;
		}

		/* Drop the index tree associated with the row in
		SYS_INDEXES table: */

		dict_drop_index_tree(btr_pcur_get_rec(&(node->pcur)), &mtr);

		mtr_commit(&mtr);

		mtr_start(&mtr);

		success = btr_pcur_restore_position(BTR_MODIFY_LEAF,
						    &(node->pcur), &mtr);
		ut_a(success);

		if (locked_dictionary) {
			row_mysql_unlock_data_dictionary(trx);
		}

		if (thawed_dictionary) {
			row_mysql_freeze_data_dictionary(trx);
		}
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

	btr_cur_pessimistic_delete(&err, FALSE, btr_cur, TRUE, &mtr);

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

/*******************************************************************
Removes a secondary index entry if found. */
static
ulint
row_undo_ins_remove_sec_low(
/*========================*/
				/* out: DB_SUCCESS, DB_FAIL, or
				DB_OUT_OF_FILE_SPACE */
	ulint		mode,	/* in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE,
				depending on whether we wish optimistic or
				pessimistic descent down the index tree */
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry)	/* in: index entry to remove */
{
	btr_pcur_t	pcur;
	btr_cur_t*	btr_cur;
	ibool		found;
	ibool		success;
	ulint		err;
	mtr_t		mtr;

	log_free_check();
	mtr_start(&mtr);

	found = row_search_index_entry(index, entry, mode, &pcur, &mtr);

	btr_cur = btr_pcur_get_btr_cur(&pcur);

	if (!found) {
		/* Not found */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);

		return(DB_SUCCESS);
	}

	if (mode == BTR_MODIFY_LEAF) {
		success = btr_cur_optimistic_delete(btr_cur, &mtr);

		if (success) {
			err = DB_SUCCESS;
		} else {
			err = DB_FAIL;
		}
	} else {
		ut_ad(mode == BTR_MODIFY_TREE);

		btr_cur_pessimistic_delete(&err, FALSE, btr_cur, TRUE, &mtr);
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	return(err);
}

/*******************************************************************
Removes a secondary index entry from the index if found. Tries first
optimistic, then pessimistic descent down the tree. */
static
ulint
row_undo_ins_remove_sec(
/*====================*/
				/* out: DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry)	/* in: index entry to insert */
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

/***************************************************************
Parses the rec_type undo record. */

byte*
row_undo_ins_parse_rec_type_and_table_id(
/*=====================================*/
					/* out: ptr to next field to parse */
	undo_node_t*	node,		/* in: row undo node */
	dulint*		table_id)	/* out: table id */
{
	byte*		ptr;
	dulint		undo_no;
	ulint		type;
	ulint		dummy;
	ibool		dummy_extern;

	ut_ad(node && node->trx);

	ptr = trx_undo_rec_get_pars(node->undo_rec, &type, &dummy,
				    &dummy_extern, &undo_no, table_id);

	node->rec_type = type;

	if (node->rec_type == TRX_UNDO_DICTIONARY_REC) {
		node->trx->dict_operation = TRUE;
	}

	return ptr;
}

/***************************************************************
Parses the row reference and other info in a fresh insert undo record. */
static
void
row_undo_ins_parse_undo_rec(
/*========================*/
	undo_node_t*	node)	/* in: row undo node */
{
	byte*		ptr;
	dulint		table_id;

	ut_ad(node);

	ptr = row_undo_ins_parse_rec_type_and_table_id(node, &table_id);

	ut_ad(node->rec_type == TRX_UNDO_INSERT_REC
	      || node->rec_type == TRX_UNDO_DICTIONARY_REC);

	if (node->rec_type == TRX_UNDO_INSERT_REC) {

		trx_t*	trx;
		ibool	thawed_dictionary = FALSE;
		ibool	locked_dictionary = FALSE;

		trx = node->trx;

		/* If it's sytem table then we have to acquire the
		dictionary lock in X mode.*/

		if (ut_dulint_cmp(table_id, DICT_FIELDS_ID) <= 0) {
			if (trx->dict_operation_lock_mode == RW_S_LATCH) {
				row_mysql_unfreeze_data_dictionary(trx);

				thawed_dictionary = TRUE;
			}

			if (trx->dict_operation_lock_mode == 0
			|| trx->dict_operation_lock_mode != RW_X_LATCH) {

				row_mysql_lock_data_dictionary(trx);

				locked_dictionary = TRUE;
			}
		}

		node->table = dict_table_get_on_id(table_id, trx);

		/* If we can't find the table or .ibd file is missing,
		we skip the UNDO.*/
		if (node->table == NULL || node->table->ibd_file_missing) {

			node->table = NULL;
		} else {
			dict_index_t*	clust_index;

			clust_index = dict_table_get_first_index(node->table);

			ptr = trx_undo_rec_get_row_ref(
				ptr, clust_index, &node->ref, node->heap);
		}

		if (locked_dictionary) {
			row_mysql_unlock_data_dictionary(trx);
		}

		if (thawed_dictionary) {
			row_mysql_freeze_data_dictionary(trx);
		}
	}
}

/***************************************************************
Undoes a fresh insert of a row to a table. A fresh insert means that
the same clustered index unique key did not have any record, even delete
marked, at the time of the insert. */

ulint
row_undo_ins(
/*=========*/
				/* out: DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
	undo_node_t*	node)	/* in: row undo node */
{
	ulint		err = DB_SUCCESS;

	ut_ad(node);
	ut_ad(node->state == UNDO_NODE_INSERT);

	row_undo_ins_parse_undo_rec(node);

	/* Dictionary records are undone in a separate function */

	if (node->rec_type == TRX_UNDO_DICTIONARY_REC) {

		err = row_undo_build_dict_undo_list(node);

	} else if (!node->table || !row_undo_search_clust_to_pcur(node)) {

		trx_undo_rec_release(node->trx, node->undo_no);

	} else {

		/* Iterate over all the indexes and undo the insert.*/

		/* Skip the clustered index (the first index) */
		node->index = dict_table_get_next_index(
			dict_table_get_first_index(node->table));

		while (node->index != NULL) {
			dtuple_t*	entry;

			entry = row_build_index_entry(node->row, node->ext,
						      node->index, node->heap);

			err = row_undo_ins_remove_sec(node->index, entry);

			if (err != DB_SUCCESS) {

				return(err);
			}

			node->index = dict_table_get_next_index(node->index);
		}

		err = row_undo_ins_remove_clust_rec(node);
	}

	return(err);
}
