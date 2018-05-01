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

#ifndef lob0first_h
#define lob0first_h

#include "btr0btr.h"
#include "buf0buf.h"
#include "dict0dict.h"
#include "fut0lst.h"
#include "lob0index.h"
#include "lob0lob.h"
#include "lob0util.h"
#include "mtr0log.h"

namespace lob {

/** The first page of an uncompressed LOB. */
struct first_page_t : public basic_page_t {
  /** Version information. One byte. */
  static const ulint OFFSET_VERSION = FIL_PAGE_DATA;

  /** One byte of flag bits.  Currently only one bit (the least
  significant bit) is used, other 7 bits are available for future use.*/
  static const ulint OFFSET_FLAGS = FIL_PAGE_DATA + 1;

  /** LOB version. 4 bytes.*/
  static const uint32_t OFFSET_LOB_VERSION = OFFSET_FLAGS + 1;

  /** The latest transaction that modified this LOB. */
  static const ulint OFFSET_LAST_TRX_ID = OFFSET_LOB_VERSION + 4;

  /** The latest transaction undo_no that modified this LOB. */
  static const ulint OFFSET_LAST_UNDO_NO = OFFSET_LAST_TRX_ID + 6;

  /** Length of data stored in this page.  4 bytes. */
  static const ulint OFFSET_DATA_LEN = OFFSET_LAST_UNDO_NO + 4;

  /** The trx that created the data stored in this page. */
  static const ulint OFFSET_TRX_ID = OFFSET_DATA_LEN + 4;

  /** The offset where the list base node is located.  This is the list
  of LOB pages. */
  static const ulint OFFSET_INDEX_LIST = OFFSET_TRX_ID + 6;

  /** The offset where the list base node is located.  This is the list
  of free nodes. */
  static const ulint OFFSET_INDEX_FREE_NODES =
      OFFSET_INDEX_LIST + FLST_BASE_NODE_SIZE;

  /** The offset where the contents of the first page begins. */
  static const ulint LOB_PAGE_DATA =
      OFFSET_INDEX_FREE_NODES + FLST_BASE_NODE_SIZE;

  static const ulint LOB_PAGE_TRAILER_LEN = FIL_PAGE_DATA_END;

  /** The default constructor. */
  first_page_t() {}

  /** Constructor.
  @param[in]	block	the buffer block of the first page.
  @param[in]	mtr	the mini-transaction context. */
  first_page_t(buf_block_t *block, mtr_t *mtr) : basic_page_t(block, mtr) {}

  /** Constructor.
  @param[in]	block	the buffer block of the first page.*/
  first_page_t(buf_block_t *block) : basic_page_t(block, nullptr) {}

  /** Constructor.
  @param[in]	block	the buffer block of the first page.
  @param[in]	mtr	the mini-transaction context.
  @param[in]	index	the clustered index containing the LOB. */
  first_page_t(buf_block_t *block, mtr_t *mtr, dict_index_t *index)
      : basic_page_t(block, mtr, index) {}

  /** Constructor.
  @param[in]	mtr	the mini-transaction context.
  @param[in]	index	the clustered index containing the LOB. */
  first_page_t(mtr_t *mtr, dict_index_t *index)
      : basic_page_t(nullptr, mtr, index) {}

  /** Set the LOB format version number to 0. */
  void set_version_0() {
    mlog_write_ulint(frame() + OFFSET_VERSION, 0, MLOG_1BYTE, m_mtr);
  }

  /** Obtain the flags value. This has 8 bits of which only the first
  bit is used.
  @return one byte flag */
  uint8_t get_flags() { return (mach_read_from_1(frame() + OFFSET_FLAGS)); }

  /** When the bit is set, the LOB is not partially updatable anymore.
  @return true, if partially updatable.
  @return false, if partially NOT updatable. */
  bool can_be_partially_updated() {
    uint8_t flags = get_flags();
    return (!(flags & 0x01));
  }

  /** Do tablespace import. */
  void import(trx_id_t trx_id);

  /** When the bit is set, the LOB is not partially updatable anymore.
  Enable the bit.
  @param[in]	trx	the current transaction. */
  void mark_cannot_be_partially_updated(trx_t *trx);

  /** Allocate the first page for uncompressed LOB.
  @param[in,out]	alloc_mtr	the allocation mtr.
  @param[in]	is_bulk		true if it is bulk operation
                                  (OPCODE_INSERT_BULK)
  @return the allocated buffer block.*/
  buf_block_t *alloc(mtr_t *alloc_mtr, bool is_bulk);

  /** Free all the index pages.  The list of index pages can be accessed
  by traversing via the FIL_PAGE_NEXT field.*/
  void free_all_index_pages();

  /** Load the first page of LOB with s-latch.
  @param[in]   page_id    the page identifier of the first page.
  @param[in]   page_size  the page size information.
  @return the buffer block of the first page. */
  buf_block_t *load_s(page_id_t page_id, page_size_t page_size) {
    m_block = buf_page_get(page_id, page_size, RW_S_LATCH, m_mtr);
    return (m_block);
  }

  /** Load the first page of LOB with x-latch.
  @param[in]   page_id    the page identifier of the first page.
  @param[in]   page_size  the page size information.
  @return the buffer block of the first page. */
  buf_block_t *load_x(const page_id_t &page_id, const page_size_t &page_size);

  /** Get the buffer block of the LOB first page.
  @return the buffer block. */
  buf_block_t *get_block() { return (m_block); }

  /** Load the file list node from the given location.  An x-latch is taken
  on the page containing the file list node.
  @param[in]	addr	the location of file list node.
  @return		the file list node.*/
  flst_node_t *addr2ptr_x(fil_addr_t &addr) const {
    space_id_t space = dict_index_get_space(m_index);
    const page_size_t page_size = dict_table_page_size(m_index->table);
    return (fut_get_ptr(space, page_size, addr, RW_X_LATCH, m_mtr));
  }

  /** Load the file list node from the given location, assuming that it
  exists in the first page itself.
  @param[in]	addr	the location of file list node.
  @return		the file list node.*/
  flst_node_t *addr2ptr(const fil_addr_t &addr) {
    ut_ad(m_block->page.id.page_no() == addr.page);
    return (buf_block_get_frame(m_block) + addr.boffset);
  }

  /** Load the file list node from the given location.  An s-latch is taken
  on the page containing the file list node.
  @param[in]	addr	the location of file list node.
  @return		the file list node.*/
  flst_node_t *addr2ptr_s(fil_addr_t &addr) {
    space_id_t space = dict_index_get_space(m_index);
    const page_size_t page_size = dict_table_page_size(m_index->table);
    return (fut_get_ptr(space, page_size, addr, RW_S_LATCH, m_mtr));
  }

  /** Load the file list node from the given location.  An s-latch is taken
  on the page containing the file list node. The given cache is checked to
  see if the page is already loaded.
  @param[in]	cache	cache of loaded buffer blocks.
  @param[in]	addr	the location of file list node.
  @return		the file list node.*/
  flst_node_t *addr2ptr_s_cache(std::map<page_no_t, buf_block_t *> &cache,
                                fil_addr_t &addr) const {
    byte *result;
    space_id_t space = dict_index_get_space(m_index);
    const page_size_t page_size = dict_table_page_size(m_index->table);

    auto iter = cache.find(addr.page);

    if (iter == cache.end()) {
      /* Not there in cached blocks.  Add the loaded block to cache. */
      buf_block_t *block = nullptr;
      result = fut_get_ptr(space, page_size, addr, RW_S_LATCH, m_mtr, &block);
      cache.insert(std::make_pair(addr.page, block));
    } else {
      buf_block_t *block = iter->second;
      ut_ad(block->page.id.page_no() == addr.page);
      result = buf_block_get_frame(block) + addr.boffset;
    }
    return (result);
  }

  /** Free the first page.  This is done when all other LOB pages have
  been freed. */
  void dealloc();

  /** Check if the index list is empty or not.
  @return true if empty, false otherwise. */
  bool is_empty() const {
    flst_base_node_t *base = index_list();
    ut_ad(base != nullptr);
    return (flst_get_len(base) == 0);
  }

  /** Allocate one index entry.  If required an index page (of type
  FIL_PAGE_TYPE_LOB_INDEX) will be allocated.
  @param[in]	bulk	true if it is a bulk operation
                          (OPCODE_INSERT_BULK), false otherwise.
  @return the file list node of the index entry. */
  flst_node_t *alloc_index_entry(bool bulk);

  /** Get a pointer to the beginning of the index entry nodes in the
  first part of the page.
  @return	the first index entry node. */
  byte *nodes_begin() const { return (frame() + LOB_PAGE_DATA); }

  /** Calculate and return the payload.
  @return the payload possible in this page. */
  static ulint payload() {
    return (UNIV_PAGE_SIZE - LOB_PAGE_DATA - LOB_PAGE_TRAILER_LEN);
  }

  /** Set the transaction identifier in the first page header without
  generating redo logs.
  @param[in]	id	the transaction identifier. */
  void set_trx_id_no_redo(trx_id_t id) {
    byte *ptr = frame() + OFFSET_TRX_ID;
    mach_write_to_6(ptr, id);
  }

  /** Set the transaction identifier in the first page header.
  @param[in]	id	the transaction identifier. */
  void set_trx_id(trx_id_t id) {
    byte *ptr = frame() + OFFSET_TRX_ID;
    mach_write_to_6(ptr, id);
    mlog_log_string(ptr, 6, m_mtr);
  }

  /** Initialize the LOB version to 1. */
  void init_lob_version() {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(frame() + OFFSET_LOB_VERSION, 1, MLOG_4BYTES, m_mtr);
  }

  /** Get the lob version number.
  @return the lob version. */
  uint32_t get_lob_version() {
    return (mach_read_from_4(frame() + OFFSET_LOB_VERSION));
  }

  /** Increment the lob version by 1. */
  uint32_t incr_lob_version();

  /** Set the last transaction identifier, without generating redo log
  records.
  @param[in]	id	the trx identifier. */
  void set_last_trx_id_no_redo(trx_id_t id) {
    byte *ptr = frame() + OFFSET_LAST_TRX_ID;
    mach_write_to_6(ptr, id);
  }

  /** Set the last transaction identifier.
  @param[in]	id	the trx identifier. */
  void set_last_trx_id(trx_id_t id) {
    byte *ptr = frame() + OFFSET_LAST_TRX_ID;
    mach_write_to_6(ptr, id);
    mlog_log_string(ptr, 6, m_mtr);
  }

  /** Set the last transaction undo number.
  @param[in]	undo_no	the trx undo number. */
  void set_last_trx_undo_no(undo_no_t undo_no) {
    ut_ad(m_mtr != nullptr);

    byte *ptr = frame() + OFFSET_LAST_UNDO_NO;
    mlog_write_ulint(ptr, undo_no, MLOG_4BYTES, m_mtr);
  }

  /** Get the last transaction identifier.
  @return the transaction identifier. */
  trx_id_t get_last_trx_id() const {
    byte *ptr = frame() + OFFSET_LAST_TRX_ID;
    return (mach_read_from_6(ptr));
  }

  /** Get the last transaction undo number.
  @return the transaction undo number. */
  undo_no_t get_last_trx_undo_no() const {
    byte *ptr = frame() + OFFSET_LAST_UNDO_NO;
    return (mach_read_from_4(ptr));
  }

  /** Set the length of data stored in bytes.
  @param[in]	len	amount of data stored in bytes. */
  void set_data_len(ulint len) {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(frame() + OFFSET_DATA_LEN, len, MLOG_4BYTES, m_mtr);
  }

  /** Write as much as possible of the given data into the page.
  @param[in]	trxid	the current transaction.
  @param[in]	data	the data to be written.
  @param[in]	len	the length of the given data.
  @return number of bytes actually written. */
  ulint write(trx_id_t trxid, const byte *&data, ulint &len);

  /** Replace data in the page by making a copy-on-write.
  @param[in]	trx	the current transaction.
  @param[in]	offset	the location where replace operation starts.
  @param[in,out]	ptr	the buffer containing new data. after the
                          call it will point to remaining data.
  @param[in,out]	want	requested amount of data to be replaced.
                          after the call it will contain amount of
                          data yet to be replaced.
  @param[in]	mtr	the mini-transaction context.
  @return	the newly allocated buffer block. */
  buf_block_t *replace(trx_t *trx, ulint offset, const byte *&ptr, ulint &want,
                       mtr_t *mtr);

  /** Replace data in the page inline.
  @param[in]	trx	the current transaction.
  @param[in]	offset	the location where replace operation starts.
  @param[in,out]	ptr	the buffer containing new data. after the
                          call it will point to remaining data.
  @param[in,out]	want	requested amount of data to be replaced.
                          after the call it will contain amount of
                          data yet to be replaced.
  @param[in]	mtr	the mini-transaction context.*/
  void replace_inline(trx_t *trx, ulint offset, const byte *&ptr, ulint &want,
                      mtr_t *mtr);

  ulint get_data_len() const {
    return (mach_read_from_4(frame() + OFFSET_DATA_LEN));
  }

  /** Read data from the first page.
  @param[in]	offset	the offset from where read starts.
  @param[out]	ptr	the output buffer
  @param[in]	want	number of bytes to read.
  @return number of bytes read. */
  ulint read(ulint offset, byte *ptr, ulint want);

  void set_page_type() {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(frame() + FIL_PAGE_TYPE, FIL_PAGE_TYPE_LOB_FIRST,
                     MLOG_2BYTES, m_mtr);
  }

  flst_base_node_t *index_list() const { return (frame() + OFFSET_INDEX_LIST); }

  flst_base_node_t *free_list() const {
    return (frame() + OFFSET_INDEX_FREE_NODES);
  }

  /** Get the number of bytes used to store LOB data in the first page
  of uncompressed LOB.
  @return Number of bytes available for LOB data. */
  static ulint max_space_available() {
    const uint16_t index_array_size = node_count() * index_entry_t::SIZE;

    return (payload() - index_array_size);
  }

  /** Get the number of index entries this page can hold.
  @return Number of index entries this page can hold. */
  constexpr static ulint node_count() {
    /* Each index entry is of size 60 bytes.  We store only 10
    index entries in the first page of the LOB.  This means that
    only 600 bytes are used for index data in the first page of
    LOB. This will help to reserve more space in the first page
    for the LOB data.*/
    return (10);
  }

  std::ostream &print_index_entries(std::ostream &out) const;

  std::ostream &print_index_entries_cache_s(std::ostream &out,
                                            BlockCache &cache) const;

  /** Obtain the location where the data begins.
  @return pointer to location within page where data begins. */
  byte *data_begin() const {
    ut_ad(buf_block_get_page_zip(m_block) == NULL);

    constexpr uint16_t index_array_size = node_count() * index_entry_t::SIZE;

    return (frame() + LOB_PAGE_DATA + index_array_size);
  }

  /** Append data into a LOB first page. */
  ulint append(trx_id_t trxid, byte *&data, ulint &len);

#ifdef UNIV_DEBUG
  /** Validate the first page. */
  bool validate() const;
#endif /* UNIV_DEBUG */

  ulint get_page_type() { return (basic_page_t::get_page_type()); }

  static ulint get_page_type(dict_index_t *index, const page_id_t &page_id,
                             const page_size_t &page_size) {
    mtr_t local_mtr;
    mtr_start(&local_mtr);
    first_page_t first(&local_mtr, index);
    first.load_x(page_id, page_size);
    page_type_t page_type = first.get_page_type();
    mtr_commit(&local_mtr);
    return (page_type);
  }
};

}; /* namespace lob */

#endif /* lob0first_h */
