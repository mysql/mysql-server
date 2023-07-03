/*****************************************************************************

Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#include <table.h>
#include <memory>
#include "db0err.h"
#include "field.h"
#include "fil0fil.h"
#include "fut0lst.h"
#include "lob0impl.h"
#include "lob0lob.h"
#include "row0upd.h"
#include "trx0trx.h"
#include "zlob0first.h"
#include "zlob0read.h"

namespace lob {

/** Replace a large object (LOB) with the given new data.
@param[in]      ctx             replace operation context.
@param[in]      trx             the transaction that is doing the read.
@param[in]      index           the clust index that contains the LOB.
@param[in]      ref             the LOB reference identifying the LOB.
@param[in]      first_page      the first page of the LOB.
@param[in]      offset          replace the LOB from the given offset.
@param[in]      len             the length of LOB data that needs to be
                                replaced.
@param[in]      buf             the buffer (owned by caller) with new data
                                (len bytes).
@return DB_SUCCESS on success, error code on failure. */
static dberr_t z_replace(InsertContext &ctx, trx_t *trx, dict_index_t *index,
                         ref_t ref, z_first_page_t &first_page, ulint offset,
                         ulint len, byte *buf);

#ifdef UNIV_DEBUG
/** Print an information message in the server log file, informing
that the ZLOB partial update feature code is hit.
@param[in]      uf      the update field information
@param[in]      index   index where partial update happens.*/
static void z_print_partial_update_hit(upd_field_t *uf, dict_index_t *index) {
  ib::info(ER_IB_MSG_633) << "ZLOB partial update of field=("
                          << uf->mysql_field->field_name << ") on index=("
                          << index->name << ") in table=(" << index->table_name
                          << ")";
}
#endif /* UNIV_DEBUG */

/** Update a portion of the given LOB.
@param[in]      ctx             update operation context information.
@param[in]      trx             the transaction that is doing the modification.
@param[in]      index           the clustered index containing the LOB.
@param[in]      upd             update vector
@param[in]      field_no        the LOB field number
@param[in]      blobref         LOB reference stored in clust record.
@return DB_SUCCESS on success, error code on failure. */
dberr_t z_update(InsertContext &ctx, trx_t *trx, dict_index_t *index,
                 const upd_t *upd, ulint field_no, ref_t blobref) {
  DBUG_TRACE;
  dberr_t err = DB_SUCCESS;
  mtr_t *mtr = ctx.get_mtr();

  const Binary_diff_vector *bdiff_vector =
      upd->get_binary_diff_by_field_no(field_no);

  upd_field_t *uf = upd->get_field_by_field_no(field_no, index);

#ifdef UNIV_DEBUG
  /* Print information on server error log file, which can be
  used to confirm if InnoDB did partial update or not. */
  DBUG_EXECUTE_IF("zlob_print_partial_update_hit",
                  z_print_partial_update_hit(uf, index););
#endif /* UNIV_DEBUG */
  page_no_t first_page_no = blobref.page_no();
  space_id_t space_id = blobref.space_id();
  const page_size_t page_size = dict_table_page_size(index->table);
  const page_id_t first_page_id(space_id, first_page_no);

  z_first_page_t first_page(mtr, index);
  first_page.load_x(first_page_id, page_size);

  ut_ad(first_page.get_page_type() == FIL_PAGE_TYPE_ZLOB_FIRST);

  const uint32_t lob_version = first_page.incr_lob_version();

  for (Binary_diff_vector::const_iterator iter = bdiff_vector->begin();
       iter != bdiff_vector->end(); ++iter) {
    const Binary_diff *bdiff = iter;

    err = z_replace(ctx, trx, index, blobref, first_page, bdiff->offset(),
                    bdiff->length(), (byte *)bdiff->new_data(uf->mysql_field));

    if (err != DB_SUCCESS) {
      break;
    }
  }

  blobref.set_offset(lob_version, nullptr);

  if (!ctx.is_bulk()) {
    ctx.zblob_write_blobref(field_no, ctx.m_mtr);
  }

  return err;
}

/** Find the location of the given offset within LOB.
@param[in]      index           The index where LOB is located.
@param[in]      node_loc        The location of first page.
@param[in,out]  offset          The requested offset.
@param[in]      mtr             Mini-transaction context.
@return the file address of requested offset or fil_addr_null. */
fil_addr_t z_find_offset(dict_index_t *index, fil_addr_t node_loc,
                         ulint &offset, mtr_t *mtr) {
  space_id_t space = dict_index_get_space(index);
  const page_size_t page_size = dict_table_page_size(index->table);

  while (!fil_addr_is_null(node_loc)) {
    flst_node_t *node =
        fut_get_ptr(space, page_size, node_loc, RW_X_LATCH, mtr);

    z_index_entry_t entry(node, mtr, index);

    /* Get the amount of data */
    ulint data_len = entry.get_data_len();

    if (offset < data_len) {
      break;
    }

    offset -= data_len;

    /* The next node should not be the same as the current node. */
    ut_ad(!node_loc.is_equal(entry.get_next()));

    node_loc = flst_get_next_addr(node, mtr);
  }

  return (node_loc);
}

/** Replace a large object (LOB) with the given new data.
@param[in]      ctx             replace operation context.
@param[in]      trx             the transaction that is doing the read.
@param[in]      index           the clust index that contains the LOB.
@param[in]      ref             the LOB reference identifying the LOB.
@param[in]      first_page      the first page of the LOB.
@param[in]      offset          replace the LOB from the given offset.
@param[in]      len             the length of LOB data that needs to be
                                replaced.
@param[in]      buf             the buffer (owned by caller) with new data
                                (len bytes).
@return DB_SUCCESS on success, error code on failure. */
static dberr_t z_replace(InsertContext &ctx, trx_t *trx, dict_index_t *index,
                         ref_t ref, z_first_page_t &first_page, ulint offset,
                         ulint len, byte *buf) {
  DBUG_TRACE;
  dberr_t ret(DB_SUCCESS);
  trx_id_t trxid = (trx == nullptr) ? 0 : trx->id;
  const undo_no_t undo_no = (trx == nullptr ? 0 : trx->undo_no - 1);
  const uint32_t lob_version = first_page.get_lob_version();

  ut_ad(offset < ref.length());

  ut_ad(dict_table_is_comp(index->table));

#ifdef LOB_DEBUG
  std::cout << "thread=" << std::this_thread::get_id()
            << ", lob::z_replace(): table=" << index->table->name
            << ", ref=" << ref << ", trx->id=" << trx->id << std::endl;
#endif /* LOB_DEBUG */

  mtr_t *mtr = ctx.get_mtr();

  page_no_t first_page_no = ref.page_no();
  space_id_t space_id = ref.space_id();
  const page_id_t first_page_id(space_id, first_page_no);

  first_page.set_last_trx_id(trx->id);
  first_page.set_last_trx_undo_no(undo_no);

  ut_ad(first_page.get_page_type() == FIL_PAGE_TYPE_ZLOB_FIRST);

  flst_base_node_t *base_node = first_page.index_list();
  fil_addr_t node_loc = flst_get_first(base_node, mtr);

  ulint yet_to_skip = offset;
  node_loc = z_find_offset(index, node_loc, yet_to_skip, mtr);

  ut_ad(!node_loc.is_null());

  /* The current entry - it is the latest version. */
  z_index_entry_t cur_entry(mtr, index);
  z_index_entry_t new_entry(mtr, index);

  /* The cur_entry points to the chunk that needs to be
  partially replaced. */
  std::unique_ptr<byte[]> tmp(new (std::nothrow) byte[Z_CHUNK_SIZE]);

  if (tmp == nullptr) {
    return (DB_OUT_OF_MEMORY);
  }

  byte *chunk = tmp.get();
  ut_ad(yet_to_skip < Z_CHUNK_SIZE);

  byte *ptr = chunk;
  byte *from_ptr = buf;
  ulint replace_len = len; /* bytes remaining to be replaced. */

  /* Replace remaining. */
  while (replace_len > 0 && !fil_addr_is_null(node_loc)) {
    cur_entry.load_x(node_loc);

    ulint size = cur_entry.get_data_len();
    ut_ad(size > yet_to_skip);
    ulint avail = size - yet_to_skip;

    if (yet_to_skip > 0 || replace_len < avail) {
      /* Only partial chunk is to be replaced. Read old
      data. */
      ulint read_len = size;
      ptr = chunk;

      ulint len1 = z_read_chunk(index, cur_entry, 0, read_len, ptr, mtr);

      ut_ad(len1 == cur_entry.get_data_len());
      ut_ad(read_len == 0);
      ut_ad(len1 == size);

      ptr = chunk;
      ptr += yet_to_skip;
      const ulint can_replace = (replace_len > avail ? avail : replace_len);
      memcpy(ptr, from_ptr, can_replace);
      replace_len -= can_replace;
      from_ptr += can_replace;
      ptr += can_replace;

#ifdef UNIV_DEBUG
      if (can_replace < avail) {
        const ulint rest_of_data = avail - can_replace;
        ut_ad(size == (yet_to_skip + can_replace + rest_of_data));
      } else {
        ut_ad(size == (yet_to_skip + can_replace));
      }
#endif /* UNIV_DEBUG */

      ut_ad(first_page.get_page_type() == FIL_PAGE_TYPE_ZLOB_FIRST);

      /* Chunk now contains new data to be inserted. */
      ret = z_insert_chunk(index, first_page, trx, chunk, len1, &new_entry, mtr,
                           false);

      if (ret != DB_SUCCESS) {
        return (ret);
      }

      cur_entry.insert_after(base_node, new_entry);
      cur_entry.remove(base_node);
      cur_entry.set_trx_id_modifier(trxid);
      cur_entry.set_trx_undo_no_modifier(undo_no);
      new_entry.set_old_version(cur_entry);
      new_entry.set_lob_version(lob_version);
      yet_to_skip = 0;

    } else {
      ut_ad(yet_to_skip == 0);
      ut_ad(replace_len >= avail);
      ut_ad(avail == size);

      /* Full chunk is to be replaced. No need to read
      old data. */
      ret = z_insert_chunk(index, first_page, trx, from_ptr, size, &new_entry,
                           mtr, false);

      if (ret != DB_SUCCESS) {
        return (ret);
      }

      ut_ad(new_entry.get_trx_id() == trx->id);

      from_ptr += size;
      ut_a(size <= replace_len);
      replace_len -= size;

      cur_entry.set_trx_id_modifier(trxid);
      cur_entry.set_trx_undo_no_modifier(undo_no);
      cur_entry.insert_after(base_node, new_entry);
      cur_entry.remove(base_node);
      new_entry.set_old_version(cur_entry);
      new_entry.set_lob_version(lob_version);
    }

    node_loc = new_entry.get_next();
    new_entry.reset(nullptr);
    cur_entry.reset(nullptr);
  }

  ut_ad(replace_len == 0);
  ut_ad(first_page.get_page_type() == FIL_PAGE_TYPE_ZLOB_FIRST);

#ifdef LOB_DEBUG
  first_page.print_index_entries(std::cout);
  ut_ad(first_page.validate());
#endif /* LOB_DEBUG */

  return ret;
}

} /* namespace lob */
