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
#ifndef zlob0first_h
#define zlob0first_h

#include "fil0types.h"
#include "fut0lst.h"
#include "lob0impl.h"
#include "univ.i"

class Btree_load;

namespace lob {

/** The first page of an zlob. */
struct z_first_page_t {
  /** Version information. One byte. */
  static const ulint OFFSET_VERSION = FIL_PAGE_DATA;

  /** One byte of flag bits.  Currently only one bit (the least
  significant bit) is used, other 7 bits are available for future use.*/
  static const ulint OFFSET_FLAGS = FIL_PAGE_DATA + 1;

  /** LOB version. 4 bytes.*/
  static const uint32_t OFFSET_LOB_VERSION = OFFSET_FLAGS + 1;

  /** The last transaction that modified this LOB. */
  static const ulint OFFSET_LAST_TRX_ID = OFFSET_LOB_VERSION + 4;

  /** The last transaction that modified this LOB. */
  static const ulint OFFSET_LAST_UNDO_NO = OFFSET_LAST_TRX_ID + 6;

  /** The length of compressed data stored in this page. */
  static const ulint OFFSET_DATA_LEN = OFFSET_LAST_UNDO_NO + 4;

  /** The transaction that created data in the data portion. */
  static const ulint OFFSET_TRX_ID = OFFSET_DATA_LEN + 4;

  /** The next index page. */
  static const ulint OFFSET_INDEX_PAGE_NO = OFFSET_TRX_ID + 6;

  /** The next frag nodes page. */
  static const ulint OFFSET_FRAG_NODES_PAGE_NO = OFFSET_INDEX_PAGE_NO + 4;

  /** List of free index entries. */
  static const ulint OFFSET_FREE_LIST = OFFSET_FRAG_NODES_PAGE_NO + 4;

  /** List of index entries. */
  static const ulint OFFSET_INDEX_LIST = OFFSET_FREE_LIST + FLST_BASE_NODE_SIZE;

  /** List of free frag entries. */
  static const ulint OFFSET_FREE_FRAG_LIST =
      OFFSET_INDEX_LIST + FLST_BASE_NODE_SIZE;

  /** List of frag entries. */
  static const ulint OFFSET_FRAG_LIST =
      OFFSET_FREE_FRAG_LIST + FLST_BASE_NODE_SIZE;

  /** Begin of index entries. */
  static const ulint OFFSET_INDEX_BEGIN =
      OFFSET_FRAG_LIST + FLST_BASE_NODE_SIZE;

  /** Given the page size, what is the number of index entries the
  first page can contain. */
  ulint get_n_index_entries() const;

  /** Given the page size, what is the number of frag entries the
  first page can contain. */
  ulint get_n_frag_entries() const;

  ulint size_of_index_entries() const;

  ulint size_of_frag_entries() const {
    return (z_frag_entry_t::SIZE * get_n_frag_entries());
  }

  ulint begin_frag_entries() const {
    return (OFFSET_INDEX_BEGIN + size_of_index_entries());
  }

  ulint begin_data() const {
    return (begin_frag_entries() + size_of_frag_entries());
  }

  bool is_empty() const {
    flst_base_node_t *flst = index_list();
    return (flst_get_len(flst) == 0);
  }

  /** Get the length of the index list.
  @return length of the index list. */
  ulint get_index_list_length() const {
    flst_base_node_t *flst = index_list();
    return (flst_get_len(flst));
  }

  /** Set the version number to zero. */
  inline void set_version_0();

  byte *begin_data_ptr() const { return (frame() + begin_data()); }

  /** Amount of zlob data that can be stored in first page (in bytes). */
  ulint payload() {
    page_size_t page_size(dict_table_page_size(m_index->table));
    ut_ad(begin_data() + FIL_PAGE_DATA_END < page_size.physical());
    return (page_size.physical() - begin_data() - FIL_PAGE_DATA_END);
  }

  z_first_page_t() : m_block(nullptr), m_mtr(nullptr), m_index(nullptr) {}

  z_first_page_t(mtr_t *mtr, dict_index_t *index)
      : m_block(nullptr), m_mtr(mtr), m_index(index) {}

  z_first_page_t(mtr_t *mtr, dict_index_t *index, Btree_load *btree_load)
      : z_first_page_t(nullptr, mtr, index, btree_load) {}

  z_first_page_t(buf_block_t *block) : m_block(block) {}

  z_first_page_t(buf_block_t *block, mtr_t *mtr, dict_index_t *index)
      : z_first_page_t(block, mtr, index, nullptr) {}

  /** Constructor.
  @param[in]  block  the underlying buffer block for this first LOB page.
  @param[in]  mtr    mini transaction context
  @param[in]  index  clustered index to which LOB belongs.
  @param[in]  btree_load  bulk load context object. */
  z_first_page_t(buf_block_t *block, mtr_t *mtr, dict_index_t *index,
                 Btree_load *btree_load)
      : m_block(block), m_mtr(mtr), m_index(index), m_btree_load(btree_load) {}

  buf_block_t *alloc(bool bulk);

  void import(trx_id_t trx_id);

  page_type_t get_page_type() const {
    return (mach_read_from_2(frame() + FIL_PAGE_TYPE));
  }

  /** Load the given page number as the first page in x-latch mode.
  @param[in]    page_no         the first page number.
  @return       the buffer block of the given page number. */
  buf_block_t *load_x(page_no_t page_no) {
    page_id_t page_id(dict_index_get_space(m_index), page_no);
    page_size_t page_size(dict_table_page_size(m_index->table));
    m_block =
        buf_page_get(page_id, page_size, RW_X_LATCH, UT_LOCATION_HERE, m_mtr);
    return (m_block);
  }

  /** Load the first page using given mini-transaction. The first page must
  already be x-latched by the m_mtr.
  @param[in]    mtr   the mini-transaction in which first page is to be loaded.
  @return the buffer block of first page. */
  buf_block_t *load_x(mtr_t *mtr) const {
    ut_ad(mtr_memo_contains(m_mtr, m_block, MTR_MEMO_PAGE_X_FIX));
    buf_block_t *tmp = buf_page_get(m_block->page.id, m_index->get_page_size(),
                                    RW_X_LATCH, UT_LOCATION_HERE, mtr);
    ut_ad(tmp == m_block);
    return (tmp);
  }

  /** Load the first page of the compressed LOB with x-latch.
  @param[in]   page_id   the page identifier of first page
  @param[in]   page_size the page size information of table.
  @return buffer block of the first page. */
  buf_block_t *load_x(const page_id_t &page_id, const page_size_t &page_size);

  /** Load the given page number as the first page in s-latch mode.
  @param[in]    page_no         the first page number.
  @return       the buffer block of the given page number. */
  buf_block_t *load_s(page_no_t page_no) {
    ut_ad(m_block == nullptr);

    page_id_t page_id(dict_index_get_space(m_index), page_no);
    page_size_t page_size(dict_table_page_size(m_index->table));
    m_block =
        buf_page_get(page_id, page_size, RW_S_LATCH, UT_LOCATION_HERE, m_mtr);
    return (m_block);
  }

  /** Deallocate the first page of a compressed LOB. */
  void dealloc();

  /** Set the FIL_PAGE_NEXT to FIL_NULL. */
  void set_next_page_null() { set_next_page_no(FIL_NULL, m_mtr); }

  /** Set the FIL_PAGE_PREV to FIL_NULL. */
  void set_prev_page_null() { set_prev_page_no(FIL_NULL, m_mtr); }

  /** Set the FIL_PAGE_NEXT to the given value.
  @param[in]    page_no   the page number to set in FIL_PAGE_NEXT.
  @param[in]    mtr       mini trx to be used for this modification. */
  inline void set_next_page_no(page_no_t page_no, mtr_t *mtr);

  /** Set the FIL_PAGE_PREV to the given value.
  @param[in]    page_no   the page number to set in FIL_PAGE_PREV.
  @param[in]    mtr       mini trx to be used for this modification. */
  inline void set_prev_page_no(page_no_t page_no, mtr_t *mtr);

  /** Write the space identifier to the page header, without generating
  redo log records.
  @param[in]    space_id        the space identifier. */
  void set_space_id_no_redo(space_id_t space_id) {
    mlog_write_ulint(frame() + FIL_PAGE_SPACE_ID, space_id, MLOG_4BYTES,
                     nullptr);
  }

  /** Initialize the first page. */
  inline void init();

  /** Get the amount of zlob data stored in this page. */
  ulint get_data_len() const {
    return (mach_read_from_4(frame() + OFFSET_DATA_LEN));
  }

  /** Get the page number. */
  page_no_t get_page_no() const { return (m_block->page.id.page_no()); }

  /** Get the page id of the first page of compressed LOB.
  @return page id of the first page of compressed LOB. */
  page_id_t get_page_id() const {
    ut_ad(m_block != nullptr);

    return (m_block->page.id);
  }

  fil_addr_t get_self_addr() const {
    page_no_t page_no = get_page_no();
    uint32_t offset = static_cast<uint32_t>(begin_data());
    return (fil_addr_t(page_no, offset));
  }

  /** All the index pages are singly linked with each other, and
  the first page contains the link to one index page.
  @param[in]  page_no  the page number of an index page. */
  inline void set_index_page_no(page_no_t page_no);

  /** All the index pages are singly linked with each other, and
  the first page contains the link to one index page.
  @param[in]  page_no  the page number of an index page.
  @param[in]  mtr      use this mini transaction context for redo logs. */
  inline void set_index_page_no(page_no_t page_no, mtr_t *mtr);

  /** All the index pages are singly linked with each other, and
  the first page contains the link to one index page. Get that index
  page number.
  @return the index page number. */
  page_no_t get_index_page_no() const {
    return (mach_read_from_4(frame() + OFFSET_INDEX_PAGE_NO));
  }

  /** All the fragment pages are doubly linked with each other, and
  the first page contains the link to one fragment page in FIL_PAGE_PREV. Get
  that frag page number.
  @return the frag page number. */
  page_no_t get_frag_page_no() const { return (m_block->get_prev_page_no()); }

  /** All the fragment pages are doubly linked with each other, and
  the first page contains the link to one fragment page in FIL_PAGE_PREV. Get
  that frag page number.
  @param[in]   mtr   Mini-transaction to use for this read operation.
  @return the frag page number. */
  page_no_t get_frag_page_no(mtr_t *mtr) const {
    return (mtr_read_ulint(frame() + FIL_PAGE_PREV, MLOG_4BYTES, mtr));
  }

#ifdef UNIV_DEBUG
  /** Verify that the page number pointed to by FIL_PAGE_PREV of the first page
  of LOB is indeed a fragment page.  It uses its own mtr internally.
  @return true if it is a fragment page, false otherwise. */
  bool verify_frag_page_no();
#endif /* UNIV_DEBUG */

  /** All the fragment pages (@see z_frag_page_t) are doubly linked with each
  other, and the first page contains the link to one fragment page in
  FIL_PAGE_PREV.
  @param[in]  mtr      Mini-transaction for this modification.
  @param[in]  page_no  The page number of a fragment page. */
  void set_frag_page_no(mtr_t *mtr, page_no_t page_no) {
    ut_ad(verify_frag_page_no());
    set_prev_page_no(page_no, mtr);
  }

  /** All the fragment pages (@see z_frag_page_t) are doubly linked with each
  other, and the first page contains the link to one fragment page in
  FIL_PAGE_PREV.
  @param[in]  page_no  the page number of a fragment page. */
  void set_frag_page_no(page_no_t page_no) {
    ut_ad(verify_frag_page_no());
    set_prev_page_no(page_no, m_mtr);
  }

  /** All the frag node pages (@see z_frag_node_page_t) are singly linked with
  each other, and the first page contains the link to the last allocated frag
  node page. The last allocated FIL_PAGE_TYPE_ZLOB_FRAG_ENTRY  page will be the
  first in this list. This list is used to free these pages.
  @param[in]  page_no  the page number of an frag node page. */
  inline void set_frag_node_page_no(page_no_t page_no);

  /** All the frag node pages (@see z_frag_node_page_t) are singly linked with
  each other, and the first page contains the link to the last allocated frag
  node page. The last allocated FIL_PAGE_TYPE_ZLOB_FRAG_ENTRY  page will be the
  first in this list. This list is used to free these pages.
  @param[in]  page_no  the page number of an frag node page.
  @param[in]  mtr      mini trx context to generate redo logs. */
  inline void set_frag_node_page_no(page_no_t page_no, mtr_t *mtr);

  /** Free all the z_frag_page_t pages. All the z_frag_page_t pages are
  singly linked to each other.  The head of the list is maintained in the
  first page.
  @return the number of pages freed. */
  size_t free_all_frag_node_pages();

  /** Free all the index pages.
  @return the number of pages freed. */
  size_t free_all_index_pages();

  /** Free all the fragment pages.
  @return the number of pages freed. */
  size_t free_all_frag_pages();

 private:
  /** Free all the fragment pages when the next page of the first LOB page IS
   NOT USED to link the fragment pages.
  @return the number of pages freed. */
  size_t free_all_frag_pages_old();

  /** Free all the fragment pages when the next page of the first LOB page IS
   USED to link the fragment pages.
  @return the number of pages freed. */
  size_t free_all_frag_pages_new();

 public:
  /** Free all the data pages.
  @return the number of pages freed. */
  size_t free_all_data_pages();

  /** All the frag node pages are singly linked with each other, and the
  first page contains the link to one frag node page. Get that frag node
  page number.
  @return the index page number. */
  page_no_t get_frag_node_page_no() {
    return (mach_read_from_4(frame() + OFFSET_FRAG_NODES_PAGE_NO));
  }

  /** Set the page type to FIL_PAGE_TYPE_UNKNOWN.  This is done while
  deallocating this page. */
  void set_page_type_unknown() {
    ut_ad(m_mtr != nullptr);
    mlog_write_ulint(frame() + FIL_PAGE_TYPE, FIL_PAGE_TYPE_UNKNOWN,
                     MLOG_2BYTES, m_mtr);
  }

  /** Set the page type to FIL_PAGE_TYPE_ZLOB_FIRST. */
  inline void set_page_type();

  /** Set the length of data stored in this page.
  @param[in]  len  length of data stored in this page. */
  inline void set_data_len(ulint len);

  /** Update the trx id in the header.
  @param[in]    tid     the given transaction identifier. */
  inline void set_trx_id(trx_id_t tid);

  /** Update the trx id in the header, without generating redo
  log records.
  @param[in]    tid     the given transaction identifier. */
  void set_trx_id_no_redo(trx_id_t tid) {
    byte *ptr = frame() + OFFSET_TRX_ID;
    mach_write_to_6(ptr, tid);
  }

  /** Initialize the LOB version to 1. */
  inline void init_lob_version();

  /** Get the LOB version
  @return the LOB version. */
  uint32_t get_lob_version() {
    return (mach_read_from_4(frame() + OFFSET_LOB_VERSION));
  }

  /** Increment LOB version by 1. */
  uint32_t incr_lob_version();

  /** Get one byte of flags
  @return one byte of flags. */
  uint8_t get_flags() { return (mach_read_from_1(frame() + OFFSET_FLAGS)); }

  /** When the bit is set, the LOB is not partially updatable anymore.
  @return true, if partially updatable.
  @return false, if partially NOT updatable. */
  bool can_be_partially_updated() {
    uint8_t flags = get_flags();
    return (!(flags & 0x01));
  }

  /** When the bit is set, the LOB is not partially updatable anymore.
  Enable the bit.
  @param[in]    trx     the current transaction.*/
  void mark_cannot_be_partially_updated(trx_t *trx);

  /** Set the last transaction that modified this blob.
  @param[in]  tid  Id of the trx that last modified this blob. */
  inline void set_last_trx_id(trx_id_t tid);

  /** Update the last transaction identifier in the header, without
  generating redo logs.
  @param[in]    tid     given transaction identifier.*/
  void set_last_trx_id_no_redo(trx_id_t tid) {
    byte *ptr = frame() + OFFSET_LAST_TRX_ID;
    mach_write_to_6(ptr, tid);
  }

  void set_last_trx_undo_no(undo_no_t undo_no) {
    ut_ad(m_mtr != nullptr);

    byte *ptr = frame() + OFFSET_LAST_UNDO_NO;
    mlog_write_ulint(ptr, undo_no, MLOG_4BYTES, m_mtr);
  }

  trx_id_t get_last_trx_id() const {
    byte *ptr = frame() + OFFSET_LAST_TRX_ID;
    return (mach_read_from_6(ptr));
  }

  undo_no_t get_last_trx_undo_no() const {
    byte *ptr = frame() + OFFSET_LAST_UNDO_NO;
    return (mach_read_from_4(ptr));
  }

  flst_base_node_t *free_list() const { return (frame() + OFFSET_FREE_LIST); }

  flst_base_node_t *index_list() const { return (frame() + OFFSET_INDEX_LIST); }

  flst_base_node_t *free_frag_list() const {
    return (frame() + OFFSET_FREE_FRAG_LIST);
  }

  flst_base_node_t *frag_list() const { return (frame() + OFFSET_FRAG_LIST); }

  inline void init_frag_entries();

  void init_index_entries();

  /** Allocate a fragment of the given size.  This involves finding a
  fragment page, that has space to store len bytes of data. If necessary,
  allocate a new fragment page.
  @param[in]    bulk            true if it is bulk operation
                                  (OPCODE_INSERT_BULK), false otherwise.
  @param[in]    len             length of data to be stored in
                                  fragment page.
  @param[out]   frag_page       the fragment page with the needed
                                  free space.
  @param[out]   entry           fragment page entry representing frag_page.
  @return fragment identifier within the fragment page.
  @return FRAG_ID_NULL if fragment could not be allocated. */
  frag_id_t alloc_fragment(bool bulk, ulint len, z_frag_page_t &frag_page,
                           z_frag_entry_t &entry);

  /** Allocate one index entry.  If there is no free index entry,
  allocate an index page (a page full of z_index_entry_t objects)
  and service the request.
  @return the allocated index entry. */
  z_index_entry_t alloc_index_entry(bool bulk);

  /** Allocate one frag page entry.  If there is no free frag
  entry, allocate an frag node page (a page full of z_frag_entry_t
  objects) and service the request.
  @return the allocated frag entry. */
  z_frag_entry_t alloc_frag_entry(bool bulk);

  /** Print the index entries. */
  std::ostream &print_index_entries(std::ostream &out) const;

  /** Print the index entries. */
  std::ostream &print_frag_entries(std::ostream &out) const;

  /** Print the page. */
  std::ostream &print(std::ostream &out) const;

  byte *frame() const { return (buf_block_get_frame(m_block)); }

  /** Load the page, in x-latch mode, containing the given file address.
  @param[in]    addr    given file address
  @return       the file list node pointer. */
  flst_node_t *addr2ptr_x(fil_addr_t &addr) const {
    return (addr2ptr_x(addr, m_mtr));
  }

  /** Load the page, in x-latch mode, containing the given file address.
  @param[in]    addr    given file address
  @param[in]    mtr     the mini-transaction context to be used.
  @return       the file list node pointer. */
  inline flst_node_t *addr2ptr_x(fil_addr_t &addr, mtr_t *mtr) const;

  /** Load the page, in s-latch mode, containing the given file address.
  @param[in]    addr    given file address
  @return       the file list node pointer. */
  flst_node_t *addr2ptr_s(fil_addr_t &addr) {
    space_id_t space = dict_index_get_space(m_index);
    const page_size_t page_size = dict_table_page_size(m_index->table);
    return (fut_get_ptr(space, page_size, addr, RW_S_LATCH, m_mtr));
  }

  /** Load the entry available in the given file address.
  @param[in]    addr    file address
  @param[out]   entry   the entry to be loaded.*/
  void load_entry_s(fil_addr_t &addr, z_index_entry_t &entry);

  /** Load the entry available in the given file address.
  @param[in]    addr    file address
  @param[out]   entry   the entry to be loaded.*/
  void load_entry_x(fil_addr_t &addr, z_index_entry_t &entry);

  /** Free all the pages of the zlob.
  @return the total number of pages freed. */
  size_t destroy();

  /** Free all the pages of the zlob except the first page.
  @return the total number of pages freed. */
  size_t make_empty();

#ifdef UNIV_DEBUG
 private:
  /** Validate the LOB.  This is a costly function.  We need to avoid using
  this directly, instead call z_first_page_t::validate().
  @return true if valid, false otherwise. */
  bool validate_low();

 public:
  /** Validate the LOB.
  @return true if valid, false otherwise. */
  bool validate() {
    static const uint32_t FREQ = 50;
    static std::atomic<uint32_t> n{0};

    bool valid = true;
    if (++n % FREQ == 0) {
      valid = validate_low();
    }
    return (valid);
  }
#endif /* UNIV_DEBUG */

  /** Get the buffer block of the first page of LOB.
  @return the buffer block of the first page of LOB. */
  buf_block_t *get_block() const { return (m_block); }

 public:
  void set_mtr(mtr_t *mtr) { m_mtr = mtr; }

  /** Restart the given mtr. The first page must already be x-latched by the
  m_mtr.
  @param[in]   mtr   the mini-transaction context which is to be restarted. */
  void restart_mtr(mtr_t *mtr) {
    ut_ad(mtr != m_mtr);

    mtr_commit(mtr);
    mtr_start(mtr);
    mtr->set_log_mode(m_mtr->get_log_mode());
    load_x(mtr);
  }

  /** Get the underlying bulk load context object.
  @return bulk load context object. */
  inline Btree_load *get_btree_load() { return m_btree_load; }

  /** Flush the data extents (only applicable for bulk load).
  @return DB_SUCCESS on success, error on failure. */
  dberr_t flush_data_extents();

 private:
  /** The buffer block of the first page. */
  buf_block_t *m_block{};

  /** The mini-transaction context. */
  mtr_t *m_mtr{};

  /** The index dictionary object. */
  dict_index_t *m_index{};

  /** Bulk load context object. */
  Btree_load *m_btree_load{};
};  // struct z_first_page_t

flst_node_t *z_first_page_t::addr2ptr_x(fil_addr_t &addr, mtr_t *mtr) const {
  ut_ad(mtr != nullptr || m_block->is_memory());
  ut_ad(mtr == nullptr || m_btree_load == nullptr);
  const space_id_t space = dict_index_get_space(m_index);
  const page_size_t page_size = dict_table_page_size(m_index->table);
  return fut_get_ptr(space, page_size, addr, RW_X_LATCH, mtr, nullptr,
                     m_btree_load);
}

void z_first_page_t::set_last_trx_id(trx_id_t tid) {
  ut_ad(m_mtr != nullptr || m_block->is_memory());
  byte *ptr = frame() + OFFSET_LAST_TRX_ID;
  mach_write_to_6(ptr, tid);
  if (m_mtr != nullptr) {
    mlog_log_string(ptr, 6, m_mtr);
  }
}

void z_first_page_t::init_lob_version() {
  ut_ad(m_mtr != nullptr || m_block->is_memory());
  mlog_write_ulint(frame() + OFFSET_LOB_VERSION, 1, MLOG_4BYTES, m_mtr);
}

void z_first_page_t::set_index_page_no(page_no_t page_no) {
  ut_ad(m_mtr != nullptr || m_block->is_memory());
  set_index_page_no(page_no, m_mtr);
}

void z_first_page_t::set_index_page_no(page_no_t page_no, mtr_t *mtr) {
  ut_ad(mtr != nullptr || m_block->is_memory());
  mlog_write_ulint(frame() + OFFSET_INDEX_PAGE_NO, page_no, MLOG_4BYTES, mtr);
}

void z_first_page_t::set_frag_node_page_no(page_no_t page_no) {
  ut_ad(m_mtr != nullptr || m_block->is_memory());
  set_frag_node_page_no(page_no, m_mtr);
}

void z_first_page_t::set_frag_node_page_no(page_no_t page_no, mtr_t *mtr) {
  ut_ad(mtr != nullptr || m_block->is_memory());
  mlog_write_ulint(frame() + OFFSET_FRAG_NODES_PAGE_NO, page_no, MLOG_4BYTES,
                   mtr);
}

void z_first_page_t::init_frag_entries() {
  ut_ad(m_mtr != nullptr || m_block->is_memory());
  ut_ad(m_mtr == nullptr || m_btree_load == nullptr);
  flst_base_node_t *free_frag_lst = free_frag_list();
  ulint n = get_n_frag_entries();
  for (ulint i = 0; i < n; ++i) {
    flst_node_t *ptr = frame() + begin_frag_entries();
    ptr += (i * z_frag_entry_t::SIZE);
    z_frag_entry_t frag_entry(ptr, m_mtr, m_btree_load);
    frag_entry.init();
    frag_entry.push_back(free_frag_lst);
  }
}

void z_first_page_t::set_trx_id(trx_id_t tid) {
  ut_ad(m_mtr != nullptr || m_block->is_memory());
  byte *ptr = frame() + OFFSET_TRX_ID;
  mach_write_to_6(ptr, tid);
  if (m_mtr != nullptr) {
    mlog_log_string(ptr, 6, m_mtr);
  }
}

void z_first_page_t::set_prev_page_no(page_no_t page_no, mtr_t *mtr) {
  ut_ad(m_mtr != nullptr || m_block->is_memory());

  mlog_write_ulint(frame() + FIL_PAGE_PREV, page_no, MLOG_4BYTES, mtr);
}

void z_first_page_t::set_next_page_no(page_no_t page_no, mtr_t *mtr) {
  ut_ad(m_mtr != nullptr || m_block->is_memory());

  mlog_write_ulint(frame() + FIL_PAGE_NEXT, page_no, MLOG_4BYTES, mtr);
}

void z_first_page_t::set_data_len(ulint len) {
  ut_ad(m_mtr != nullptr || m_block->is_memory());

  mlog_write_ulint(frame() + OFFSET_DATA_LEN, len, MLOG_4BYTES, m_mtr);
}

void z_first_page_t::set_version_0() {
  ut_ad(m_mtr != nullptr || m_block->is_memory());

  mlog_write_ulint(frame() + OFFSET_VERSION, 0, MLOG_1BYTE, m_mtr);
}

void z_first_page_t::set_page_type() {
  ut_ad(m_mtr != nullptr || m_block->is_memory());

  mlog_write_ulint(frame() + FIL_PAGE_TYPE, FIL_PAGE_TYPE_ZLOB_FIRST,
                   MLOG_2BYTES, m_mtr);
}

void z_first_page_t::init() {
  ut_ad(m_mtr != nullptr || m_block->is_memory());

  set_page_type();
  set_version_0();
  set_data_len(0);
  set_next_page_null();
  set_prev_page_null();
  set_trx_id(0);
  flst_base_node_t *flst = free_list();
  flst_init(flst, m_mtr);
  flst_base_node_t *ilst = index_list();
  flst_init(ilst, m_mtr);
  flst_base_node_t *free_frag_lst = free_frag_list();
  flst_init(free_frag_lst, m_mtr);
  flst_base_node_t *frag_lst = frag_list();
  flst_init(frag_lst, m_mtr);
  init_index_entries();
  init_frag_entries();
  set_frag_node_page_no(FIL_NULL);
  set_index_page_no(FIL_NULL);
}

/** Overloading the global output parameter to print object of type
z_first_page_t into the given output stream.
@param[in,out]          out     output stream.
@param[in]              obj     object to be printed.
@return the output stream. */
inline std::ostream &operator<<(std::ostream &out, const z_first_page_t &obj) {
  return (obj.print(out));
}

} /* namespace lob */

#endif /* zlob0first_h */
