/*****************************************************************************

Copyright (c) 1997, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************//**
@file row/row0umod.cc
Undo modify of a row

Created 2/27/1997 Heikki Tuuri
*******************************************************/

#include <stddef.h>

#include "btr0btr.h"
#include "dict0boot.h"
#include "dict0dict.h"
#include "dict0boot.h"
#include "dict0dd.h"
#include "btr0btr.h"
#include "ha_prototypes.h"
#include "log0log.h"
#include "mach0data.h"
#include "my_compiler.h"
#include "my_inttypes.h"
#include "que0que.h"
#include "row0log.h"
#include "row0row.h"
#include "row0umod.h"
#include "row0undo.h"
#include "row0upd.h"
#include "row0vers.h"
#include "trx0rec.h"
#include "trx0roll.h"
#include "trx0trx.h"
#include "trx0undo.h"

#include <debug_sync.h>
#include "current_thd.h"

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
@return DB_SUCCESS, DB_FAIL, or error code: we may run out of file space */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_undo_mod_clust_low(
/*===================*/
	undo_node_t*	node,	/*!< in: row undo node */
	ulint**		offsets,/*!< out: rec_get_offsets() on the record */
	mem_heap_t**	offsets_heap,
				/*!< in/out: memory heap that can be emptied */
	mem_heap_t*	heap,	/*!< in/out: memory heap */
	const dtuple_t**rebuilt_old_pk,
				/*!< out: row_log_table_get_pk()
				before the update, or NULL if
				the table is not being rebuilt online or
				the PRIMARY KEY definition does not change */
	byte*		sys,	/*!< out: DB_TRX_ID, DB_ROLL_PTR
				for row_log_table_delete() */
	que_thr_t*	thr,	/*!< in: query thread */
	mtr_t*		mtr,	/*!< in: mtr; must be committed before
				latching any further pages */
	ulint		mode)	/*!< in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE */
{
	DBUG_ENTER("row_undo_mod_clust_low");

	DBUG_LOG("undo", "undo_no=" << node->undo_no);

	btr_pcur_t*	pcur;
	btr_cur_t*	btr_cur;
	dberr_t		err;
	trx_t*		trx = thr_get_trx(thr);
#ifdef UNIV_DEBUG
	ibool		success;
#endif /* UNIV_DEBUG */

	pcur = &node->pcur;
	btr_cur = btr_pcur_get_btr_cur(pcur);

#ifdef UNIV_DEBUG
	success =
#endif /* UNIV_DEBUG */
	btr_pcur_restore_position(mode, pcur, mtr);

	ut_ad(success);
	ut_ad(rec_get_trx_id(btr_cur_get_rec(btr_cur),
			     btr_cur_get_index(btr_cur))
	      == thr_get_trx(thr)->id);

	if (mode != BTR_MODIFY_LEAF
	    && dict_index_is_online_ddl(btr_cur_get_index(btr_cur))) {
		*rebuilt_old_pk = row_log_table_get_pk(
			trx,
			btr_cur_get_rec(btr_cur),
			btr_cur_get_index(btr_cur), NULL, sys, &heap);
	} else {
		*rebuilt_old_pk = NULL;
	}

	if (mode != BTR_MODIFY_TREE) {
		ut_ad((mode & ~BTR_ALREADY_S_LATCHED) == BTR_MODIFY_LEAF);

		err = btr_cur_optimistic_update(
			BTR_NO_LOCKING_FLAG | BTR_NO_UNDO_LOG_FLAG
			| BTR_KEEP_SYS_FLAG,
			btr_cur, offsets, offsets_heap,
			node->update, node->cmpl_info,
			thr, thr_get_trx(thr)->id, mtr);
	} else {
		big_rec_t*	dummy_big_rec;

		err = btr_cur_pessimistic_update(
			BTR_NO_LOCKING_FLAG
			| BTR_NO_UNDO_LOG_FLAG
			| BTR_KEEP_SYS_FLAG,
			btr_cur, offsets, offsets_heap, heap,
			&dummy_big_rec, node->update,
			node->cmpl_info, thr, thr_get_trx(thr)->id,
			node->undo_no, mtr);

		ut_a(!dummy_big_rec);
	}

	DBUG_RETURN(err);
}

/***********************************************************//**
Purges a clustered index record after undo if possible.
This is attempted when the record was inserted by updating a
delete-marked record and there no longer exist transactions
that would see the delete-marked record.
@return	DB_SUCCESS, DB_FAIL, or error code: we may run out of file space */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_undo_mod_remove_clust_low(
/*==========================*/
	undo_node_t*	node,	/*!< in: row undo node */
	mtr_t*		mtr,	/*!< in/out: mini-transaction */
	ulint		mode)	/*!< in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE */
{
	btr_cur_t*	btr_cur;
	dberr_t		err;
	ulint		trx_id_offset;

	ut_ad(node->rec_type == TRX_UNDO_UPD_DEL_REC);

	/* Find out if the record has been purged already
	or if we can remove it. */

	if (!btr_pcur_restore_position(mode, &node->pcur, mtr)
	    || row_vers_must_preserve_del_marked(node->new_trx_id,
						 node->table->name,
						 mtr)) {

		return(DB_SUCCESS);
	}

	btr_cur = btr_pcur_get_btr_cur(&node->pcur);

	trx_id_offset = btr_cur_get_index(btr_cur)->trx_id_offset;

	if (!trx_id_offset) {
		mem_heap_t*	heap	= NULL;
		ulint		trx_id_col;
		const ulint*	offsets;
		ulint		len;

		trx_id_col =
			btr_cur_get_index(btr_cur)->get_sys_col_pos(DATA_TRX_ID);
		ut_ad(trx_id_col > 0);
		ut_ad(trx_id_col != ULINT_UNDEFINED);

		offsets = rec_get_offsets(
			btr_cur_get_rec(btr_cur), btr_cur_get_index(btr_cur),
			NULL, trx_id_col + 1, &heap);

		trx_id_offset = rec_get_nth_field_offs(
			offsets, trx_id_col, &len);
		ut_ad(len == DATA_TRX_ID_LEN);
		mem_heap_free(heap);
	}

	if (trx_read_trx_id(btr_cur_get_rec(btr_cur) + trx_id_offset)
	    != node->new_trx_id) {
		/* The record must have been purged and then replaced
		with a different one. */
		return(DB_SUCCESS);
	}

	/* We are about to remove an old, delete-marked version of the
	record that may have been delete-marked by a different transaction
	than the rolling-back one. */
	ut_ad(rec_get_deleted_flag(btr_cur_get_rec(btr_cur),
				   dict_table_is_comp(node->table)));

	if (mode == BTR_MODIFY_LEAF) {
		err = btr_cur_optimistic_delete(btr_cur, 0, mtr)
			? DB_SUCCESS
			: DB_FAIL;
	} else {
		ut_ad(mode == (BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE));

		/* This operation is analogous to purge, we can free also
		inherited externally stored fields.
		We can also assume that the record was complete
		(including BLOBs), because it had been delete-marked
		after it had been completely inserted. Therefore, we
		are passing rollback=false, just like purge does. */

		btr_cur_pessimistic_delete(&err, FALSE, btr_cur, 0,
					   false, node->trx->id,
					   node->undo_no, node->rec_type,
					   mtr);

		/* The delete operation may fail if we have little
		file space left: TODO: easiest to crash the database
		and restart with more file space */
	}

	return(err);
}

/***********************************************************//**
Undoes a modify in a clustered index record. Sets also the node state for the
next round of undo.
@return DB_SUCCESS or error code: we may run out of file space */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_undo_mod_clust(
/*===============*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	btr_pcur_t*	pcur;
	mtr_t		mtr;
	dberr_t		err;
	dict_index_t*	index;
	bool		online;

	ut_ad(thr_get_trx(thr) == node->trx);
	ut_ad(node->trx->in_rollback);

	log_free_check();
	pcur = &node->pcur;
	index = btr_cur_get_index(btr_pcur_get_btr_cur(pcur));

	mtr_start(&mtr);

	dict_disable_redo_if_temporary(index->table, &mtr);

	online = dict_index_is_online_ddl(index);
	DEBUG_SYNC(current_thd, "row_undo_mod_clust");

	if (online) {
		mtr_s_lock(dict_index_get_lock(index), &mtr);
	}

	mem_heap_t*	heap		= mem_heap_create(1024);
	mem_heap_t*	offsets_heap	= NULL;
	ulint*		offsets		= NULL;
	const dtuple_t*	rebuilt_old_pk;
	byte		sys[DATA_TRX_ID_LEN + DATA_ROLL_PTR_LEN];

	/* Try optimistic processing of the record, keeping changes within
	the index page */

	err = row_undo_mod_clust_low(node, &offsets, &offsets_heap,
				     heap, &rebuilt_old_pk, sys,
				     thr, &mtr, online
				     ? BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED
				     : BTR_MODIFY_LEAF);

	if (err != DB_SUCCESS) {
		btr_pcur_commit_specify_mtr(pcur, &mtr);

		/* We may have to modify tree structure: do a pessimistic
		descent down the index tree */

		mtr_start(&mtr);

		dict_disable_redo_if_temporary(index->table, &mtr);

		err = row_undo_mod_clust_low(
			node, &offsets, &offsets_heap,
			heap, &rebuilt_old_pk, sys,
			thr, &mtr, BTR_MODIFY_TREE);
		ut_ad(err == DB_SUCCESS || err == DB_OUT_OF_FILE_SPACE);
	}

	/* Online rebuild cannot be initiated while we are holding
	dict_operation_lock and index->lock. (It can be aborted.) */
	ut_ad(online || !dict_index_is_online_ddl(index));

	if (err == DB_SUCCESS && online) {

		ut_ad(rw_lock_own_flagged(
				&index->lock,
				RW_LOCK_FLAG_S | RW_LOCK_FLAG_X
				| RW_LOCK_FLAG_SX));

		switch (node->rec_type) {
		case TRX_UNDO_DEL_MARK_REC:
			row_log_table_insert(
				btr_pcur_get_rec(pcur), node->row,
				index, offsets);
			break;
		case TRX_UNDO_UPD_EXIST_REC:
			row_log_table_update(
				btr_pcur_get_rec(pcur), index, offsets,
				rebuilt_old_pk, node->undo_row, node->row);
			break;
		case TRX_UNDO_UPD_DEL_REC:
			row_log_table_delete(
				node->trx, btr_pcur_get_rec(pcur), node->row,
				index, offsets, sys);
			break;
		default:
			ut_ad(0);
			break;
		}
	}

	ut_ad(rec_get_trx_id(btr_pcur_get_rec(pcur), index)
	      == node->new_trx_id);

	btr_pcur_commit_specify_mtr(pcur, &mtr);

	if (err == DB_SUCCESS && node->rec_type == TRX_UNDO_UPD_DEL_REC) {

		mtr_start(&mtr);

		dict_disable_redo_if_temporary(index->table, &mtr);

		/* It is not necessary to call row_log_table,
		because the record is delete-marked and would thus
		be omitted from the rebuilt copy of the table. */
		err = row_undo_mod_remove_clust_low(
			node, &mtr, BTR_MODIFY_LEAF);
		if (err != DB_SUCCESS) {
			btr_pcur_commit_specify_mtr(pcur, &mtr);

			/* We may have to modify tree structure: do a
			pessimistic descent down the index tree */

			mtr_start(&mtr);

			dict_disable_redo_if_temporary(index->table, &mtr);

			err = row_undo_mod_remove_clust_low(
				node, &mtr,
				BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE);

			ut_ad(err == DB_SUCCESS
			      || err == DB_OUT_OF_FILE_SPACE);
		}

		btr_pcur_commit_specify_mtr(pcur, &mtr);
	}

	node->state = UNDO_NODE_FETCH_NEXT;

	if (offsets_heap) {
		mem_heap_free(offsets_heap);
	}
	mem_heap_free(heap);
	return(err);
}

/***********************************************************//**
Delete marks or removes a secondary index entry if found.
@return DB_SUCCESS, DB_FAIL, or DB_OUT_OF_FILE_SPACE */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_undo_mod_del_mark_or_remove_sec_low(
/*====================================*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr,	/*!< in: query thread */
	dict_index_t*	index,	/*!< in: index */
	dtuple_t*	entry,	/*!< in: index entry */
	ulint		mode)	/*!< in: latch mode BTR_MODIFY_LEAF or
				BTR_MODIFY_TREE */
{
	btr_pcur_t		pcur;
	btr_cur_t*		btr_cur;
	ibool			success;
	ibool			old_has;
	dberr_t			err	= DB_SUCCESS;
	mtr_t			mtr;
	mtr_t			mtr_vers;
	row_search_result	search_result;
	ibool			modify_leaf = false;

	log_free_check();

	mtr_start(&mtr);

	dict_disable_redo_if_temporary(index->table, &mtr);

	if (mode == BTR_MODIFY_LEAF) {
		modify_leaf = true;
	}

	if (!index->is_committed()) {
		/* The index->online_status may change if the index is
		or was being created online, but not committed yet. It
		is protected by index->lock. */
		if (mode == BTR_MODIFY_LEAF) {
			mode = BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED;
			mtr_s_lock(dict_index_get_lock(index), &mtr);
		} else {
			ut_ad(mode == (BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE));
			mtr_sx_lock(dict_index_get_lock(index), &mtr);
		}

		if (row_log_online_op_try(index, entry, 0)) {
			goto func_exit_no_pcur;
		}
	} else {
		/* For secondary indexes,
		index->online_status==ONLINE_INDEX_COMPLETE if
		index->is_committed(). */
		ut_ad(!dict_index_is_online_ddl(index));
	}

	btr_cur = btr_pcur_get_btr_cur(&pcur);

	if (dict_index_is_spatial(index)) {
		if (mode & BTR_MODIFY_LEAF) {
			btr_cur->thr = thr;
			mode |= BTR_RTREE_DELETE_MARK;
		}
		mode |= BTR_RTREE_UNDO_INS;
	}

	search_result = row_search_index_entry(index, entry, mode,
					       &pcur, &mtr);

	switch (UNIV_EXPECT(search_result, ROW_FOUND)) {
	case ROW_NOT_FOUND:
		/* In crash recovery, the secondary index record may
		be missing if the UPDATE did not have time to insert
		the secondary index records before the crash.  When we
		are undoing that UPDATE in crash recovery, the record
		may be missing.

		In normal processing, if an update ends in a deadlock
		before it has inserted all updated secondary index
		records, then the undo will not find those records. */
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

	/* We should remove the index record if no prior version of the row,
	which cannot be purged yet, requires its existence. If some requires,
	we should delete mark the record. */

	mtr_start(&mtr_vers);

	success = btr_pcur_restore_position(BTR_SEARCH_LEAF, &(node->pcur),
					    &mtr_vers);
	ut_a(success);

	old_has = row_vers_old_has_index_entry(FALSE,
					       btr_pcur_get_rec(&(node->pcur)),
					       &mtr_vers, index, entry,
					       0, 0);
	if (old_has) {
		err = btr_cur_del_mark_set_sec_rec(BTR_NO_LOCKING_FLAG,
						   btr_cur, TRUE, thr, &mtr);
		ut_ad(err == DB_SUCCESS);
	} else {
		/* Remove the index record */

		if (dict_index_is_spatial(index)) {
			rec_t*	rec = btr_pcur_get_rec(&pcur);
			if (rec_get_deleted_flag(rec,
						 dict_table_is_comp(index->table))) {
				ib::error() << "Record found in index "
					<< index->name << " is deleted marked"
					" on rollback update.";
			}
		}

		if (modify_leaf) {
			success = btr_cur_optimistic_delete(btr_cur, 0, &mtr);
			if (success) {
				err = DB_SUCCESS;
			} else {
				err = DB_FAIL;
			}
		} else {
			/* Passing rollback=false,
			because we are deleting a secondary index record:
			the distinction only matters when deleting a
			record that contains externally stored columns. */
			ut_ad(!index->is_clustered());
			btr_cur_pessimistic_delete(
				&err, FALSE, btr_cur, 0, false, node->trx->id,
				node->undo_no, node->rec_type, &mtr);
			/* The delete operation may fail if we have little
			file space left: TODO: easiest to crash the database
			and restart with more file space */
		}
	}

	btr_pcur_commit_specify_mtr(&(node->pcur), &mtr_vers);

func_exit:
	btr_pcur_close(&pcur);
func_exit_no_pcur:
	mtr_commit(&mtr);

	return(err);
}

/***********************************************************//**
Delete marks or removes a secondary index entry if found.
NOTE that if we updated the fields of a delete-marked secondary index record
so that alphabetically they stayed the same, e.g., 'abc' -> 'aBc', we cannot
return to the original values because we do not know them. But this should
not cause problems because in row0sel.cc, in queries we always retrieve the
clustered index record or an earlier version of it, if the secondary index
record through which we do the search is delete-marked.
@return DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_undo_mod_del_mark_or_remove_sec(
/*================================*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr,	/*!< in: query thread */
	dict_index_t*	index,	/*!< in: index */
	dtuple_t*	entry)	/*!< in: index entry */
{
	dberr_t	err;

	err = row_undo_mod_del_mark_or_remove_sec_low(node, thr, index,
						      entry, BTR_MODIFY_LEAF);
	if (err == DB_SUCCESS) {

		return(err);
	}

	err = row_undo_mod_del_mark_or_remove_sec_low(node, thr, index,
		entry, BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE);
	return(err);
}

/***********************************************************//**
Delete unmarks a secondary index entry which must be found. It might not be
delete-marked at the moment, but it does not harm to unmark it anyway. We also
need to update the fields of the secondary index record if we updated its
fields but alphabetically they stayed the same, e.g., 'abc' -> 'aBc'.
@retval DB_SUCCESS on success
@retval DB_FAIL if BTR_MODIFY_TREE should be tried
@retval DB_OUT_OF_FILE_SPACE when running out of tablespace
@retval DB_DUPLICATE_KEY if the value was missing
	and an insert would lead to a duplicate exists */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_undo_mod_del_unmark_sec_and_undo_update(
/*========================================*/
	ulint		mode,	/*!< in: search mode: BTR_MODIFY_LEAF or
				BTR_MODIFY_TREE */
	que_thr_t*	thr,	/*!< in: query thread */
	dict_index_t*	index,	/*!< in: index */
	dtuple_t*	entry,	/*!< in: index entry */
	undo_no_t	undo_no)
				/*!< in: undo number upto which to rollback.*/
{
	btr_pcur_t		pcur;
	btr_cur_t*		btr_cur		= btr_pcur_get_btr_cur(&pcur);
	upd_t*			update;
	dberr_t			err		= DB_SUCCESS;
	big_rec_t*		dummy_big_rec;
	mtr_t			mtr;
	trx_t*			trx		= thr_get_trx(thr);
	const ulint		flags
		= BTR_KEEP_SYS_FLAG | BTR_NO_LOCKING_FLAG;
	row_search_result	search_result;
	ulint			orig_mode = mode;

	ut_ad(trx->id != 0);

	/* FIXME: Currently we do a 2-pass search for the undo due to
	avoid undel-mark a wrong rec in rolling back in partial update.
	Later, we could log some info in secondary index updates to avoid
	this. */
	if (dict_index_is_spatial(index)) {
		ut_ad(mode & BTR_MODIFY_LEAF);
		mode |=  BTR_RTREE_DELETE_MARK;
	}

try_again:
	log_free_check();

	mtr_start(&mtr);

	dict_disable_redo_if_temporary(index->table, &mtr);

	if (!index->is_committed()) {
		/* The index->online_status may change if the index is
		or was being created online, but not committed yet. It
		is protected by index->lock. */
		if (mode == BTR_MODIFY_LEAF) {
			mode = BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED;
			mtr_s_lock(dict_index_get_lock(index), &mtr);
		} else {
			ut_ad(mode == BTR_MODIFY_TREE);
			mtr_sx_lock(dict_index_get_lock(index), &mtr);
		}

		if (row_log_online_op_try(index, entry, trx->id)) {
			goto func_exit_no_pcur;
		}
	} else {
		/* For secondary indexes,
		index->online_status==ONLINE_INDEX_COMPLETE if
		index->is_committed(). */
		ut_ad(!dict_index_is_online_ddl(index));
	}

	btr_cur->thr = thr;

	search_result = row_search_index_entry(index, entry, mode,
					       &pcur, &mtr);

	switch (search_result) {
		mem_heap_t*	heap;
		mem_heap_t*	offsets_heap;
		ulint*		offsets;
	case ROW_BUFFERED:
	case ROW_NOT_DELETED_REF:
		/* These are invalid outcomes, because the mode passed
		to row_search_index_entry() did not include any of the
		flags BTR_INSERT, BTR_DELETE, or BTR_DELETE_MARK. */
		ut_error;
	case ROW_NOT_FOUND:
		/* For spatial index, if first search didn't find an
		undel-marked rec, try to find a del-marked rec. */
		if (dict_index_is_spatial(index) && btr_cur->rtr_info->fd_del) {
			if (mode != orig_mode) {
				mode = orig_mode;
				btr_pcur_close(&pcur);
				mtr_commit(&mtr);
				goto try_again;
			}
		}

		if (index->is_committed()) {
			/* During online secondary index creation, it
			is possible that MySQL is waiting for a
			meta-data lock upgrade before invoking
			ha_innobase::commit_inplace_alter_table()
			while this ROLLBACK is executing. InnoDB has
			finished building the index, but it does not
			yet exist in MySQL. In this case, we suppress
			the printout to the error log. */
			ib::warn() << "Record in index " << index->name
				<< " of table " << index->table->name
				<< " was not found on rollback, trying to"
				" insert: " << *entry
				<< " at: " << rec_index_print(
					btr_cur_get_rec(btr_cur), index);
		}

		if (btr_cur->up_match >= dict_index_get_n_unique(index)
		    || btr_cur->low_match >= dict_index_get_n_unique(index)) {
			if (index->is_committed()) {
				ib::warn() << "Record in index " << index->name
					<< " was not found on rollback, and"
					" a duplicate exists";
			}
			err = DB_DUPLICATE_KEY;
			break;
		}

		/* Insert the missing record that we were trying to
		delete-unmark. */
		big_rec_t*	big_rec;
		rec_t*		insert_rec;
		offsets = NULL;
		offsets_heap = NULL;

		err = btr_cur_optimistic_insert(
			flags, btr_cur, &offsets, &offsets_heap,
			entry, &insert_rec, &big_rec,
			0, thr, &mtr);
		ut_ad(!big_rec);

		if (err == DB_FAIL && mode == BTR_MODIFY_TREE) {
			err = btr_cur_pessimistic_insert(
				flags, btr_cur,
				&offsets, &offsets_heap,
				entry, &insert_rec, &big_rec,
				0, thr, &mtr);
			/* There are no off-page columns in
			secondary indexes. */
			ut_ad(!big_rec);
		}

		if (err == DB_SUCCESS) {
			page_update_max_trx_id(
				btr_cur_get_block(btr_cur),
				btr_cur_get_page_zip(btr_cur),
				trx->id, &mtr);
		}

		if (offsets_heap) {
			mem_heap_free(offsets_heap);
		}

		break;
	case ROW_FOUND:
		err = btr_cur_del_mark_set_sec_rec(
			BTR_NO_LOCKING_FLAG,
			btr_cur, FALSE, thr, &mtr);

		ut_a(err == DB_SUCCESS);
		heap = mem_heap_create(
			sizeof(upd_t)
			+ dtuple_get_n_fields(entry) * sizeof(upd_field_t));
		offsets_heap = NULL;
		offsets = rec_get_offsets(
			btr_cur_get_rec(btr_cur),
			index, NULL, ULINT_UNDEFINED, &offsets_heap);
		update = row_upd_build_sec_rec_difference_binary(
			btr_cur_get_rec(btr_cur), index, offsets, entry, heap);
		if (upd_get_n_fields(update) == 0) {

			/* Do nothing */

		} else if (mode != BTR_MODIFY_TREE) {
			/* Try an optimistic updating of the record, keeping
			changes within the page */

			/* TODO: pass offsets, not &offsets */
			err = btr_cur_optimistic_update(
				flags, btr_cur, &offsets, &offsets_heap,
				update, 0, thr, thr_get_trx(thr)->id, &mtr);
			switch (err) {
			case DB_OVERFLOW:
			case DB_UNDERFLOW:
			case DB_ZIP_OVERFLOW:
				err = DB_FAIL;
			default:
				break;
			}
		} else {
			err = btr_cur_pessimistic_update(
				flags, btr_cur, &offsets, &offsets_heap,
				heap, &dummy_big_rec,
				update, 0, thr, thr_get_trx(thr)->id,
				undo_no, &mtr);
			ut_a(!dummy_big_rec);
		}

		mem_heap_free(heap);
		mem_heap_free(offsets_heap);
	}

	btr_pcur_close(&pcur);
func_exit_no_pcur:
	mtr_commit(&mtr);

	return(err);
}

/***********************************************************//**
Flags a secondary index corrupted. */
static
void
row_undo_mod_sec_flag_corrupted(
/*============================*/
	trx_t*		trx,	/*!< in/out: transaction */
	dict_index_t*	index)	/*!< in: secondary index */
{
	ut_ad(!index->is_clustered());

	switch (trx->dict_operation_lock_mode) {
	case RW_S_LATCH:
		/* This should be the normal rollback */
		dict_set_corrupted(index);
		break;
	default:
		/* fall through */
	case RW_X_LATCH:
		/* This should be the rollback of a data dictionary
		transaction. */
		dict_set_corrupted(index);
	}
}

/***********************************************************//**
Undoes a modify in secondary indexes when undo record type is UPD_DEL.
@return DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_undo_mod_upd_del_sec(
/*=====================*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	mem_heap_t*	heap;
	dberr_t		err	= DB_SUCCESS;

	ut_ad(node->rec_type == TRX_UNDO_UPD_DEL_REC);
	ut_ad(!node->undo_row);

	heap = mem_heap_create(1024);

	while (node->index != NULL) {
		dict_index_t*	index	= node->index;
		dtuple_t*	entry;

		if (index->type & DICT_FTS) {
			dict_table_next_uncorrupted_index(node->index);
			continue;
		}

		/* During online index creation,
		HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE should
		guarantee that any active transaction has not modified
		indexed columns such that col->ord_part was 0 at the
		time when the undo log record was written. When we get
		to roll back an undo log entry TRX_UNDO_DEL_MARK_REC,
		it should always cover all affected indexes. */
		entry = row_build_index_entry(
			node->row, node->ext, index, heap);

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

			if (UNIV_UNLIKELY(err != DB_SUCCESS)) {

				break;
			}
		}

		mem_heap_empty(heap);
		dict_table_next_uncorrupted_index(node->index);
	}

	mem_heap_free(heap);

	return(err);
}

/***********************************************************//**
Undoes a modify in secondary indexes when undo record type is DEL_MARK.
@return DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_undo_mod_del_mark_sec(
/*======================*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	mem_heap_t*	heap;
	dberr_t		err	= DB_SUCCESS;

	ut_ad(!node->undo_row);

	heap = mem_heap_create(1024);

	while (node->index != NULL) {
		dict_index_t*	index	= node->index;
		dtuple_t*	entry;

		if (index->type == DICT_FTS) {
			dict_table_next_uncorrupted_index(node->index);
			continue;
		}

		/* During online index creation,
		HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE should
		guarantee that any active transaction has not modified
		indexed columns such that col->ord_part was 0 at the
		time when the undo log record was written. When we get
		to roll back an undo log entry TRX_UNDO_DEL_MARK_REC,
		it should always cover all affected indexes. */
		entry = row_build_index_entry(
			node->row, node->ext, index, heap);

		ut_a(entry);

		err = row_undo_mod_del_unmark_sec_and_undo_update(
			BTR_MODIFY_LEAF, thr, index, entry, node->undo_no);
		if (err == DB_FAIL) {
			err = row_undo_mod_del_unmark_sec_and_undo_update(
				BTR_MODIFY_TREE, thr, index, entry,
				node->undo_no);
		}

		if (err == DB_DUPLICATE_KEY) {
			row_undo_mod_sec_flag_corrupted(
				thr_get_trx(thr), index);
			err = DB_SUCCESS;
			/* Do not return any error to the caller. The
			duplicate will be reported by ALTER TABLE or
			CREATE UNIQUE INDEX. Unfortunately we cannot
			report the duplicate key value to the DDL
			thread, because the altered_table object is
			private to its call stack. */
		} else if (err != DB_SUCCESS) {
			break;
		}

		mem_heap_empty(heap);
		dict_table_next_uncorrupted_index(node->index);
	}

	mem_heap_free(heap);

	return(err);
}

/***********************************************************//**
Undoes a modify in secondary indexes when undo record type is UPD_EXIST.
@return DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_undo_mod_upd_exist_sec(
/*=======================*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	mem_heap_t*	heap;
	dberr_t		err	= DB_SUCCESS;

	if (node->index == NULL
	    || ((node->cmpl_info & UPD_NODE_NO_ORD_CHANGE))) {
		/* No change in secondary indexes */

		return(err);
	}

	heap = mem_heap_create(1024);


	while (node->index != NULL) {
		dict_index_t*	index	= node->index;
		dtuple_t*	entry;

		if (dict_index_is_spatial(index)) {
			if (!row_upd_changes_ord_field_binary_func(
				index, node->update,
#ifdef UNIV_DEBUG
				thr,
#endif /* UNIV_DEBUG */
                                node->row,
				node->ext, ROW_BUILD_FOR_UNDO)) {
				dict_table_next_uncorrupted_index(node->index);
				continue;
			}
		} else {
			if (index->type == DICT_FTS
			    || !row_upd_changes_ord_field_binary(index,
								 node->update,
								 thr, node->row,
								 node->ext)) {
				dict_table_next_uncorrupted_index(node->index);
				continue;
			}
		}

		/* Build the newest version of the index entry */
		entry = row_build_index_entry(node->row, node->ext,
					      index, heap);
		if (UNIV_UNLIKELY(!entry)) {
			/* The server must have crashed in
			row_upd_clust_rec_by_insert() before
			the updated externally stored columns (BLOBs)
			of the new clustered index entry were written. */

			/* The table must be in DYNAMIC or COMPRESSED
			format.  REDUNDANT and COMPACT formats
			store a local 768-byte prefix of each
			externally stored column. */
			ut_a(dict_table_has_atomic_blobs(index->table));

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
			in row0sel.cc, in queries we always retrieve
			the clustered index record or an earlier
			version of it, if the secondary index record
			through which we do the search is
			delete-marked. */

			err = row_undo_mod_del_mark_or_remove_sec(
				node, thr, index, entry);
			if (err != DB_SUCCESS) {
				break;
			}
		}

		mem_heap_empty(heap);
		/* We may have to update the delete mark in the
		secondary index record of the previous version of
		the row. We also need to update the fields of
		the secondary index record if we updated its fields
		but alphabetically they stayed the same, e.g.,
		'abc' -> 'aBc'. */
		if (dict_index_is_spatial(index)) {
			entry = row_build_index_entry_low(node->undo_row,
							  node->undo_ext,
							  index, heap,
							  ROW_BUILD_FOR_UNDO);
		} else {
			entry = row_build_index_entry(node->undo_row,
						      node->undo_ext,
						      index, heap);
		}

		ut_a(entry);

		err = row_undo_mod_del_unmark_sec_and_undo_update(
			BTR_MODIFY_LEAF, thr, index, entry, node->undo_no);
		if (err == DB_FAIL) {
			err = row_undo_mod_del_unmark_sec_and_undo_update(
				BTR_MODIFY_TREE, thr, index, entry,
				node->undo_no);
		}

		if (err == DB_DUPLICATE_KEY) {
			row_undo_mod_sec_flag_corrupted(
				thr_get_trx(thr), index);
			err = DB_SUCCESS;
		} else if (err != DB_SUCCESS) {
			break;
		}

		mem_heap_empty(heap);
		dict_table_next_uncorrupted_index(node->index);
	}

	mem_heap_free(heap);

	return(err);
}

/** Parses the row reference and other info in a modify undo log record.
@param[in]	node	row undo node
@param[in,out]	mdl	MDL ticket or nullptr if unnecessary */
static
void
row_undo_mod_parse_undo_rec(
	undo_node_t*	node,
	MDL_ticket**	mdl)
{
	dict_index_t*	clust_index;
	byte*		ptr;
	undo_no_t	undo_no;
	table_id_t	table_id;
	trx_id_t	trx_id;
	roll_ptr_t	roll_ptr;
	ulint		info_bits;
	ulint		type;
	ulint		cmpl_info;
	bool		dummy_extern;

	ptr = trx_undo_rec_get_pars(node->undo_rec, &type, &cmpl_info,
				    &dummy_extern, &undo_no, &table_id);
	node->rec_type = type;

	/* Although table IX lock is held now, DROP TABLE could still be
	done concurrently. To prevent this, MDL for this table should be
	took here. Notably, there cannot be a race between ROLLBACK and
	DROP TEMPORARY TABLE, because temporary tables are
	private to a single connection. */
	node->table = dd_table_open_on_id(
		table_id, current_thd, mdl, false, true);

	if (node->table == NULL) {
		/* Table was dropped */
		return;
	}

	if (node->table->ibd_file_missing) {
		dd_table_close(node->table, current_thd, mdl, false);

		/* We skip undo operations to missing .ibd files */
		node->table = NULL;

		return;
	}

	ut_ad(!node->table->skip_alter_undo);

	clust_index = node->table->first_index();

	ptr = trx_undo_update_rec_get_sys_cols(ptr, &trx_id, &roll_ptr,
					       &info_bits);

	ptr = trx_undo_rec_get_row_ref(ptr, clust_index, &(node->ref),
				       node->heap);

	ptr = trx_undo_update_rec_get_update(ptr, clust_index, type, trx_id,
				       roll_ptr, info_bits, node->trx,
				       node->heap, &(node->update));
	node->new_trx_id = trx_id;
	node->cmpl_info = cmpl_info;

	if (!row_undo_search_clust_to_pcur(node)) {

		dd_table_close(node->table, current_thd, mdl, false);

		node->table = NULL;
	}

	/* Extract indexed virtual columns from undo log */
	if (node->table && node->table->n_v_cols) {
		row_upd_replace_vcol(node->row, node->table,
				     node->update, false, node->undo_row,
				     (node->cmpl_info & UPD_NODE_NO_ORD_CHANGE)
					? NULL : ptr);
	}
}

/***********************************************************//**
Undoes a modify operation on a row of a table.
@return DB_SUCCESS or error code */
dberr_t
row_undo_mod(
/*=========*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	dberr_t		err;
	MDL_ticket*	mdl = nullptr;

	ut_ad(node != NULL);
	ut_ad(thr != NULL);
	ut_ad(node->state == UNDO_NODE_MODIFY);
	ut_ad(node->trx->in_rollback);
	ut_ad(!trx_undo_roll_ptr_is_insert(node->roll_ptr));

	ut_ad(thr_get_trx(thr) == node->trx);

	row_undo_mod_parse_undo_rec(
		node, dd_mdl_for_undo(node->trx) ? &mdl : nullptr);

	if (node->table == NULL) {
		/* It is already undone, or will be undone by another query
		thread, or table was dropped */

		node->state = UNDO_NODE_FETCH_NEXT;

		return(DB_SUCCESS);
	}

	node->index = node->table->first_index();
	ut_ad(node->index->is_clustered());
	/* Skip the clustered index (the first index) */
	node->index = node->index->next();

	/* Skip all corrupted secondary index */
	dict_table_skip_corrupt_index(node->index);

	switch (node->rec_type) {
	case TRX_UNDO_UPD_EXIST_REC:
		err = row_undo_mod_upd_exist_sec(node, thr);
		break;
	case TRX_UNDO_DEL_MARK_REC:
		err = row_undo_mod_del_mark_sec(node, thr);
		break;
	case TRX_UNDO_UPD_DEL_REC:
		err = row_undo_mod_upd_del_sec(node, thr);
		break;
	default:
		ut_error;
		err = DB_ERROR;
	}

	if (err == DB_SUCCESS) {

		err = row_undo_mod_clust(node, thr);
	}

	dd_table_close(node->table, current_thd, &mdl, false);

	node->table = NULL;

	return(err);
}
