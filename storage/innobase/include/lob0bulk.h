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
/** @file include/lob0bulk.h

For bulk loading large objects.

*************************************************************************/

#ifndef lob0bulk_h
#define lob0bulk_h

#include "lob0first.h"
#include "lob0impl.h"
#include "lob0pages.h"

namespace Btree_multi {
class Page_load;
} /* namespace Btree_multi */

namespace lob {

namespace bulk {

/** This is the bulk version in the namespace lob::bulk. */
struct index_entry_t : public lob::index_entry_t {
  index_entry_t() : lob::index_entry_t(nullptr) {}

  index_entry_t(flst_node_t *node) : lob::index_entry_t(node) {}

  void init() {
    set_prev_null();
    set_next_null();
    set_versions_null();
    set_trx_id(0);
    set_trx_undo_no(0);
    set_page_no(FIL_NULL);
    set_data_len(0);
  }

  void set_page_no(page_no_t num) {
    ut_ad(num > 0);
    byte *ptr = get_pageno_ptr();
    return mach_write_ulint(ptr, num, MLOG_4BYTES);
  }

  void set_trx_undo_no(undo_no_t undo_no) {
    byte *ptr = get_trx_undo_no_ptr();
    mach_write_ulint(ptr, undo_no, MLOG_4BYTES);
  }

  void set_prev_null() {
    ::bulk::flst_write_addr(m_node + OFFSET_PREV, fil_addr_null);
  }

  void set_next_null() {
    ::bulk::flst_write_addr(m_node + OFFSET_NEXT, fil_addr_null);
  }

  /** The versions base node is set to NULL. */
  void set_versions_null() {
    byte *base_node = get_versions_ptr();
    ::bulk::flst_init(base_node);
  }

  void set_trx_id(trx_id_t id) {
    byte *ptr = get_trxid_ptr();
    mach_write_to_6(ptr, id);
  }

  void set_trx_id_modifier(trx_id_t id) {
    byte *ptr = get_trxid_modifier_ptr();
    mach_write_to_6(ptr, id);
  }

  void set_trx_undo_no_modifier(undo_no_t undo_no) {
    byte *ptr = get_trx_undo_no_modifier_ptr();
    mach_write_ulint(ptr, undo_no, MLOG_4BYTES);
  }

  void incr_data_len(ulint len) {
    ulint new_len = get_data_len() + len;
    set_data_len(new_len);
  }

  void set_data_len(ulint len) {
    byte *ptr = get_datalen_ptr();
    return mach_write_ulint(ptr, len, MLOG_2BYTES);
  }

  ulint get_data_len() const {
    byte *ptr = get_datalen_ptr();
    return mach_read_from_2(ptr);
  }
};

/** An adapter class for handling blobs in bulk load. */
struct first_page_t : public lob::first_page_t {
  first_page_t() : lob::first_page_t(nullptr, nullptr, nullptr) {}

  /** Initialize the first page. */
  void init(Btree_multi::Page_load *page_load);

  void set_next_page(page_no_t page_no);

  void set_page_type() {
    mach_write_ulint(frame() + FIL_PAGE_TYPE, FIL_PAGE_TYPE_LOB_FIRST,
                     MLOG_2BYTES);
  }

  void set_version_0() {
    constexpr size_t offset = lob::first_page_t::OFFSET_VERSION;
    mach_write_ulint(frame() + offset, 0, MLOG_1BYTE);
  }

  void set_trx_id(trx_id_t id) {
    constexpr size_t offset = lob::first_page_t::OFFSET_TRX_ID;
    byte *ptr = frame() + offset;
    mach_write_to_6(ptr, id);
  }

  /** Set the length of data stored in bytes.
  @param[in]    len     amount of data stored in bytes. */
  void set_data_len(ulint len) {
    constexpr size_t offset = lob::first_page_t::OFFSET_DATA_LEN;
    mach_write_ulint(frame() + offset, len, MLOG_4BYTES);
  }

  /** Increment the length of data stored in bytes.
  @param[in]    len     amount of data stored in bytes. */
  void incr_data_len(ulint len) {
    ulint new_len = get_data_len() + len;
    mach_write_ulint(frame() + OFFSET_DATA_LEN, new_len, MLOG_4BYTES);
  }

  /** Allocate an index entry.
  @return nullptr if the free list is empty.
  @return pointer to an index entry. */
  flst_node_t *alloc_index_entry();

  /** Set the last transaction identifier.
  @param[in]    id      the trx identifier. */
  void set_last_trx_id(trx_id_t id) {
    byte *ptr = frame() + OFFSET_LAST_TRX_ID;
    mach_write_to_6(ptr, id);
  }

  /** Initialize the LOB version to 1. */
  void init_lob_version() {
    mach_write_ulint(frame() + OFFSET_LOB_VERSION, 1, MLOG_4BYTES);
  }

  /** Get the current transaction identifier.
  @return the current transaction identifier.*/
  trx_id_t get_trx_id() const;

  /** Get the page number of this first page.
  @return the page number of this first page. */
  page_no_t get_page_no() const;

  void reset_index_entry(flst_node_t *node) { m_index_entry.reset(node); }

  void append_to_index(flst_node_t *node);

  index_entry_t *get_index_entry() { return &m_index_entry; }

  /** Get a reference to the cache of blocks.
  @return a reference to the cache of blocks.*/
  std::vector<buf_block_t *> &get_blocks_cache() { return m_blocks; }

 private:
  /** Initialize the free list (base node at OFFSET_INDEX_FREE_NODES) and the
  index list (base node at OFFSET_INDEX_LIST). */
  void init_lists();

  Btree_multi::Page_load *m_page_load{nullptr};

  /** Current index entry. */
  index_entry_t m_index_entry;

  /** blocks containing the LOB index . */
  std::vector<buf_block_t *> m_blocks;
};

struct node_page_t : public lob::node_page_t {
  void set_page_type();
  void set_version_0();
  void set_next_page(page_no_t page_no);
  void init(Btree_multi::Page_load *node_page, first_page_t &first_page);

  Btree_multi::Page_load *m_page_load{nullptr};
};

struct data_page_t : public lob::data_page_t {
  void set_page_type();
  void set_version_0();
  void set_next_page_null();
  void init(Btree_multi::Page_load *data_page);

  /** Set the length of data stored in bytes.
  @param[in]    len     amount of data stored in bytes. */
  void set_data_len(ulint len) {
    constexpr size_t offset = lob::data_page_t::OFFSET_DATA_LEN;
    mach_write_ulint(frame() + offset, len, MLOG_4BYTES);
  }

  /** Increment the length of data stored in bytes.
  @param[in]    len     amount of data stored in bytes. */
  void incr_data_len(ulint len) {
    constexpr size_t offset = lob::data_page_t::OFFSET_DATA_LEN;
    ulint new_len = get_data_len() + len;
    mach_write_ulint(frame() + offset, new_len, MLOG_4BYTES);
  }

  Btree_multi::Page_load *m_page_load{nullptr};
};

} /* namespace bulk  */
} /* namespace lob */
#endif /* lob0bulk_h */
