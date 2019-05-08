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
#ifndef lob0index_h
#define lob0index_h

#include "fut0lst.h"
#include "lob0util.h"
#include "trx0trx.h"
#include "univ.i"

namespace lob {

typedef std::map<page_no_t, buf_block_t *> BlockCache;
struct first_page_t;

/** An in-memory copy of an index_entry_t data */
struct index_entry_mem_t {
  fil_addr_t m_self;
  fil_addr_t m_prev;
  fil_addr_t m_next;
  flst_bnode_t m_versions;
  trx_id_t m_trx_id;
  trx_id_t m_trx_id_modifier;
  undo_no_t m_undo_no;
  undo_no_t m_undo_no_modifier;
  page_no_t m_page_no;
  ulint m_data_len;

  index_entry_mem_t() { reset(); }

  void reset();

  bool is_null() { return (m_self.is_equal(fil_addr_null)); }

  page_no_t get_page_no() const { return (m_page_no); }

  /** Print this object into the given output stream.
  @param[in]	out	the output stream.
  @return the output stream. */
  std::ostream &print(std::ostream &out) const;
};

/** List of index entry memory (iem) objects. */
using List_iem_t = std::list<index_entry_mem_t>;

/** Overloading the global output operator to print the index_entry_mem_t
object.
@param[in,out]	out	the output stream.
@param[in]	obj	an object of type index_entry_mem_t
@return the output stream. */
inline std::ostream &operator<<(std::ostream &out,
                                const index_entry_mem_t &obj) {
  return (obj.print(out));
}

/** An index entry pointing to an LOB page. */
struct index_entry_t {
  /** Index entry offsets within node. */
  static const ulint OFFSET_PREV = 0;
  static const ulint OFFSET_NEXT = OFFSET_PREV + FIL_ADDR_SIZE;

  /** Points to base node of the list of versions. The size of base node is
  16 bytes. */
  static const ulint OFFSET_VERSIONS = OFFSET_NEXT + FIL_ADDR_SIZE;

  /** The creator trx id. */
  static const ulint OFFSET_TRXID = OFFSET_VERSIONS + FLST_BASE_NODE_SIZE;

  /** The modifier trx id. */
  static const ulint OFFSET_TRXID_MODIFIER = OFFSET_TRXID + 6;
  static const ulint OFFSET_TRX_UNDO_NO = OFFSET_TRXID_MODIFIER + 6;

  /** The undo number of the modifier trx. */
  static const ulint OFFSET_TRX_UNDO_NO_MODIFIER = OFFSET_TRX_UNDO_NO + 4;
  static const ulint OFFSET_PAGE_NO = OFFSET_TRX_UNDO_NO_MODIFIER + 4;
  static const ulint OFFSET_DATA_LEN = OFFSET_PAGE_NO + 4;

  /** The LOB version number. */
  static const ulint OFFSET_LOB_VERSION = OFFSET_DATA_LEN + 4;

  /** Total length of an index node. */
  static const ulint SIZE = OFFSET_LOB_VERSION + 4;

  /** Constructor.
  @param[in]	node	the pointer where index entry is located. */
  index_entry_t(flst_node_t *node)
      : m_node(node), m_mtr(nullptr), m_index(nullptr), m_block(nullptr) {}

  index_entry_t(flst_node_t *node, mtr_t *mtr)
      : m_node(node), m_mtr(mtr), m_index(nullptr), m_block(nullptr) {}

  index_entry_t(flst_node_t *node, mtr_t *mtr, dict_index_t *index)
      : m_node(node), m_mtr(mtr), m_index(index), m_block(nullptr) {}

  index_entry_t(mtr_t *mtr, const dict_index_t *index)
      : m_node(nullptr), m_mtr(mtr), m_index(index), m_block(nullptr) {}

  /* Move the node pointer to a different place within the same page.
  @param[in]	addr	new location of node pointer. */
  void reset(fil_addr_t &addr) {
    ut_ad(m_block->page.id.page_no() == addr.page);

    m_node = buf_block_get_frame(m_block) + addr.boffset;
  }

  /* Get the buffer block of the current index entry.
  @return the buffer block of the current index entry.*/
  buf_block_t *get_block() const { return (m_block); }

  /* Reset the current object to point to a different node.
  @param[in]	node	the new file list node. */
  void reset(flst_node_t *node) { m_node = node; }

  bool is_null() const {
    const byte zero[SIZE] = {0x00};
    return (m_node == nullptr || memcmp(m_node, zero, SIZE) == 0);
  }

  /** Initialize the object fully. */
  void init() {
    set_prev_null();
    set_next_null();
    set_versions_null();
    set_trx_id(0);
    set_trx_undo_no(0);
    set_page_no(FIL_NULL);
    set_data_len(0);
  }

  /** Get the location of the current index entry. */
  fil_addr_t get_self() const;

  /** The versions base node is set to NULL. */
  void set_versions_null() {
    ut_ad(m_mtr != nullptr);

    byte *base_node = get_versions_ptr();
    flst_init(base_node, m_mtr);
  }

  /** Determine if the current index entry be rolled back.
  @param[in]	trxid		the transaction that is being purged.
  @param[in]	undo_no		the undo number of trx.
  @return true if this entry can be rolled back, false otherwise. */
  bool can_rollback(trx_id_t trxid, undo_no_t undo_no) {
    /* For rollback, make use of creator trx id. */
    return ((trxid == get_trx_id()) && (get_trx_undo_no() >= undo_no));
  }

  /** Determine if the current index entry be purged.
  @param[in]	trxid		the transaction that is being purged.
  @param[in]	undo_no		the undo number of trx.
  @return true if this entry can be purged, false otherwise. */
  bool can_be_purged(trx_id_t trxid, undo_no_t undo_no) {
    return ((trxid == get_trx_id_modifier()) &&
            (get_trx_undo_no_modifier() == undo_no));
  }

  /* The given entry becomes the old version of the current entry.
  Move the version base node from old entry to current entry.
  @param[in]  entry  the old entry */
  void set_old_version(index_entry_t &entry) {
    flst_node_t *node = entry.get_node_ptr();
    flst_base_node_t *version_list = get_versions_ptr();
    ut_ad(flst_get_len(version_list) == 0);

    entry.move_version_base_node(*this);
    flst_add_first(version_list, node, m_mtr);
  }

  /** The current index entry points to a latest LOB page.  It may or
  may not have older versions.  If older version is there, bring it
  back to the index list from the versions list.  Then remove the
  current entry from the index list.  Move the versions list from
  current entry to older entry.
  @param[in]	index		the clustered index containing the LOB.
  @param[in]	trxid		The transaction identifier.
  @param[in]	first_page	The first lob page containing index
                                  list and free list.
  @return the location of next entry. */
  fil_addr_t make_old_version_current(dict_index_t *index, trx_id_t trxid,
                                      first_page_t &first_page);

  /** Purge the current entry.
  @param[in]  index  the clustered index containing the LOB.
  @param[in]  trxid  The transaction identifier.
  @param[in]  lst    the base node of index list.
  @param[in]  free_list    the base node of free list.
  @return the location of the next entry. */
  fil_addr_t purge_version(dict_index_t *index, trx_id_t trxid,
                           flst_base_node_t *lst, flst_base_node_t *free_list);

  void add_version(index_entry_t &entry) const {
    flst_node_t *node = entry.get_node_ptr();
    flst_base_node_t *version_list = get_versions_ptr();
    flst_add_first(version_list, node, m_mtr);
  }

  flst_base_node_t *get_versions_list() const { return (get_versions_ptr()); }

  /** Add this node as the last node in the given list.
  @param[in]  bnode  the base node of the file list. */
  void push_back(flst_base_node_t *bnode) {
    flst_add_last(bnode, m_node, m_mtr);
  }

  /** Get the base node of the list of versions. */
  flst_bnode_t get_versions_mem() const {
    flst_base_node_t *node = get_versions_list();
    return (flst_bnode_t(node, m_mtr));
  }

  trx_id_t get_trx_id() const {
    byte *ptr = get_trxid_ptr();
    return (mach_read_from_6(ptr));
  }

  trx_id_t get_trx_id_modifier() const {
    byte *ptr = get_trxid_modifier_ptr();
    return (mach_read_from_6(ptr));
  }

  undo_no_t get_trx_undo_no() const {
    byte *ptr = get_trx_undo_no_ptr();
    return (mach_read_from_4(ptr));
  }

  uint32_t get_lob_version() const {
    byte *ptr = get_lob_version_ptr();
    return (mach_read_from_4(ptr));
  }

  /** Get the undo number of the modifier trx.
  @return the undo number of the modifier trx. */
  undo_no_t get_trx_undo_no_modifier() const {
    byte *ptr = get_trx_undo_no_modifier_ptr();
    return (mach_read_from_4(ptr));
  }

  fil_addr_t get_next() const {
    ut_ad(m_node != nullptr);

    return (flst_read_addr(m_node + OFFSET_NEXT, m_mtr));
  }

  /** Make the current index entry object to point to the next index
  entry object.
  @return the buffer block in which the next index entry is available.*/
  buf_block_t *next() {
    fil_addr_t node_loc = get_next();

    if (node_loc.is_null()) {
      return (nullptr);
    }

    if (m_block == nullptr || m_block->page.id.page_no() != node_loc.page) {
      load_x(node_loc);
    } else {
      /* Next entry in the same page. */
      reset(node_loc);
    }

    return (m_block);
  }

  /** Get the previous index entry.
  @return The file address of previous index entry. */
  fil_addr_t get_prev() const {
    return (flst_read_addr(m_node + OFFSET_PREV, m_mtr));
  }

  /** Write the trx identifier to the index entry. No redo log
  is generated for this modification.  This is meant to be used
  during tablespace import.
  @param[in]	id	the trx identifier.*/
  void set_trx_id_no_redo(trx_id_t id) {
    byte *ptr = get_trxid_ptr();
    mach_write_to_6(ptr, id);
  }

  /** Write the modifier trx identifier to the index entry. No redo log
  is generated for this modification.  This is meant to be used
  during tablespace import.
  @param[in]	id	the trx identifier.*/
  void set_trx_id_modifier_no_redo(trx_id_t id) {
    byte *ptr = get_trxid_modifier_ptr();
    mach_write_to_6(ptr, id);
  }

  void set_trx_id(trx_id_t id) {
    byte *ptr = get_trxid_ptr();
    mach_write_to_6(ptr, id);
    mlog_log_string(ptr, 6, m_mtr);
  }

  void set_trx_id_modifier(trx_id_t id) {
    ut_ad(m_mtr != nullptr);

    byte *ptr = get_trxid_modifier_ptr();
    mach_write_to_6(ptr, id);
    mlog_log_string(ptr, 6, m_mtr);
  }

  void set_trx_undo_no(undo_no_t undo_no) {
    byte *ptr = get_trx_undo_no_ptr();
    mlog_write_ulint(ptr, undo_no, MLOG_4BYTES, m_mtr);
  }

  /** Set the LOB version of this entry.
  @param[in]	version		the LOB version number. */
  void set_lob_version(uint32_t version) {
    byte *ptr = get_lob_version_ptr();
    mlog_write_ulint(ptr, version, MLOG_4BYTES, m_mtr);
  }

  void set_trx_undo_no_modifier(undo_no_t undo_no) {
    ut_ad(m_mtr != nullptr);

    byte *ptr = get_trx_undo_no_modifier_ptr();
    mlog_write_ulint(ptr, undo_no, MLOG_4BYTES, m_mtr);
  }

  void set_page_no(page_no_t num) {
    ut_ad(num > 0);
    byte *ptr = get_pageno_ptr();
    return (mlog_write_ulint(ptr, num, MLOG_4BYTES, m_mtr));
  }

  void set_prev_null() {
    flst_write_addr(m_node + OFFSET_PREV, fil_addr_null, m_mtr);
  }

  void set_next_null() {
    flst_write_addr(m_node + OFFSET_NEXT, fil_addr_null, m_mtr);
  }

  page_no_t get_page_no() const {
    byte *ptr = get_pageno_ptr();
    return (mach_read_from_4(ptr));
  }

  void set_data_len(ulint len) {
    byte *ptr = get_datalen_ptr();
    return (mlog_write_ulint(ptr, len, MLOG_2BYTES, m_mtr));
  }

  ulint get_data_len() const {
    byte *ptr = get_datalen_ptr();
    return (mach_read_from_2(ptr));
  }

  std::ostream &print(std::ostream &out) const;

  bool is_same(const index_entry_t &that) { return (m_node == that.m_node); }

  void read(index_entry_mem_t &entry_mem) const;

  /** Load the index entry available in the given file address.
  Take x-latch on the index page.
  @param[in]	addr	the file address of the index entry.
  @return the buffer block containing the index entry. */
  buf_block_t *load_x(const fil_addr_t &addr);

  /** Load the index entry available in the given file address.
  Take s-latch on the index page.
  @param[in]	addr	the file location of index entry.
  @return the buffer block. */
  buf_block_t *load_s(const fil_addr_t &addr);

  void insert_after(flst_base_node_t *base, index_entry_t &entry) {
    flst_insert_after(base, m_node, entry.get_node(), m_mtr);
  }

  void insert_before(flst_base_node_t *base, index_entry_t &entry) {
    flst_insert_before(base, entry.get_node(), m_node, m_mtr);
  }

  void remove(flst_base_node_t *bnode) { flst_remove(bnode, m_node, m_mtr); }

 private:
  /** Move the version base node from current entry to the given entry.
  @param[in]	to_entry	The index entry to which the version
                                  base node is moved to.*/
  void move_version_base_node(index_entry_t &to_entry);

  /** Purge the current index entry. An index entry points to either a
  FIRST page or DATA page.  That LOB page will be freed if it is DATA
  page.  A FIRST page should not be freed. */
  void purge(dict_index_t *index);

  byte *get_versions_ptr() const { return (m_node + OFFSET_VERSIONS); }

  byte *get_trxid_ptr() const { return (m_node + OFFSET_TRXID); }

  byte *get_trxid_modifier_ptr() const {
    return (m_node + OFFSET_TRXID_MODIFIER);
  }

  byte *get_trx_undo_no_ptr() const { return (m_node + OFFSET_TRX_UNDO_NO); }

  byte *get_lob_version_ptr() const { return (m_node + OFFSET_LOB_VERSION); }

  byte *get_trx_undo_no_modifier_ptr() const {
    return (m_node + OFFSET_TRX_UNDO_NO_MODIFIER);
  }

  byte *get_pageno_ptr() const { return (m_node + OFFSET_PAGE_NO); }

  byte *get_datalen_ptr() const { return (m_node + OFFSET_DATA_LEN); }

  byte *get_node_ptr() const { return (m_node); }

  byte *get_node() const { return (m_node); }

  byte *m_node;
  mtr_t *m_mtr;
  const dict_index_t *m_index;
  buf_block_t *m_block;
};

/** Overloading the global output operator to easily print an index entry.
@param[in]	out	the output stream.
@param[in]	obj	the index entry.
@return	the output stream. */
inline std::ostream &operator<<(std::ostream &out, const index_entry_t &obj) {
  return (obj.print(out));
}

} /* namespace lob */

#endif /* lob0index_h */
