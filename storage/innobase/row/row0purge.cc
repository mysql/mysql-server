/*****************************************************************************

Copyright (c) 1997, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file row/row0purge.cc
 Purge obsolete records

 Created 3/14/1997 Heikki Tuuri
 *******************************************************/

#include "row0purge.h"

#include <stddef.h>

#include "fsp0fsp.h"
#include "ha_innodb.h"
#include "handler.h"
#include "lob0lob.h"
#include "log0log.h"
#include "mach0data.h"
#include "my_inttypes.h"
#include "mysqld.h"
#include "que0que.h"
#include "row0log.h"
#include "row0mysql.h"
#include "row0row.h"
#include "row0upd.h"
#include "row0vers.h"
#include "srv0mon.h"
#include "srv0start.h"
#include "trx0purge.h"
#include "trx0rec.h"
#include "trx0roll.h"
#include "trx0rseg.h"
#include "trx0trx.h"
#include "trx0undo.h"

#include "current_thd.h"
#include "dict0dd.h"
#include "sql_base.h"
#include "table.h"

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
purge_node_t *row_purge_node_create(que_thr_t *parent, mem_heap_t *heap) {
  ut_ad(parent != NULL);
  ut_ad(heap != NULL);

  purge_node_t *node;

  node = static_cast<purge_node_t *>(mem_heap_zalloc(heap, sizeof(*node)));

  node->common.type = QUE_NODE_PURGE;
  node->common.parent = parent;
  node->done = true;

  node->heap = mem_heap_create(256);

  node->recs = nullptr;

  return (node);
}

/** Repositions the pcur in the purge node on the clustered index record,
 if found. If the record is not found, close pcur.
 @return true if the record was found */
static ibool row_purge_reposition_pcur(
    ulint mode,         /*!< in: latching mode */
    purge_node_t *node, /*!< in: row purge node */
    mtr_t *mtr)         /*!< in: mtr */
{
  if (node->found_clust) {
    ut_ad(node->validate_pcur());

    node->found_clust = btr_pcur_restore_position(mode, &node->pcur, mtr);

  } else {
    node->found_clust =
        row_search_on_row_ref(&node->pcur, mode, node->table, node->ref, mtr);

    if (node->found_clust) {
      btr_pcur_store_position(&node->pcur, mtr);
    }
  }

  /* Close the current cursor if we fail to position it correctly. */
  if (!node->found_clust) {
    btr_pcur_close(&node->pcur);
  }

  return (node->found_clust);
}

/** Removes a delete marked clustered index record if possible.
 @retval true if the row was not found, or it was successfully removed
 @retval false if the row was modified after the delete marking */
static MY_ATTRIBUTE((warn_unused_result)) bool row_purge_remove_clust_if_poss_low(
    purge_node_t *node, /*!< in/out: row purge node */
    ulint mode)         /*!< in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE */
{
  dict_index_t *index;
  bool success = true;
  mtr_t mtr;
  rec_t *rec;
  mem_heap_t *heap = NULL;
  ulint *offsets;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  rec_offs_init(offsets_);

  index = node->table->first_index();

  fil_space_t *space = fil_space_acquire_silent(index->space);
  if (space == NULL) {
    /* This can happen only for SDI in General Tablespaces.
     */
    ut_ad(dict_table_is_sdi(node->table->id));
    return (true);
  } else {
    fil_space_release(space);
  }

  log_free_check();
  mtr_start(&mtr);

  if (!row_purge_reposition_pcur(mode, node, &mtr)) {
    /* The record was already removed. */
    goto func_exit;
  }

  rec = btr_pcur_get_rec(&node->pcur);

  offsets = rec_get_offsets(rec, index, offsets_, ULINT_UNDEFINED, &heap);

  if (node->roll_ptr != row_get_rec_roll_ptr(rec, index, offsets)) {
    /* Someone else has modified the record later: do not remove */
    goto func_exit;
  }

  ut_ad(rec_get_deleted_flag(rec, rec_offs_comp(offsets)));

  if (mode == BTR_MODIFY_LEAF) {
    success =
        btr_cur_optimistic_delete(btr_pcur_get_btr_cur(&node->pcur), 0, &mtr);
  } else {
    dberr_t err;
    ut_ad(mode == (BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE));
    btr_cur_pessimistic_delete(&err, FALSE, btr_pcur_get_btr_cur(&node->pcur),
                               0, false, node->trx_id, node->undo_no,
                               node->rec_type, &mtr);

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

  return (success);
}

/** Removes a clustered index record if it has not been modified after the
 delete marking.
 @retval true if the row was not found, or it was successfully removed
 @retval false the purge needs to be suspended because of running out
 of file space. */
static MY_ATTRIBUTE((warn_unused_result)) bool row_purge_remove_clust_if_poss(
    purge_node_t *node) /*!< in/out: row purge node */
{
  if (row_purge_remove_clust_if_poss_low(node, BTR_MODIFY_LEAF)) {
    return (true);
  }

  for (ulint n_tries = 0; n_tries < BTR_CUR_RETRY_DELETE_N_TIMES; n_tries++) {
    if (row_purge_remove_clust_if_poss_low(
            node, BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE)) {
      return (true);
    }

    os_thread_sleep(BTR_CUR_RETRY_SLEEP_TIME);
  }

  return (false);
}

/** Determines if it is possible to remove a secondary index entry.
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
bool row_purge_poss_sec(purge_node_t *node,    /*!< in/out: row purge node */
                        dict_index_t *index,   /*!< in: secondary index */
                        const dtuple_t *entry) /*!< in: secondary index entry */
{
  bool can_delete;
  mtr_t mtr;

  ut_ad(!index->is_clustered());
  mtr_start(&mtr);

  can_delete =
      !row_purge_reposition_pcur(BTR_SEARCH_LEAF, node, &mtr) ||
      !row_vers_old_has_index_entry(TRUE, btr_pcur_get_rec(&node->pcur), &mtr,
                                    index, entry, node->roll_ptr, node->trx_id);

  /* Persistent cursor is closed if reposition fails. */
  if (node->found_clust) {
    btr_pcur_commit_specify_mtr(&node->pcur, &mtr);
  } else {
    mtr_commit(&mtr);
  }

  return (can_delete);
}

/***************************************************************
Removes a secondary index entry if possible, by modifying the
index tree.  Does not try to buffer the delete.
@return true if success or if not found */
static MY_ATTRIBUTE((warn_unused_result)) ibool
    row_purge_remove_sec_if_poss_tree(
        purge_node_t *node,    /*!< in: row purge node */
        dict_index_t *index,   /*!< in: index */
        const dtuple_t *entry) /*!< in: index entry */
{
  btr_pcur_t pcur;
  btr_cur_t *btr_cur;
  ibool success = TRUE;
  dberr_t err;
  mtr_t mtr;
  enum row_search_result search_result;

  log_free_check();
  mtr_start(&mtr);

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
      index, entry, BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE, &pcur, &mtr);

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
      ib::error(ER_IB_MSG_1007)
          << "tried to purge non-delete-marked record"
             " in index "
          << index->name << " of table " << index->table->name
          << ": tuple: " << *entry
          << ", record: " << rec_index_print(btr_cur_get_rec(btr_cur), index);

      ut_ad(0);

      goto func_exit;
    }

    btr_cur_pessimistic_delete(&err, FALSE, btr_cur, 0, false, 0, node->undo_no,
                               node->rec_type, &mtr);
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

  return (success);
}

/***************************************************************
Removes a secondary index entry without modifying the index tree,
if possible.
@retval true if success or if not found
@retval false if row_purge_remove_sec_if_poss_tree() should be invoked */
static MY_ATTRIBUTE((warn_unused_result)) bool row_purge_remove_sec_if_poss_leaf(
    purge_node_t *node,    /*!< in: row purge node */
    dict_index_t *index,   /*!< in: index */
    const dtuple_t *entry) /*!< in: index entry */
{
  mtr_t mtr;
  btr_pcur_t pcur;
  ulint mode;
  enum row_search_result search_result;
  bool success = true;

  log_free_check();

  mtr_start(&mtr);

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
    mode = (index->table->is_temporary())
               ? BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED
               : BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED | BTR_DELETE;
  } else {
    /* For secondary indexes,
    index->online_status==ONLINE_INDEX_COMPLETE if
    index->is_committed(). */
    ut_ad(!dict_index_is_online_ddl(index));

    /* Change buffering is disabled for temporary tables
    and spatial index. */
    mode = (index->table->is_temporary() || dict_index_is_spatial(index))
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
    pcur.btr_cur.thr = static_cast<que_thr_t *>(que_node_get_parent(node));
  }

  search_result = row_search_index_entry(index, entry, mode, &pcur, &mtr);

  if (dict_index_is_spatial(index)) {
    rw_lock_sx_unlock(dict_index_get_lock(index));
  }

  switch (search_result) {
    case ROW_FOUND:
      /* Before attempting to purge a record, check
      if it is safe to do so. */
      if (row_purge_poss_sec(node, index, entry)) {
        btr_cur_t *btr_cur = btr_pcur_get_btr_cur(&pcur);

        /* Only delete-marked records should be purged. */
        if (!rec_get_deleted_flag(btr_cur_get_rec(btr_cur),
                                  dict_table_is_comp(index->table))) {
          ib::error(ER_IB_MSG_1008)
              << "tried to purge non-delete-marked"
                 " record"
                 " in index "
              << index->name << " of table " << index->table->name
              << ": tuple: " << *entry << ", record: "
              << rec_index_print(btr_cur_get_rec(btr_cur), index);
          ut_ad(0);

          btr_pcur_close(&pcur);

          goto func_exit_no_pcur;
        }

        if (dict_index_is_spatial(index)) {
          const page_t *page;
          const trx_t *trx = NULL;

          if (btr_cur->rtr_info != NULL && btr_cur->rtr_info->thr != NULL) {
            trx = thr_get_trx(btr_cur->rtr_info->thr);
          }

          page = btr_cur_get_page(btr_cur);

          if (!lock_test_prdt_page_lock(trx, page_get_space_id(page),
                                        page_get_page_no(page)) &&
              page_get_n_recs(page) < 2 &&
              page_get_page_no(page) != dict_index_get_page(index)) {
          /* this is the last record on page,
          and it has a "page" lock on it,
          which mean search is still depending
          on it, so do not delete */
#ifdef UNIV_DEBUG
            ib::info(ER_IB_MSG_1009) << "skip purging last"
                                        " record on page "
                                     << page_get_page_no(page) << ".";
#endif /* UNIV_DEBUG */

            btr_pcur_close(&pcur);
            mtr_commit(&mtr);
            return (success);
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
      return (success);
  }

  ut_error;
  return (false);
}

/** Removes a secondary index entry if possible. */
UNIV_INLINE
void row_purge_remove_sec_if_poss(purge_node_t *node, /*!< in: row purge node */
                                  dict_index_t *index,   /*!< in: index */
                                  const dtuple_t *entry) /*!< in: index entry */
{
  ibool success;
  ulint n_tries = 0;

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
static inline void row_purge_skip_uncommitted_virtual_index(
    dict_index_t *&index) {
  /* We need to skip virtual indexes which is not
  committed yet. It's safe because these indexes are
  newly created by alter table, and because we do
  not support LOCK=NONE when adding an index on newly
  added virtual column.*/
  while (index != NULL && dict_index_has_virtual(index) &&
         !index->is_committed() && index->has_new_v_col) {
    index = index->next();
  }
}

/** Purges a delete marking of a record.
 @retval true if the row was not found, or it was successfully removed
 @retval false the purge needs to be suspended because of
 running out of file space */
static MY_ATTRIBUTE((warn_unused_result)) bool row_purge_del_mark(
    purge_node_t *node) /*!< in/out: row purge node */
{
  mem_heap_t *heap;

  heap = mem_heap_create(1024);

  while (node->index != NULL) {
    /* skip corrupted secondary index */
    dict_table_skip_corrupt_index(node->index);

    row_purge_skip_uncommitted_virtual_index(node->index);

    if (!node->index) {
      break;
    }

    if (node->index->type != DICT_FTS) {
      dtuple_t *entry = row_build_index_entry_low(node->row, NULL, node->index,
                                                  heap, ROW_BUILD_FOR_PURGE);
      row_purge_remove_sec_if_poss(node, node->index, entry);
      mem_heap_empty(heap);
    }

    node->index = node->index->next();
  }

  mem_heap_free(heap);

  return (row_purge_remove_clust_if_poss(node));
}

/** Purges an update of an existing record. Also purges an update of a delete
 marked record if that record contained an externally stored field. */
static void row_purge_upd_exist_or_extern_func(
#ifdef UNIV_DEBUG
    const que_thr_t *thr,     /*!< in: query thread */
#endif                        /* UNIV_DEBUG */
    purge_node_t *node,       /*!< in: row purge node */
    trx_undo_rec_t *undo_rec) /*!< in: record to purge */
{
  mem_heap_t *heap;

  ut_ad(!node->table->skip_alter_undo);

  if (node->rec_type == TRX_UNDO_UPD_DEL_REC ||
      (node->cmpl_info & UPD_NODE_NO_ORD_CHANGE)) {
    goto skip_secondaries;
  }

  heap = mem_heap_create(1024);

  while (node->index != NULL) {
    dict_table_skip_corrupt_index(node->index);

    row_purge_skip_uncommitted_virtual_index(node->index);

    if (!node->index) {
      break;
    }

    if (row_upd_changes_ord_field_binary(node->index, node->update, thr, NULL,
                                         NULL)) {
      /* Build the older version of the index entry */
      dtuple_t *entry = row_build_index_entry_low(node->row, NULL, node->index,
                                                  heap, ROW_BUILD_FOR_PURGE);
      row_purge_remove_sec_if_poss(node, node->index, entry);
      mem_heap_empty(heap);
    }

    node->index = node->index->next();
  }

  mem_heap_free(heap);

skip_secondaries:

  /* Free possible externally stored fields */
  for (ulint i = 0; i < upd_get_n_fields(node->update); i++) {
    const upd_field_t *ufield = upd_get_nth_field(node->update, i);

    if (dfield_is_ext(&ufield->new_val)) {
      buf_block_t *block;
      ulint internal_offset;
      byte *data_field;
      dict_index_t *index;
      ibool is_insert;
      ulint rseg_id;
      page_no_t page_no;
      space_id_t undo_space_id;
      ulint offset;
      mtr_t mtr;

      /* We use the fact that new_val points to
      undo_rec and get thus the offset of
      dfield data inside the undo record. Then we
      can calculate from node->roll_ptr the file
      address of the new_val data */

      internal_offset =
          ((const byte *)dfield_get_data(&ufield->new_val)) - undo_rec;

      ut_a(internal_offset < UNIV_PAGE_SIZE);

      trx_undo_decode_roll_ptr(node->roll_ptr, &is_insert, &rseg_id, &page_no,
                               &offset);

      /* If table is temp then it can't have its undo log
      residing in rollback segment with REDO log enabled. */
      bool is_temp = node->table->is_temporary();

      undo_space_id = trx_rseg_id_to_space_id(rseg_id, is_temp);

      mtr_start(&mtr);

      /* We have to acquire an SX-latch to the clustered
      index tree (exclude other tree changes) */

      index = node->table->first_index();

      mtr_sx_lock(dict_index_get_lock(index), &mtr);

      /* NOTE: we must also acquire an X-latch to the
      root page of the tree. We will need it when we
      free pages from the tree. If the tree is of height 1,
      the tree X-latch does NOT protect the root page,
      because it is also a leaf page. Since we will have a
      latch on an undo log page, we would break the
      latching order if we would only later latch the
      root page of such a tree! */

      btr_root_get(index, &mtr);

      block = buf_page_get(page_id_t(undo_space_id, page_no), univ_page_size,
                           RW_X_LATCH, &mtr);

      buf_block_dbg_add_level(block, SYNC_TRX_UNDO_PAGE);

      data_field = buf_block_get_frame(block) + offset + internal_offset;

      ut_a(dfield_get_len(&ufield->new_val) >= BTR_EXTERN_FIELD_REF_SIZE);

      byte *field_ref = data_field + dfield_get_len(&ufield->new_val) -
                        BTR_EXTERN_FIELD_REF_SIZE;

      lob::BtrContext btr_ctx(&mtr, NULL, index, NULL, NULL, block);

      lob::DeleteContext ctx(btr_ctx, field_ref, 0, false);

      lob::ref_t lobref(field_ref);

      lob::purge(&ctx, index, node->modifier_trx_id,
                 trx_undo_rec_get_undo_no(undo_rec), lobref, node->rec_type,
                 ufield);

      mtr_commit(&mtr);
    }
  }
}

#ifdef UNIV_DEBUG
#define row_purge_upd_exist_or_extern(thr, node, undo_rec) \
  row_purge_upd_exist_or_extern_func(thr, node, undo_rec)
#else /* UNIV_DEBUG */
#define row_purge_upd_exist_or_extern(thr, node, undo_rec) \
  row_purge_upd_exist_or_extern_func(node, undo_rec)
#endif /* UNIV_DEBUG */

/** Parses the row reference and other info in a modify undo log record.
 @param[in,out]	node			row undo node
 @param[in]	undo_rec		undo record to purge
 @param[out]	updated_extern		whether an externally stored
 field was updated
 @param[in,out]	thd			current thread
 @param[in,out]	thr			execution thread
 @return true if purge operation required */
static bool row_purge_parse_undo_rec(purge_node_t *node,
                                     trx_undo_rec_t *undo_rec,
                                     bool *updated_extern, THD *thd,
                                     que_thr_t *thr) {
  dict_index_t *clust_index;
  byte *ptr;
  trx_t *trx;
  undo_no_t undo_no;
  table_id_t table_id;
  trx_id_t trx_id;
  roll_ptr_t roll_ptr;
  ulint info_bits;
  ulint type;
  type_cmpl_t type_cmpl;

  ut_ad(node != NULL);
  ut_ad(thr != NULL);

  ptr = trx_undo_rec_get_pars(undo_rec, &type, &node->cmpl_info, updated_extern,
                              &undo_no, &table_id, type_cmpl);

  node->rec_type = type;

  if (type == TRX_UNDO_UPD_DEL_REC && !*updated_extern) {
    return (false);
  }

  ptr = trx_undo_update_rec_get_sys_cols(ptr, &trx_id, &roll_ptr, &info_bits);
  node->table = NULL;
  node->trx_id = trx_id;

  /* TODO: Remove all INNODB_DD_VC_SUPPORT, nest opening
  table should never happen again after new DD */
#ifdef INNODB_DD_VC_SUPPORT
try_again:
#endif /* INNODB_DD_VC_SUPPORT */

  /* Cannot call dd_table_open_on_id() before server is fully up */
  if (!srv_upgrade_old_undo_found && !dict_table_is_system(table_id)) {
    while (!mysqld_server_started) {
      if (srv_shutdown_state != SRV_SHUTDOWN_NONE) {
        return (false);
      }
      os_thread_sleep(1000000);
    }
  }

  /* SDI tables are hidden tables and are not registered with global
  dictionary. Open the table internally. Also acquire shared MDL
  of SDI tables. Concurrent DROP TABLE/TABLESPACE would acquire
  exclusive MDL on SDI tables */
  ut_ad(!dict_table_is_system(table_id));

  if (dict_table_is_sdi(table_id) || srv_upgrade_old_undo_found) {
    if (dict_table_is_sdi(table_id)) {
      space_id_t space_id = dict_sdi_get_space_id(table_id);

      dberr_t err = dd_sdi_acquire_shared_mdl(thd, space_id, &node->mdl);
      if (err != DB_SUCCESS) {
        node->table = nullptr;
        return (false);
      }

      node->table = dd_table_open_on_id(table_id, thd, &node->mdl, false, true);

      if (node->table == nullptr) {
        /* Tablespace containing SDI table
        is already dropped */
        dd::release_mdl(thd, node->mdl);
        node->mdl = nullptr;
      }
    }
  } else {
    for (;;) {
      const auto no_mdl = nullptr;
      node->mdl = no_mdl;

      mutex_enter(&dict_sys->mutex);
      node->table = dd_table_open_on_id(table_id, thd, &node->mdl, true, true);

      if (node->table && node->table->is_temporary()) {
        /* Temp table does not do purge */
        ut_ad(node->mdl == nullptr);
        dd_table_close(node->table, nullptr, nullptr, true);
        mutex_exit(&dict_sys->mutex);
        goto err_exit;
      }

      mutex_exit(&dict_sys->mutex);

      if (node->table != nullptr) {
        if (node->table->is_fts_aux()) {
          table_id_t parent_id = node->table->parent_id;

          dd_table_close(node->table, thd, &node->mdl, false);

          node->parent_mdl = nullptr;
          node->parent = dd_table_open_on_id(parent_id, thd, &node->parent_mdl,
                                             false, true);

          if (node->parent == nullptr) {
            goto err_exit;
          }

          ut_ad(node->parent_mdl != nullptr);
          node->mdl = nullptr;
          node->table =
              dd_table_open_on_id(table_id, thd, &node->mdl, false, true);
        }
        break;
      }

      if (node->mdl == no_mdl) {
        /* The table has been dropped: no need
        to do purge */
        node->mdl = nullptr;
        goto err_exit;
      }
    }
  }

  if (node->table == NULL) {
    /* The table has been dropped: no need to do purge */
    goto err_exit;
  }

#ifdef INNODB_DD_VC_SUPPORT
  if (node->table->n_v_cols && !node->table->vc_templ &&
      dict_table_has_indexed_v_cols(node->table)) {
    /* Need server fully up for virtual column computation */
    if (!mysqld_server_started) {
      if (dict_table_is_sdi(node->table->id)) {
        dd_table_close(node->table, thd, &node->mdl, false);
        node->table = nullptr;

      } else {
        bool is_aux = node->table->is_fts_aux();

        dd_table_close(node->table, thd, &node->mdl, false);
        if (is_aux && node->parent) {
          dd_table_close(node->parent, thd & node->parent_mdl, false);
        }
      }
      if (srv_shutdown_state != SRV_SHUTDOWN_NONE) {
        return (false);
      }
      os_thread_sleep(1000000);
      goto try_again;
    }

    /* Initialize the template for the table */
    innobase_init_vc_templ(node->table);
  }
#endif /* INNODB_DD_VC_SUPPORT */

  /* Disable purging for temp-tables as they are short-lived
  and no point in re-organzing such short lived tables */
  if (node->table->is_temporary()) {
    goto close_exit;
  }

  if (node->table->ibd_file_missing) {
    /* We skip purge of missing .ibd files */

    if (dict_table_is_sdi(node->table->id)) {
      dd_table_close(node->table, thd, &node->mdl, false);
      node->table = NULL;
    } else {
      bool is_aux = node->table->is_fts_aux();
      dd_table_close(node->table, thd, &node->mdl, false);
      if (is_aux && node->parent) {
        dd_table_close(node->parent, thd, &node->parent_mdl, false);
      }
    }

    node->table = NULL;

    goto err_exit;
  }

  clust_index = node->table->first_index();

  if (clust_index == NULL || clust_index->is_corrupted()) {
    /* The table was corrupt in the data dictionary.
    dict_set_corrupted() works on an index, and
    we do not have an index to call it with. */
  close_exit:
    /* Purge requires no changes to indexes: we may return */
    if (dict_table_is_sdi(node->table->id) || srv_upgrade_old_undo_found) {
      if (dict_table_is_sdi(node->table->id)) {
        dd_table_close(node->table, thd, &node->mdl, false);
      } else {
        dict_table_close(node->table, FALSE, FALSE);
      }
      node->table = NULL;
    } else {
      bool is_aux = node->table->is_fts_aux();
      dd_table_close(node->table, thd, &node->mdl, false);
      if (is_aux && node->parent) {
        dd_table_close(node->parent, thd, &node->parent_mdl, false);
      }
    }
  err_exit:
    return (false);
  }

  if (type == TRX_UNDO_UPD_EXIST_REC &&
      (node->cmpl_info & UPD_NODE_NO_ORD_CHANGE) && !*updated_extern) {
    /* Purge requires no changes to indexes: we may return */
    goto close_exit;
  }

  ptr = trx_undo_rec_get_row_ref(ptr, clust_index, &(node->ref), node->heap);

  trx = thr_get_trx(thr);

  ptr = trx_undo_update_rec_get_update(ptr, clust_index, type, trx_id, roll_ptr,
                                       info_bits, trx, node->heap,
                                       &(node->update), nullptr, type_cmpl);

  /* Read to the partial row the fields that occur in indexes */

  if (!(node->cmpl_info & UPD_NODE_NO_ORD_CHANGE)) {
    ptr = trx_undo_rec_get_partial_row(
        ptr, clust_index, &node->row, type == TRX_UNDO_UPD_DEL_REC, node->heap);
  }

  return (true);
}

/** Purges the parsed record.
@param[in,out]	node		row purge node
@param[in]	undo_rec	undo record to purge
@param[in,out]	thr		query thread
@param[in]	updated_extern	whether external columns were updated
@param[in,out]	thd		current thread
@return true if purged, false if skipped */
static MY_ATTRIBUTE((warn_unused_result)) bool row_purge_record_func(
    purge_node_t *node, trx_undo_rec_t *undo_rec,
#ifdef UNIV_DEBUG
    const que_thr_t *thr,
#endif /* UNIV_DEBUG */
    bool updated_extern, THD *thd) {
  dict_index_t *clust_index;
  bool purged = true;

  ut_ad(!node->found_clust);
  ut_ad(!node->table->skip_alter_undo);

  clust_index = node->table->first_index();

  node->index = clust_index->next();
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

  if (node->update != nullptr) {
    node->update->destroy();
  }

  if (node->found_clust) {
    btr_pcur_close(&node->pcur);
    node->found_clust = FALSE;
  }

  if (node->table != NULL) {
    if (node->mysql_table != nullptr) {
      close_thread_tables(thd);
      node->mysql_table = nullptr;
    }

    if (dict_table_is_sdi(node->table->id)) {
      dd_table_close(node->table, thd, &node->mdl, false);
      node->table = NULL;
    } else {
      bool is_aux = node->table->is_fts_aux();
      dd_table_close(node->table, thd, &node->mdl, false);
      if (is_aux && node->parent) {
        dd_table_close(node->parent, thd, &node->parent_mdl, false);
      }
    }
  }

  return (purged);
}

#ifdef UNIV_DEBUG
#define row_purge_record(node, undo_rec, thr, updated_extern, thd) \
  row_purge_record_func(node, undo_rec, thr, updated_extern, thd)
#else /* UNIV_DEBUG */
#define row_purge_record(node, undo_rec, thr, updated_extern, thd) \
  row_purge_record_func(node, undo_rec, updated_extern, thd)
#endif /* UNIV_DEBUG */

/** Fetches an undo log record and does the purge for the recorded operation.
 If none left, or the current purge completed, returns the control to the
 parent node, which is always a query thread node. */
static void row_purge(purge_node_t *node,       /*!< in: row purge node */
                      trx_undo_rec_t *undo_rec, /*!< in: record to purge */
                      que_thr_t *thr)           /*!< in: query thread */
{
  bool updated_extern;
  THD *thd = current_thd;

  DBUG_EXECUTE_IF("do_not_meta_lock_in_background",
                  while (srv_shutdown_state == SRV_SHUTDOWN_NONE) {
                    os_thread_sleep(500000);
                  } return;);

  while (row_purge_parse_undo_rec(node, undo_rec, &updated_extern, thd, thr)) {
    bool purged;

    purged = row_purge_record(node, undo_rec, thr, updated_extern, thd);

    if (purged || srv_shutdown_state != SRV_SHUTDOWN_NONE) {
      return;
    }

    /* Retry the purge in a second. */
    os_thread_sleep(1000000);
  }
}

/** Reset the purge query thread.
@param[in,out]	thr		The query thread to execute */
static void row_purge_end(que_thr_t *thr) {
  purge_node_t *node;

  node = static_cast<purge_node_t *>(thr->run_node);

  ut_ad(que_node_get_type(node) == QUE_NODE_PURGE);

  thr->run_node = que_node_get_parent(node);

  if (node->recs != nullptr) {
    ut_ad(node->recs->empty());

    /* Note: We call the destructor explicitly here, but don't
    want to free the memory. The Recs (and rows contained within)
    were allocated from the purge_sys->heap */

    call_destructor(node->recs);

    node->recs = nullptr;
  }

  node->done = true;

  ut_a(thr->run_node != NULL);

  mem_heap_empty(node->heap);
}

/** Does the purge operation for a single undo log record. This is a high-level
function used in an SQL execution graph.
@param[in,out]	thr		The query thread to execute
@return query thread to run next or nullptr */
que_thr_t *row_purge_step(que_thr_t *thr) {
  purge_node_t *node;

  node = static_cast<purge_node_t *>(thr->run_node);

  node->table = nullptr;
  node->row = nullptr;
  node->ref = nullptr;
  node->index = nullptr;
  node->update = nullptr;
  node->found_clust = FALSE;
  node->rec_type = ULINT_UNDEFINED;
  node->cmpl_info = ULINT_UNDEFINED;

  ut_a(!node->done);

  ut_ad(que_node_get_type(node) == QUE_NODE_PURGE);

  if (node->recs != nullptr && !node->recs->empty()) {
    purge_node_t::rec_t rec;

    rec = node->recs->front();
    node->recs->pop_front();

    node->roll_ptr = rec.roll_ptr;
    node->modifier_trx_id = rec.modifier_trx_id;

    row_purge(node, rec.undo_rec, thr);

    if (node->recs->empty()) {
      row_purge_end(thr);
    } else {
      thr->run_node = node;
    }

  } else {
    row_purge_end(thr);
  }

  return (thr);
}

#ifdef UNIV_DEBUG
/** Validate the persisent cursor. The purge node has two references
 to the clustered index record - one via the ref member, and the
 other via the persistent cursor.  These two references must match
 each other if the found_clust flag is set.
 @return true if the stored copy of persistent cursor is consistent
 with the ref member.*/
bool purge_node_t::validate_pcur() {
  if (!found_clust) {
    return (true);
  }

  if (index == NULL) {
    return (true);
  }

  if (index->type == DICT_FTS) {
    return (true);
  }

  if (!pcur.old_stored) {
    return (true);
  }

  dict_index_t *clust_index = pcur.btr_cur.index;

  ulint *offsets = rec_get_offsets(pcur.old_rec, clust_index, NULL,
                                   pcur.old_n_fields, &heap);

  /* Here we are comparing the purge ref record and the stored initial
  part in persistent cursor. Both cases we store n_uniq fields of the
  cluster index and so it is fine to do the comparison. We note this
  dependency here as pcur and ref belong to different modules. */
  int st = cmp_dtuple_rec(ref, pcur.old_rec, clust_index, offsets);

  if (st != 0) {
    ib::error(ER_IB_MSG_1010) << "Purge node pcur validation failed";
    ib::error(ER_IB_MSG_1011) << rec_printer(ref).str();
    ib::error(ER_IB_MSG_1012) << rec_printer(pcur.old_rec, offsets).str();
    return (false);
  }

  return (true);
}
#endif /* UNIV_DEBUG */

bool purge_node_t::is_table_id_exists(table_id_t table_id) const {
  if (recs == nullptr) {
    return (false);
  }

  for (auto iter = recs->begin(); iter != recs->end(); ++iter) {
    table_id_t table_id2 = trx_undo_rec_get_table_id(iter->undo_rec);
    if (table_id == table_id2) {
      return (true);
    }
  }
  return (false);
}

#ifdef UNIV_DEBUG
/** Check if there are more than one undo record with same (trx_id, undo_no)
combination.
@return true when no duplicates are found, false otherwise. */
bool purge_node_t::check_duplicate_undo_no() const {
  using Two = std::pair<trx_id_t, undo_no_t>;
  std::set<Two> trx_info;
  using Iter = std::set<Two>::iterator;

  if (recs == nullptr) {
    return (true);
  }

  for (auto iter = recs->begin(); iter != recs->end(); ++iter) {
    trx_id_t trxid = iter->modifier_trx_id;
    undo_no_t undo_no = trx_undo_rec_get_undo_no(iter->undo_rec);

    std::pair<Iter, bool> ret = trx_info.insert(std::make_pair(trxid, undo_no));
    ut_ad(ret.second == true);
  }

  return (true);
}
#endif /* UNIV_DEBUG */
