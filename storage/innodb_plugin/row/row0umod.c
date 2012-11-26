/*****************************************************************************

Copyright (c) 1997, 2010, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file row/row0umod.c
Undo modify of a row

Created 2/27/1997 Heikki Tuuri
*******************************************************/

#include "row0umod.h"

#ifdef UNIV_NONINL
#include "row0umod.ic"
#endif

#include "dict0dict.h"
#include "dict0boot.h"
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
#include "log0log.h"

/* Considerations on undoing a modify operation.
(1) Undoing a delete marking: all index records should be found. Some of
them may have delete mark already FALSE, if the delete mark operation was
stopped underway, or if the undo operation ended prematurely because of a
system crash.
(2) Undoing an update of a delete unmarked record: the newer version of
an updated secondary index entry should be removed if no prior version
of the clustered index record requires its existence. Otherwise, it should
be delete marked.
(3) Undoing an update of a delete marked record. In this kind of update a
delete marked clustered index record was delete unmarked and possibly also
some of its fields were changed. Now, it is possible that the delete marked
version has become obsolete at the time the undo is started. */

/*************************************************************************
IMPORTANT NOTE: Any operation that generates redo MUST check that there
is enough space in the redo log before for that operation. This is
done by calling log_free_check(). The reason for checking the
availability of the redo log space before the start of the operation is
that we MUST not hold any synchonization objects when performing the
check.
If you make a change in this module make sure that no codepath is
introduced where a call to log_free_check() is bypassed. */

/***********************************************************//**
Undoes a modify in a clustered index record.
@return	DB_SUCCESS, DB_FAIL, or error code: we may run out of file space */
static
ulint
row_undo_mod_clust_low(
/*===================*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr,	/*!< in: query thread */
	mtr_t*		mtr,	/*!< in: mtr; must be committed before
				latching any further pages */
	ulint		mode)	/*!< in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE */
{
	btr_pcur_t*	pcur;
	btr_cur_t*	btr_cur;
	ulint		err;
#ifdef UNIV_DEBUG
	ibool		success;
#endif /* UNIV_DEBUG */

	pcur = &(node->pcur);
	btr_cur = btr_pcur_get_btr_cur(pcur);

#ifdef UNIV_DEBUG
	success =
#endif /* UNIV_DEBUG */
	btr_pcur_restore_position(mode, pcur, mtr);

	ut_ad(success);

	if (mode == BTR_MODIFY_LEAF) {

		err = btr_cur_optimistic_update(BTR_NO_LOCKING_FLAG
						| BTR_NO_UNDO_LOG_FLAG
						| BTR_KEEP_SYS_FLAG,
						btr_cur, node->update,
						node->cmpl_info, thr, mtr);
	} else {
		mem_heap_t*	heap		= NULL;
		big_rec_t*	dummy_big_rec;

		ut_ad(mode == BTR_MODIFY_TREE);

		err = btr_cur_pessimistic_update(
			BTR_NO_LOCKING_FLAG
			| BTR_NO_UNDO_LOG_FLAG
			| BTR_KEEP_SYS_FLAG,
			btr_cur, &heap, &dummy_big_rec, node->update,
			node->cmpl_info, thr, mtr);

		ut_a(!dummy_big_rec);
		if (UNIV_LIKELY_NULL(heap)) {
			mem_heap_free(heap);
		}
	}

	return(err);
}

/***********************************************************//**
Removes a clustered index record after undo if possible.
This is attempted when the record was inserted by updating a
delete-marked record and there no longer exist transactions
that would see the delete-marked record.  In other words, we
roll back the insert by purging the record.
@return	DB_SUCCESS, DB_FAIL, or error code: we may run out of file space */
static
ulint
row_undo_mod_remove_clust_low(
/*==========================*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr,	/*!< in: query thread */
	mtr_t*		mtr,	/*!< in: mtr */
	ulint		mode)	/*!< in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE */
{
	btr_cur_t*	btr_cur;
	ulint		err;

	ut_ad(node->rec_type == TRX_UNDO_UPD_DEL_REC);

	/* Find out if the record has been purged already
	or if we can remove it. */

	if (!btr_pcur_restore_position(mode, &node->pcur, mtr)
	    || row_vers_must_preserve_del_marked(node->new_trx_id, mtr)) {

		return(DB_SUCCESS);
	}

	btr_cur = btr_pcur_get_btr_cur(&node->pcur);

	if (mode == BTR_MODIFY_LEAF) {
		err = btr_cur_optimistic_delete(btr_cur, mtr)
			? DB_SUCCESS
			: DB_FAIL;
	} else {
		ut_ad(mode == BTR_MODIFY_TREE);

		/* This operation is analogous to purge, we can free also
		inherited externally stored fields */

		btr_cur_pessimistic_delete(&err, FALSE, btr_cur,
					   thr_is_recv(thr)
					   ? RB_RECOVERY_PURGE_REC
					   : RB_NONE, mtr);

		/* The delete operation may fail if we have little
		file space left: TODO: easiest to crash the database
		and restart with more file space */
	}

	return(err);
}

/***********************************************************//**
Undoes a modify in a clustered index record. Sets also the node state for the
next round of undo.
@return	DB_SUCCESS or error code: we may run out of file space */
static
ulint
row_undo_mod_clust(
/*===============*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	btr_pcur_t*	pcur;
	mtr_t		mtr;
	ulint		err;

	ut_ad(node && thr);

	log_free_check();

	pcur = &(node->pcur);

	mtr_start(&mtr);

	/* Try optimistic processing of the record, keeping changes within
	the index page */

	err = row_undo_mod_clust_low(node, thr, &mtr, BTR_MODIFY_LEAF);

	if (err != DB_SUCCESS) {
		btr_pcur_commit_specify_mtr(pcur, &mtr);

		/* We may have to modify tree structure: do a pessimistic
		descent down the index tree */

		mtr_start(&mtr);

		err = row_undo_mod_clust_low(node, thr, &mtr, BTR_MODIFY_TREE);
	}

	btr_pcur_commit_specify_mtr(pcur, &mtr);

	if (err == DB_SUCCESS && node->rec_type == TRX_UNDO_UPD_DEL_REC) {

		mtr_start(&mtr);

		err = row_undo_mod_remove_clust_low(node, thr, &mtr,
						    BTR_MODIFY_LEAF);
		if (err != DB_SUCCESS) {
			btr_pcur_commit_specify_mtr(pcur, &mtr);

			/* We may have to modify tree structure: do a
			pessimistic descent down the index tree */

			mtr_start(&mtr);

			err = row_undo_mod_remove_clust_low(node, thr, &mtr,
							    BTR_MODIFY_TREE);
		}

		btr_pcur_commit_specify_mtr(pcur, &mtr);
	}

	node->state = UNDO_NODE_FETCH_NEXT;

	trx_undo_rec_release(node->trx, node->undo_no);

	return(err);
}

/***********************************************************//**
Delete marks or removes a secondary index entry if found.
@return	DB_SUCCESS, DB_FAIL, or DB_OUT_OF_FILE_SPACE */
static
ulint
row_undo_mod_del_mark_or_remove_sec_low(
/*====================================*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr,	/*!< in: query thread */
	dict_index_t*	index,	/*!< in: index */
	dtuple_t*	entry,	/*!< in: index entry */
	ulint		mode)	/*!< in: latch mode BTR_MODIFY_LEAF or
				BTR_MODIFY_TREE */
{
	ibool		found;
	btr_pcur_t	pcur;
	btr_cur_t*	btr_cur;
	ibool		success;
	ibool		old_has;
	ulint		err;
	mtr_t		mtr;
	mtr_t		mtr_vers;

	log_free_check();
	mtr_start(&mtr);

	found = row_search_index_entry(index, entry, mode, &pcur, &mtr);

	btr_cur = btr_pcur_get_btr_cur(&pcur);

	if (!found) {
		/* In crash recovery, the secondary index record may
		be missing if the UPDATE did not have time to insert
		the secondary index records before the crash.  When we
		are undoing that UPDATE in crash recovery, the record
		may be missing.

		In normal processing, if an update ends in a deadlock
		before it has inserted all updated secondary index
		records, then the undo will not find those records. */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);

		return(DB_SUCCESS);
	}

	/* We should remove the index record if no prior version of the row,
	which cannot be purged yet, requires its existence. If some requires,
	we should delete mark the record. */

	mtr_start(&mtr_vers);

	success = btr_pcur_restore_position(BTR_SEARCH_LEAF, &(node->pcur),
					    &mtr_vers);
	ut_a(success);

	old_has = row_vers_old_has_index_entry(FALSE,
					       btr_pcur_get_rec(&(node->pcur)),
					       &mtr_vers, index, entry);
	if (old_has) {
		err = btr_cur_del_mark_set_sec_rec(BTR_NO_LOCKING_FLAG,
						   btr_cur, TRUE, thr, &mtr);
		ut_ad(err == DB_SUCCESS);
	} else {
		/* Remove the index record */

		if (mode == BTR_MODIFY_LEAF) {
			success = btr_cur_optimistic_delete(btr_cur, &mtr);
			if (success) {
				err = DB_SUCCESS;
			} else {
				err = DB_FAIL;
			}
		} else {
			ut_ad(mode == BTR_MODIFY_TREE);

			/* No need to distinguish RB_RECOVERY_PURGE here,
			because we are deleting a secondary index record:
			the distinction between RB_NORMAL and
			RB_RECOVERY_PURGE only matters when deleting a
			record that contains externally stored
			columns. */
			ut_ad(!dict_index_is_clust(index));
			btr_cur_pessimistic_delete(&err, FALSE, btr_cur,
						   RB_NORMAL, &mtr);

			/* The delete operation may fail if we have little
			file space left: TODO: easiest to crash the database
			and restart with more file space */
		}
	}

	btr_pcur_commit_specify_mtr(&(node->pcur), &mtr_vers);
	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	return(err);
}

/***********************************************************//**
Delete marks or removes a secondary index entry if found.
NOTE that if we updated the fields of a delete-marked secondary index record
so that alphabetically they stayed the same, e.g., 'abc' -> 'aBc', we cannot
return to the original values because we do not know them. But this should
not cause problems because in row0sel.c, in queries we always retrieve the
clustered index record or an earlier version of it, if the secondary index
record through which we do the search is delete-marked.
@return	DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
static
ulint
row_undo_mod_del_mark_or_remove_sec(
/*================================*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr,	/*!< in: query thread */
	dict_index_t*	index,	/*!< in: index */
	dtuple_t*	entry)	/*!< in: index entry */
{
	ulint	err;

	err = row_undo_mod_del_mark_or_remove_sec_low(node, thr, index,
						      entry, BTR_MODIFY_LEAF);
	if (err == DB_SUCCESS) {

		return(err);
	}

	err = row_undo_mod_del_mark_or_remove_sec_low(node, thr, index,
						      entry, BTR_MODIFY_TREE);
	return(err);
}

/***********************************************************//**
Delete unmarks a secondary index entry which must be found. It might not be
delete-marked at the moment, but it does not harm to unmark it anyway. We also
need to update the fields of the secondary index record if we updated its
fields but alphabetically they stayed the same, e.g., 'abc' -> 'aBc'.
@return	DB_FAIL or DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
static
ulint
row_undo_mod_del_unmark_sec_and_undo_update(
/*========================================*/
	ulint		mode,	/*!< in: search mode: BTR_MODIFY_LEAF or
				BTR_MODIFY_TREE */
	que_thr_t*	thr,	/*!< in: query thread */
	dict_index_t*	index,	/*!< in: index */
	const dtuple_t*	entry)	/*!< in: index entry */
{
	mem_heap_t*	heap;
	btr_pcur_t	pcur;
	upd_t*		update;
	ulint		err		= DB_SUCCESS;
	big_rec_t*	dummy_big_rec;
	mtr_t		mtr;
	trx_t*		trx		= thr_get_trx(thr);

	/* Ignore indexes that are being created. */
	if (UNIV_UNLIKELY(*index->name == TEMP_INDEX_PREFIX)) {

		return(DB_SUCCESS);
	}

	log_free_check();
	mtr_start(&mtr);

	if (UNIV_UNLIKELY(!row_search_index_entry(index, entry,
						  mode, &pcur, &mtr))) {
		fputs("InnoDB: error in sec index entry del undo in\n"
		      "InnoDB: ", stderr);
		dict_index_name_print(stderr, trx, index);
		fputs("\n"
		      "InnoDB: tuple ", stderr);
		dtuple_print(stderr, entry);
		fputs("\n"
		      "InnoDB: record ", stderr);
		rec_print(stderr, btr_pcur_get_rec(&pcur), index);
		putc('\n', stderr);
		trx_print(stderr, trx, 0);
		fputs("\n"
		      "InnoDB: Submit a detailed bug report"
		      " to http://bugs.mysql.com\n", stderr);
	} else {
		btr_cur_t*	btr_cur = btr_pcur_get_btr_cur(&pcur);

		err = btr_cur_del_mark_set_sec_rec(BTR_NO_LOCKING_FLAG,
						   btr_cur, FALSE, thr, &mtr);
		ut_a(err == DB_SUCCESS);
		heap = mem_heap_create(100);

		update = row_upd_build_sec_rec_difference_binary(
			index, entry, btr_cur_get_rec(btr_cur), trx, heap);
		if (upd_get_n_fields(update) == 0) {

			/* Do nothing */

		} else if (mode == BTR_MODIFY_LEAF) {
			/* Try an optimistic updating of the record, keeping
			changes within the page */

			err = btr_cur_optimistic_update(
				BTR_KEEP_SYS_FLAG | BTR_NO_LOCKING_FLAG,
				btr_cur, update, 0, thr, &mtr);
			switch (err) {
			case DB_OVERFLOW:
			case DB_UNDERFLOW:
			case DB_ZIP_OVERFLOW:
				err = DB_FAIL;
			}
		} else {
			ut_a(mode == BTR_MODIFY_TREE);
			err = btr_cur_pessimistic_update(
				BTR_KEEP_SYS_FLAG | BTR_NO_LOCKING_FLAG,
				btr_cur, &heap, &dummy_big_rec,
				update, 0, thr, &mtr);
			ut_a(!dummy_big_rec);
		}

		mem_heap_free(heap);
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	return(err);
}

/***********************************************************//**
Undoes a modify in secondary indexes when undo record type is UPD_DEL.
@return	DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
static
ulint
row_undo_mod_upd_del_sec(
/*=====================*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	mem_heap_t*	heap;
	dtuple_t*	entry;
	dict_index_t*	index;
	ulint		err	= DB_SUCCESS;

	ut_ad(node->rec_type == TRX_UNDO_UPD_DEL_REC);
	ut_ad(!node->undo_row);
	heap = mem_heap_create(1024);

	while (node->index != NULL) {
		index = node->index;

		entry = row_build_index_entry(node->row, node->ext,
					      index, heap);
		if (UNIV_UNLIKELY(!entry)) {
			/* The database must have crashed after
			inserting a clustered index record but before
			writing all the externally stored columns of
			that record.  Because secondary index entries
			are inserted after the clustered index record,
			we may assume that the secondary index record
			does not exist.  However, this situation may
			only occur during the rollback of incomplete
			transactions. */
			ut_a(thr_is_recv(thr));
		} else {
			err = row_undo_mod_del_mark_or_remove_sec(
				node, thr, index, entry);

			if (err != DB_SUCCESS) {

				break;
			}
		}

		mem_heap_empty(heap);

		node->index = dict_table_get_next_index(node->index);
	}

	mem_heap_free(heap);

	return(err);
}

/***********************************************************//**
Undoes a modify in secondary indexes when undo record type is DEL_MARK.
@return	DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
static
ulint
row_undo_mod_del_mark_sec(
/*======================*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	mem_heap_t*	heap;
	dtuple_t*	entry;
	dict_index_t*	index;
	ulint		err;

	ut_ad(!node->undo_row);

	heap = mem_heap_create(1024);

	while (node->index != NULL) {
		index = node->index;

		entry = row_build_index_entry(node->row, node->ext,
					      index, heap);
		ut_a(entry);
		err = row_undo_mod_del_unmark_sec_and_undo_update(
			BTR_MODIFY_LEAF, thr, index, entry);
		if (err == DB_FAIL) {
			err = row_undo_mod_del_unmark_sec_and_undo_update(
				BTR_MODIFY_TREE, thr, index, entry);
		}

		if (err != DB_SUCCESS) {

			mem_heap_free(heap);

			return(err);
		}

		node->index = dict_table_get_next_index(node->index);
	}

	mem_heap_free(heap);

	return(DB_SUCCESS);
}

/***********************************************************//**
Undoes a modify in secondary indexes when undo record type is UPD_EXIST.
@return	DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
static
ulint
row_undo_mod_upd_exist_sec(
/*=======================*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	mem_heap_t*	heap;
	dtuple_t*	entry;
	dict_index_t*	index;
	ulint		err;

	if (node->cmpl_info & UPD_NODE_NO_ORD_CHANGE) {
		/* No change in secondary indexes */

		return(DB_SUCCESS);
	}

	heap = mem_heap_create(1024);

	while (node->index != NULL) {
		index = node->index;

		if (row_upd_changes_ord_field_binary(node->index, node->update,
						     thr,
						     node->row, node->ext)) {

			/* Build the newest version of the index entry */
			entry = row_build_index_entry(node->row, node->ext,
						      index, heap);
			if (UNIV_UNLIKELY(!entry)) {
				/* The server must have crashed in
				row_upd_clust_rec_by_insert() before
				the updated externally stored columns (BLOBs)
				of the new clustered index entry were
				written. */

				/* The table must be in DYNAMIC or COMPRESSED
				format.  REDUNDANT and COMPACT formats
				store a local 768-byte prefix of each
				externally stored column. */
				ut_a(dict_table_get_format(index->table)
				     >= DICT_TF_FORMAT_ZIP);

				/* This is only legitimate when
				rolling back an incomplete transaction
				after crash recovery. */
				ut_a(thr_get_trx(thr)->is_recovered);

				/* The server must have crashed before
				completing the insert of the new
				clustered index entry and before
				inserting to the secondary indexes.
				Because node->row was not yet written
				to this index, we can ignore it.  But
				we must restore node->undo_row. */
			} else {
				/* NOTE that if we updated the fields of a
				delete-marked secondary index record so that
				alphabetically they stayed the same, e.g.,
				'abc' -> 'aBc', we cannot return to the
				original values because we do not know them.
				But this should not cause problems because
				in row0sel.c, in queries we always retrieve
				the clustered index record or an earlier
				version of it, if the secondary index record
				through which we do the search is
				delete-marked. */

				err = row_undo_mod_del_mark_or_remove_sec(
					node, thr, index, entry);
				if (err != DB_SUCCESS) {
					mem_heap_free(heap);

					return(err);
				}

				mem_heap_empty(heap);
			}

			/* We may have to update the delete mark in the
			secondary index record of the previous version of
			the row. We also need to update the fields of
			the secondary index record if we updated its fields
			but alphabetically they stayed the same, e.g.,
			'abc' -> 'aBc'. */
			entry = row_build_index_entry(node->undo_row,
						      node->undo_ext,
						      index, heap);
			ut_a(entry);

			err = row_undo_mod_del_unmark_sec_and_undo_update(
				BTR_MODIFY_LEAF, thr, index, entry);
			if (err == DB_FAIL) {
				err = row_undo_mod_del_unmark_sec_and_undo_update(
					BTR_MODIFY_TREE, thr, index, entry);
			}

			if (err != DB_SUCCESS) {
				mem_heap_free(heap);

				return(err);
			}
		}

		node->index = dict_table_get_next_index(node->index);
	}

	mem_heap_free(heap);

	return(DB_SUCCESS);
}

/***********************************************************//**
Parses the row reference and other info in a modify undo log record. */
static
void
row_undo_mod_parse_undo_rec(
/*========================*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	dict_index_t*	clust_index;
	byte*		ptr;
	undo_no_t	undo_no;
	dulint		table_id;
	trx_id_t	trx_id;
	roll_ptr_t	roll_ptr;
	ulint		info_bits;
	ulint		type;
	ulint		cmpl_info;
	ibool		dummy_extern;
	trx_t*		trx;

	ut_ad(node && thr);
	trx = thr_get_trx(thr);
	ptr = trx_undo_rec_get_pars(node->undo_rec, &type, &cmpl_info,
				    &dummy_extern, &undo_no, &table_id);
	node->rec_type = type;

	node->table = dict_table_get_on_id(table_id, trx);

	/* TODO: other fixes associated with DROP TABLE + rollback in the
	same table by another user */

	if (node->table == NULL) {
		/* Table was dropped */
		return;
	}

	if (node->table->ibd_file_missing) {
		/* We skip undo operations to missing .ibd files */
		node->table = NULL;

		return;
	}

	clust_index = dict_table_get_first_index(node->table);

	ptr = trx_undo_update_rec_get_sys_cols(ptr, &trx_id, &roll_ptr,
					       &info_bits);

	ptr = trx_undo_rec_get_row_ref(ptr, clust_index, &(node->ref),
				       node->heap);

	trx_undo_update_rec_get_update(ptr, clust_index, type, trx_id,
				       roll_ptr, info_bits, trx,
				       node->heap, &(node->update));
	node->new_trx_id = trx_id;
	node->cmpl_info = cmpl_info;
}

/***********************************************************//**
Undoes a modify operation on a row of a table.
@return	DB_SUCCESS or error code */
UNIV_INTERN
ulint
row_undo_mod(
/*=========*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	ulint	err;

	ut_ad(node && thr);
	ut_ad(node->state == UNDO_NODE_MODIFY);

	row_undo_mod_parse_undo_rec(node, thr);

	if (!node->table || !row_undo_search_clust_to_pcur(node)) {
		/* It is already undone, or will be undone by another query
		thread, or table was dropped */

		trx_undo_rec_release(node->trx, node->undo_no);
		node->state = UNDO_NODE_FETCH_NEXT;

		return(DB_SUCCESS);
	}

	node->index = dict_table_get_next_index(
		dict_table_get_first_index(node->table));

	if (node->rec_type == TRX_UNDO_UPD_EXIST_REC) {

		err = row_undo_mod_upd_exist_sec(node, thr);

	} else if (node->rec_type == TRX_UNDO_DEL_MARK_REC) {

		err = row_undo_mod_del_mark_sec(node, thr);
	} else {
		ut_ad(node->rec_type == TRX_UNDO_UPD_DEL_REC);
		err = row_undo_mod_upd_del_sec(node, thr);
	}

	if (err != DB_SUCCESS) {

		return(err);
	}

	err = row_undo_mod_clust(node, thr);

	return(err);
}
