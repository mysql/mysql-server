/*****************************************************************************

Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#include <scope_guard.h>
#include "lob0del.h"
#include "lob0first.h"
#include "lob0index.h"
#include "lob0inf.h"
#include "lob0lob.h"
#include "log0buf.h"
#include "row0purge.h"
#include "row0upd.h"
#include "trx0purge.h"
#include "trx0rec.h"
#include "zlob0first.h"
#include "zlob0index.h"
#include "zlob0read.h"

namespace lob {

/** Rollback from undo log information.
@param[in]      ctx     the delete operation context.
@param[in]      index   the clustered index to which LOB belongs.
@param[in]      uf      the update vector of concerned field. */
static void rollback_from_undolog(DeleteContext *ctx, dict_index_t *index,
                                  const upd_field_t *uf) {
  DBUG_TRACE;

  dberr_t err = apply_undolog(ctx->get_mtr(), index, ctx->m_blobref, uf);
  ut_a(err == DB_SUCCESS);
}

/** Rollback modification of a uncompressed LOB.
@param[in]      ctx             the delete operation context information.
@param[in]      index           clustered index in which LOB is present
@param[in]      trxid           the transaction that is being rolled back.
@param[in]      undo_no         during rollback to savepoint, rollback only
                                up to this undo number.
@param[in]      rec_type        undo record type.
@param[in]      uf              update vector of the concerned field. */
static void rollback(DeleteContext *ctx, dict_index_t *index, trx_id_t trxid,
                     undo_no_t undo_no, ulint rec_type, const upd_field_t *uf) {
  DBUG_TRACE;

  ref_t &ref = ctx->m_blobref;

  ut_ad(ctx->m_rollback);

  if (uf != nullptr && uf->lob_diffs != nullptr && uf->lob_diffs->size() > 0) {
    /* Undo log contains changes done to the LOB.  This must have
    been a small change done to LOB.  Apply the undo log on the
    LOB.*/
    rollback_from_undolog(ctx, index, uf);
    return;
  }

  /* Our local_mtr needs to use the same mode as the ctx's mtr, as for example
  built-in tables do not expect redo logging, so we should respect that */
  mtr_log_t parent_mtr_log_mode = ctx->get_mtr()->get_log_mode();

  mtr_t local_mtr;
  mtr_start(&local_mtr);
  local_mtr.set_log_mode(parent_mtr_log_mode);

  ctx->x_latch_rec_page(&local_mtr);

#ifdef UNIV_DEBUG
  const ulint lob_size = ref.length();
#endif /* UNIV_DEBUG */

  /* We mark the LOB as partially deleted here, so that if we crash during the
  while() loop below, then during recovery we will know that the remaining LOB
  data should not be read. OTOH we do not ref.set_page_no(FIL_NULL, &local_mtr)
  until we delete all the pages, so that the recovery can use the reference to
  find the remaining parts of the LOB. */
  ref.set_length(0, &local_mtr);

  page_no_t first_page_no = ref.page_no();
  page_id_t page_id(ref.space_id(), first_page_no);
  page_size_t page_size(dict_table_page_size(index->table));

  first_page_t first(&local_mtr, index);
  first.load_x(page_id, page_size);

  flst_base_node_t *flst = first.index_list();
  fil_addr_t node_loc = flst_get_first(flst, &local_mtr);

#ifdef UNIV_DEBUG
  size_t iteration = 0;
#endif /* UNIV_DEBUG */

  while (!fil_addr_is_null(node_loc)) {
    flst_node_t *node = first.addr2ptr_x(node_loc);
    index_entry_t cur_entry(node, &local_mtr, index);
    if (cur_entry.can_rollback(trxid, undo_no)) {
      node_loc = cur_entry.make_old_version_current(index, first);
    } else {
      node_loc = cur_entry.get_next();
    }

#ifdef UNIV_DEBUG
    ++iteration;
    const ulint index_len = flst_get_len(first.index_list());
    DBUG_EXECUTE_IF("lob_rollback_print_index_size", {
      ib::info(ER_IB_LOB_ROLLBACK_INDEX_LEN, ulonglong{trxid},
               ulonglong{undo_no}, ulonglong{index_len}, ulonglong{iteration});
    });
#endif /* UNIV_DEBUG */

    /* Ensure that the parent mtr (btr_mtr) and the child mtr (lob_mtr)
    do not make conflicting modifications. */
    ut_ad(!local_mtr.conflicts_with(ctx->get_mtr()));
    mtr_commit(&local_mtr);

    DBUG_INJECT_CRASH_WITH_LOG_FLUSH("crash_middle_of_lob_rollback",
                                     static_cast<unsigned>(iteration));

    mtr_start(&local_mtr);
    local_mtr.set_log_mode(parent_mtr_log_mode);
    /* We need to reacquire the first page, because in the next iteration of
    the loop we might access not only the FIL_PAGE_TYPE_LOB_INDEX page which
    contains node_loc, but also the FIL_PAGE_TYPE_LOB_FIRST which contains
    the crucial entry index lists base nodes */
    first.load_x(page_id, page_size);
  }

  if (rec_type == TRX_UNDO_INSERT_REC || first.is_empty()) {
    if (dict_index_is_online_ddl(index)) {
      row_log_table_blob_free(index, ref.page_no());
    }

    first.destroy();

  } else {
    ut_ad(first.validate());
#ifdef UNIV_DEBUG
    /* We set ref length to 0 at the beginning of a rollback(), so seeing 0
    indicates a crash might have happened in the middle of a previous rollback()
    and we are now cleaning the left-overs during recovery. */
    if (lob_size != 0) {
      fil_addr_t first_node_loc = flst_get_first(flst, &local_mtr);
      ut_ad(validate_size(lob_size, index, first_node_loc, &local_mtr));
    }
#endif /* UNIV_DEBUG */
  }

  DBUG_INJECT_CRASH_WITH_LOG_FLUSH("crash_almost_end_of_lob_rollback", 0);
  /* We are done with cleaning up index entries for the given version, so now we
  can modify the reference, so that it is no longer reachable. */
  ctx->x_latch_rec_page(&local_mtr);
  ref.set_page_no(FIL_NULL, &local_mtr);
  ut_ad(ref.length() == 0);

  /* Ensure that the parent mtr (btr_mtr) and the child mtr (lob_mtr)
  do not make conflicting modifications. */
  ut_ad(!local_mtr.conflicts_with(ctx->get_mtr()));
  mtr_commit(&local_mtr);

  DBUG_INJECT_CRASH_WITH_LOG_FLUSH("crash_end_of_lob_rollback", 0);
}

/** Rollback modification of a compressed LOB.
@param[in]      ctx             the delete operation context information.
@param[in]      index           clustered index in which LOB is present
@param[in]      trxid           the transaction that is being rolled back.
@param[in]      undo_no         during rollback to savepoint, rollback only
                                up to this undo number.
@param[in]      rec_type        undo record type. */
static void z_rollback(DeleteContext *ctx, dict_index_t *index, trx_id_t trxid,
                       undo_no_t undo_no, ulint rec_type) {
  ut_ad(ctx->m_rollback);

  ref_t &ref = ctx->m_blobref;

  mtr_t local_mtr;
  mtr_start(&local_mtr);

  ctx->x_latch_rec_page(&local_mtr);
  /* We mark the LOB as partially deleted here, so that if we crash during the
  while() loop below, then during recovery we will know that the remaining LOB
  data should not be read. OTOH we do not ref.set_page_no(FIL_NULL, 0)
  until we delete all the pages, so that the recovery can use the reference to
  find the remaining parts of the LOB. */
  ref.set_length(0, nullptr);
  ctx->zblob_write_blobref(ctx->m_field_no, &local_mtr);

  page_no_t first_page_no = ref.page_no();
  page_id_t page_id(ref.space_id(), first_page_no);
  page_size_t page_size(dict_table_page_size(index->table));

  z_first_page_t first(&local_mtr, index);
  first.load_x(page_id, page_size);

  flst_base_node_t *flst = first.index_list();
  fil_addr_t node_loc = flst_get_first(flst, &local_mtr);

#ifdef UNIV_DEBUG
  size_t iteration = 0;
#endif /* UNIV_DEBUG */

  while (!fil_addr_is_null(node_loc)) {
    flst_node_t *node = first.addr2ptr_x(node_loc);
    z_index_entry_t cur_entry(node, &local_mtr, index);

    if (cur_entry.can_rollback(trxid, undo_no)) {
      node_loc = cur_entry.make_old_version_current(index, first);
    } else {
      node_loc = cur_entry.get_next();
    }

#ifdef UNIV_DEBUG
    ++iteration;
    const ulint index_len = flst_get_len(first.index_list());
    DBUG_EXECUTE_IF("lob_rollback_print_index_size", {
      ib::info(ER_IB_LOB_ROLLBACK_INDEX_LEN, ulonglong{trxid},
               ulonglong{undo_no}, ulonglong{index_len}, ulonglong{iteration});
    });
#endif /* UNIV_DEBUG */

    /* Ensure that the parent mtr (btr_mtr) and the child mtr (lob_mtr)
    do not make conflicting modifications. */
    ut_ad(!local_mtr.conflicts_with(ctx->get_mtr()));
    mtr_commit(&local_mtr);

    DBUG_INJECT_CRASH_WITH_LOG_FLUSH("crash_middle_of_lob_rollback",
                                     static_cast<unsigned>(iteration));

    mtr_start(&local_mtr);
    first.load_x(page_id, page_size);
  }

  if (rec_type == TRX_UNDO_INSERT_REC || first.is_empty()) {
    if (dict_index_is_online_ddl(index)) {
      row_log_table_blob_free(index, ref.page_no());
    }

    first.destroy();

  } else {
    ut_ad(first.validate());
  }

  DBUG_INJECT_CRASH_WITH_LOG_FLUSH("crash_almost_end_of_lob_rollback", 0);
  ut_ad(ctx->get_page_zip() != nullptr);
  /* We are done with cleaning up index entries for the given version, so now we
  can modify the reference, so that it is no longer reachable. */
  ref.set_page_no(FIL_NULL, nullptr);
  ut_ad(ref.length() == 0);
  ctx->x_latch_rec_page(&local_mtr);
  ctx->zblob_write_blobref(ctx->m_field_no, &local_mtr);

  mtr_commit(&local_mtr);

  DBUG_INJECT_CRASH_WITH_LOG_FLUSH("crash_end_of_lob_rollback", 0);
}

/** Purge a compressed LOB.
@param[in] ctx                        The delete operation context information.
@param[in] index                Clustered index in which LOB is present
@param[in] trxid                The transaction that is being purged.
@param[in] undo_no              During rollback to savepoint, purge only up to
                                this undo number.
@param[in] rec_type                 Undo record type.
@param[in,out] purge_node       if nullptr, free the complete LOB. Otherwise,
                                save the first page of LOB in this purge node.*/
static void z_purge(DeleteContext *ctx, dict_index_t *index, trx_id_t trxid,
                    undo_no_t undo_no, ulint rec_type,
                    purge_node_t *purge_node) {
  const bool is_rollback = ctx->m_rollback;
  ref_t &ref = ctx->m_blobref;
  const page_size_t page_size(dict_table_page_size(index->table));

  if (is_rollback) {
    z_rollback(ctx, index, trxid, undo_no, rec_type);
    return;
  }

  mtr_t *mtr = ctx->get_mtr();
  page_no_t first_page_no = ref.page_no();
  page_id_t page_id(ref.space_id(), first_page_no);

  /* Hold exclusive access to LOB */
  z_first_page_t btr_first(mtr, index);
  btr_first.load_x(page_id, page_size);

  const trx_id_t last_trx_id = btr_first.get_last_trx_id();
  const undo_no_t last_undo_no = btr_first.get_last_trx_undo_no();

  bool ok_to_free_2 = (rec_type == TRX_UNDO_UPD_EXIST_REC ||
                       rec_type == TRX_UNDO_UPD_DEL_REC) &&
                      !btr_first.can_be_partially_updated() &&
                      (last_trx_id == trxid) && (last_undo_no == undo_no);

  if (rec_type == TRX_UNDO_DEL_MARK_REC || ok_to_free_2) {
    if (dict_index_is_online_ddl(index)) {
      row_log_table_blob_free(index, ref.page_no());
    }

    if (purge_node == nullptr) {
      btr_first.destroy();
    } else {
      /* In this case, the LOB is left with only the first page.  Subsequently
      the LOB first page number in the LOB reference is set to FIL_NULL.
      This means that the LOB page is only accessible via an in-memory
      reference held in the purge node. If a crash happens after the btr_mtr
      commit and before freeing the LOB first page, then the LOB first page
      will be leaked. We need to come up with a mechanism to avoid this leak.*/
      btr_first.make_empty();
      purge_node->add_lob_page(index, page_id);
    }

    if (ctx->get_page_zip() != nullptr) {
      ref.set_page_no(FIL_NULL, nullptr);
      ref.set_length(0, nullptr);
      ctx->zblob_write_blobref(ctx->m_field_no, mtr);
    } else {
      /* Only purge operation should reach this else block. */
      ut_ad(purge_node != nullptr);

      /* Note that page_zip will be NULL in
      row_purge_upd_exist_or_extern(). */
      ref.set_page_no(FIL_NULL, mtr);
      ref.set_length(0, mtr);
    }

    return;
  }

  mtr_t lob_mtr;
  mtr_start(&lob_mtr);
  lob_mtr.set_log_mode(mtr->get_log_mode());

  z_first_page_t first(&lob_mtr, index);
  first.load_x(first_page_no);

  ut_ad(first.validate());

  ut_ad(first.get_page_type() == FIL_PAGE_TYPE_ZLOB_FIRST);

  flst_base_node_t *flst = first.index_list();
  flst_base_node_t *free_list = first.free_list();
  fil_addr_t node_loc = flst_get_first(flst, &lob_mtr);

  z_index_entry_t cur_entry(&lob_mtr, index);

  while (!fil_addr_is_null(node_loc)) {
    flst_node_t *node = first.addr2ptr_x(node_loc);
    cur_entry.reset(node);

    flst_base_node_t *vers = cur_entry.get_versions_list();
    fil_addr_t ver_loc = flst_get_first(vers, &lob_mtr);

    /* Scan the older versions. */
    while (!fil_addr_is_null(ver_loc)) {
      flst_node_t *ver_node = first.addr2ptr_x(ver_loc);
      z_index_entry_t vers_entry(ver_node, &lob_mtr, index);

      if (vers_entry.can_be_purged(trxid, undo_no)) {
        ver_loc = vers_entry.purge_version(index, first, vers, free_list);
      } else {
        ver_loc = vers_entry.get_next();
      }
    }

    node_loc = cur_entry.get_next();
    cur_entry.reset(nullptr);

    ut_ad(!lob_mtr.conflicts_with(mtr));
    mtr_commit(&lob_mtr);
    mtr_start(&lob_mtr);
    lob_mtr.set_log_mode(mtr->get_log_mode());
    first.load_x(first_page_no);
  }

  ut_ad(!lob_mtr.conflicts_with(mtr));
  mtr_commit(&lob_mtr);

  if (ctx->get_page_zip() != nullptr) {
    ref.set_page_no(FIL_NULL, nullptr);
    ref.set_length(0, nullptr);
    ctx->zblob_write_blobref(ctx->m_field_no, mtr);
  } else {
    /* Note that page_zip will be NULL in
    row_purge_upd_exist_or_extern(). */
    ref.set_page_no(FIL_NULL, mtr);
    ref.set_length(0, mtr);
  }
}

void purge(DeleteContext *ctx, dict_index_t *index, trx_id_t trxid,
           undo_no_t undo_no, ulint rec_type, const upd_field_t *uf,
           purge_node_t *purge_node) {
  DBUG_TRACE;
  mtr_t lob_mtr;

  ref_t &ref = ctx->m_blobref;
  mtr_t *mtr = ctx->get_mtr();

#ifdef UNIV_DEBUG
  /* Ensure that the btr_mtr is not restarted. */
  const auto restart_count = mtr->m_restart_count;
  auto guard = create_scope_guard(
      [mtr, restart_count]() { ut_ad(restart_count == mtr->m_restart_count); });
#endif /* UNIV_DEBUG */

  const mtr_log_t log_mode = mtr->get_log_mode();
  const bool is_rollback = ctx->m_rollback;

  /* Update the context object based on the persistent cursor. */
  if (ctx->need_recalc()) {
    ctx->recalc();
  }

  if (ref.is_null()) {
    /* In the rollback, we may encounter a clustered index
    record with some unwritten off-page columns. There is
    nothing to free then. */
    ut_a(ctx->m_rollback);
    return;
  }

  /* In case ref.length()==0, the LOB might be partially deleted (for example
  a crash has happened during a rollback() of insert operation) and we want
  to make sure we delete the remaining parts of the LOB so we don't exit here.
  OTOH, if the reason for ref.length()==0, is because of ref.is_null_relaxed(),
  then we should exit.*/
  if (!ref.is_owner() || ref.page_no() == FIL_NULL || ref.is_null_relaxed() ||
      (ctx->m_rollback && ref.is_inherited())) {
    return;
  }

  if (!is_rollback && uf != nullptr && uf->lob_diffs != nullptr &&
      uf->lob_diffs->size() > 0) {
    /* Undo record contains LOB diffs.  So purge shouldn't look
    at the LOB. */
    return;
  }

  if (!ctx->is_ref_valid()) {
    /* The LOB reference has changed.  Don't proceed. */
    return;
  }

  /* If rec_type is 0, it is not the purge operation. */
  if (!is_rollback && rec_type != 0 && !ctx->is_delete_marked()) {
    /* This is the purge operation. The delete marked clustered record has been
    reused. Purge shouldn't proceed. */
    return;
  }

  space_id_t space_id = ref.space_id();
  ut_ad(space_id == index->space_id());
  page_no_t first_page_no = ref.page_no();
  page_id_t page_id(space_id, first_page_no);
  page_size_t page_size(dict_table_page_size(index->table));

  page_type_t page_type =
      first_page_t::get_page_type(index, page_id, page_size);

  if (page_type == FIL_PAGE_TYPE_ZBLOB ||
      page_type == FIL_PAGE_TYPE_ZBLOB2 || /* Partially purged ZBLOB */
      page_type == FIL_PAGE_TYPE_BLOB || page_type == FIL_PAGE_SDI_BLOB ||
      page_type == FIL_PAGE_SDI_ZBLOB) {
    lob::Deleter free_blob(*ctx);
    free_blob.destroy();
    return;
  }

  if (page_type == FIL_PAGE_TYPE_ZLOB_FIRST) {
    z_purge(ctx, index, trxid, undo_no, rec_type, purge_node);
    return;
  }

  ut_a(page_type == FIL_PAGE_TYPE_LOB_FIRST);

  if (is_rollback) {
    rollback(ctx, index, trxid, undo_no, rec_type, uf);
    return;
  }

  /* Hold exclusive access to LOB */
  first_page_t btr_first(mtr, index);
  btr_first.load_x(page_id, page_size);
  const trx_id_t last_trx_id = btr_first.get_last_trx_id();
  const undo_no_t last_undo_no = btr_first.get_last_trx_undo_no();

  /* Check if the LOB has to be destroyed. */
  bool ok_to_free = (rec_type == TRX_UNDO_UPD_EXIST_REC ||
                     rec_type == TRX_UNDO_UPD_DEL_REC) &&
                    !btr_first.can_be_partially_updated() &&
                    (last_trx_id == trxid) && (last_undo_no == undo_no);

  if (rec_type == TRX_UNDO_DEL_MARK_REC || ok_to_free) {
    ut_ad(btr_first.get_page_type() == FIL_PAGE_TYPE_LOB_FIRST);

    if (dict_index_is_online_ddl(index)) {
      row_log_table_blob_free(index, ref.page_no());
    }
    if (purge_node == nullptr) {
      /* During rollback, when a record has multiple blobs, freeing the first
      page of one blob in btr_mtr and then attempting to free the next blob
      in a local_mtr will cause mtr conflict between btr_mtr and local_mtr.
      To avoid this problem, free the first page of blobs later.  */
      btr_first.make_empty();
      ctx->add_lob_block(btr_first.get_block());
    } else {
      /* In this case, the LOB is left with only the first page.  Subsequently
      the LOB first page number in the LOB reference is set to FIL_NULL.
      This means that the LOB page is only accessible via an in-memory
      reference held in the purge node. If a crash happens after the btr_mtr
      commit and before freeing the LOB first page, then the LOB first page
      will be leaked. We need to come up with a mechanism to avoid this leak.*/
      btr_first.make_empty();
      purge_node->add_lob_page(index, page_id);
    }

    ref.set_page_no(FIL_NULL, mtr);
    ref.set_length(0, mtr);

    return;
  }

  mtr_start(&lob_mtr);
  lob_mtr.set_log_mode(log_mode);

  /* The current entry - it is the latest version. */
  index_entry_t cur_entry(&lob_mtr, index);
  first_page_t first(&lob_mtr, index);
  first.load_x(page_id, page_size);

  flst_base_node_t *flst = first.index_list();
  flst_base_node_t *free_list = first.free_list();
  fil_addr_t node_loc = flst_get_first(flst, &lob_mtr);

  while (!fil_addr_is_null(node_loc)) {
    flst_node_t *node = first.addr2ptr_x(node_loc);
    cur_entry.reset(node);

    flst_base_node_t *vers = cur_entry.get_versions_list();
    fil_addr_t ver_loc = flst_get_first(vers, &lob_mtr);

    /* Scan the older versions. */
    while (!fil_addr_is_null(ver_loc)) {
      flst_node_t *ver_node = first.addr2ptr_x(ver_loc);
      index_entry_t vers_entry(ver_node, &lob_mtr, index);

      if (vers_entry.can_be_purged(trxid, undo_no)) {
        ver_loc = vers_entry.purge_version(index, vers, free_list);
      } else {
        ver_loc = vers_entry.get_next();
      }
    }

    node_loc = cur_entry.get_next();
    cur_entry.reset(nullptr);

    /* Ensure that the parent mtr (btr_mtr) and the child mtr (lob_mtr)
    do not make conflicting modifications. */
    ut_ad(!lob_mtr.conflicts_with(mtr));
    mtr_commit(&lob_mtr);
    mtr_start(&lob_mtr);
    lob_mtr.set_log_mode(log_mode);
    first.load_x(page_id, page_size);
  }

  /* Ensure that the parent mtr (btr_mtr) and the child mtr (lob_mtr)
  do not make conflicting modifications. */
  ut_ad(!lob_mtr.conflicts_with(mtr));
  mtr_commit(&lob_mtr);

  ref.set_page_no(FIL_NULL, mtr);
  ref.set_length(0, mtr);
}

} /* namespace lob */
