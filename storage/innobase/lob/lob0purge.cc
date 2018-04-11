/*****************************************************************************

Copyright (c) 2016, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

#include "lob0del.h"
#include "lob0first.h"
#include "lob0index.h"
#include "lob0inf.h"
#include "lob0lob.h"
#include "row0upd.h"
#include "trx0purge.h"
#include "trx0rec.h"
#include "zlob0first.h"
#include "zlob0index.h"
#include "zlob0read.h"

namespace lob {

/** Rollback from undo log information.
@param[in]	ctx	the delete operation context.
@param[in]	index	the clustered index to which LOB belongs.
@param[in]	ref	the LOB reference object.
@param[in]	uf	the update vector of concerned field. */
static void rollback_from_undolog(DeleteContext *ctx, dict_index_t *index,
                                  ref_t &ref, const upd_field_t *uf) {
  DBUG_ENTER("rollback_from_undolog");

  trx_t *trx = nullptr;

  dberr_t err = apply_undolog(ctx->get_mtr(), trx, index, ref, uf);
  ut_a(err == DB_SUCCESS);

  DBUG_VOID_RETURN;
}

/** Rollback modification of a uncompressed LOB.
@param[in]	ctx		the delete operation context information.
@param[in]	index		clustered index in which LOB is present
@param[in]	trxid		the transaction that is being rolled back.
@param[in]	undo_no		during rollback to savepoint, rollback only
                                upto this undo number.
@param[in]	ref		reference to LOB that is being rolled back.
@param[in]	rec_type	undo record type.
@param[in]	uf		update vector of the concerned field. */
static void rollback(DeleteContext *ctx, dict_index_t *index, trx_id_t trxid,
                     undo_no_t undo_no, ref_t &ref, ulint rec_type,
                     const upd_field_t *uf) {
  DBUG_ENTER("lob::rollback");

  ut_ad(ctx->m_rollback);

  if (uf != nullptr && uf->lob_diffs != nullptr && uf->lob_diffs->size() > 0) {
    /* Undo log contains changes done to the LOB.  This must have
    been a small change done to LOB.  Apply the undo log on the
    LOB.*/
    rollback_from_undolog(ctx, index, ref, uf);
    DBUG_VOID_RETURN;
  }

  mtr_t *mtr = ctx->get_mtr();

  page_no_t first_page_no = ref.page_no();
  page_id_t page_id(ref.space_id(), first_page_no);
  page_size_t page_size(dict_table_page_size(index->table));

  first_page_t first(mtr, index);
  first.load_x(page_id, page_size);

  flst_base_node_t *flst = first.index_list();
  fil_addr_t node_loc = flst_get_first(flst, mtr);

  while (!fil_addr_is_null(node_loc)) {
    flst_node_t *node = first.addr2ptr_x(node_loc);
    index_entry_t cur_entry(node, mtr, index);

    if (cur_entry.can_rollback(trxid, undo_no)) {
      node_loc = cur_entry.make_old_version_current(index, trxid, first);

    } else {
      node_loc = cur_entry.get_next();
    }
  }

  if (rec_type == TRX_UNDO_INSERT_REC || first.is_empty()) {
    if (dict_index_is_online_ddl(index)) {
      row_log_table_blob_free(index, ref.page_no());
    }

    first.free_all_index_pages();
    first.dealloc();

  } else {
#ifdef UNIV_DEBUG
    const ulint lob_size = ref.length();
    fil_addr_t first_node_loc = flst_get_first(flst, mtr);
    ut_ad(validate_size(lob_size, index, first_node_loc, mtr));
#endif /* UNIV_DEBUG */
  }

  ref.set_page_no(FIL_NULL, mtr);
  ref.set_length(0, mtr);

  DBUG_EXECUTE_IF("crash_endof_lob_rollback", DBUG_SUICIDE(););
  DBUG_VOID_RETURN;
}

/** Rollback modification of a compressed LOB.
@param[in]	ctx		the delete operation context information.
@param[in]	index		clustered index in which LOB is present
@param[in]	trxid		the transaction that is being rolled back.
@param[in]	undo_no		during rollback to savepoint, rollback only
                                upto this undo number.
@param[in]	ref		reference to LOB that is purged.
@param[in]	rec_type	undo record type. */
static void z_rollback(DeleteContext *ctx, dict_index_t *index, trx_id_t trxid,
                       undo_no_t undo_no, ref_t &ref, ulint rec_type) {
  ut_ad(ctx->m_rollback);
  const ulint commit_freq = 1;
  ulint n_entries = 0;

  mtr_t local_mtr;
  mtr_start(&local_mtr);

  page_no_t first_page_no = ref.page_no();
  page_id_t page_id(ref.space_id(), first_page_no);
  page_size_t page_size(dict_table_page_size(index->table));

  z_first_page_t first(&local_mtr, index);
  first.load_x(page_id, page_size);

  flst_base_node_t *flst = first.index_list();
  fil_addr_t node_loc = flst_get_first(flst, &local_mtr);

  while (!fil_addr_is_null(node_loc)) {
    flst_node_t *node = first.addr2ptr_x(node_loc);
    z_index_entry_t cur_entry(node, &local_mtr, index);

    if (cur_entry.can_rollback(trxid, undo_no)) {
      node_loc = cur_entry.make_old_version_current(index, trxid, first);

      n_entries++;

    } else {
      node_loc = cur_entry.get_next();
    }

    if (n_entries % commit_freq == 0) {
      mtr_commit(&local_mtr);
      mtr_start(&local_mtr);
      first.load_x(page_id, page_size);
    }
  }

  if (rec_type == TRX_UNDO_INSERT_REC || first.is_empty()) {
    if (dict_index_is_online_ddl(index)) {
      row_log_table_blob_free(index, ref.page_no());
    }

    /* Ensure that the btr mtr is not holding any x-latch on the first page of
     * LOB. */
    ut_ad(!mtr_is_block_fix(ctx->get_mtr(), first.get_block(),
                            MTR_MEMO_PAGE_X_FIX, index->table));

    first.free_all_frag_node_pages();
    first.free_all_index_pages();
    first.dealloc();

  } else {
    ut_ad(first.validate());
  }

  ut_ad(ctx->get_page_zip() != nullptr);
  ref.set_page_no(FIL_NULL, 0);
  ref.set_length(0, 0);
  ctx->x_latch_rec_page(&local_mtr);
  ctx->zblob_write_blobref(ctx->m_field_no, &local_mtr);

  mtr_commit(&local_mtr);

  DBUG_EXECUTE_IF("crash_endof_zlob_rollback", DBUG_SUICIDE(););
}

/** Purge a compressed LOB.
@param[in]	ctx		the delete operation context information.
@param[in]	index		clustered index in which LOB is present
@param[in]	trxid		the transaction that is being purged.
@param[in]	undo_no		during rollback to savepoint, purge only upto
                                this undo number.
@param[in]	ref		reference to LOB that is purged.
@param[in]	rec_type	undo record type. */
static void z_purge(DeleteContext *ctx, dict_index_t *index, trx_id_t trxid,
                    undo_no_t undo_no, ref_t &ref, ulint rec_type) {
  const bool is_rollback = ctx->m_rollback;

  if (is_rollback) {
    z_rollback(ctx, index, trxid, undo_no, ref, rec_type);
    return;
  }

  mtr_t *mtr = ctx->get_mtr();

  page_no_t first_page_no = ref.page_no();
  page_id_t page_id(ref.space_id(), first_page_no);

  z_first_page_t first(mtr, index);
  first.load_x(first_page_no);

  ut_ad(first.validate());

  trx_id_t last_trx_id = first.get_last_trx_id();
  undo_no_t last_undo_no = first.get_last_trx_undo_no();
  ut_ad(first.get_page_type() == FIL_PAGE_TYPE_ZLOB_FIRST);

  flst_base_node_t *flst = first.index_list();
  flst_base_node_t *free_list = first.free_list();
  fil_addr_t node_loc = flst_get_first(flst, mtr);

  z_index_entry_t cur_entry(mtr, index);

  while (!fil_addr_is_null(node_loc)) {
    flst_node_t *node = first.addr2ptr_x(node_loc);
    cur_entry.reset(node);

    flst_base_node_t *vers = cur_entry.get_versions_list();
    fil_addr_t ver_loc = flst_get_first(vers, mtr);

    /* Scan the older versions. */
    while (!fil_addr_is_null(ver_loc)) {
      flst_node_t *ver_node = first.addr2ptr_x(ver_loc);
      z_index_entry_t vers_entry(ver_node, mtr, index);

      if (vers_entry.can_be_purged(trxid, undo_no)) {
        ver_loc =
            vers_entry.purge_version(index, trxid, first, vers, free_list);
      } else {
        ver_loc = vers_entry.get_next();
      }
    }

    node_loc = cur_entry.get_next();
    cur_entry.reset(nullptr);
  }

  bool ok_to_free_2 = (rec_type == TRX_UNDO_UPD_EXIST_REC) &&
                      !first.can_be_partially_updated() &&
                      (last_trx_id == trxid) && (last_undo_no == undo_no);

  if (rec_type == TRX_UNDO_DEL_MARK_REC || ok_to_free_2) {
    if (dict_index_is_online_ddl(index)) {
      row_log_table_blob_free(index, ref.page_no());
    }

    first.free_all_frag_node_pages();
    first.free_all_index_pages();
    first.dealloc();

  } else {
    ut_ad(first.validate());
  }

  if (ctx->get_page_zip() != nullptr) {
    ref.set_page_no(FIL_NULL, 0);
    ref.set_length(0, 0);
    ctx->zblob_write_blobref(ctx->m_field_no, mtr);
  } else {
    /* Note that page_zip will be NULL in
    row_purge_upd_exist_or_extern(). */
    ref.set_page_no(FIL_NULL, mtr);
    ref.set_length(0, mtr);
  }
}

/** Purge an uncompressed LOB.
@param[in]	ctx		the delete operation context information.
@param[in]	index		clustered index in which LOB is present
@param[in]	trxid		the transaction that is being purged.
@param[in]	undo_no		during rollback to savepoint, purge only upto
                                this undo number.
@param[in]	ref		reference to LOB that is purged.
@param[in]	rec_type	undo record type.
@param[in]	uf		the update vector for the field. */
void purge(DeleteContext *ctx, dict_index_t *index, trx_id_t trxid,
           undo_no_t undo_no, ref_t ref, ulint rec_type,
           const upd_field_t *uf) {
  DBUG_ENTER("lob::purge");

  mtr_t *mtr = ctx->get_mtr();
  const bool is_rollback = ctx->m_rollback;

  if (ref.is_null()) {
    /* In the rollback, we may encounter a clustered index
    record with some unwritten off-page columns. There is
    nothing to free then. */
    ut_a(ctx->m_rollback);
    DBUG_VOID_RETURN;
  }

  if (!ref.is_owner() || ref.page_no() == FIL_NULL || ref.length() == 0 ||
      (ctx->m_rollback && ref.is_inherited())) {
    DBUG_VOID_RETURN;
  }

  if (!is_rollback && uf != nullptr && uf->lob_diffs != nullptr &&
      uf->lob_diffs->size() > 0) {
    /* Undo record contains LOB diffs.  So purge shouldn't look
    at the LOB. */
    DBUG_VOID_RETURN;
  }

  space_id_t space_id = ref.space_id();

  /* The current entry - it is the latest version. */
  index_entry_t cur_entry(mtr, index);

  page_no_t first_page_no = ref.page_no();
  page_id_t page_id(space_id, first_page_no);
  page_size_t page_size(dict_table_page_size(index->table));
  page_type_t page_type =
      first_page_t::get_page_type(index, page_id, page_size);

  if (page_type == FIL_PAGE_TYPE_ZBLOB || page_type == FIL_PAGE_TYPE_BLOB ||
      page_type == FIL_PAGE_SDI_BLOB || page_type == FIL_PAGE_SDI_ZBLOB) {
    lob::Deleter free_blob(*ctx);
    free_blob.destroy();
    DBUG_VOID_RETURN;
  }

  if (page_type == FIL_PAGE_TYPE_ZLOB_FIRST) {
    z_purge(ctx, index, trxid, undo_no, ref, rec_type);
    DBUG_VOID_RETURN;
  }

  first_page_t first(mtr, index);
  first.load_x(page_id, page_size);

  ut_a(page_type == FIL_PAGE_TYPE_LOB_FIRST);

  if (is_rollback) {
    rollback(ctx, index, trxid, undo_no, ref, rec_type, uf);
    DBUG_VOID_RETURN;
  }

  trx_id_t last_trx_id = first.get_last_trx_id();
  undo_no_t last_undo_no = first.get_last_trx_undo_no();

  flst_base_node_t *flst = first.index_list();
  flst_base_node_t *free_list = first.free_list();
  fil_addr_t node_loc = flst_get_first(flst, mtr);

  while (!fil_addr_is_null(node_loc)) {
    flst_node_t *node = first.addr2ptr_x(node_loc);
    cur_entry.reset(node);

    flst_base_node_t *vers = cur_entry.get_versions_list();
    fil_addr_t ver_loc = flst_get_first(vers, mtr);

    /* Scan the older versions. */
    while (!fil_addr_is_null(ver_loc)) {
      flst_node_t *ver_node = first.addr2ptr_x(ver_loc);
      index_entry_t vers_entry(ver_node, mtr, index);

      if (vers_entry.can_be_purged(trxid, undo_no)) {
        ver_loc = vers_entry.purge_version(index, trxid, vers, free_list);
      } else {
        ver_loc = vers_entry.get_next();
      }
    }

    node_loc = cur_entry.get_next();
    cur_entry.reset(nullptr);
  }

  bool ok_to_free = (rec_type == TRX_UNDO_UPD_EXIST_REC) &&
                    !first.can_be_partially_updated() &&
                    (last_trx_id == trxid) && (last_undo_no == undo_no);

  if (rec_type == TRX_UNDO_DEL_MARK_REC || ok_to_free) {
    ut_ad(first.get_page_type() == FIL_PAGE_TYPE_LOB_FIRST);

    if (dict_index_is_online_ddl(index)) {
      row_log_table_blob_free(index, ref.page_no());
    }

    first.free_all_index_pages();
    first.dealloc();
  }

  ref.set_page_no(FIL_NULL, mtr);
  ref.set_length(0, mtr);

  DBUG_VOID_RETURN;
}

}; /* namespace lob */
