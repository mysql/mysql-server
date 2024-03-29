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
#ifndef lob0ins_h
#define lob0ins_h

#include "lob0lob.h"

namespace lob {

/** This struct can hold BLOB routines/functions, and state variables,
that are common for compressed and uncompressed BLOB. */
struct BaseInserter {
  /** Constructor.
  @param[in]    ctx     blob operation context. */
  BaseInserter(InsertContext *ctx)
      : m_ctx(ctx),
        m_err(DB_SUCCESS),
        m_prev_page_no(ctx->get_page_no()),
        m_cur_blob_block(nullptr),
        m_cur_blob_page_no(FIL_NULL) {}

  /** Start the BLOB mtr.
  @return pointer to the BLOB mtr. */
  mtr_t *start_blob_mtr() {
    mtr_start(&m_blob_mtr);
    m_blob_mtr.set_log_mode(m_ctx->get_log_mode());
    m_blob_mtr.set_flush_observer(m_ctx->get_flush_observer());
    return (&m_blob_mtr);
  }

  /** Allocate one BLOB page.
  @return the allocated block of the BLOB page. */
  buf_block_t *alloc_blob_page();

  /** Get the previous BLOB page frame.  This will return a BLOB page.
  It should not be called for the first BLOB page, because it will not
  have a previous BLOB page.
  @return       the previous BLOB page frame. */
  page_t *get_previous_blob_page();

  /** Get the previous BLOB page block.  This will return a BLOB block.
  It should not be called for the first BLOB page, because it will not
  have a previous BLOB page.
  @return       the previous BLOB block. */
  buf_block_t *get_previous_blob_block();

  /** Check if the index is SDI index
  @return true if index is SDI index else false */
  bool is_index_sdi() { return (dict_index_is_sdi(m_ctx->index())); }

  /** Get the current BLOB page frame.
  @return the current BLOB page frame. */
  page_t *cur_page() const { return (buf_block_get_frame(m_cur_blob_block)); }

 protected:
  /** The BLOB operation context */
  InsertContext *m_ctx;

  /** Success or failure status of the operation so far. */
  dberr_t m_err;

  /** The mini trx used to write into blob pages */
  mtr_t m_blob_mtr;

  /** The previous BLOB page number.  This is needed to maintain
  the linked list of BLOB pages. */
  page_no_t m_prev_page_no;

  /** The current BLOB buf_block_t object. */
  buf_block_t *m_cur_blob_block;

  /** The current BLOB page number. */
  page_no_t m_cur_blob_page_no;
};

/** Insert or write an uncompressed BLOB */
class Inserter : private BaseInserter {
 public:
  /** Constructor.
  @param[in]    ctx     blob operation context. */
  Inserter(InsertContext *ctx) : BaseInserter(ctx) {}

  /** Destructor. */
  ~Inserter() = default;

  /** Write all the BLOBs of the clustered index record.
  @return DB_SUCCESS on success, error code on failure. */
  dberr_t write();

  /** Write one blob field data.
  @param[in]    blob_j  the blob field number
  @return DB_SUCCESS on success, error code on failure. */
  dberr_t write_one_blob(size_t blob_j);

  /** Write one blob field data.
  @param[in]    blob_j  the blob field number
  @return DB_SUCCESS on success, error code on failure. */
  dberr_t write_one_small_blob(size_t blob_j);

  /** Write one blob page.  This function will be repeatedly called
  with an increasing nth_blob_page to completely write a BLOB.
  @param[in]    field           the big record field.
  @param[in]    nth_blob_page   count of the BLOB page (starting from 1).
  @return DB_SUCCESS or DB_FAIL. */
  dberr_t write_single_blob_page(big_rec_field_t &field, ulint nth_blob_page);

  /** Check if the BLOB operation has reported any errors.
  @return       true if BLOB operation is successful, false otherwise. */
  bool is_ok() const { return (m_err == DB_SUCCESS); }

  /** Make the current page as next page of previous page.  In other
  words, make the page m_cur_blob_page_no as the next page of page
  m_prev_page_no. */
  void set_page_next();

  /** Write the page type of the current BLOB page and also generate the
  redo log record. */
  void log_page_type() {
    page_type_t page_type;
    page_t *blob_page = cur_page();

    if (is_index_sdi()) {
      page_type = FIL_PAGE_SDI_BLOB;
    } else {
      page_type = FIL_PAGE_TYPE_BLOB;
    }

    mlog_write_ulint(blob_page + FIL_PAGE_TYPE, page_type, MLOG_2BYTES,
                     &m_blob_mtr);
  }

  /** Calculate the payload size of the BLOB page.
  @return       payload size in bytes. */
  ulint payload() const {
    const page_size_t page_size = m_ctx->page_size();
    const ulint payload_size =
        page_size.physical() - FIL_PAGE_DATA - LOB_HDR_SIZE - FIL_PAGE_DATA_END;
    return (payload_size);
  }

  /** Write contents into a single BLOB page.
  @param[in]    field           the big record field. */
  void write_into_single_page(big_rec_field_t &field);

  /** Write first blob page.
  @param[in]    field   the big record field.
  @return DB_SUCCESS on success. */
  dberr_t write_first_page(big_rec_field_t &field);

 private:
  /** The BLOB directory information. */
  blob_dir_t m_dir;

  /** Data remaining to be written. */
  ulint m_remaining;
};

}  // namespace lob

#endif /* lob0ins_h */
