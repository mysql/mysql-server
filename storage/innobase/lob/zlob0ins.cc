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

#include "db0err.h"
#include "lob0zip.h"

namespace lob {

/** Write first blob page.
@param[in]	blob_j		the jth blob object of the record.
@param[in]	field		the big record field.
@return code as returned by the zlib. */
int zInserter::write_first_page(size_t blob_j, big_rec_field_t &field) {
  buf_block_t *rec_block = m_ctx->block();
  mtr_t *mtr = start_blob_mtr();

  buf_page_get(rec_block->page.id, rec_block->page.size, RW_X_LATCH, mtr);

  buf_block_t *blob_block = alloc_blob_page();

  if (dict_index_is_online_ddl(m_ctx->index())) {
    row_log_table_blob_alloc(m_ctx->index(), m_cur_blob_page_no);
  }

  page_t *blob_page = buf_block_get_frame(blob_block);

  log_page_type(blob_page, 0);

  int err = write_into_single_page();

  ut_ad(!dict_index_is_spatial(m_ctx->index()));

  const ulint field_no = field.field_no;
  byte *field_ref =
      btr_rec_get_field_ref(m_ctx->rec(), m_ctx->get_offsets(), field_no);
  ref_t blobref(field_ref);

  if (err == Z_OK) {
    blobref.set_length(0, nullptr);
  } else if (err == Z_STREAM_END) {
    blobref.set_length(m_stream.total_in, nullptr);
  } else {
    ut_ad(0);
    return (err);
  }

  blobref.update(m_ctx->space(), m_cur_blob_page_no, FIL_PAGE_NEXT, NULL);

  /* After writing the first blob page, update the blob reference. */
  if (!m_ctx->is_bulk()) {
    m_ctx->zblob_write_blobref(field_no, &m_blob_mtr);
  }

  m_prev_page_no = page_get_page_no(blob_page);

  /* Commit mtr and release uncompressed page frame to save memory.*/
  blob_free(m_ctx->index(), m_cur_blob_block, FALSE, mtr);

  return (err);
}

/** For the given blob field, update its length in the blob reference
which is available in the clustered index record.
@param[in]	field	the concerned blob field. */
void zInserter::update_length_in_blobref(big_rec_field_t &field) {
  /* After writing the last blob page, update the blob reference
  with the correct length. */

  const ulint field_no = field.field_no;
  byte *field_ref =
      btr_rec_get_field_ref(m_ctx->rec(), m_ctx->get_offsets(), field_no);

  ref_t blobref(field_ref);
  blobref.set_length(m_stream.total_in, nullptr);

  if (!m_ctx->is_bulk()) {
    m_ctx->zblob_write_blobref(field_no, &m_blob_mtr);
  }
}

/** Write one blob field data.
@param[in]	blob_j	the blob field number
@return DB_SUCCESS on success, error code on failure. */
dberr_t zInserter::write_one_small_blob(size_t blob_j) {
  const big_rec_t *vec = m_ctx->get_big_rec_vec();
  big_rec_field_t &field = vec->fields[blob_j];

  int err = deflateReset(&m_stream);
  ut_a(err == Z_OK);

  m_stream.next_in = (Bytef *)field.data;
  m_stream.avail_in = static_cast<uInt>(field.len);

  err = write_first_page(blob_j, field);

  for (ulint nth_blob_page = 1; err == Z_OK; ++nth_blob_page) {
    err = write_single_blob_page(blob_j, field, nth_blob_page);
  }

  ut_ad(err == Z_STREAM_END);
  m_ctx->make_nth_extern(field.field_no);
  return (DB_SUCCESS);
}

/** Write one blob field data.
@param[in]	blob_j	the blob field number
@return DB_SUCCESS on success, error code on failure. */
dberr_t zInserter::write_one_blob(size_t blob_j) {
  const big_rec_t *vec = m_ctx->get_big_rec_vec();
  big_rec_field_t &field = vec->fields[blob_j];

  int err = deflateReset(&m_stream);
  ut_a(err == Z_OK);

  m_stream.next_in = (Bytef *)field.data;
  m_stream.avail_in = static_cast<uInt>(field.len);

  m_ctx->check_redolog();

  err = write_first_page(blob_j, field);

  for (ulint nth_blob_page = 1; err == Z_OK; ++nth_blob_page) {
    const ulint commit_freq = 4;

    err = write_single_blob_page(blob_j, field, nth_blob_page);

    if (nth_blob_page % commit_freq == 0) {
      m_ctx->check_redolog();
    }
  }

  ut_ad(err == Z_STREAM_END);
  m_ctx->make_nth_extern(field.field_no);
  return (DB_SUCCESS);
}

/** Write contents into a single BLOB page.
@return code as returned by zlib. */
int zInserter::write_into_single_page() {
  const uint in_before = m_stream.avail_in;

  mtr_t *const mtr = &m_blob_mtr;

  /* Space available in compressed page to carry blob data */
  const page_size_t page_size = m_ctx->page_size();
  const uint payload_size_zip = page_size.physical() - FIL_PAGE_DATA;

  page_t *blob_page = buf_block_get_frame(m_cur_blob_block);

  m_stream.next_out = blob_page + FIL_PAGE_DATA;
  m_stream.avail_out = static_cast<uInt>(payload_size_zip);

  int err = deflate(&m_stream, Z_FINISH);
  ut_a(err == Z_OK || err == Z_STREAM_END);
  ut_a(err == Z_STREAM_END || m_stream.avail_out == 0);

  const blob_page_info_t page_info(m_cur_blob_page_no,
                                   in_before - m_stream.avail_in,
                                   payload_size_zip - m_stream.avail_out);

  add_to_blob_dir(page_info);

  /* Write the "next BLOB page" pointer */
  mlog_write_ulint(blob_page + FIL_PAGE_NEXT, FIL_NULL, MLOG_4BYTES, mtr);

  /* Initialize the unused "prev page" pointer */
  mlog_write_ulint(blob_page + FIL_PAGE_PREV, FIL_NULL, MLOG_4BYTES, mtr);

  /* Write a back pointer to the record into the otherwise unused area.
  This information could be useful in debugging.  Later, we might want
  to implement the possibility to relocate BLOB pages.  Then, we would
  need to be able to adjust the BLOB pointer in the record.  We do not
  store the heap number of the record, because it can change in
  page_zip_reorganize() or btr_page_reorganize().  However, also the
  page number of the record may change when B-tree nodes are split or
  merged. */
  mlog_write_ulint(blob_page + FIL_PAGE_FILE_FLUSH_LSN, m_ctx->space(),
                   MLOG_4BYTES, mtr);

  mlog_write_ulint(blob_page + FIL_PAGE_FILE_FLUSH_LSN + 4,
                   m_ctx->get_page_no(), MLOG_4BYTES, mtr);

  if (m_stream.avail_out > 0) {
    /* Zero out the unused part of the page. */
    memset(blob_page + page_zip_get_size(m_ctx->get_page_zip()) -
               m_stream.avail_out,
           0, m_stream.avail_out);
  }

  /* Redo log the page contents (the page is not modified). */
  mlog_log_string(
      blob_page + FIL_PAGE_FILE_FLUSH_LSN,
      page_zip_get_size(m_ctx->get_page_zip()) - FIL_PAGE_FILE_FLUSH_LSN, mtr);

  /* Copy the page to compressed storage, because it will be flushed
  to disk from there. */
  page_zip_des_t *blob_page_zip = buf_block_get_page_zip(m_cur_blob_block);

  ut_ad(blob_page_zip);
  ut_ad(page_zip_get_size(blob_page_zip) ==
        page_zip_get_size(m_ctx->get_page_zip()));

  page_zip_des_t *page_zip = buf_block_get_page_zip(m_ctx->block());
  memcpy(blob_page_zip->data, blob_page, page_zip_get_size(page_zip));

  return (err);
}

/** Write one blob page.  This function will be repeatedly called
with an increasing nth_blob_page to completely write a BLOB.
@param[in]	blob_j		the jth blob object of the record.
@param[in]	field		the big record field.
@param[in]	nth_blob_page	count of the BLOB page (starting from 1).
@return code as returned by the zlib. */
int zInserter::write_single_blob_page(size_t blob_j, big_rec_field_t &field,
                                      ulint nth_blob_page) {
  ut_ad(nth_blob_page > 0);

  buf_block_t *rec_block = m_ctx->block();
  mtr_t *mtr = start_blob_mtr();

  buf_page_get(rec_block->page.id, rec_block->page.size, RW_X_LATCH, mtr);

  buf_block_t *blob_block = alloc_blob_page();
  page_t *blob_page = buf_block_get_frame(blob_block);

  set_page_next();

  m_prev_page_no = page_get_page_no(blob_page);

  log_page_type(blob_page, nth_blob_page);

  int err = write_into_single_page();

  ut_ad(!dict_index_is_spatial(m_ctx->index()));

  if (err == Z_STREAM_END) {
    update_length_in_blobref(field);
  }

  /* Commit mtr and release uncompressed page frame to save memory.*/
  blob_free(m_ctx->index(), m_cur_blob_block, FALSE, mtr);

  return (err);
}

/** Prepare to write a compressed BLOB. Setup the zlib
compression stream.
@return DB_SUCCESS on success, error code on failure. */
dberr_t zInserter::prepare() {
  /* Zlib deflate needs 128 kilobytes for the default
  window size, plus 512 << memLevel, plus a few
  kilobytes for small objects.  We use reduced memLevel
  to limit the memory consumption, and preallocate the
  heap, hoping to avoid memory fragmentation. */
  m_heap = mem_heap_create(250000);

  if (m_heap == NULL) {
    return (DB_OUT_OF_MEMORY);
  }

  page_zip_set_alloc(&m_stream, m_heap);
  int ret = deflateInit2(&m_stream, page_zip_level, Z_DEFLATED, 15, 7,
                         Z_DEFAULT_STRATEGY);
  if (ret != Z_OK) {
    return (DB_FAIL);
  }

  return (DB_SUCCESS);
}

/** Write all the BLOBs of the clustered index record.
@return DB_SUCCESS on success, error code on failure. */
dberr_t zInserter::write() {
  /* Loop through each blob field of the record and write one blob
  at a time.*/
  for (ulint i = 0; i < m_ctx->get_big_rec_vec_size() && m_err == DB_SUCCESS;
       i++) {
    ut_d(m_dir.clear(););
    m_err = write_one_blob(i);
  }

  return (m_err);
}

/** Make the current page as next page of previous page.  In other
words, make the page m_cur_blob_page_no as the next page
(FIL_PAGE_NEXT) of page m_prev_page_no.
@return DB_SUCCESS on success, or error code on failure. */
dberr_t zInserter::set_page_next() {
  buf_block_t *prev_block = get_previous_blob_block();
  page_t *prev_page = buf_block_get_frame(prev_block);

  mlog_write_ulint(prev_page + FIL_PAGE_NEXT, m_cur_blob_page_no, MLOG_4BYTES,
                   &m_blob_mtr);

  memcpy(buf_block_get_page_zip(prev_block)->data + FIL_PAGE_NEXT,
         prev_page + FIL_PAGE_NEXT, 4);

  return (m_err);
}

};  // namespace lob
