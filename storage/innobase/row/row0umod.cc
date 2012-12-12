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
@file row/row0umod.cc
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
#include "row0log.h"
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
static __attribute__((nonnull, warn_unused_result))
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
	que_thr_t*	thr,	/*!< in: query thread */
	mtr_t*		mtr,	/*!< in: mtr; must be committed before
				latching any further pages */
	ulint		mode)	/*!< in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE */
{
	btr_pcur_t*	pcur;
	btr_cur_t*	btr_cur;
	dberr_t		err;
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
			btr_cur_get_rec(btr_cur),
			btr_cur_get_index(btr_cur), NULL, &heap);
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
			node->cmpl_info, thr, thr_get_trx(thr)->id, mtr);

		ut_a(!dummy_big_rec);
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
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_undo_mod_remove_clust_low(
/*==========================*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr,	/*!< in: query thread */
	mtr_t*		mtr,	/*!< in: mtr */
	ulint		mode)	/*!< in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE */
{
	btr_cur_t*	btr_cur;
	dberr_t		err;

	ut_ad(node->rec_type == TRX_UNDO_UPD_DEL_REC);

	/* Find out if the record has been purged already
	or if we can remove it. */

	if (!btr_pcur_restore_position(mode, &node->pcur, mtr)
	    || row_vers_must_preserve_del_marked(node->new_trx_id, mtr)) {

		return(DB_SUCCESS);
	}

	btr_cur = btr_pcur_get_btr_cur(&node->pcur);

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
		ut_ad(mode == BTR_MODIFY_TREE);

		/* This operation is analogous to purge, we can free also
		inherited externally stored fields */

		btr_cur_pessimistic_delete(&err, FALSE, btr_cur, 0,
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
static __attribute__((nonnull, warn_unused_result))
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
	ut_ad(node->trx->dict_operation_lock_mode);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&dict_operation_lock, RW_LOCK_SHARED)
	      || rw_lock_own(&dict_operation_lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	log_free_check();
	pcur = &node->pcur;
	index = btr_cur_get_index(btr_pcur_get_btr_cur(pcur));

	mtr_start(&mtr);
	turn_off_logging_if_temp_table(
		dict_table_is_temporary(index->table), NULL, &mtr, NULL);

	online = dict_index_is_online_ddl(index);
	if (online) {
		ut_ad(node->trx->dict_operation_lock_mode != RW_X_LATCH);
		mtr_s_lock(dict_index_get_lock(index), &mtr);
	}

	mem_heap_t*	heap		= mem_heap_create(1024);
	mem_heap_t*	offsets_heap	= NULL;
	ulint*		offsets		= NULL;
	const dtuple_t*	rebuilt_old_pk;

	/* Try optimistic processing of the record, keeping changes within
	the index page */

	err = row_undo_mod_clust_low(node, &offsets, &offsets_heap,
				     heap, &rebuilt_old_pk,
				     thr, &mtr, online
				     ? BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED
				     : BTR_MODIFY_LEAF);

	if (err != DB_SUCCESS) {
		btr_pcur_commit_specify_mtr(pcur, &mtr);

		/* We may have to modify tree structure: do a pessimistic
		descent down the index tree */

		mtr_start(&mtr);
		turn_off_logging_if_temp_table(
			dict_table_is_temporary(index->table), NULL,
			&mtr, NULL);

		err = row_undo_mod_clust_low(
			node, &offsets, &offsets_heap, heap, &rebuilt_old_pk,
			thr, &mtr, BTR_MODIFY_TREE);
		ut_ad(err == DB_SUCCESS || err == DB_OUT_OF_FILE_SPACE);
	}

	/* Online rebuild cannot be initiated while we are holding
	dict_operation_lock and index->lock. (It can be aborted.) */
	ut_ad(online || !dict_index_is_online_ddl(index));

	if (err == DB_SUCCESS && online) {
#ifdef UNIV_SYNC_DEBUG
		ut_ad(rw_lock_own(&index->lock, RW_LOCK_SHARED)
		      || rw_lock_own(&index->lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
		switch (node->rec_type) {
		case TRX_UNDO_DEL_MARK_REC:
			row_log_table_insert(
				btr_pcur_get_rec(pcur), index, offsets);
			break;
		case TRX_UNDO_UPD_EXIST_REC:
			row_log_table_update(
				btr_pcur_get_rec(pcur), index, offsets,
				rebuilt_old_pk);
			break;
		case TRX_UNDO_UPD_DEL_REC:
			row_log_table_delete(
				btr_pcur_get_rec(pcur), index, offsets,
				node->trx->id);
			break;
		default:
			ut_ad(0);
			break;
		}
	}

	btr_pcur_commit_specify_mtr(pcur, &mtr);

	if (err == DB_SUCCESS && node->rec_type == TRX_UNDO_UPD_DEL_REC) {

		mtr_start(&mtr);
		turn_off_logging_if_temp_table(
			dict_table_is_temporary(index->table), NULL,
			&mtr, NULL);

		/* It is not necessary to call row_log_table,
		because the record is delete-marked and would thus
		be omitted from the rebuilt copy of the table. */
		err = row_undo_mod_remove_clust_low(
			node, thr, &mtr, BTR_MODIFY_LEAF);
		if (err != DB_SUCCESS) {
			btr_pcur_commit_specify_mtr(pcur, &mtr);

			/* We may have to modify tree structure: do a
			pessimistic descent down the index tree */

			mtr_start(&mtr);
			turn_off_logging_if_temp_table(
				dict_table_is_temporary(index->table), NULL,
				&mtr, NULL);

			err = row_undo_mod_remove_clust_low(node, thr, &mtr,
							    BTR_MODIFY_TREE);

			ut_ad(err == DB_SUCCESS
			      || err == DB_OUT_OF_FILE_SPACE);
		}

		btr_pcur_commit_specify_mtr(pcur, &mtr);
	}

	node->state = UNDO_NODE_FETCH_NEXT;

	trx_undo_rec_release(node->trx, node->undo_no);

	if (offsets_heap) {
		mem_heap_free(offsets_heap);
	}
	mem_heap_free(heap);
	return(err);
}

/***********************************************************//**
Delete marks or removes a secondary index entry if found.
@return	DB_SUCCESS, DB_FAIL, or DB_OUT_OF_FILE_SPACE */
static __attribute__((nonnull, warn_unused_result))
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
	enum row_search_result	search_result;

	log_free_check();
	mtr_start(&mtr);
	turn_off_logging_if_temp_table(
		dict_table_is_temporary(index->table), NULL, &mtr, NULL);

	if (*index->name == TEMP_INDEX_PREFIX) {
		/* The index->online_status may change if the
		index->name starts with TEMP_INDEX_PREFIX (meaning
		that the index is or was being created online). It is
		protected by index->lock. */
		if (mode == BTR_MODIFY_LEAF) {
			mode = BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED;
			mtr_s_lock(dict_index_get_lock(index), &mtr);
		} else {
			ut_ad(mode == BTR_MODIFY_TREE);
			mtr_x_lock(dict_index_get_lock(index), &mtr);
		}

		if (row_log_online_op_try(index, entry, 0)) {
			goto func_exit_no_pcur;
		}
	} else {
		/* For secondary indexes,
		index->online_status==ONLINE_INDEX_CREATION unless
		index->name starts with TEMP_INDEX_PREFIX. */
		ut_ad(!dict_index_is_online_ddl(index));
	}

	btr_cur = btr_pcur_get_btr_cur(&pcur);

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
	turn_off_logging_if_temp_table(
		dict_table_is_temporary(index->table), NULL, &mtr, NULL);

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

		if (mode != BTR_MODIFY_TREE) {
			success = btr_cur_optimistic_delete(btr_cur, 0, &mtr);
			if (success) {
				err = DB_SUCCESS;
			} else {
				err = DB_FAIL;
			}
		} else {
			/* No need to distinguish RB_RECOVERY_PURGE here,
			because we are deleting a secondary index record:
			the distinction between RB_NORMAL and
			RB_RECOVERY_PURGE only matters when deleting a
			record that contains externally stored
			columns. */
			ut_ad(!dict_index_is_clust(index));
			btr_cur_pessimistic_delete(&err, FALSE, btr_cur, 0,
						   RB_NORMAL, &mtr);

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
@return	DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
static __attribute__((nonnull, warn_unused_result))
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
						      entry, BTR_MODIFY_TREE);
	return(err);
}

/***********************************************************//**
Delete unmarks a secondary index entry which must be found. It might not be
delete-marked at the moment, but it does not harm to unmark it anyway. We also
need to update the fields of the secondary index record if we updated its
fields but alphabetically they stayed the same, e.g., 'abc' -> 'aBc'.
@retval	DB_SUCCESS on success
@retval	DB_FAIL if BTR_MODIFY_TREE should be tried
@retval	DB_OUT_OF_FILE_SPACE when running out of tablespace
@retval	DB_DUPLICATE_KEY if the value was missing
	and an insert would lead to a duplicate exists */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_undo_mod_del_unmark_sec_and_undo_update(
/*========================================*/
	ulint		mode,	/*!< in: search mode: BTR_MODIFY_LEAF or
				BTR_MODIFY_TREE */
	que_thr_t*	thr,	/*!< in: query thread */
	dict_index_t*	index,	/*!< in: index */
	dtuple_t*	entry)	/*!< in: index entry */
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
	enum row_search_result	search_result;

	ut_ad(trx->id);

	log_free_check();
	mtr_start(&mtr);
	turn_off_logging_if_temp_table(
		dict_table_is_temporary(index->table), NULL, &mtr, NULL);

	if (*index->name == TEMP_INDEX_PREFIX) {
		/* The index->online_status may change if the
		index->name starts with TEMP_INDEX_PREFIX (meaning
		that the index is or was being created online). It is
		protected by index->lock. */
		if (mode == BTR_MODIFY_LEAF) {
			mode = BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED;
			mtr_s_lock(dict_index_get_lock(index), &mtr);
		} else {
			ut_ad(mode == BTR_MODIFY_TREE);
			mtr_x_lock(dict_index_get_lock(index), &mtr);
		}

		if (row_log_online_op_try(index, entry, trx->id)) {
			goto func_exit_no_pcur;
		}
	} else {
		/* For secondary indexes,
		index->online_status==ONLINE_INDEX_CREATION unless
		index->name starts with TEMP_INDEX_PREFIX. */
		ut_ad(!dict_index_is_online_ddl(index));
	}

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
		if (*index->name != TEMP_INDEX_PREFIX) {
			/* During online secondary index creation, it
			is possible that MySQL is waiting for a
			meta-data lock upgrade before invoking
			ha_innobase::commit_inplace_alter_table()
			while this ROLLBACK is executing. InnoDB has
			finished building the index, but it does not
			yet exist in MySQL. In this case, we suppress
			the printout to the error log. */
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

			ib_logf(IB_LOG_LEVEL_WARN,
				"record in index %s was not found"
				" on rollback, trying to insert",
				index->name);
		}

		if (btr_cur->up_match >= dict_index_get_n_unique(index)
		    || btr_cur->low_match >= dict_index_get_n_unique(index)) {
			if (*index->name != TEMP_INDEX_PREFIX) {
				ib_logf(IB_LOG_LEVEL_WARN,
					"record in index %s was not found on"
					" rollback, and a duplicate exists",
					index->name);
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
				update, 0, thr, thr_get_trx(thr)->id, &mtr);
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
static __attribute__((nonnull))
void
row_undo_mod_sec_flag_corrupted(
/*============================*/
	trx_t*		trx,	/*!< in/out: transaction */
	dict_index_t*	index)	/*!< in: secondary index */
{
	ut_ad(!dict_index_is_clust(index));

	switch (trx->dict_operation_lock_mode) {
	case RW_S_LATCH:
		/* Because row_undo() is holding an S-latch
		on the data dictionary during normal rollback,
		we can only mark the index corrupted in the
		data dictionary cache. TODO: fix this somehow.*/
		mutex_enter(&dict_sys->mutex);
		dict_set_corrupted_index_cache_only(index, index->table);
		mutex_exit(&dict_sys->mutex);
		break;
	default:
		ut_ad(0);
		/* fall through */
	case RW_X_LATCH:
		/* This should be the rollback of a data dictionary
		transaction. */
		dict_set_corrupted(index, trx, "rollback");
	}
}

/***********************************************************//**
Undoes a modify in secondary indexes when undo record type is UPD_DEL.
@return	DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
static __attribute__((nonnull, warn_unused_result))
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
@return	DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
static __attribute__((nonnull, warn_unused_result))
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
			BTR_MODIFY_LEAF, thr, index, entry);
		if (err == DB_FAIL) {
			err = row_undo_mod_del_unmark_sec_and_undo_update(
				BTR_MODIFY_TREE, thr, index, entry);
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
@return	DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
static __attribute__((nonnull, warn_unused_result))
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

		if (index->type == DICT_FTS
		    || !row_upd_changes_ord_field_binary(
			index, node->update, thr, node->row, node->ext)) {
			dict_table_next_uncorrupted_index(node->index);
			continue;
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
			ut_a(dict_table_get_format(index->table)
			     >= UNIV_FORMAT_B);

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

/***********************************************************//**
Parses the row reference and other info in a modify undo log record. */
static __attribute__((nonnull))
void
row_undo_mod_parse_undo_rec(
/*========================*/
	undo_node_t*	node,		/*!< in: row undo node */
	ibool		dict_locked)	/*!< in: TRUE if own dict_sys->mutex */
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

	node->table = dict_table_open_on_id(table_id, dict_locked, FALSE);

	/* TODO: other fixes associated with DROP TABLE + rollback in the
	same table by another user */

	if (node->table == NULL) {
		/* Table was dropped */
		return;
	}

	if (node->table->ibd_file_missing) {
		dict_table_close(node->table, dict_locked, FALSE);

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
				       roll_ptr, info_bits, node->trx,
				       node->heap, &(node->update));
	node->new_trx_id = trx_id;
	node->cmpl_info = cmpl_info;

	if (!row_undo_search_clust_to_pcur(node)) {

		dict_table_close(node->table, dict_locked, FALSE);

		node->table = NULL;
	}
}

/***********************************************************//**
Undoes a modify operation on a row of a table.
@return	DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
row_undo_mod(
/*=========*/
	undo_node_t*	node,	/*!< in: row undo node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	dberr_t	err;
	ibool	dict_locked;

	ut_ad(node && thr);
	ut_ad(node->state == UNDO_NODE_MODIFY);

	dict_locked = thr_get_trx(thr)->dict_operation_lock_mode == RW_X_LATCH;

	ut_ad(thr_get_trx(thr) == node->trx);

	row_undo_mod_parse_undo_rec(node, dict_locked);

	if (node->table == NULL) {
		/* It is already undone, or will be undone by another query
		thread, or table was dropped */

		trx_undo_rec_release(node->trx, node->undo_no);
		node->state = UNDO_NODE_FETCH_NEXT;

		return(DB_SUCCESS);
	}

	node->index = dict_table_get_first_index(node->table);
	ut_ad(dict_index_is_clust(node->index));

	if (dict_index_is_online_ddl(node->index)) {
		/* Note that we are rolling back this transaction, so
		that all inserts and updates with this DB_TRX_ID can
		be skipped. */
		row_log_table_rollback(node->index, node->trx->id);
	}

	/* Skip the clustered index (the first index) */
	node->index = dict_table_get_next_index(node->index);

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

	dict_table_close(node->table, dict_locked, FALSE);

	node->table = NULL;

	return(err);
}
