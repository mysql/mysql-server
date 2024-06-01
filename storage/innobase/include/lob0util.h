/*****************************************************************************

Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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
#ifndef lob0util_h
#define lob0util_h

#include <stdint.h>

#include "buf0buf.h"
#include "fil0fil.h"
#include "fut0lst.h"

namespace lob {

/** Number of times an LOB can be partially updated.  Once this limit is
reached, then the LOB will be fully updated. */
const uint32_t MAX_PARTIAL_UPDATE_LIMIT = 1000;

struct basic_page_t {
  basic_page_t() : m_block(nullptr), m_mtr(nullptr), m_index(nullptr) {}

  basic_page_t(buf_block_t *block, mtr_t *mtr)
      : m_block(block), m_mtr(mtr), m_index(nullptr) {}

  basic_page_t(buf_block_t *block, mtr_t *mtr, dict_index_t *index)
      : m_block(block), m_mtr(mtr), m_index(index) {}

  /** Update the space identifier to given value without generating
  any redo log records.
  @param[in]    space_id        the space identifier. */
  void set_space_id_no_redo(space_id_t space_id) {
    mach_write_to_4(frame() + FIL_PAGE_SPACE_ID, space_id);
  }

  /** Get page number of the current page.
  @return the page number of the current page. */
  page_no_t get_page_no() const {
    ut_ad(m_block != nullptr);
    return (m_block->page.id.page_no());
  }

  /** Get the page id of the current page.
  @return the page id of current page. */
  page_id_t get_page_id() const {
    ut_ad(m_block != nullptr);
    return (m_block->page.id);
  }

  /** Set the FIL_PAGE_NEXT to the given page number, using the given mini
  transaction context.
  @param[in]    page_no   The page number to set.
  @param[in]    mtr       The mini-transaction context. */
  void set_next_page(page_no_t page_no, mtr_t *mtr) {
    mlog_write_ulint(frame() + FIL_PAGE_NEXT, page_no, MLOG_4BYTES, mtr);
  }

  /** Set the FIL_PAGE_NEXT to the given page number.
  @param[in]    page_no   The page number to set. */
  void set_next_page(page_no_t page_no) { set_next_page(page_no, m_mtr); }

  /** Set the FIL_PAGE_NEXT to FIL_NULL. */
  void set_next_page_null() {
    ut_ad(m_mtr != nullptr);

    set_next_page(FIL_NULL);
  }

  page_no_t get_next_page() {
    return (mach_read_from_4(frame() + FIL_PAGE_NEXT));
  }

  page_type_t get_page_type() const {
    return (mach_read_from_2(frame() + FIL_PAGE_TYPE));
  }

  byte *frame() const { return (buf_block_get_frame(m_block)); }

  flst_node_t *get_flst_node(const fil_addr_t &addr) {
    ut_ad(!addr.is_null());

    flst_node_t *node = nullptr;
    if (addr.page == get_page_no()) {
      node = frame() + addr.boffset;
    }
    return (node);
  }

  static ulint payload();
  ulint max_space_available();

  /** Get the underlying buffer block.
  @return the buffer block. */
  [[nodiscard]] buf_block_t *get_block() const noexcept;

  void set_block(buf_block_t *block) {
    ut_ad(mtr_memo_contains(m_mtr, block, MTR_MEMO_PAGE_X_FIX) ||
          mtr_memo_contains(m_mtr, block, MTR_MEMO_PAGE_S_FIX));

    m_block = block;
  }

  void set_mtr(mtr_t *mtr) { m_mtr = mtr; }

 protected:
  buf_block_t *m_block{nullptr};
  mtr_t *m_mtr{nullptr};
  dict_index_t *m_index{nullptr};
};

[[nodiscard]] inline buf_block_t *basic_page_t::get_block() const noexcept {
  return m_block;
}

/** Allocate one LOB page.
@param[in]  index   Index in which LOB exists.
@param[in]  lob_mtr Mini-transaction context.
@param[in]  hint    Hint page number for allocation.
@param[in]  bulk    true if operation is OPCODE_INSERT_BULK,
                    false otherwise.
@return the allocated block of the BLOB page or nullptr. */
buf_block_t *alloc_lob_page(dict_index_t *index, mtr_t *lob_mtr, page_no_t hint,
                            bool bulk);

/** Check if the index entry is visible to the given transaction.
@param[in]      index           the index to which LOB belongs.
@param[in]      trx             the transaction reading the index entry.
@param[in]      entry_trx_id    the trx id in the index entry. */
bool entry_visible_to(dict_index_t *index, trx_t *trx, trx_id_t entry_trx_id);

} /* namespace lob */

#endif /* lob0util_h */
