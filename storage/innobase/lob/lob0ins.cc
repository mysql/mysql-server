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

#include "lob0ins.h"
#include "buf0buf.h"

namespace lob {

/** Allocate one BLOB page.
@return the allocated block of the BLOB page. */
buf_block_t *BaseInserter::alloc_blob_page() {
  ulint r_extents;
  mtr_t mtr_bulk;
  mtr_t *alloc_mtr;

  ut_ad(fsp_check_tablespace_size(m_ctx->space()));

  if (m_ctx->is_bulk()) {
    mtr_start(&mtr_bulk);
    alloc_mtr = &mtr_bulk;
  } else {
    alloc_mtr = &m_blob_mtr;
  }

  page_no_t hint_page_no = m_prev_page_no + 1;

  if (!fsp_reserve_free_extents(&r_extents, m_ctx->space(), 1, FSP_BLOB,
                                alloc_mtr, 1)) {
    alloc_mtr->commit();
    m_err = DB_OUT_OF_FILE_SPACE;
    return (nullptr);
  }

  m_cur_blob_block = btr_page_alloc(m_ctx->index(), hint_page_no, FSP_NO_DIR, 0,
                                    alloc_mtr, &m_blob_mtr);

  fil_space_release_free_extents(m_ctx->space(), r_extents);

  if (m_ctx->is_bulk()) {
    alloc_mtr->commit();
  }

  m_cur_blob_page_no = page_get_page_no(buf_block_get_frame(m_cur_blob_block));

  return (m_cur_blob_block);
}

/** Get the previous BLOB page block.  This will return a BLOB block.
It should not be called for the first BLOB page, because it will not
have a previous BLOB page.
@return the previous BLOB block. */
buf_block_t *BaseInserter::get_previous_blob_block() {
  DBUG_TRACE;

  DBUG_LOG("lob", "m_prev_page_no=" << m_prev_page_no);
  ut_ad(m_prev_page_no != m_ctx->get_page_no());

  space_id_t space_id = m_ctx->space();
  buf_block_t *rec_block = m_ctx->block();

  buf_block_t *prev_block =
      buf_page_get(page_id_t(space_id, m_prev_page_no), rec_block->page.size,
                   RW_X_LATCH, UT_LOCATION_HERE, &m_blob_mtr);

  buf_block_dbg_add_level(prev_block, SYNC_EXTERN_STORAGE);

  return prev_block;
}

/** Get the previous BLOB page frame.  This will return a BLOB page.
It should not be called for the first BLOB page, because it will not
have a previous BLOB page.
@return the previous BLOB page frame. */
page_t *BaseInserter::get_previous_blob_page() {
  buf_block_t *prev_block = get_previous_blob_block();
  return (buf_block_get_frame(prev_block));
}

/** Write all the BLOBs of the clustered index record.
@return DB_SUCCESS on success, error code on failure. */
dberr_t Inserter::write() {
  /* Loop through each blob field of the record and write one blob
  at a time. */
  for (ulint i = 0; i < m_ctx->get_big_rec_vec_size() && is_ok(); i++) {
    ut_d(m_dir.clear(););
    m_err = write_one_blob(i);

    DBUG_EXECUTE_IF("btr_store_big_rec_extern", m_err = DB_OUT_OF_FILE_SPACE;);
  }

  ut_ad(m_err != DB_SUCCESS || m_ctx->are_all_blobrefs_valid());

  return (m_err);
}

/** Write one small blob field data.
@param[in]      blob_j  the blob field number
@return DB_SUCCESS on success, error code on failure. */
dberr_t Inserter::write_one_small_blob(size_t blob_j) {
  const big_rec_t *vec = m_ctx->get_big_rec_vec();
  big_rec_field_t &field = vec->fields[blob_j];

  m_err = write_first_page(field);

  for (ulint nth_blob_page = 1; is_ok() && m_remaining > 0; ++nth_blob_page) {
    m_err = write_single_blob_page(field, nth_blob_page);
  }

  m_ctx->make_nth_extern(field.field_no);

  ut_ad(m_remaining == 0);

  return (m_err);
}

/** Write one blob field data.
@param[in]      blob_j  the blob field number
@return DB_SUCCESS on success, error code on failure. */
dberr_t Inserter::write_one_blob(size_t blob_j) {
  const big_rec_t *vec = m_ctx->get_big_rec_vec();
  big_rec_field_t &field = vec->fields[blob_j];

  m_ctx->check_redolog();

  m_err = write_first_page(field);

  for (ulint nth_blob_page = 1; is_ok() && m_remaining > 0; ++nth_blob_page) {
    const ulint commit_freq = 4;

    if (nth_blob_page % commit_freq == 0) {
      m_ctx->check_redolog();
    }

    m_err = write_single_blob_page(field, nth_blob_page);
  }

  m_ctx->make_nth_extern(field.field_no);

  ut_ad(m_remaining == 0);

  return (m_err);
}

/** Make the current page as next page of previous page.  In other
words, make the page m_cur_blob_page_no as the next page of page
m_prev_page_no. */
void Inserter::set_page_next() {
  page_t *prev_page = get_previous_blob_page();

  mlog_write_ulint(prev_page + FIL_PAGE_DATA + LOB_HDR_NEXT_PAGE_NO,
                   m_cur_blob_page_no, MLOG_4BYTES, &m_blob_mtr);
}

dberr_t Inserter::write_first_page(big_rec_field_t &field) {
  buf_block_t *rec_block = m_ctx->block();
  mtr_t *mtr = start_blob_mtr();

  buf_page_get(rec_block->page.id, rec_block->page.size, RW_X_LATCH,
               UT_LOCATION_HERE, mtr);

  alloc_blob_page();

  if (dict_index_is_online_ddl(m_ctx->index())) {
    row_log_table_blob_alloc(m_ctx->index(), m_cur_blob_page_no);
  }

  log_page_type();

  m_remaining = field.len;
  write_into_single_page(field);

  const ulint field_no = field.field_no;
  byte *field_ref = btr_rec_get_field_ref(m_ctx->index(), m_ctx->rec(),
                                          m_ctx->get_offsets(), field_no);
  ref_t blobref(field_ref);

  blobref.set_length(field.len - m_remaining, mtr);
  blobref.update(m_ctx->space(), m_cur_blob_page_no, FIL_PAGE_DATA, mtr);

  m_prev_page_no = m_cur_blob_page_no;

  mtr->commit();

  return (m_err);
}

dberr_t Inserter::write_single_blob_page(big_rec_field_t &field,
                                         ulint nth_blob_page) {
  buf_block_t *rec_block = m_ctx->block();
  mtr_t *mtr = start_blob_mtr();
  ut_a(nth_blob_page > 0);

  buf_page_get(rec_block->page.id, rec_block->page.size, RW_X_LATCH,
               UT_LOCATION_HERE, mtr);

  alloc_blob_page();
  set_page_next();
  log_page_type();
  write_into_single_page(field);
  const ulint field_no = field.field_no;
  byte *field_ref = btr_rec_get_field_ref(m_ctx->index(), m_ctx->rec(),
                                          m_ctx->get_offsets(), field_no);
  ref_t blobref(field_ref);
  blobref.set_length(field.len - m_remaining, mtr);
  m_prev_page_no = m_cur_blob_page_no;
  mtr->commit();

  return (m_err);
}

/** Write contents into a single BLOB page.
@param[in]      field           the big record field. */
void Inserter::write_into_single_page(big_rec_field_t &field) {
  const ulint payload_size = payload();
  const ulint store_len =
      (m_remaining > payload_size) ? payload_size : m_remaining;

  page_t *page = buf_block_get_frame(m_cur_blob_block);

  mlog_write_string(page + FIL_PAGE_DATA + LOB_HDR_SIZE,
                    (const byte *)field.data + field.len - m_remaining,
                    store_len, &m_blob_mtr);

  mlog_write_ulint(page + FIL_PAGE_DATA + LOB_HDR_PART_LEN, store_len,
                   MLOG_4BYTES, &m_blob_mtr);

  mlog_write_ulint(page + FIL_PAGE_DATA + LOB_HDR_NEXT_PAGE_NO, FIL_NULL,
                   MLOG_4BYTES, &m_blob_mtr);

  m_remaining -= store_len;
}

}  // namespace lob
