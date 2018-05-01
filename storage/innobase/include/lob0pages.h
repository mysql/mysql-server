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
#ifndef lob0pages_h
#define lob0pages_h

#include "lob0first.h"

namespace lob {

/** The LOB data page carrying the user data. */
struct data_page_t : public basic_page_t {
  static const ulint OFFSET_VERSION = FIL_PAGE_DATA;
  static const ulint OFFSET_DATA_LEN = OFFSET_VERSION + 1;
  static const ulint OFFSET_TRX_ID = OFFSET_DATA_LEN + 4;
  static const ulint LOB_PAGE_DATA = OFFSET_TRX_ID + 6;

  data_page_t() {}

  /** Contructor. */
  data_page_t(buf_block_t *block, mtr_t *mtr) : basic_page_t(block, mtr) {
    page_type_t type = get_page_type();
    ut_a(type == FIL_PAGE_TYPE_LOB_DATA);
  }

  data_page_t(buf_block_t *block, mtr_t *mtr, dict_index_t *index)
      : basic_page_t(block, mtr, index) {}

  data_page_t(mtr_t *mtr, dict_index_t *index)
      : basic_page_t(nullptr, mtr, index) {}

  /** Constructor.
  @param[in]	block	the buffer block.*/
  data_page_t(buf_block_t *block) : basic_page_t(block, nullptr, nullptr) {}

  buf_block_t *alloc(mtr_t *alloc_mtr, bool is_bulk);

  buf_block_t *load_x(page_no_t page_no);

  void set_version_0() {
    mlog_write_ulint(frame() + OFFSET_VERSION, 0, MLOG_1BYTE, m_mtr);
  }

  void dealloc() {
    btr_page_free_low(m_index, m_block, ULINT_UNDEFINED, m_mtr);
    m_block = nullptr;
  }

  void set_page_type() {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(frame() + FIL_PAGE_TYPE, FIL_PAGE_TYPE_LOB_DATA,
                     MLOG_2BYTES, m_mtr);
  }

  void set_trx_id(trx_id_t id) {
    byte *ptr = frame() + OFFSET_TRX_ID;
    mach_write_to_6(ptr, id);
    mlog_log_string(ptr, 6, m_mtr);
  }

  /** Write the trx identifier to the header, without
  generating redo log.
  @param[in]	id	the transaction identifier.*/
  void set_trx_id_no_redo(trx_id_t id) {
    byte *ptr = frame() + OFFSET_TRX_ID;
    mach_write_to_6(ptr, id);
  }

  static ulint payload() {
    return (UNIV_PAGE_SIZE - LOB_PAGE_DATA - FIL_PAGE_DATA_END);
  }

  byte *data_begin() const { return (frame() + LOB_PAGE_DATA); }

  /** Create a new data page and replace some or all parts of the old data
  with data.
  @param[in]	trx	the current transaction.
  @param[in]	offset	the offset where replace begins.
  @param[in,out]	ptr	pointer to new data.
  @param[in]	want	amount of data the caller wants to replace.
  @param[in]	mtr	the mini transaction context.
  @return the buffer block of the new data page. */
  buf_block_t *replace(trx_t *trx, ulint offset, const byte *&ptr, ulint &want,
                       mtr_t *mtr);

  /** Replace some or all parts of the data inline.
  @param[in]	trx	the current transaction.
  @param[in]	offset	the offset where replace begins.
  @param[in,out]	ptr	pointer to new data.
  @param[in]	want	amount of data the caller wants to replace.
  @param[in]	mtr	the mini transaction context. */
  void replace_inline(trx_t *trx, ulint offset, const byte *&ptr, ulint &want,
                      mtr_t *mtr);

  ulint get_data_len() const {
    return (mach_read_from_4(frame() + OFFSET_DATA_LEN));
  }

  void set_data_len(ulint len) {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(frame() + OFFSET_DATA_LEN, len, MLOG_4BYTES, m_mtr);
  }

  /** Read data from the data page.
  @param[in]	offset	read begins at this offset.
  @param[out]	ptr	the output buffer.
  @param[in]	want	bytes to read
  @return bytes actually read. */
  ulint read(ulint offset, byte *ptr, ulint want);

  /** Write data into a data page.
  @param[in]	trxid	the transaction identifier of the session
                          writing data.
  @param[in,out]	data	the data to be written.  it will be updated
                          to point to the byte not yet written.
  @param[in,out]	len	length of data to be written.
  @return amount of data actually written into the page. */
  ulint write(trx_id_t trxid, const byte *&data, ulint &len);

  /** Append given data in data page.
  @param[in]	trxid	transaction doing append.
  @param[in,out]	data	data to be appended.
  @param[in,out]	len	length of data.
  @return number of bytes appended. */
  ulint append(trx_id_t trxid, byte *&data, ulint &len);

  std::pair<ulint, byte *> insert_middle(trx_t *trx, ulint offset, byte *&data,
                                         ulint &len, buf_block_t *&new_block);

  buf_block_t *remove_middle(trx_t *trx, ulint offset, ulint &len);

  ulint max_space_available() const { return (payload()); }

  ulint space_left() const;
};

}; /* namespace lob */

#endif /* lob0pages_h */
