/*****************************************************************************

Copyright (c) 1997, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file row/row0undo.cc
 Row undo

 Created 1/8/1997 Heikki Tuuri
 *******************************************************/

#include <stddef.h>
#include <type_traits>

#include "fsp0fsp.h"
#include "ha_prototypes.h"
#include "mach0data.h"
#include "que0que.h"
#include "row0mysql.h"
#include "row0row.h"
#include "row0uins.h"
#include "row0umod.h"
#include "row0undo.h"
#include "row0upd.h"
#include "srv0srv.h"
#include "trx0purge.h"
#include "trx0rec.h"
#include "trx0roll.h"
#include "trx0rseg.h"
#include "trx0trx.h"
#include "trx0undo.h"

/* How to undo row operations?
(1) For an insert, we have stored a prefix of the clustered index record
in the undo log. Using it, we look for the clustered record, and using
that we look for the records in the secondary indexes. The insert operation
may have been left incomplete, if the database crashed, for example.
We may have look at the trx id and roll ptr to make sure the record in the
clustered index is really the one for which the undo log record was
written. We can use the framework we get from the original insert op.
(2) Delete marking: We can use the framework we get from the original
delete mark op. We only have to check the trx id.
(3) Update: This may be the most complicated. We have to use the framework
we get from the original update op.

What if the same trx repeatedly deletes and inserts an identical row.
Then the row id changes and also roll ptr. What if the row id was not
part of the ordering fields in the clustered index? Maybe we have to write
it to undo log. Well, maybe not, because if we order the row id and trx id
in descending order, then the only undeleted copy is the first in the
index. Our searches in row operations always position the cursor before
the first record in the result set. But, if there is no key defined for
a table, then it would be desirable that row id is in ascending order.
So, lets store row id in descending order only if it is not an ordering
field in the clustered index.

NOTE: Deletes and inserts may lead to situation where there are identical
records in a secondary index. Is that a problem in the B-tree? Yes.
Also updates can lead to this, unless trx id and roll ptr are included in
ord fields.
(1) Fix in clustered indexes: include row id, trx id, and roll ptr
in node pointers of B-tree.
(2) Fix in secondary indexes: include all fields in node pointers, and
if an entry is inserted, check if it is equal to the right neighbor,
in which case update the right neighbor: the neighbor must be delete
marked, set it unmarked and write the trx id of the current transaction.

What if the same trx repeatedly updates the same row, updating a secondary
index field or not? Updating a clustered index ordering field?

(1) If it does not update the secondary index and not the clustered index
ord field. Then the secondary index record stays unchanged, but the
trx id in the secondary index record may be smaller than in the clustered
index record. This is no problem?
(2) If it updates secondary index ord field but not clustered: then in
secondary index there are delete marked records, which differ in an
ord field. No problem.
(3) Updates clustered ord field but not secondary, and secondary index
is unique. Then the record in secondary index is just updated at the
clustered ord field.
(4)

Problem with duplicate records:
Fix 1: Add a trx op no field to all indexes. A problem: if a trx with a
bigger trx id has inserted and delete marked a similar row, our trx inserts
again a similar row, and a trx with an even bigger id delete marks it. Then
the position of the row should change in the index if the trx id affects
the alphabetical ordering.

Fix 2: If an insert encounters a similar row marked deleted, we turn the
insert into an 'update' of the row marked deleted. Then we must write undo
info on the update. A problem: what if a purge operation tries to remove
the delete marked row?

We can think of the database row versions as a linked list which starts
from the record in the clustered index, and is linked by roll ptrs
through undo logs. The secondary index records are references which tell
what kinds of records can be found in this linked list for a record
in the clustered index.

How to do the purge? A record can be removed from the clustered index
if its linked list becomes empty, i.e., the row has been marked deleted
and its roll ptr points to the record in the undo log we are going through,
doing the purge. Similarly, during a rollback, a record can be removed
if the stored roll ptr in the undo log points to a trx already (being) purged,
or if the roll ptr is NULL, i.e., it was a fresh insert. */

using namespace std::literals::chrono_literals;

undo_node_t::undo_node_t(trx_t &trx, que_thr_t *parent, bool partial_rollback)
    : common{},
      state{UNDO_NODE_FETCH_NEXT},
      trx{trx},
      heap{mem_heap_create(256, UT_LOCATION_HERE)},
      partial{partial_rollback},
      long_undo_state(trx) {
  common.type = QUE_NODE_UNDO;
  common.parent = parent;
  pcur.init();
}

Long_undo_state::Long_undo_state(const trx_t &trx)
    : throttler(30s),
      rows_total{trx.undo_no - trx.roll_limit},
      trx_state{trx.state.load()} {
  throttler.apply();
}

undo_node_t *row_undo_node_create(trx_t &trx, que_thr_t *parent,
                                  mem_heap_t *heap, bool partial_rollback) {
  undo_node_t *undo;

  ut_ad(trx_state_eq(&trx, TRX_STATE_ACTIVE) ||
        trx_state_eq(&trx, TRX_STATE_PREPARED));
  ut_ad(parent);

  // no destructor call
  static_assert(std::is_trivially_destructible_v<undo_node_t>);
  undo = static_cast<undo_node_t *>(mem_heap_alloc(heap, sizeof(undo_node_t)));

  return new (undo) undo_node_t(trx, parent, partial_rollback);
}

/** Looks for the clustered index record when node has the row reference.
 The pcur in node is used in the search. If found, stores the row to node,
 and stores the position of pcur, and detaches it. The pcur must be closed
 by the caller in any case.
 @return true if found; NOTE the node->pcur must be closed by the
 caller, regardless of the return value */
bool row_undo_search_clust_to_pcur(
    undo_node_t *node) /*!< in/out: row undo node */
{
  dict_index_t *clust_index;
  bool found;
  mtr_t mtr;
  row_ext_t **ext;
  const rec_t *rec;
  mem_heap_t *heap = nullptr;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  ut_ad(!node->table->skip_alter_undo);

  mtr_start(&mtr);
  dict_disable_redo_if_temporary(node->table, &mtr);

  clust_index = node->table->first_index();

  found = row_search_on_row_ref(&node->pcur, BTR_MODIFY_LEAF, node->table,
                                node->ref, &mtr);

  if (!found) {
    goto func_exit;
  }

  rec = node->pcur.get_rec();

  offsets = rec_get_offsets(rec, clust_index, offsets, ULINT_UNDEFINED,
                            UT_LOCATION_HERE, &heap);

  found = row_get_rec_roll_ptr(rec, clust_index, offsets) == node->roll_ptr;

  if (found) {
    ut_ad(row_get_rec_trx_id(rec, clust_index, offsets) == node->trx.id);

    if (dict_table_has_atomic_blobs(node->table)) {
      /* There is no prefix of externally stored
      columns in the clustered index record. Build a
      cache of column prefixes. */
      ext = &node->ext;
    } else {
      /* REDUNDANT and COMPACT formats store a local
      768-byte prefix of each externally stored
      column. No cache is needed. */
      ext = nullptr;
      node->ext = nullptr;
    }

    node->row = row_build(ROW_COPY_DATA, clust_index, rec, offsets, nullptr,
                          nullptr, nullptr, ext, node->heap);

    /* We will need to parse out virtual column info from undo
    log, first mark them DATA_MISSING. So we will know if the
    value gets updated */
    if (node->table->n_v_cols && node->state != UNDO_NODE_INSERT &&
        !(node->cmpl_info & UPD_NODE_NO_ORD_CHANGE)) {
      for (ulint i = 0; i < dict_table_get_n_v_cols(node->table); i++) {
        dfield_get_type(dtuple_get_nth_v_field(node->row, i))->mtype =
            DATA_MISSING;
      }
    }

    if (node->rec_type == TRX_UNDO_UPD_EXIST_REC) {
      node->undo_row = dtuple_copy(node->row, node->heap);
      row_upd_replace(node->undo_row, &node->undo_ext, clust_index,
                      node->update, node->heap);
    } else {
      node->undo_row = nullptr;
      node->undo_ext = nullptr;
    }

    node->pcur.store_position(&mtr);
  }

  if (heap) {
    mem_heap_free(heap);
  }

func_exit:
  node->pcur.commit_specify_mtr(&mtr);
  return (found);
}

/** Called for every row, prints diagnostics for long running rollbacks */
static void long_running_diag(undo_node_t &node) {
  auto &diag = node.long_undo_state;

  /* To minimize risk of impact on performance the throttler,
  which uses system clock function, will only be called every n rows.

  We arrive at the value of n by estimating risk - on one hand of the clock
  function affecting performance (e.g. because of making a kernel call, which
  has been observed in the past with some platforms/toolchains), on the
  other, of undoing n rows taking up significant time (at least order of tens
  of seocnds). The value of 100 was chosen based on speculative estimation
  that both risks are sufficiently small for this value. Should either
  scenario actually materialize, the value of throttle_interval may be
  adjusted. */
  constexpr auto throttler_interval = 100;

  auto const rows_todo = node.trx.undo_no - node.trx.roll_limit;
  ut_ad(diag.rows_total >= rows_todo);
  ulonglong const rows_processed{diag.rows_total - rows_todo};
  if (rows_processed % throttler_interval == 0) {
    bool do_log = diag.throttler.apply();
    DBUG_EXECUTE_IF("log_long_rollback", { do_log = true; });
    if (do_log) {
      ulonglong const rows_total{diag.rows_total};
      ulonglong const trx_id{node.trx.id};
      ulong const pct =
          rows_total != 0 ? (100ULL * rows_processed / rows_total) : 0UL;
      if (diag.have_logged) {
        ib::info(ER_IB_LONG_ROLLBACK, trx_id, rows_processed, rows_total, pct);
      } else {
        std::ostringstream desc;
        if (auto const state_string = trx_state_string(diag.trx_state)) {
          desc << state_string;
        } else {
          desc << "state " << to_int(diag.trx_state);
        }
        if (node.trx.xid) {
          desc << "; XID: " << *node.trx.xid;
        }
        ib::info(ER_IB_LONG_ROLLBACK_FULL, trx_id, rows_processed, rows_total,
                 pct, desc.str().c_str());
        diag.have_logged = true;
      }
    }
  }
}

/** Fetches an undo log record and does the undo for the recorded operation.
 If none left, or a partial rollback completed, returns control to the
 parent node, which is always a query thread node.
 @return DB_SUCCESS if operation successfully completed, else error code */
[[nodiscard]] static dberr_t row_undo(
    undo_node_t *node, /*!< in: row undo node */
    que_thr_t *thr)    /*!< in: query thread */
{
  dberr_t err;
  roll_ptr_t roll_ptr;

  ut_a(node != nullptr);
  ut_ad(thr != nullptr);

  trx_t &trx = node->trx;
  ut_ad(trx.in_rollback);

  long_running_diag(*node);

  if (node->state == UNDO_NODE_FETCH_NEXT) {
    node->undo_rec = trx_roll_pop_top_rec_of_trx(&trx, trx.roll_limit,
                                                 &roll_ptr, node->heap);

    if (!node->undo_rec) {
      /* Rollback completed for this query thread */

      thr->run_node = que_node_get_parent(node);

      /* Mark any partial rollback completed, so
      that if the transaction object is committed
      and reused later, the roll_limit will remain
      at 0. trx->roll_limit will be nonzero during a
      partial rollback only. */
      trx.roll_limit = 0;
      ut_d(trx.in_rollback = false);

      return (DB_SUCCESS);
    }

    node->roll_ptr = roll_ptr;
    node->undo_no = trx_undo_rec_get_undo_no(node->undo_rec);

    if (trx_undo_roll_ptr_is_insert(roll_ptr)) {
      node->state = UNDO_NODE_INSERT;
    } else {
      node->state = UNDO_NODE_MODIFY;
    }
  }

  /* During rollback, trx is holding at least LOCK_IX on each
  modified table. It may also hold MDL. A concurrent DROP TABLE
  or ALTER TABLE should be impossible, because it should be
  holding both LOCK_X and MDL_EXCLUSIVE on the table. */
  if (node->state == UNDO_NODE_INSERT) {
    err = row_undo_ins(node, thr);

    node->state = UNDO_NODE_FETCH_NEXT;
  } else {
    ut_ad(node->state == UNDO_NODE_MODIFY);
    err = row_undo_mod(node, thr);
  }

  /* Do some cleanup */
  node->pcur.close();

  mem_heap_empty(node->heap);

  thr->run_node = node;

  return (err);
}

void row_convert_impl_to_expl_if_needed(btr_cur_t *cursor, undo_node_t *node) {
  /* In case of partial rollback implicit lock on the
  record is released in the middle of transaction, which
  can break the serializability of IODKU and REPLACE
  statements. Normal rollback is not affected by this
  because we release the locks after the rollback. So
  to prevent any other transaction modifying the record
  in between the partial rollback we convert the implicit
  lock on the record to explicit. When the record is actually
  deleted this lock will be inherited by the next record.  */

  if (!node->partial || node->trx.isolation_level < trx_t::REPEATABLE_READ) {
    return;
  }

  ut_ad(node->trx.in_rollback);
  auto index = cursor->index;
  auto rec = btr_cur_get_rec(cursor);
  auto block = btr_cur_get_block(cursor);
  auto heap_no = page_rec_get_heap_no(rec);

  if (heap_no != PAGE_HEAP_NO_SUPREMUM && !dict_index_is_spatial(index) &&
      !index->table->is_temporary() && !index->table->is_intrinsic()) {
    lock_rec_convert_impl_to_expl(block, rec, index,
                                  Rec_offsets().compute(rec, index));
  }
}

/** Undoes a row operation in a table. This is a high-level function used
 in SQL execution graphs.
 @return query thread to run next or NULL */
que_thr_t *row_undo_step(que_thr_t *thr) /*!< in: query thread */
{
  dberr_t err;
  undo_node_t *node;
  trx_t *trx;

  ut_ad(thr);

  srv_inc_activity_count();

  trx = thr_get_trx(thr);

  node = static_cast<undo_node_t *>(thr->run_node);

  ut_ad(que_node_get_type(node) == QUE_NODE_UNDO);

  err = row_undo(node, thr);

  trx->error_state = err;

  if (err != DB_SUCCESS) {
    /* SQL error detected */

    if (err == DB_OUT_OF_FILE_SPACE) {
      ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_1041)
          << "Out of tablespace during rollback."
             " Consider increasing your tablespace.";
    }

    ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_1042)
        << "Error (" << ut_strerr(err) << ") in rollback.";
  }

  return (thr);
}
