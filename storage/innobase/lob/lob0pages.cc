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

#include "lob0pages.h"
#include "lob0impl.h"
#include "my_dbug.h"
#include "trx0trx.h"

namespace lob {

void data_page_t::replace_inline(ulint offset, const byte *&ptr, ulint &want,
                                 mtr_t *mtr) {
  byte *old_ptr = data_begin() + offset;

  ulint data_len = get_data_len();
  ut_ad(data_len > offset);

  /** Copy the new data to page. */
  ulint data_avail = data_len - offset;
  ulint data_to_copy = want > data_avail ? data_avail : want;
  mlog_write_string(old_ptr, ptr, data_to_copy, mtr);

  ptr += data_to_copy;
  want -= data_to_copy;
}

/** Create a new data page and replace some or all parts of the old data
with data.
@param[in]      trx     Current transaction.
@param[in]      offset  Offset where replace begins.
@param[in,out]  ptr     Pointer to new data.
@param[in]      want    Amount of data the caller wants to replace.
@param[in]      mtr     Mini-transaction context.
@return the buffer block of the new data page. */
buf_block_t *data_page_t::replace(trx_t *trx, ulint offset, const byte *&ptr,
                                  ulint &want, mtr_t *mtr) {
  ulint cur_data_len = get_data_len();
  ut_a(offset > 0 || want < cur_data_len);
  buf_block_t *new_block = nullptr;

  /** Allocate a new data page. */
  data_page_t new_page(mtr, m_index);
  new_block = new_page.alloc(mtr, false);

  if (new_block == nullptr) {
    return (nullptr);
  }

  byte *new_ptr = new_page.data_begin();
  byte *old_ptr = data_begin();

  DBUG_LOG("data_page_t", PrintBuffer(old_ptr, cur_data_len));
  DBUG_LOG("data_page_t", "offset=" << offset << ", want=" << want);

  new_page.set_trx_id(trx->id);
  new_page.set_data_len(get_data_len());

  /** Copy contents from old page to new page. */
  mlog_write_string(new_ptr, old_ptr, offset, mtr);

  new_ptr += offset;
  old_ptr += offset;

  /** Copy the new data to new page. */
  ulint data_avail = get_data_len() - offset;
  ulint data_to_copy = want > data_avail ? data_avail : want;
  mlog_write_string(new_ptr, ptr, data_to_copy, mtr);

  new_ptr += data_to_copy;
  old_ptr += data_to_copy;
  ptr += data_to_copy;

  /** Copy contents from old page to new page. */
  if (want < data_avail) {
    ut_ad(data_to_copy == want);
    ulint remain = data_avail - want;
    mlog_write_string(new_ptr, old_ptr, remain, mtr);
  }

  want -= data_to_copy;

  DBUG_LOG("data_page_t", PrintBuffer(new_page.data_begin(), cur_data_len));

  return (new_block);
}

/** Append given data in data page.
@param[in]      trxid   transaction doing append.
@param[in,out]  data    data to be appended.
@param[in,out]  len     length of data.
@return number of bytes appended. */
ulint data_page_t::append(trx_id_t trxid, byte *&data, ulint &len) {
  DBUG_TRACE;

  ulint old_data_len = get_data_len();

  byte *ptr = data_begin() + old_data_len;
  ulint space_available = max_space_available() - old_data_len;

  if (space_available == 0 || len == 0) {
    return 0;
  }

  ulint written = (len > space_available) ? space_available : len;

  mlog_write_string(ptr, data, written, m_mtr);

  set_data_len(old_data_len + written);
  set_trx_id(trxid);

  data += written;
  len -= written;

  return written;
}

ulint data_page_t::space_left() const { return (payload() - get_data_len()); }

buf_block_t *data_page_t::alloc(mtr_t *alloc_mtr, bool is_bulk) {
  ut_ad(m_block == nullptr);
  ut_ad(m_index != nullptr);
  ut_ad(m_mtr != nullptr);
  ut_ad(alloc_mtr != nullptr);

  page_no_t hint = FIL_NULL;

  /* For testing purposes, pretend that the LOB page allocation failed.*/
  DBUG_EXECUTE_IF("innodb_lob_data_page_alloc_failed", return (nullptr););

  m_block = alloc_lob_page(m_index, alloc_mtr, hint, is_bulk);

  if (m_block == nullptr) {
    return (m_block);
  }

  set_page_type();
  set_version_0();
  set_next_page_null();

  page_type_t type = fil_page_get_type(m_block->frame);
  ut_a(type == FIL_PAGE_TYPE_LOB_DATA);

  return (m_block);
}

ulint data_page_t::write(const byte *&data, ulint &len) {
  byte *ptr = data_begin();
  ulint written = (len > payload()) ? payload() : len;

  /* Write data into page. */
  mlog_write_string(ptr, data, written, m_mtr);
  set_data_len(written);

  data += written;
  len -= written;

  return (written);
}

buf_block_t *data_page_t::load_x(page_no_t page_no) {
  ut_ad(m_mtr != nullptr);
  ut_ad(m_index != nullptr);

  space_id_t space_id = dict_index_get_space(m_index);
  const page_id_t page_id(space_id, page_no);
  const page_size_t page_size = dict_table_page_size(m_index->table);

  m_block =
      buf_page_get(page_id, page_size, RW_X_LATCH, UT_LOCATION_HERE, m_mtr);
  return (m_block);
}

/** Read data from the data page.
@param[in]      offset  read begins at this offset.
@param[out]     ptr     the output buffer.
@param[in]      want    bytes to read
@return bytes actually read. */
ulint data_page_t::read(ulint offset, byte *ptr, ulint want) {
  DBUG_TRACE;

  byte *start = data_begin();
  start += offset;
  ulint avail_data = get_data_len() - offset;

  ulint copy_len = want < avail_data ? want : avail_data;
  memcpy(ptr, start, copy_len);

  DBUG_LOG("lob", "page_no=" << get_page_no());
  DBUG_LOG("lob", PrintBuffer(ptr, copy_len));

  return copy_len;
}

}  // namespace lob
