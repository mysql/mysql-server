/*****************************************************************************

Copyright (c) 1997, 2023, Oracle and/or its affiliates.

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

/** @file row/row0uins.cc
 Fresh insert undo

 Created 2/25/1997 Heikki Tuuri
 *******************************************************/

#include "row0uins.h"

#include "btr0btr.h"
#include "dict0boot.h"
#include "dict0crea.h"
#include "dict0dd.h"
#include "dict0dict.h"
#include "ibuf0ibuf.h"
#include "log0chkp.h"
#include "mach0data.h"
#include "que0que.h"
#include "row0log.h"
#include "row0row.h"
#include "row0undo.h"
#include "row0upd.h"
#include "row0vers.h"
#include "trx0rec.h"
#include "trx0roll.h"
#include "trx0trx.h"
#include "trx0undo.h"

/*************************************************************************
IMPORTANT NOTE: Any operation that generates redo MUST check that there
is enough space in the redo log before for that operation. This is
done by calling log_free_check(). The reason for checking the
availability of the redo log space before the start of the operation is
that we MUST not hold any synchronization objects when performing the
check.
If you make a change in this module make sure that no codepath is
introduced where a call to log_free_check() is bypassed. */

/** Removes a clustered index record. The pcur in node was positioned on the
 record, now it is detached.
 @return DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
[[nodiscard]] static dberr_t row_undo_ins_remove_clust_rec(
    undo_node_t *node) /*!< in: undo node */
{
  btr_cur_t *btr_cur;
  dberr_t err;
  ulint n_tries = 0;
  mtr_t mtr;
  dict_index_t *index = node->pcur.m_btr_cur.index;
  bool online;

  ut_ad(index->is_clustered());
  ut_ad(node->trx->in_rollback);

  mtr_start(&mtr);

  dict_disable_redo_if_temporary(index->table, &mtr);

  /* This is similar to row_undo_mod_clust(). The DDL thread may
  already have copied this row from the log to the new table.
  We must log the removal, so that the row will be correctly
  purged. However, we can log the removal out of sync with the
  B-tree modification. */

  online = dict_index_is_online_ddl(index);
  if (online) {
    ut_ad(node->trx->dict_operation_lock_mode != RW_X_LATCH);
    ut_ad(node->table->id != DICT_INDEXES_ID);
    mtr_s_lock(dict_index_get_lock(index), &mtr, UT_LOCATION_HERE);
  }

  auto success = node->pcur.restore_position(
      online ? BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED : BTR_MODIFY_LEAF, &mtr,
      UT_LOCATION_HERE);
  ut_a(success);

  btr_cur = node->pcur.get_btr_cur();

  ut_ad(rec_get_trx_id(btr_cur_get_rec(btr_cur), btr_cur->index) ==
        node->trx->id);
  ut_ad(!rec_get_deleted_flag(btr_cur_get_rec(btr_cur),
                              dict_table_is_comp(btr_cur->index->table)));

  if (online && dict_index_is_online_ddl(index)) {
    const rec_t *rec = btr_cur_get_rec(btr_cur);
    mem_heap_t *heap = nullptr;
    const ulint *offsets = rec_get_offsets(rec, index, nullptr, ULINT_UNDEFINED,
                                           UT_LOCATION_HERE, &heap);
    row_log_table_delete(rec, node->row, index, offsets, nullptr);
    mem_heap_free(heap);
  }

  row_convert_impl_to_expl_if_needed(btr_cur, node);
  if (btr_cur_optimistic_delete(btr_cur, 0, &mtr)) {
    err = DB_SUCCESS;
    goto func_exit;
  }

  node->pcur.commit_specify_mtr(&mtr);
retry:
  /* If did not succeed, try pessimistic descent to tree */
  mtr_start(&mtr);

  dict_disable_redo_if_temporary(index->table, &mtr);

  success = node->pcur.restore_position(BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE,
                                        &mtr, UT_LOCATION_HERE);
  ut_a(success);

  btr_cur_pessimistic_delete(&err, false, btr_cur, 0, true, node->trx->id,
                             node->undo_no, node->rec_type, &mtr, &node->pcur,
                             nullptr);

  /* The delete operation may fail if we have little
  file space left: TODO: easiest to crash the database
  and restart with more file space */

  if (err == DB_OUT_OF_FILE_SPACE && n_tries < BTR_CUR_RETRY_DELETE_N_TIMES) {
    node->pcur.commit_specify_mtr(&mtr);

    n_tries++;

    std::this_thread::sleep_for(
        std::chrono::milliseconds(BTR_CUR_RETRY_SLEEP_TIME_MS));

    goto retry;
  }

func_exit:
  node->pcur.commit_specify_mtr(&mtr);

  return (err);
}

/** Removes a secondary index entry if found.
@param[in]      mode    BTR_MODIFY_LEAF or BTR_MODIFY_TREE,
                        depending on whether we wish optimistic or
                        pessimistic descent down the index tree
@param[in]      index   index
@param[in]      entry   index entry to remove
@param[in]      thr     query thread
@param[in]      node    undo node
@return DB_SUCCESS, DB_FAIL, or DB_OUT_OF_FILE_SPACE */
[[nodiscard]] static dberr_t row_undo_ins_remove_sec_low(ulint mode,
                                                         dict_index_t *index,
                                                         dtuple_t *entry,
                                                         que_thr_t *thr,
                                                         undo_node_t *node) {
  btr_pcur_t pcur;
  btr_cur_t *btr_cur;
  dberr_t err = DB_SUCCESS;
  mtr_t mtr;
  enum row_search_result search_result;
  bool modify_leaf = false;
  ulint rec_deleted;

  log_free_check();

  mtr_start(&mtr);

  dict_disable_redo_if_temporary(index->table, &mtr);

  if (mode == BTR_MODIFY_LEAF) {
    mode = BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED;
    mtr_s_lock(dict_index_get_lock(index), &mtr, UT_LOCATION_HERE);
    modify_leaf = true;
  } else {
    ut_ad(mode == (BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE));
    mtr_sx_lock(dict_index_get_lock(index), &mtr, UT_LOCATION_HERE);
  }

  if (row_log_online_op_try(index, entry, 0)) {
    goto func_exit_no_pcur;
  }

  if (dict_index_is_spatial(index)) {
    if (mode & BTR_MODIFY_LEAF) {
      mode |= BTR_RTREE_DELETE_MARK;
    }
    pcur.get_btr_cur()->thr = thr;
    mode |= BTR_RTREE_UNDO_INS;
  }

  search_result = row_search_index_entry(index, entry, mode, &pcur, &mtr);

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

  rec_deleted =
      rec_get_deleted_flag(pcur.get_rec(), dict_table_is_comp(index->table));

  if (search_result == ROW_FOUND && dict_index_is_spatial(index)) {
    if (rec_deleted) {
      ib::error(ER_IB_MSG_1036) << "Record found in index " << index->name
                                << " is deleted marked on insert rollback.";
    }
  }

  btr_cur = pcur.get_btr_cur();

  if (rec_deleted == 0) {
    /* This record is not delete marked and has an implicit
    lock on it. For delete marked record, INSERT has not
    modified it yet and we don't have implicit lock on it.
    We must convert to explicit if and only if we have
    implicit lock on the record.*/
    row_convert_impl_to_expl_if_needed(btr_cur, node);
  }

  if (modify_leaf) {
    err = btr_cur_optimistic_delete(btr_cur, 0, &mtr) ? DB_SUCCESS : DB_FAIL;
  } else {
    /* Passing rollback=false here, because we are
    deleting a secondary index record: the distinction
    only matters when deleting a record that contains
    externally stored columns. */
    ut_ad(!index->is_clustered());
    btr_cur_pessimistic_delete(&err, false, btr_cur, 0, false, 0, 0, 0, &mtr,
                               &pcur, nullptr);
  }
func_exit:
  pcur.close();
func_exit_no_pcur:
  mtr_commit(&mtr);

  return (err);
}

/** Removes a secondary index entry from the index if found. Tries first
 optimistic, then pessimistic descent down the tree.
@param[in]      index   index
@param[in]      entry   index entry to insert
@param[in]      thr     query thread
@param[in]      node    undo node
@return DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
[[nodiscard]] static dberr_t row_undo_ins_remove_sec(dict_index_t *index,
                                                     dtuple_t *entry,
                                                     que_thr_t *thr,
                                                     undo_node_t *node) {
  dberr_t err;
  ulint n_tries = 0;

  /* Try first optimistic descent to the B-tree */

  err = row_undo_ins_remove_sec_low(BTR_MODIFY_LEAF, index, entry, thr, node);

  if (err == DB_SUCCESS) {
    return (err);
  }

/* Try then pessimistic descent to the B-tree */
retry:
  err = row_undo_ins_remove_sec_low(BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE,
                                    index, entry, thr, node);

  /* The delete operation may fail if we have little
  file space left: TODO: easiest to crash the database
  and restart with more file space */

  if (err != DB_SUCCESS && n_tries < BTR_CUR_RETRY_DELETE_N_TIMES) {
    n_tries++;

    std::this_thread::sleep_for(
        std::chrono::milliseconds(BTR_CUR_RETRY_SLEEP_TIME_MS));

    goto retry;
  }

  return (err);
}

/** Parses the row reference and other info in a fresh insert undo record.
@param[in,out]  node    row undo node
@param[in]      thd     THD associated with the node
@param[in,out]  mdl     MDL ticket or nullptr if unnecessary */
static void row_undo_ins_parse_undo_rec(undo_node_t *node, THD *thd,
                                        MDL_ticket **mdl) {
  dict_index_t *clust_index;
  byte *ptr;
  undo_no_t undo_no;
  table_id_t table_id;
  ulint type;
  ulint dummy;
  bool dummy_extern;
  type_cmpl_t type_cmpl;

  ut_ad(node);

  ptr = trx_undo_rec_get_pars(node->undo_rec, &type, &dummy, &dummy_extern,
                              &undo_no, &table_id, type_cmpl);
  ut_ad(type == TRX_UNDO_INSERT_REC);
  node->rec_type = type;

  node->update = nullptr;

  node->table = dd_table_open_on_id(table_id, thd, mdl, false, true);

  /* Skip the UNDO if we can't find the table or the .ibd file. */
  if (node->table == nullptr) {
  } else if (node->table->ibd_file_missing) {
  close_table:
    dd_table_close(node->table, thd, mdl, false);

    node->table = nullptr;
  } else {
    ut_ad(!node->table->skip_alter_undo);

    clust_index = node->table->first_index();

    if (clust_index != nullptr) {
      ptr = trx_undo_rec_get_row_ref(ptr, clust_index, &node->ref, node->heap);

      if (!row_undo_search_clust_to_pcur(node)) {
        goto close_table;
      }
      if (node->table->n_v_cols) {
        trx_undo_read_v_cols(node->table, ptr, node->row, false, false, nullptr,
                             node->heap);
      }

    } else {
      ib::warn(ER_IB_MSG_1037) << "Table " << node->table->name
                               << " has no indexes,"
                                  " ignoring the table";
      goto close_table;
    }
  }
}

/** Removes a secondary index entry from the index, which is built on
multi-value field, if found. For each value, it tries first optimistic,
then pessimistic descent down the tree.
@param[in,out]  index   multi-value index
@param[in]      node    undo node
@param[in]      thr     query thread
@param[in,out]  heap    memory heap
@return DB_SUCCESS or error code */
static dberr_t row_undo_ins_remove_multi_sec(dict_index_t *index,
                                             undo_node_t *node, que_thr_t *thr,
                                             mem_heap_t *heap) {
  dberr_t err = DB_SUCCESS;
  Multi_value_entry_builder_normal mv_entry_builder(node->row, node->ext, index,
                                                    heap, true, false);

  ut_ad(index->is_multi_value());

  for (dtuple_t *entry = mv_entry_builder.begin(); entry != nullptr;
       entry = mv_entry_builder.next()) {
    err = row_undo_ins_remove_sec(index, entry, thr, node);
    if (UNIV_UNLIKELY(err != DB_SUCCESS)) {
      break;
    }
  }

  return (err);
}

/** Removes secondary index records.
 @return DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
[[nodiscard]] static dberr_t row_undo_ins_remove_sec_rec(
    undo_node_t *node, /*!< in/out: row undo node */
    que_thr_t *thr)    /*!< in: query thread */
{
  dberr_t err = DB_SUCCESS;
  dict_index_t *index = node->index;
  mem_heap_t *heap;

  heap = mem_heap_create(1024, UT_LOCATION_HERE);

  while (index != nullptr) {
    dtuple_t *entry;

    if (index->type & DICT_FTS) {
      dict_table_next_uncorrupted_index(index);
      continue;
    }

    if (index->is_multi_value()) {
      err = row_undo_ins_remove_multi_sec(index, node, thr, heap);
      if (err != DB_SUCCESS) {
        goto func_exit;
      }
      mem_heap_empty(heap);
      dict_table_next_uncorrupted_index(index);
      continue;
    }

    /* An insert undo record TRX_UNDO_INSERT_REC will
    always contain all fields of the index. It does not
    matter if any indexes were created afterwards; all
    index entries can be reconstructed from the row. */
    entry = row_build_index_entry(node->row, node->ext, index, heap);
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
      err = row_undo_ins_remove_sec(index, entry, thr, node);

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
  return (err);
}

/** Undoes a fresh insert of a row to a table. A fresh insert means that
 the same clustered index unique key did not have any record, even delete
 marked, at the time of the insert.  InnoDB is eager in a rollback:
 if it figures out that an index record will be removed in the purge
 anyway, it will remove it in the rollback.
 @return DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
dberr_t row_undo_ins(undo_node_t *node, /*!< in: row undo node */
                     que_thr_t *thr)    /*!< in: query thread */
{
  dberr_t err;
  MDL_ticket *mdl = nullptr;

  ut_ad(node->state == UNDO_NODE_INSERT);
  ut_ad(node->trx->in_rollback);
  ut_ad(trx_undo_roll_ptr_is_insert(node->roll_ptr));

  THD *thd = dd_thd_for_undo(node->trx);

  row_undo_ins_parse_undo_rec(node, thd,
                              dd_mdl_for_undo(node->trx) ? &mdl : nullptr);

  if (node->table == nullptr) {
    return (DB_SUCCESS);
  }

  /* Iterate over all the indexes and undo the insert.*/

  node->index = node->table->first_index();
  ut_ad(node->index->is_clustered());
  /* Skip the clustered index (the first index) */
  node->index = node->index->next();

  dict_table_skip_corrupt_index(node->index);

  err = row_undo_ins_remove_sec_rec(node, thr);

  if (err == DB_SUCCESS) {
    log_free_check();

    // FIXME: We need to update the dict_index_t::space and
    // page number fields too.
    err = row_undo_ins_remove_clust_rec(node);
  }

  dd_table_close(node->table, thd, &mdl, false);

  node->table = nullptr;

  return (err);
}
