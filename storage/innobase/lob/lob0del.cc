/*****************************************************************************

Copyright (c) 2015, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

namespace lob {

/* Obtain an x-latch on the clustered index record page.*/
void Deleter::x_latch_rec_page() {
  bool found;
  page_t *rec_page = m_ctx.m_blobref.page_align();
  page_no_t rec_page_no = page_get_page_no(rec_page);
  space_id_t rec_space_id = page_get_space_id(rec_page);

  const page_size_t &rec_page_size =
      fil_space_get_page_size(rec_space_id, &found);
  ut_ad(found);

  buf_page_get(page_id_t(rec_space_id, rec_page_no), rec_page_size, RW_X_LATCH,
               &m_mtr);
}

/** Returns the page number where the next BLOB part is stored.
@param[in]	blob_header	the BLOB header.
@return page number or FIL_NULL if no more pages */
static inline page_no_t btr_blob_get_next_page_no(const byte *blob_header) {
  return (mach_read_from_4(blob_header + LOB_HDR_NEXT_PAGE_NO));
}

/** Free the first page of the BLOB and update the BLOB reference
in the clustered index.
@return DB_SUCCESS on pass, error code on failure. */
dberr_t Deleter::free_first_page() {
  dberr_t err(DB_SUCCESS);
  page_no_t next_page_no;

  mtr_start(&m_mtr);
  m_mtr.set_log_mode(m_ctx.m_mtr->get_log_mode());

  ut_ad(m_ctx.m_pcur == nullptr || !m_ctx.table()->is_temporary() ||
        m_ctx.m_mtr->get_log_mode() == MTR_LOG_NO_REDO);

  page_no_t page_no = m_ctx.m_blobref.page_no();
  space_id_t space_id = m_ctx.m_blobref.space_id();

  x_latch_rec_page();

  buf_block_t *blob_block = buf_page_get(page_id_t(space_id, page_no),
                                         m_ctx.m_page_size, RW_X_LATCH, &m_mtr);

  buf_block_dbg_add_level(blob_block, SYNC_EXTERN_STORAGE);
  page_t *page = buf_block_get_frame(blob_block);

  ut_a(validate_page_type(page));

  if (m_ctx.is_compressed()) {
    next_page_no = mach_read_from_4(page + FIL_PAGE_NEXT);
  } else {
    next_page_no = btr_blob_get_next_page_no(page + FIL_PAGE_DATA);
  }

  btr_page_free_low(m_ctx.m_index, blob_block, ULINT_UNDEFINED, &m_mtr);

  if (m_ctx.is_compressed() && m_ctx.get_page_zip() != nullptr) {
    m_ctx.m_blobref.set_page_no(next_page_no, nullptr);
    m_ctx.m_blobref.set_length(0, nullptr);
    page_zip_write_blob_ptr(m_ctx.get_page_zip(), m_ctx.m_rec, m_ctx.m_index,
                            m_ctx.m_offsets, m_ctx.m_field_no, &m_mtr);
  } else {
    m_ctx.m_blobref.set_page_no(next_page_no, &m_mtr);
    m_ctx.m_blobref.set_length(0, &m_mtr);
  }

  /* Commit mtr and release the BLOB block to save memory. */
  blob_free(m_ctx.m_index, blob_block, TRUE, &m_mtr);

  return (err);
}

/** Free the LOB object.
@return DB_SUCCESS on success. */
dberr_t Deleter::destroy() {
  dberr_t err(DB_SUCCESS);

  if (!can_free()) {
    return (DB_SUCCESS);
  }

  if (dict_index_is_online_ddl(m_ctx.index())) {
    row_log_table_blob_free(m_ctx.index(), m_ctx.m_blobref.page_no());
  }

  while (m_ctx.m_blobref.page_no() != FIL_NULL) {
    ut_ad(m_ctx.m_blobref.page_no() > 0);

    err = free_first_page();
    if (err != DB_SUCCESS) {
      break;
    }
  }

  return (err);
}

/** Check if the BLOB can be freed.  If the clustered index record
is not the owner of the LOB, then it cannot be freed.  Also, during
rollback, if inherited flag is set, then LOB will not be freed.
@return true if the BLOB can be freed, false otherwise. */
bool Deleter::can_free() const {
  if (m_ctx.m_blobref.is_null()) {
    /* In the rollback, we may encounter a clustered index
    record with some unwritten off-page columns. There is
    nothing to free then. */
    ut_a(m_ctx.m_rollback);
    return (false);
  }

  if (!m_ctx.m_blobref.is_owner() || m_ctx.m_blobref.page_no() == FIL_NULL ||
      (m_ctx.m_rollback && m_ctx.m_blobref.is_inherited())) {
    return (false);
  }

  return (true);
}

}  // namespace lob
