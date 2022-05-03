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
#ifndef zlob0index_h
#define zlob0index_h

#include "fil0fil.h"
#include "fut0lst.h"
#include "lob0impl.h"

namespace lob {

/** In-memory copy of the information from z_index_entry_t.  */
struct z_index_entry_mem_t {
  z_index_entry_mem_t()
      : m_trx_id(0), m_z_page_no(FIL_NULL), m_z_frag_id(FRAG_ID_NULL) {}

  /** The location of this index entry node. */
  fil_addr_t m_self;

  fil_addr_t m_prev;
  fil_addr_t m_next;
  flst_bnode_t m_versions;
  trx_id_t m_trx_id;
  trx_id_t m_trx_id_modifier;
  undo_no_t m_trx_undo_no;
  undo_no_t m_trx_undo_no_modifier;
  page_no_t m_z_page_no;
  frag_id_t m_z_frag_id;

  /** Uncompressed data length. */
  ulint m_data_len;

  /** Compressed data length. */
  ulint m_z_data_len;

  std::ostream &print(std::ostream &out) const;

  /** Initialize all the members. */
  void reset() {
    m_self = fil_addr_null;
    m_prev = fil_addr_null;
    m_next = fil_addr_null;
    m_versions.reset();
    m_trx_id = 0;
    m_trx_id_modifier = 0;
    m_trx_undo_no = 0;
    m_trx_undo_no_modifier = 0;
    m_z_page_no = FIL_NULL;
    m_z_frag_id = 0;
    m_data_len = 0;
    m_z_data_len = 0;
  }

  bool is_null() { return (m_self.is_equal(fil_addr_null)); }
};

inline std::ostream &operator<<(std::ostream &out,
                                const z_index_entry_mem_t &obj) {
  return (obj.print(out));
}

/** An index entry pointing to one zlib stream. */
struct z_index_entry_t {
  /** Offset with index entry pointing to the prev index entry. */
  static const ulint OFFSET_PREV = 0;

  /** Offset with index entry pointing to the next index entry. */
  static const ulint OFFSET_NEXT = OFFSET_PREV + FIL_ADDR_SIZE;

  /** Offset within index entry pointing to base node of list of
  versions.*/
  static const ulint OFFSET_VERSIONS = OFFSET_NEXT + FIL_ADDR_SIZE;

  /** Offset within index entry pointing to creator trxid.*/
  static const ulint OFFSET_TRXID = OFFSET_VERSIONS + FLST_BASE_NODE_SIZE;

  /** The modifier trx id. */
  static const ulint OFFSET_TRXID_MODIFIER = OFFSET_TRXID + 6;

  /** Offset within index entry pointing to trx undo no.*/
  static const ulint OFFSET_TRX_UNDO_NO = OFFSET_TRXID_MODIFIER + 6;

  /** Offset within index entry pointing to modifier trx undo no.*/
  static const ulint OFFSET_TRX_UNDO_NO_MODIFIER = OFFSET_TRX_UNDO_NO + 4;

  /** Offset within index entry pointing to page number where zlib
  stream starts. This could be a data page or a fragment page. */
  static const ulint OFFSET_Z_PAGE_NO = OFFSET_TRX_UNDO_NO_MODIFIER + 4;

  /** Offset within index entry pointing to location of zlib stream.*/
  static const ulint OFFSET_Z_FRAG_ID = OFFSET_Z_PAGE_NO + 4;

  /** Offset within index entry pointing to uncompressed data
  len (bytes).*/
  static const ulint OFFSET_DATA_LEN = OFFSET_Z_FRAG_ID + 2;

  /** Offset within index entry pointing to compressed data len
  (bytes).*/
  static const ulint OFFSET_ZDATA_LEN = OFFSET_DATA_LEN + 4;

  /** LOB version */
  static const ulint OFFSET_LOB_VERSION = OFFSET_ZDATA_LEN + 4;

  /** Total size of one index entry. */
  static const ulint SIZE = OFFSET_LOB_VERSION + 4;

  /** Constructor. */
  z_index_entry_t(flst_node_t *node, mtr_t *mtr) : m_node(node), m_mtr(mtr) {}

  /** Constructor. */
  z_index_entry_t(flst_node_t *node, mtr_t *mtr, dict_index_t *index)
      : m_node(node), m_mtr(mtr), m_index(index) {}

  /** Constructor
  @param[in]    mtr     the mini-transaction
  @param[in]    index   the clustered index to which LOB belongs. */
  z_index_entry_t(mtr_t *mtr, dict_index_t *index)
      : m_node(nullptr),
        m_mtr(mtr),
        m_index(index),
        m_block(nullptr),
        m_page_no(FIL_NULL) {}

  /** Constructor
  @param[in]    node    the location where index entry starts. */
  z_index_entry_t(flst_node_t *node)
      : m_node(node),
        m_mtr(nullptr),
        m_index(nullptr),
        m_block(nullptr),
        m_page_no(FIL_NULL) {}

  /** Default constructor */
  z_index_entry_t()
      : m_node(nullptr),
        m_mtr(nullptr),
        m_index(nullptr),
        m_block(nullptr),
        m_page_no(FIL_NULL) {}

  void set_index(dict_index_t *index) { m_index = index; }

  /** Point to another index entry.
  @param[in]  node  point to this file list node. */
  void reset(flst_node_t *node) { m_node = node; }

  /** Point to another index entry.
  @param[in]  entry  another index entry.*/
  void reset(const z_index_entry_t &entry) { m_node = entry.m_node; }

  /** Initialize an index entry to some sane value. */
  void init() {
    ut_ad(m_mtr != nullptr);

    set_prev_null();
    set_next_null();
    set_versions_null();
    set_trx_id(0);
    set_trx_undo_no(0);
    set_z_page_no(FIL_NULL);
    set_z_frag_id(FRAG_ID_NULL);
    set_data_len(0);
    set_zdata_len(0);
  }

  /** Determine if the current index entry be rolled back.
  @param[in]    trxid           the transaction that is being rolled
                                  back.
  @param[in]    undo_no         the savepoint undo number of trx,
                                  up to which rollback happens.
  @return true if this entry can be rolled back, false otherwise. */
  bool can_rollback(trx_id_t trxid, undo_no_t undo_no) {
    /* For rollback, make use of creator trx id. */
    return ((trxid == get_trx_id()) && (get_trx_undo_no() >= undo_no));
  }

  /** Determine if the current index entry be purged.
  @param[in]    trxid           the transaction that is being purged.
  @param[in]    undo_no         the undo number of trx.
  @return true if this entry can be purged, false otherwise. */
  bool can_be_purged(trx_id_t trxid, undo_no_t undo_no) {
    return ((trxid == get_trx_id_modifier()) &&
            (get_trx_undo_no_modifier() == undo_no));
  }

  /** Purge one index entry.
  @param[in]    index           index to which LOB belongs.
  @param[in]    first           first page of LOB.
  @param[in,out]        lst             list from which this entry will be
                                  removed.
  @param[in,out]        free_list       list to which this entry will be
                                  added.*/
  fil_addr_t purge_version(dict_index_t *index, z_first_page_t &first,
                           flst_base_node_t *lst, flst_base_node_t *free_list);

  /** Purge the current index entry. An index entry points to either a
  FIRST page or DATA page.  That LOB page will be freed if it is DATA
  page.  A FIRST page should not be freed. */
  void purge(dict_index_t *index, z_first_page_t &first);

  /** Remove this node from the given list.
  @param[in]    bnode   the base node of the list from which to remove
                          current node. */
  void remove(flst_base_node_t *bnode) {
    ut_ad(m_mtr != nullptr);

    flst_remove(bnode, m_node, m_mtr);
  }

  /** Insert the given index entry after the current index entry.
  @param[in]    base    the base node of the file based list.
  @param[in]    entry   the new node to be inserted.*/
  void insert_after(flst_base_node_t *base, z_index_entry_t &entry) {
    ut_ad(m_mtr != nullptr);

    flst_insert_after(base, m_node, entry.get_node(), m_mtr);
  }

  void insert_before(flst_base_node_t *base, z_index_entry_t &entry) {
    ut_ad(m_mtr != nullptr);

    flst_insert_before(base, entry.get_node(), m_node, m_mtr);
  }

  /** Add this node as the last node in the given list.
  @param[in]  bnode  the base node of the file list. */
  void push_back(flst_base_node_t *bnode) {
    ut_ad(m_mtr != nullptr);

    flst_add_last(bnode, m_node, m_mtr);
  }

  /** Add this node as the last node in the given list.
  @param[in]  bnode  the base node of the file list. */
  void push_front(flst_base_node_t *bnode) {
    ut_ad(m_mtr != nullptr);
    flst_add_first(bnode, m_node, m_mtr);
  }

  /** Set the previous index entry as null. */
  void set_prev_null() {
    flst_write_addr(m_node + OFFSET_PREV, fil_addr_null, m_mtr);
  }

  /** Get the location of previous index entry. */
  fil_addr_t get_prev() const {
    return (flst_read_addr(m_node + OFFSET_PREV, m_mtr));
  }

  /** Get the location of next index entry.
  @return the file address of the next index entry. */
  fil_addr_t get_next() const {
    return (flst_read_addr(m_node + OFFSET_NEXT, m_mtr));
  }

  /** Set the next index entry as null. */
  void set_next_null() {
    ut_ad(m_mtr != nullptr);
    flst_write_addr(m_node + OFFSET_NEXT, fil_addr_null, m_mtr);
  }

  /** Set the versions list as null. */
  void set_versions_null() {
    flst_base_node_t *bnode = get_versions_list();
    flst_init(bnode, m_mtr);
  }

  /** Get the base node of the list of versions. */
  flst_base_node_t *get_versions_list() const {
    return (m_node + OFFSET_VERSIONS);
  }

  /** Get the base node of the list of versions. */
  flst_bnode_t get_versions_mem() const {
    ut_ad(m_mtr != nullptr);
    flst_base_node_t *node = get_versions_list();
    return (flst_bnode_t(node, m_mtr));
  }

  trx_id_t get_trx_id() const {
    return (mach_read_from_6(m_node + OFFSET_TRXID));
  }

  trx_id_t get_trx_id_modifier() const {
    return (mach_read_from_6(m_node + OFFSET_TRXID_MODIFIER));
  }

  /** Get the undo number of the creator transaction.  This is used
  for rollback purposes.
  @return the undo number of creator trx. */
  undo_no_t get_trx_undo_no() const {
    byte *ptr = m_node + OFFSET_TRX_UNDO_NO;
    return (mach_read_from_4(ptr));
  }

  /** Get the undo number of the modifier transaction.  This is used
  for purging purposes.
  @return the undo number of modifier trx. */
  undo_no_t get_trx_undo_no_modifier() const {
    byte *ptr = m_node + OFFSET_TRX_UNDO_NO_MODIFIER;
    return (mach_read_from_4(ptr));
  }

  /** Set the trx identifier to given value, without generating redo
  log records.
  @param[in]    id      the given trx identifier.*/
  void set_trx_id_no_redo(trx_id_t id) {
    byte *ptr = m_node + OFFSET_TRXID;
    mach_write_to_6(ptr, id);
  }

  /** Set the trx identifier to given value.
  @param[in]    id      the given trx identifier.*/
  void set_trx_id(trx_id_t id) {
    ut_ad(m_mtr != nullptr);
    byte *ptr = m_node + OFFSET_TRXID;
    mach_write_to_6(ptr, id);
    mlog_log_string(ptr, 6, m_mtr);
  }

  /** Set the modifier trxid to the given value.
  @param[in]    id      the modifier trxid.*/
  void set_trx_id_modifier(trx_id_t id) {
    ut_ad(m_mtr != nullptr);

    byte *ptr = m_node + OFFSET_TRXID_MODIFIER;
    mach_write_to_6(ptr, id);
    mlog_log_string(ptr, 6, m_mtr);
  }

  /** Set the modifier trxid to the given value, without generating
  redo log records.
  @param[in]    id      the modifier trxid.*/
  void set_trx_id_modifier_no_redo(trx_id_t id) {
    byte *ptr = m_node + OFFSET_TRXID_MODIFIER;
    mach_write_to_6(ptr, id);
  }

  /** Set the undo number of the creator trx.
  @param[in]    undo_no         the undo number value.*/
  void set_trx_undo_no(undo_no_t undo_no) {
    ut_ad(m_mtr != nullptr);
    byte *ptr = m_node + OFFSET_TRX_UNDO_NO;
    mlog_write_ulint(ptr, undo_no, MLOG_4BYTES, m_mtr);
  }

  /** Set the undo number of the modifier trx.
  @param[in]    undo_no         the undo number value.*/
  void set_trx_undo_no_modifier(undo_no_t undo_no) {
    ut_ad(m_mtr != nullptr);
    byte *ptr = m_node + OFFSET_TRX_UNDO_NO_MODIFIER;
    mlog_write_ulint(ptr, undo_no, MLOG_4BYTES, m_mtr);
  }

  page_no_t get_z_page_no() const {
    return (mach_read_from_4(m_node + OFFSET_Z_PAGE_NO));
  }

  /** Set the page number pointed to by this index entry to FIL_NULL.
   @param[in]   mtr    The mini-transaction used for this modification. */
  void set_z_page_no_null(mtr_t *mtr) {
    mlog_write_ulint(m_node + OFFSET_Z_PAGE_NO, FIL_NULL, MLOG_4BYTES, mtr);
  }

  /** Free the data pages pointed to by this index entry.
  @param[in]   mtr   the mini-transaction used to free the pages.
  @return the number of pages freed. */
  size_t free_data_pages(mtr_t *mtr);

  /** Set the page number pointed to by this index entry to given value.
   @param[in]   page_no    Page number to be put in index entry. */
  void set_z_page_no(page_no_t page_no) {
    ut_ad(m_mtr != nullptr);
    mlog_write_ulint(m_node + OFFSET_Z_PAGE_NO, page_no, MLOG_4BYTES, m_mtr);
  }

  page_no_t get_z_frag_id() const {
    return (mach_read_from_2(m_node + OFFSET_Z_FRAG_ID));
  }

  void set_z_frag_id(frag_id_t id) {
    ut_ad(m_mtr != nullptr);
    mlog_write_ulint(m_node + OFFSET_Z_FRAG_ID, id, MLOG_2BYTES, m_mtr);
  }

  /** Get the uncompressed data length in bytes. */
  ulint get_data_len() const {
    return (mach_read_from_4(m_node + OFFSET_DATA_LEN));
  }

  /** Set the uncompressed data length in bytes.
  @param[in]  len  the uncompressed data length in bytes */
  void set_data_len(ulint len) {
    ut_ad(m_mtr != nullptr);
    mlog_write_ulint(m_node + OFFSET_DATA_LEN, len, MLOG_4BYTES, m_mtr);
  }

  /** Get the compressed data length in bytes. */
  ulint get_zdata_len() const {
    return (mach_read_from_4(m_node + OFFSET_ZDATA_LEN));
  }

  /** Set the compressed data length in bytes.
  @param[in]  len  the compressed data length in bytes */
  void set_zdata_len(ulint len) {
    ut_ad(m_mtr != nullptr);
    mlog_write_ulint(m_node + OFFSET_ZDATA_LEN, len, MLOG_4BYTES, m_mtr);
  }

  /** Get the LOB version. */
  uint32_t get_lob_version() const {
    return (mach_read_from_4(m_node + OFFSET_LOB_VERSION));
  }

  /** Set the LOB version .
  @param[in]  version  the lob version. */
  void set_lob_version(ulint version) {
    ut_ad(m_mtr != nullptr);
    mlog_write_ulint(m_node + OFFSET_LOB_VERSION, version, MLOG_4BYTES, m_mtr);
  }

  /* The given entry becomes the old version of the current entry.
  Move the version base node from old entry to current entry.
  @param[in]  entry  the old entry */
  void set_old_version(z_index_entry_t &entry);

  /** The current index entry points to a latest LOB page.  It may or
  may not have older versions.  If older version is there, bring it
  back to the index list from the versions list.  Then remove the
  current entry from the index list.  Move the versions list from
  current entry to older entry.
  @param[in]    index   the index in which LOB exists.
  @param[in]    first   The first lob page containing index list and free
  list. */
  fil_addr_t make_old_version_current(dict_index_t *index,
                                      z_first_page_t &first);

  flst_node_t *get_node() { return (m_node); }
  bool is_null() const { return (m_node == nullptr); }

  std::ostream &print(std::ostream &out) const;
  std::ostream &print_pages(std::ostream &out) const;

  /** Load the page (in shared mode) whose number was cached.
  @return       the buffer block of the page loaded. */
  buf_block_t *load_s() {
    ut_ad(m_page_no != FIL_NULL);

    page_id_t page_id(dict_index_get_space(m_index), m_page_no);
    page_size_t page_size(dict_table_page_size(m_index->table));
    m_block =
        buf_page_get(page_id, page_size, RW_S_LATCH, UT_LOCATION_HERE, m_mtr);
    return (m_block);
  }

  /** Load the given file address in s mode.
  @param[in]    addr    the file address of the required node. */
  void load_s(fil_addr_t &addr) {
    space_id_t space = dict_index_get_space(m_index);
    const page_size_t page_size = dict_table_page_size(m_index->table);
    m_node = fut_get_ptr(space, page_size, addr, RW_S_LATCH, m_mtr, &m_block);
    m_page_no = m_block->get_page_no();
  }

  /** Load the given file address in x mode.
  @param[in]    addr    the file address of the required node. */
  void load_x(fil_addr_t &addr) {
    space_id_t space = dict_index_get_space(m_index);
    const page_size_t page_size = dict_table_page_size(m_index->table);
    m_node = fut_get_ptr(space, page_size, addr, RW_X_LATCH, m_mtr, &m_block);
    m_page_no = m_block->get_page_no();
  }

  /** Read the given LOB index entry.
  @param[in]    entry_mem       the LOB index entry. */
  void read(z_index_entry_mem_t &entry_mem) const;

  /** Read the given LOB index entry and then commit the mtr.
  @param[in]    entry_mem       the LOB index entry. */
  void read_and_commit(z_index_entry_mem_t &entry_mem) {
    read(entry_mem);
    mtr_commit(m_mtr);
    m_node = nullptr;
  }

  /** Get the location of the current index entry. */
  fil_addr_t get_self() const;

 private:
  /** Move the version base node from current entry to the given entry.
  @param[in]    entry   The index entry to which the version base
                          node is moved to. */
  void move_version_base_node(z_index_entry_t &entry);

  /** The file list node in a db page. This node is persisted. */
  flst_node_t *m_node;

  /** A mini-transaction. */
  mtr_t *m_mtr;

  /** The index containing the LOB. */
  dict_index_t *m_index;

  /** The buffer block in which this entry exists.  While reading data
  from m_node, appropriate latches must be held on this block. */
  buf_block_t *m_block;

  /** The page number in which this entry is available.  This
  information will be cached and can be used to reload the page
  conveniently. */
  page_no_t m_page_no;
};

inline std::ostream &operator<<(std::ostream &out, const z_index_entry_t &obj) {
  return (obj.print(out));
}

} /* namespace lob */

#endif /* zlob0index_h */
