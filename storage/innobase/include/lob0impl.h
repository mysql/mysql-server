/*****************************************************************************

Copyright (c) 2016, 2019, Oracle and/or its affiliates. All Rights Reserved.

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
#ifndef lob0impl_h
#define lob0impl_h

#include "btr0btr.h"
#include "fut0lst.h"
#include "lob0first.h"
#include "lob0lob.h"
#include "mach0data.h"
#include "mtr0log.h"
#include "mtr0mtr.h"
#include "mtr0types.h"

namespace lob {

struct z_index_entry_t;
struct z_first_page_t;
struct z_frag_page_t;
struct index_entry_t;
struct first_page_t;

using paddr_t = ulint;

/** The node of page list.  The page list is similar to the file list
(flst_node_t) except that it is completely within one page. */
class plist_node_t {
 public:
  /** Offset of the previous node. (2 bytes) */
  static const uint16_t OFFSET_PREV = 0;

  /** Offset of the next node. (2 bytes) */
  static const uint16_t OFFSET_NEXT = 2;

  /** The size of a page list node. */
  static const uint8_t SIZE = 4;

  /** Constructor.
  @param[in]	mtr	the mini transaction context. */
  explicit plist_node_t(mtr_t *mtr)
      : m_frame(nullptr), m_node(nullptr), m_mtr(mtr) {}

  /** Default constructor. */
  plist_node_t() : m_frame(nullptr), m_node(nullptr), m_mtr(nullptr) {}

  /** Constructor.
  @param[in]	mtr	the mini transaction context
  @param[in]	frame	the page frame of this plist. */
  plist_node_t(mtr_t *mtr, byte *frame)
      : m_frame(frame), m_node(nullptr), m_mtr(mtr) {}

  /** Constructor.
  @param[in]	frame	the page frame of this plist.
  @param[in]	node	the location of plist node. */
  plist_node_t(byte *frame, byte *node)
      : m_frame(frame), m_node(node), m_mtr(nullptr) {}

  /** Constructor.
  @param[in]	frame	the page frame where the page list node is
                          located.
  @param[in]	node	the location of page list node within page
                          frame.
  @param[in]	mtr	the mini-transaction context. */
  plist_node_t(byte *frame, byte *node, mtr_t *mtr)
      : m_frame(frame), m_node(node), m_mtr(mtr) {}

  /** Copy constructor. */
  plist_node_t(const plist_node_t &other) = default;

  plist_node_t &operator=(const plist_node_t &) = default;

  /** Check if the current node is before the given node in the
  page (w.r.t the offset).
  @param[in]	node	the other node.
  @return true if current node is before the given node.
  @return false if current node is after the given node. */
  bool is_before(const plist_node_t &node) const {
    ut_ad(!is_null());
    ut_ad(!node.is_null());
    return (addr() < node.addr());
  }

  /** Initialize the current page list node. The offset of next and
  previous nodes are set to 0. */
  void init() {
    ut_ad(!is_null());
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(m_node + OFFSET_PREV, 0, MLOG_2BYTES, m_mtr);
    mlog_write_ulint(m_node + OFFSET_NEXT, 0, MLOG_2BYTES, m_mtr);
  }

  /** Set the offset of the previous node.
  @param[in]	addr	the offset of previous node.*/
  void set_prev(paddr_t addr) {
    ut_ad(addr < UNIV_PAGE_SIZE);
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(m_node + OFFSET_PREV, addr, MLOG_2BYTES, m_mtr);
  }

  /** Set the previous page list node.
  @param[in]	prev	the previous page list node.*/
  void set_prev_node(plist_node_t &prev) { set_prev(prev.addr()); }

  /** Set the offset of the next node.
  @param[in]	addr	the offset of next node.*/
  void set_next(paddr_t addr) {
    ut_ad(!is_null());
    ut_ad(addr < UNIV_PAGE_SIZE);
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(m_node + OFFSET_NEXT, addr, MLOG_2BYTES, m_mtr);
  }

  /** Set the next page list node.
  @param[in]	next	the next page list node.*/
  void set_next_node(const plist_node_t &next) { set_next(next.addr()); }

  /** Get the offset of the previous page list node.
  @return offset of previous node of the page list. */
  paddr_t get_prev() const { return (mach_read_from_2(m_node + OFFSET_PREV)); }

  /** Get the offset of the next page list node.
  @return offset of next node of the page list. */
  paddr_t get_next() const { return (mach_read_from_2(m_node + OFFSET_NEXT)); }

  /** Get the next page list node.
  @return next node of the page list. */
  plist_node_t get_next_node() const {
    paddr_t addr = get_next();
    byte *node = nullptr;

    if (addr != 0) {
      node = m_frame + addr;
      ut_ad(addr < UNIV_PAGE_SIZE);
    }

    return (plist_node_t(m_frame, node, m_mtr));
  }

  /** Get the previous page list node.
  @return previous node of the page list. */
  plist_node_t get_prev_node() const {
    paddr_t addr = get_prev();
    byte *node = nullptr;

    if (addr != 0) {
      ut_ad(addr < UNIV_PAGE_SIZE);
      node = m_frame + addr;
    }

    return (plist_node_t(m_frame, node, m_mtr));
  }

  /** Obtain the offset of the page list node within the given
  page frame.
  @return offset from the beginning of the page. */
  paddr_t addr() const {
    return ((m_node == nullptr) ? 0 : (m_node - m_frame));
  }

  /** Obtain the memory location of the page list node.
  @return the pointer to the page list node. */
  byte *ptr() const { return (m_node); }

  /** Check if the given page list node is null.
  @return true if null, false otherwise. */
  bool is_null() const { return (m_node == nullptr); }

  /** Print the page list node into the given output stream.
  @param[in]	out	the output stream.
  @return the output stream. */
  std::ostream &print(std::ostream &out) const {
    out << "[plist_node_t: next=" << get_next() << ", prev=" << get_prev()
        << ", this=" << addr() << ", frame=" << (void *)m_frame
        << ", m_node=" << (void *)m_node << "]";
    return (out);
  }

  /** Set the page frame to the given value.
  @param[in]	frame	the page frame */
  void set_frame(byte *frame) { m_frame = frame; }

  /** Set the page list node to the given value.
  @param[in]	node	the page list node. */
  void set_node(byte *node) { m_node = node; }

  /** Set the mini transaction context to the given value.
  @param[in]	mtr	the mini transaction context. */
  void set_mtr(mtr_t *mtr) { m_mtr = mtr; }

  /** Get the page frame where this page list exists.
  @return the page frame. */
  byte *get_frame() const { return (m_frame); }

  bool is_equal(const plist_node_t &that) const {
    if (m_node == nullptr || that.m_node == nullptr) {
      return (false);
    }
    return (m_node == that.m_node);
  }

 private:
  /** The page frame where this page list exists. */
  byte *m_frame;

  /** The plist node is located at this address. */
  byte *m_node;

  /** The mini transaction context. */
  mtr_t *m_mtr;
};

inline std::ostream &operator<<(std::ostream &out, const plist_node_t &obj) {
  return (obj.print(out));
}

/** The base node of page list. */
struct plist_base_node_t {
  /** The offset where the length of the page list is stored.
  This is 4 bytes long.*/
  static const ulint OFFSET_LEN = 0;

  /** The offset where the first node is located.
  This is 2 bytes long. */
  static const ulint OFFSET_FIRST = 4;

  /** The offset where the last node is located.
  This is 2 bytes long. */
  static const ulint OFFSET_LAST = 6;

  /** The total size (in bytes) of a page list base node. */
  static const ulint SIZE = 8;

  plist_base_node_t(byte *frame, byte *base, mtr_t *mtr)
      : m_frame(frame), m_base(base), m_mtr(mtr) {}

  void init() {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(m_base + OFFSET_LEN, 0, MLOG_4BYTES, m_mtr);
    mlog_write_ulint(m_base + OFFSET_FIRST, 0, MLOG_2BYTES, m_mtr);
    mlog_write_ulint(m_base + OFFSET_LAST, 0, MLOG_2BYTES, m_mtr);
  }

  void remove(plist_node_t &node) {
    ut_ad(m_mtr != nullptr);

    plist_node_t prev = node.get_prev_node();
    plist_node_t next = node.get_next_node();

    if (prev.is_null()) {
      set_first(next.addr());
    } else {
      prev.set_next(next.addr());
    }

    if (next.is_null()) {
      set_last(prev.addr());
    } else {
      next.set_prev(prev.addr());
    }

    node.set_next(0);
    node.set_prev(0);

    decr_len();
  }

  void push_front(plist_node_t &node) {
    ut_ad(m_mtr != nullptr);

    if (get_len() == 0) {
      add_to_empty(node);
    } else {
      paddr_t cur_addr = node.addr();
      paddr_t first_addr = get_first();
      plist_node_t first_node = get_node(first_addr);
      node.set_next(first_addr);
      node.set_prev(0);
      first_node.set_prev(cur_addr);
      set_first(cur_addr);
      incr_len();
    }
  }

  /** Insert node2 after node1. */
  void insert_after(plist_node_t &node1, plist_node_t &node2) {
    ut_ad(m_mtr != nullptr);

    if (node1.is_null()) {
      push_back(node2);
    } else {
      plist_node_t node3 = node1.get_next_node();
      node1.set_next_node(node2);
      node2.set_next_node(node3);

      if (node3.is_null()) {
        set_last(node2.addr());
      } else {
        node3.set_prev_node(node2);
      }

      node2.set_prev_node(node1);

      incr_len();
    }
  }

  /** Insert node2 before node3. */
  void insert_before(plist_node_t &node3, plist_node_t &node2) {
    ut_ad(m_mtr != nullptr);

    if (node3.is_null()) {
      push_back(node2);
    } else {
      plist_node_t node1 = node3.get_prev_node();

      if (node1.is_null()) {
        set_first(node2.addr());
      } else {
        node1.set_next_node(node2);
      }

      node2.set_next_node(node3);
      node3.set_prev_node(node2);
      node2.set_prev_node(node1);

      incr_len();
    }
  }

  void add_to_empty(plist_node_t &node) {
    ut_ad(m_mtr != nullptr);
    ut_ad(get_len() == 0);

    set_first(node.addr());
    set_last(node.addr());
    incr_len();
  }

  void push_back(plist_node_t &node) {
    ut_ad(m_mtr != nullptr);

    if (get_len() == 0) {
      add_to_empty(node);
    } else {
      paddr_t cur_addr = node.addr();
      paddr_t last_addr = get_last();
      plist_node_t last_node = get_node(last_addr);
      node.set_next(0);
      node.set_prev_node(last_node);
      last_node.set_next(cur_addr);
      set_last(cur_addr);
      incr_len();
    }
  }

  bool empty() const { return (get_len() == 0); }

  ulint get_len() const { return (mach_read_from_4(m_base + OFFSET_LEN)); }

  paddr_t get_first() const {
    return (mach_read_from_2(m_base + OFFSET_FIRST));
  }

  plist_node_t get_first_node() const {
    plist_node_t result(m_mtr, m_frame);

    if (!empty()) {
      byte *node = m_frame + get_first();
      result.set_node(node);
    }
    return (result);
  }

  paddr_t get_last() const { return (mach_read_from_2(m_base + OFFSET_LAST)); }

  plist_node_t get_last_node() const {
    plist_node_t result(m_mtr, m_frame);

    if (!empty()) {
      result.set_node(m_frame + get_last());
    }

    return (result);
  }

  void set_len(ulint len) {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(m_base + OFFSET_LEN, len, MLOG_4BYTES, m_mtr);
  }

  void incr_len() {
    ut_ad(m_mtr != nullptr);

    ulint len = mach_read_from_4(m_base + OFFSET_LEN);
    mlog_write_ulint(m_base + OFFSET_LEN, len + 1, MLOG_4BYTES, m_mtr);
  }

  void decr_len() {
    ut_ad(m_mtr != nullptr);

    ulint len = mach_read_from_4(m_base + OFFSET_LEN);

    ut_ad(len > 0);

    mlog_write_ulint(m_base + OFFSET_LEN, len - 1, MLOG_4BYTES, m_mtr);
  }

  void set_first(paddr_t addr) {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(m_base + OFFSET_FIRST, addr, MLOG_2BYTES, m_mtr);
  }

  void set_last(paddr_t addr) {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(m_base + OFFSET_LAST, addr, MLOG_2BYTES, m_mtr);
  }

  plist_node_t get_node(paddr_t addr) {
    byte *node = m_frame + addr;
    return (plist_node_t(m_frame, node, m_mtr));
  }

  paddr_t addr() const { return (m_base - m_frame); }

  std::ostream &print(std::ostream &out) const {
    out << "[plist_base_node_t: len=" << get_len() << ", first=" << get_first()
        << ", last=" << get_last() << ", this=" << addr() << "]";
    return (out);
  }

  std::ostream &print_list(std::ostream &out) const {
    print(out);
    out << std::endl;

    for (plist_node_t cur = get_first_node(); !cur.is_null();
         cur = cur.get_next_node()) {
      out << cur << std::endl;
    }
    return (out);
  }

#ifdef UNIV_DEBUG
  /** Validate the page list.
  @return true if valid, false otherwise. */
  bool validate() const;
#endif /* UNIV_DEBUG */

  byte *m_frame;
  byte *m_base;
  mtr_t *m_mtr;
};

inline std::ostream &operator<<(std::ostream &out,
                                const plist_base_node_t &obj) {
  return (obj.print(out));
}

using frag_id_t = ulint;
const ulint FRAG_ID_NULL = std::numeric_limits<uint16_t>::max();
const ulint KB16 = 16 * 1024;

/** The node page (also can be called as the index page) contains a list of
index_entry_t objects. */
struct node_page_t : public basic_page_t {
  /** Version information. One byte. */
  static const ulint OFFSET_VERSION = FIL_PAGE_DATA;
  static const ulint LOB_PAGE_DATA = OFFSET_VERSION + 1;

  void set_version_0() {
    mlog_write_ulint(frame() + OFFSET_VERSION, 0, MLOG_1BYTE, m_mtr);
  }

  /** Default ctor */
  node_page_t() {}

  node_page_t(buf_block_t *block, mtr_t *mtr) : basic_page_t(block, mtr) {}

  node_page_t(buf_block_t *block, mtr_t *mtr, dict_index_t *index)
      : basic_page_t(block, mtr, index) {}

  node_page_t(mtr_t *mtr, dict_index_t *index)
      : basic_page_t(nullptr, mtr, index) {}

  /** Constructor
  @param[in]	block	the buffer block. */
  node_page_t(buf_block_t *block) : basic_page_t(block, nullptr, nullptr) {}

  /** Import the node page or the index page.
  @param[in]	trx_id	transaction identifier. */
  void import(trx_id_t trx_id);

  buf_block_t *alloc(first_page_t &first_page, bool bulk);

  buf_block_t *load_x(page_id_t page_id, page_size_t page_size) {
    m_block = buf_page_get(page_id, page_size, RW_X_LATCH, m_mtr);
    return (m_block);
  }

  void dealloc() {
    btr_page_free_low(m_index, m_block, ULINT_UNDEFINED, m_mtr);
    m_block = nullptr;
  }

  static ulint payload() {
    return (UNIV_PAGE_SIZE - LOB_PAGE_DATA - FIL_PAGE_DATA_END);
  }

  static ulint max_space_available() { return (payload()); }

  /** Get the number of index entries this page can hold.
  @return Number of index entries this page can hold. */
  static ulint node_count();

  void set_page_type() {
    mlog_write_ulint(frame() + FIL_PAGE_TYPE, FIL_PAGE_TYPE_LOB_INDEX,
                     MLOG_2BYTES, m_mtr);
  }

  byte *nodes_begin() const { return (frame() + LOB_PAGE_DATA); }
};

/** An entry representing one fragment page. */
struct z_frag_entry_t {
 public:
  /** Offset within frag entry pointing to prev frag entry. */
  static const ulint OFFSET_PREV = 0;

  /** Offset within frag entry pointing to next frag entry. */
  static const ulint OFFSET_NEXT = OFFSET_PREV + FIL_ADDR_SIZE;

  /** Offset within frag entry holding the page number of frag page. */
  static const ulint OFFSET_PAGE_NO = OFFSET_NEXT + FIL_ADDR_SIZE;

  /** Number of used fragments. */
  static const ulint OFFSET_N_FRAGS = OFFSET_PAGE_NO + 4;

  /** Used space in bytes. */
  static const ulint OFFSET_USED_LEN = OFFSET_N_FRAGS + 2;

  /** Total free space in bytes. */
  static const ulint OFFSET_TOTAL_FREE_LEN = OFFSET_USED_LEN + 2;

  /** The biggest free frag space in bytes. */
  static const ulint OFFSET_BIG_FREE_LEN = OFFSET_TOTAL_FREE_LEN + 2;

  /** Total size of one frag entry. */
  static const ulint SIZE = OFFSET_BIG_FREE_LEN + 2;

  /** Constructor. */
  z_frag_entry_t(flst_node_t *node, mtr_t *mtr) : m_node(node), m_mtr(mtr) {}

  /** Constructor. */
  z_frag_entry_t() : m_node(nullptr), m_mtr(nullptr) {}

  /** Constructor. */
  z_frag_entry_t(mtr_t *mtr) : m_node(nullptr), m_mtr(mtr) {}

  /** Initialize the fragment entry contents.  For this to correctly
  work, the current object must be initialized with proper file list
  node and the mini transaction context. */
  void init() {
    ut_ad(m_mtr != nullptr);
    ut_ad(m_node != nullptr);

    set_prev_null();
    set_next_null();
    set_page_no(FIL_NULL);
    set_n_frags(0);
    set_used_len(0);
    set_total_free_len(0);
    set_big_free_len(0);
  }

  /** Set the current fragment entry to null. */
  void set_null() { m_node = nullptr; }

  /** Check if the current fragment entry is null.
  @return true if the current fragment entry is null, false otherwise. */
  bool is_null() const { return (m_node == nullptr); }

  fil_addr_t get_self_addr() const {
    page_t *frame = page_align(m_node);
    page_no_t page_no = mach_read_from_4(frame + FIL_PAGE_OFFSET);
    uint16_t offset = static_cast<uint16_t>(m_node - frame);
    ut_ad(offset < UNIV_PAGE_SIZE);
    return (fil_addr_t(page_no, offset));
  }

  /** Update the current fragment entry with information about
  the given fragment page.
  @param[in]	frag_page	the fragment page whose information
                          will be stored in current fragment entry. */
  void update(const z_frag_page_t &frag_page);

  /** Remove this node from the given list.
  @param[in]	bnode	the base node of the list from which to remove
                          current node. */
  void remove(flst_base_node_t *bnode) {
    ut_ad(m_mtr != nullptr);

    flst_remove(bnode, m_node, m_mtr);
  }

  /** Add this node as the last node in the given list.
  @param[in]  bnode  the base node of the file list. */
  void push_back(flst_base_node_t *bnode) {
    ut_ad(m_mtr != nullptr);

    flst_add_last(bnode, m_node, m_mtr);
  }

  /** Add this node as the first node in the given list.
  @param[in]  bnode  the base node of the file list. */
  void push_front(flst_base_node_t *bnode) {
    ut_ad(m_mtr != nullptr);

    flst_add_first(bnode, m_node, m_mtr);
  }

  /** Point to another frag entry.
  @param[in]  node  point to this file list node. */
  void reset(flst_node_t *node) { m_node = node; }

  /** Set the previous frag entry as null. */
  void set_prev_null() {
    ut_ad(m_mtr != nullptr);

    flst_write_addr(m_node + OFFSET_PREV, fil_addr_null, m_mtr);
  }

  /** Set the previous frag entry as null. */
  void set_prev(const fil_addr_t &addr) {
    ut_ad(m_mtr != nullptr);

    flst_write_addr(m_node + OFFSET_PREV, addr, m_mtr);
  }

  /** Get the location of previous frag entry. */
  fil_addr_t get_prev() const {
    return (flst_read_addr(m_node + OFFSET_PREV, m_mtr));
  }

  /** Set the next frag entry as null. */
  void set_next_null() {
    ut_ad(m_mtr != nullptr);

    flst_write_addr(m_node + OFFSET_NEXT, fil_addr_null, m_mtr);
  }

  /** Set the next frag entry. */
  void set_next(const fil_addr_t &addr) {
    ut_ad(m_mtr != nullptr);

    flst_write_addr(m_node + OFFSET_NEXT, addr, m_mtr);
  }

  /** Get the location of next frag entry. */
  fil_addr_t get_next() const {
    return (flst_read_addr(m_node + OFFSET_NEXT, m_mtr));
  }

  /** Get the frag page number. */
  page_no_t get_page_no() const {
    return (mach_read_from_4(m_node + OFFSET_PAGE_NO));
  }

  /** Set the frag page number. */
  void set_page_no(page_no_t page_no) const {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(m_node + OFFSET_PAGE_NO, page_no, MLOG_4BYTES, m_mtr);
  }

  /** Get the frag page number. */
  ulint get_n_frags() const {
    return (mach_read_from_2(m_node + OFFSET_N_FRAGS));
  }

  /** Set the frag page number. */
  void set_n_frags(ulint frags) const {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(m_node + OFFSET_N_FRAGS, frags, MLOG_2BYTES, m_mtr);
  }

  /** Get the used bytes. */
  ulint get_used_len() const {
    return (mach_read_from_2(m_node + OFFSET_USED_LEN));
  }

  /** Set the used bytes. */
  void set_used_len(ulint used) const {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(m_node + OFFSET_USED_LEN, used, MLOG_2BYTES, m_mtr);
  }

  /** Get the total cumulative free bytes. */
  ulint get_total_free_len() const {
    return (mach_read_from_2(m_node + OFFSET_TOTAL_FREE_LEN));
  }

  /** Get the biggest free frag bytes. */
  ulint get_big_free_len() const {
    return (mach_read_from_2(m_node + OFFSET_BIG_FREE_LEN));
  }

  /** Set the total free bytes. */
  void set_total_free_len(ulint n) {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(m_node + OFFSET_TOTAL_FREE_LEN, n, MLOG_2BYTES, m_mtr);
  }

  /** Set the big free frag bytes. */
  void set_big_free_len(ulint n) {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(m_node + OFFSET_BIG_FREE_LEN, n, MLOG_2BYTES, m_mtr);
  }

  void purge(flst_base_node_t *used_lst, flst_base_node_t *free_lst);

  std::ostream &print(std::ostream &out) const;

 private:
  /** The location where the fragment entry node is located. */
  flst_node_t *m_node;

  /** The mini transaction context for operating on this fragment
  entry. */
  mtr_t *m_mtr;
};

inline std::ostream &operator<<(std::ostream &out, const z_frag_entry_t &obj) {
  return (obj.print(out));
}

/** An index page containing an array of z_index_entry_t objects. */
struct z_index_page_t {
  /** Version information. One byte. */
  static const ulint OFFSET_VERSION = FIL_PAGE_DATA;
  static const ulint LOB_PAGE_DATA = OFFSET_VERSION + 1;

  explicit z_index_page_t(mtr_t *mtr) : m_block(nullptr), m_mtr(mtr) {}

  z_index_page_t(mtr_t *mtr, dict_index_t *index)
      : m_block(nullptr), m_mtr(mtr), m_index(index) {}

  /** Constructor
  @param[in]	block	the buffer block. */
  explicit z_index_page_t(buf_block_t *block)
      : m_block(block), m_mtr(nullptr), m_index(nullptr) {}

  /** Write the space identifier to the page header, without generating
  redo log records.
  @param[in]	space_id	the space identifier. */
  void set_space_id_no_redo(space_id_t space_id) {
    mlog_write_ulint(frame() + FIL_PAGE_SPACE_ID, space_id, MLOG_4BYTES,
                     nullptr);
  }

  /** Set the correct page type. */
  void set_page_type(mtr_t *mtr) {
    mlog_write_ulint(frame() + FIL_PAGE_TYPE, FIL_PAGE_TYPE_ZLOB_INDEX,
                     MLOG_2BYTES, mtr);
  }

  void set_version_0() {
    mlog_write_ulint(frame() + OFFSET_VERSION, 0, MLOG_1BYTE, m_mtr);
  }

  /** Set the next page number. */
  void set_next_page_no(page_no_t page_no) {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(frame() + FIL_PAGE_NEXT, page_no, MLOG_4BYTES, m_mtr);
  }

  /** Get the page number. */
  page_no_t get_page_no() const {
    return (mach_read_from_4(frame() + FIL_PAGE_OFFSET));
  }

  /** Get the next page number. */
  page_no_t get_next_page_no() const {
    return (mach_read_from_4(frame() + FIL_PAGE_NEXT));
  }

  /** Allocate an ZLOB index page.
  @return the buffer block of the allocated zlob index page. */
  buf_block_t *alloc(z_first_page_t &first, bool bulk);

  void import(trx_id_t trx_id);

  /** Load the given compressed LOB index page.
  @param[in]	page_no		compressed LOB index page number.
  @return	the buffer block of the given page number. */
  buf_block_t *load_x(page_no_t page_no) {
    page_id_t page_id(dict_index_get_space(m_index), page_no);
    page_size_t page_size(dict_table_page_size(m_index->table));
    m_block = buf_page_get(page_id, page_size, RW_X_LATCH, m_mtr);

    ut_ad(m_block->get_page_type() == FIL_PAGE_TYPE_ZLOB_INDEX);
    return (m_block);
  }

  void dealloc() {
    btr_page_free_low(m_index, m_block, ULINT_UNDEFINED, m_mtr);
    m_block = nullptr;
  }

  void init(flst_base_node_t *free_lst, mtr_t *mtr);

  ulint payload() const {
    const page_size_t page_size(dict_table_page_size(m_index->table));

    return (page_size.physical() - FIL_PAGE_DATA_END - LOB_PAGE_DATA);
  }

  ulint get_n_index_entries() const;

  byte *frame() const { return (buf_block_get_frame(m_block)); }

  /** The buffer block of the compressed LOB index page. */
  buf_block_t *m_block;

  /** The mini-transaction context. */
  mtr_t *m_mtr;

  /** The index to which the LOB belongs. */
  dict_index_t *m_index;
};

/** The data page holding the zlob. */
struct z_data_page_t {
  /** Version information. One byte. */
  static const ulint OFFSET_VERSION = FIL_PAGE_DATA;

  /* The length of compressed data stored in this page. */
  static const ulint OFFSET_DATA_LEN = OFFSET_VERSION + 1;

  /* The transaction that created this page. */
  static const ulint OFFSET_TRX_ID = OFFSET_DATA_LEN + 4;

  /* The data stored in this page begins at this offset. */
  static const ulint OFFSET_DATA_BEGIN = OFFSET_TRX_ID + 6;

  ulint payload() {
    page_size_t page_size(dict_table_page_size(m_index->table));
    return (page_size.physical() - OFFSET_DATA_BEGIN - FIL_PAGE_DATA_END);
  }

  z_data_page_t(mtr_t *mtr, dict_index_t *index)
      : m_block(nullptr), m_mtr(mtr), m_index(index) {}

  z_data_page_t(buf_block_t *block, mtr_t *mtr, dict_index_t *index)
      : m_block(block), m_mtr(mtr), m_index(index) {}

  /* Constructor.
  @param[in]	block	the buffer block. */
  z_data_page_t(buf_block_t *block)
      : m_block(block), m_mtr(nullptr), m_index(nullptr) {}

  /** Write the space identifier to the page header, without generating
  redo log records.
  @param[in]	space_id	the space identifier. */
  void set_space_id_no_redo(space_id_t space_id) {
    mlog_write_ulint(frame() + FIL_PAGE_SPACE_ID, space_id, MLOG_4BYTES,
                     nullptr);
  }

  /** Allocate one data page.
  @param[in]	hint	hint page number for allocation.
  @param[in]	bulk	true if bulk operation (OPCODE_INSERT_BULK)
                          false otherwise.
  @return the allocated buffer block. */
  buf_block_t *alloc(page_no_t hint, bool bulk);

  /** Set the correct page type. */
  void set_page_type() {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(frame() + FIL_PAGE_TYPE, FIL_PAGE_TYPE_ZLOB_DATA,
                     MLOG_2BYTES, m_mtr);
  }

  void set_version_0() {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(frame() + OFFSET_VERSION, 0, MLOG_1BYTE, m_mtr);
  }

  /** Set the next page. */
  void set_next_page(page_no_t page_no) {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(frame() + FIL_PAGE_NEXT, page_no, MLOG_4BYTES, m_mtr);
  }

  void init() {
    ut_ad(m_mtr != nullptr);

    set_page_type();
    set_version_0();
    set_next_page(FIL_NULL);
    set_data_len(0);
    set_trx_id(0);
  }

  byte *begin_data_ptr() const { return (frame() + OFFSET_DATA_BEGIN); }

  void set_data_len(ulint len) {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(frame() + OFFSET_DATA_LEN, len, MLOG_4BYTES, m_mtr);
  }

  ulint get_data_len() const {
    return (mach_read_from_4(frame() + OFFSET_DATA_LEN));
  }

  void set_trx_id(trx_id_t tid) {
    ut_ad(m_mtr != nullptr);

    byte *ptr = frame() + OFFSET_TRX_ID;
    mach_write_to_6(ptr, tid);
    mlog_log_string(ptr, 6, m_mtr);
  }

  /** Update the header with given transaction identifier, without
  writing redo log records.
  @param[in]	tid	transaction identifier.*/
  void set_trx_id_no_redo(trx_id_t tid) {
    byte *ptr = frame() + OFFSET_TRX_ID;
    mach_write_to_6(ptr, tid);
  }

  /** Get the page number. */
  page_no_t get_page_no() const {
    return (mach_read_from_4(frame() + FIL_PAGE_OFFSET));
  }

  fil_addr_t get_self_addr() const {
    page_no_t page_no = get_page_no();
    return (fil_addr_t(page_no, OFFSET_DATA_BEGIN));
  }

  byte *frame() const { return (buf_block_get_frame(m_block)); }

  buf_block_t *m_block;
  mtr_t *m_mtr;
  dict_index_t *m_index;
};

/** A frag nodes page containing an array of z_frag_entry_t objects. */
struct z_frag_node_page_t {
  /** Version information. One byte. */
  static const ulint OFFSET_VERSION = FIL_PAGE_DATA;
  static const ulint LOB_PAGE_DATA = OFFSET_VERSION + 1;

  z_frag_node_page_t(mtr_t *mtr, dict_index_t *index)
      : m_block(nullptr), m_mtr(mtr), m_index(index) {}

  /** Constructor
  @param[in]	block	the buffer block.*/
  explicit z_frag_node_page_t(buf_block_t *block)
      : m_block(block), m_mtr(nullptr), m_index(nullptr) {}

  /** Write the space identifier to the page header, without generating
  redo log records.
  @param[in]	space_id	the space identifier. */
  void set_space_id_no_redo(space_id_t space_id) {
    mlog_write_ulint(frame() + FIL_PAGE_SPACE_ID, space_id, MLOG_4BYTES,
                     nullptr);
  }

  /** Set the correct page type. */
  void set_page_type() {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(frame() + FIL_PAGE_TYPE, FIL_PAGE_TYPE_ZLOB_FRAG_ENTRY,
                     MLOG_2BYTES, m_mtr);
  }

  /** Set the next page number. */
  void set_next_page_no(page_no_t page_no) {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(frame() + FIL_PAGE_NEXT, page_no, MLOG_4BYTES, m_mtr);
  }

  void set_version_0() {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(frame() + OFFSET_VERSION, 0, MLOG_1BYTE, m_mtr);
  }

  /** Get the page number. */
  page_no_t get_page_no() const {
    return (mach_read_from_4(frame() + FIL_PAGE_OFFSET));
  }

  /** Get the next page number. */
  page_no_t get_next_page_no() const {
    return (mach_read_from_4(frame() + FIL_PAGE_NEXT));
  }

  /** Allocate a fragment nodes page.
  @return buffer block of the allocated fragment nodes page or nullptr. */
  buf_block_t *alloc(z_first_page_t &first, bool bulk);

  void dealloc() {
    btr_page_free_low(m_index, m_block, ULINT_UNDEFINED, m_mtr);
    m_block = nullptr;
  }

  /** Load the given compressed LOB fragment page.
  @param[in]	page_no		compressed LOB fragment page number.
  @return	the buffer block of the given page number. */
  buf_block_t *load_x(page_no_t page_no) {
    page_id_t page_id(dict_index_get_space(m_index), page_no);
    page_size_t page_size(dict_table_page_size(m_index->table));
    m_block = buf_page_get(page_id, page_size, RW_X_LATCH, m_mtr);

    ut_ad(m_block->get_page_type() == FIL_PAGE_TYPE_ZLOB_FRAG_ENTRY);

    return (m_block);
  }

  void init(flst_base_node_t *free_lst) {
    ut_ad(m_mtr != nullptr);

    ulint n = get_n_frag_entries();
    for (ulint i = 0; i < n; ++i) {
      byte *ptr = frame() + LOB_PAGE_DATA;
      ptr += (i * z_frag_entry_t::SIZE);
      z_frag_entry_t entry(ptr, m_mtr);
      entry.init();
      entry.push_back(free_lst);
    }
  }

  ulint payload() const {
    const page_size_t page_size = dict_table_page_size(m_index->table);
    return (page_size.physical() - FIL_PAGE_DATA_END - LOB_PAGE_DATA);
  }

  ulint get_n_frag_entries() const {
    return (payload() / z_frag_entry_t::SIZE);
  }

  byte *frame() const { return (buf_block_get_frame(m_block)); }

  /** The buffer block of the fragment page. */
  buf_block_t *m_block;

  /** The mini-transaction context. */
  mtr_t *m_mtr;

  /** The index to which the LOB belongs. */
  dict_index_t *m_index;
};  // struct z_frag_node_page_t

/** Print information about the given compressed lob.
@param[in]  index  the index dictionary object.
@param[in]  ref    the LOB reference
@param[out] out    the output stream where information is printed.
@return DB_SUCCESS on success, or an error code. */
dberr_t z_print_info(const dict_index_t *index, const lob::ref_t &ref,
                     std::ostream &out);

/** The fragment node represents one fragment. */
struct frag_node_t {
  /** The offset where the length of fragment is stored.  The length
  includes both the payload and the meta data overhead. */
  static const ulint OFFSET_LEN = plist_node_t::SIZE;

  /** The offset where fragment id is stored. */
  static const ulint OFFSET_FRAG_ID = OFFSET_LEN + 2;

  /** The offset where fragment data is stored. */
  static const ulint OFFSET_DATA = OFFSET_FRAG_ID + 2;

  /** The size of a page directory entry in a fragment page in bytes.
  This must be equal to z_frag_page_t::SIZE_OF_PAGE_DIR_ENTRY*/
  static const ulint SIZE_OF_PAGE_DIR_ENTRY = 2;

  /** Constructor.
  @param[in]	node	page list node.
  @param[in]	mtr	mini-transaction. */
  frag_node_t(const plist_node_t &node, mtr_t *mtr)
      : m_node(node), m_mtr(mtr) {}

  frag_node_t(byte *frame, byte *ptr) : m_node(frame, ptr), m_mtr(nullptr) {}

  /** Constructor.
  @param[in]	frame	the page frame where the fragment node is
                          located.
  @param[in]	ptr	the location of fragment node within page
                          frame.
  @param[in]	mtr	the mini-transaction context. */
  frag_node_t(byte *frame, byte *ptr, mtr_t *mtr)
      : m_node(frame, ptr, mtr), m_mtr(mtr) {}

  /** Amount of space that will be used up by meta data. When a free
  space is taken from the fragment page to be used as a fragment
  node, header and footer will be the overhead. Footer is the page dir
  entry. The page dir entry may not be contiguous with the fragment.*/
  static ulint overhead() { return (SIZE_OF_PAGE_DIR_ENTRY + OFFSET_DATA); }

  /** Only the header size. Don't include the page dir entry size here.*/
  static ulint header_size() { return (OFFSET_DATA); }

  /** Constructor.
  @param[in]	frame	the page frame where the fragment node is
                          located.
  @param[in]	ptr	the location of fragment node within page
                          frame.
  @param[in]	len	the length of the fragment.
  @param[in]	mtr	the mini-transaction context. */
  frag_node_t(byte *frame, byte *ptr, ulint len, mtr_t *mtr)
      : m_node(frame, ptr, mtr), m_mtr(mtr) {
    ut_ad(mtr != nullptr);

    mlog_write_ulint(m_node.ptr() + OFFSET_LEN, len, MLOG_2BYTES, mtr);
  }

  byte *frag_begin() const { return (m_node.ptr() + OFFSET_DATA); }

  byte *data_begin() const { return (m_node.ptr() + OFFSET_DATA); }

  void set_total_len(ulint len) {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(m_node.ptr() + OFFSET_LEN, len, MLOG_2BYTES, m_mtr);
  }

  /** Increment the total length of this fragment by 2 bytes. */
  void incr_length_by_2() {
    ut_ad(m_mtr != nullptr);

    ulint len = mach_read_from_2(m_node.ptr() + OFFSET_LEN);
    mlog_write_ulint(m_node.ptr() + OFFSET_LEN, len + SIZE_OF_PAGE_DIR_ENTRY,
                     MLOG_2BYTES, m_mtr);
  }

  /** Decrement the total length of this fragment by 2 bytes. */
  void decr_length_by_2() {
    ut_ad(m_mtr != nullptr);

    ulint len = mach_read_from_2(m_node.ptr() + OFFSET_LEN);
    mlog_write_ulint(m_node.ptr() + OFFSET_LEN, len - SIZE_OF_PAGE_DIR_ENTRY,
                     MLOG_2BYTES, m_mtr);
  }

  bool is_before(const frag_node_t &frag) const {
    return (m_node.is_before(frag.m_node));
  }

  void set_frag_id_null() {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(m_node.ptr() + OFFSET_FRAG_ID, FRAG_ID_NULL, MLOG_2BYTES,
                     m_mtr);
  }

  void set_frag_id(ulint id) {
    ut_ad(m_mtr != nullptr);

    mlog_write_ulint(m_node.ptr() + OFFSET_FRAG_ID, id, MLOG_2BYTES, m_mtr);
  }

  ulint get_frag_id() const {
    return (mach_read_from_2(m_node.ptr() + OFFSET_FRAG_ID));
  }

  /** Get the space available in this fragment for storing data. */
  ulint payload() const { return (get_total_len() - header_size()); }

  /** Get the total length of this fragment, including its metadata. */
  ulint get_total_len() const {
    return (mach_read_from_2(m_node.ptr() + OFFSET_LEN));
  }

  /** Get the offset of the current fragment within page.
  @return the offset of the current fragment within. */
  paddr_t addr() const { return (m_node.addr()); }

  /** Gets the pointer to the beginning of the current fragment.  Note
  that the beginning of the fragment contains meta data.
  @return pointer to the beginning of the current fragment. */
  byte *ptr() const {
    ut_ad(!m_node.is_null());
    return (m_node.ptr());
  }

  /** Gets the pointer just after the current fragment.  The pointer
  returned does not belong to this fragment.  This is used to check
  adjacency.
  @return pointer to the end of the current fragment. */
  byte *end_ptr() const {
    ut_ad(!m_node.is_null());
    return (ptr() + get_total_len());
  }

  /** Get the page frame.
  @return the page frame. */
  byte *frame() const { return (m_node.get_frame()); }

  std::ostream &print(std::ostream &out) const {
    if (!m_node.is_null()) {
      ulint len = get_total_len();
      out << "[frag_node_t: " << m_node << ", len=" << len << "/" << payload()
          << ", frag_id=" << get_frag_id() << "]";
    } else {
      out << "[frag_node_t: null, len=0]";
    }
    return (out);
  }

  frag_node_t get_next_frag() {
    ut_ad(!is_null());
    plist_node_t next = m_node.get_next_node();
    return (frag_node_t(next, m_mtr));
  }

  frag_node_t get_next_node() { return (get_next_frag()); }

  frag_node_t get_prev_node() { return (get_prev_frag()); }

  frag_node_t get_prev_frag() {
    ut_ad(!is_null());
    plist_node_t prev = m_node.get_prev_node();
    return (frag_node_t(prev, m_mtr));
  }

  /** Merge the current fragment node with the given next fragment node.
  This will succeed only if they are adjacent to each other.
  Detailed Note: There is a new page type FIL_PAGE_TYPE_ZLOB_FRAG_ENTRY
  - and we can call it the fragment pages.  Each fragment page contains
  one or more fragments.  Each fragment is represented by a frag_node_t.
  And each fragment can be of different size.  Consider a fragment page
  containing 4 fragments - f1, f2, f3 and f4.  Suppose we free f2 and
  f3, then we can merge them into one single bigger fragment which is
  free.
  @param[in]	next	the next fragment.
  @return true if merge done, false otherwise. */
  bool merge(frag_node_t &next) {
    ut_ad(m_mtr != nullptr);

    byte *p1 = ptr();
    ulint len1 = get_total_len();
    byte *p2 = next.ptr();
    ulint len2 = next.get_total_len();

    if (p2 == (p1 + len1)) {
      set_total_len(len1 + len2);
      return (true);
    }

    return (false);
  }

  bool is_null() const { return (m_node.is_null()); }

  bool is_equal(const frag_node_t &that) const {
    return (m_node.is_equal(that.m_node));
  }

  bool is_equal(const plist_node_t &node) const {
    return (m_node.is_equal(node));
  }

  /** The page list node. */
  plist_node_t m_node;

 private:
  /** The mini-transaction context.  It is only in-memory. */
  mtr_t *m_mtr;
};

inline std::ostream &operator<<(std::ostream &out, const frag_node_t &obj) {
  return (obj.print(out));
}

/** The fragment page.  This page will contain fragments from different
zlib streams. */
struct z_frag_page_t {
  /** Version information. One byte. */
  static const ulint OFFSET_VERSION = FIL_PAGE_DATA;

  /** The location of z_frag_entry_t for this page. */
  static const ulint OFFSET_FRAG_ENTRY = OFFSET_VERSION + 1;

  /** The offset within page where the free space list begins. */
  static const ulint OFFSET_FREE_LIST = OFFSET_FRAG_ENTRY + FIL_ADDR_SIZE;

  /** The offset within page where the fragment list begins. */
  static const ulint OFFSET_FRAGS_LIST =
      OFFSET_FREE_LIST + plist_base_node_t::SIZE;

  /** The offset within page where the fragments can occupy . */
  static const ulint OFFSET_FRAGS_BEGIN =
      OFFSET_FRAGS_LIST + plist_base_node_t::SIZE;

  /** Offset of number of page directory entries (from end) */
  static const ulint OFFSET_PAGE_DIR_ENTRY_COUNT = FIL_PAGE_DATA_END + 2;

  /** Offset of first page directory entry (from end) */
  static const ulint OFFSET_PAGE_DIR_ENTRY_FIRST =
      OFFSET_PAGE_DIR_ENTRY_COUNT + 2;

  static const ulint SIZE_OF_PAGE_DIR_ENTRY = 2; /* bytes */

  /** Constructor.
  @param[in]	block	the buffer block containing the fragment page.
  @param[in]	mtr	the mini transaction context.
  @param[in]	index	the clustered index to which LOB belongs. */
  z_frag_page_t(buf_block_t *block, mtr_t *mtr, dict_index_t *index)
      : m_block(block), m_mtr(mtr), m_index(index) {
    ut_ad(frag_node_t::SIZE_OF_PAGE_DIR_ENTRY ==
          z_frag_page_t::SIZE_OF_PAGE_DIR_ENTRY);
  }

  /** Constructor.
  @param[in]	mtr	the mini transaction context.
  @param[in]	index	the clustered index to which LOB belongs. */
  z_frag_page_t(mtr_t *mtr, dict_index_t *index)
      : z_frag_page_t(nullptr, mtr, index) {}

  /** Constructor.
  @param[in]	block	the buffer block containing the fragment page.*/
  explicit z_frag_page_t(buf_block_t *block)
      : m_block(block), m_mtr(nullptr), m_index(nullptr) {
    ut_ad(frag_node_t::SIZE_OF_PAGE_DIR_ENTRY ==
          z_frag_page_t::SIZE_OF_PAGE_DIR_ENTRY);
  }

  /** Write the space identifier to the page header, without generating
  redo log records.
  @param[in]	space_id	the space identifier. */
  void set_space_id_no_redo(space_id_t space_id) {
    mlog_write_ulint(frame() + FIL_PAGE_SPACE_ID, space_id, MLOG_4BYTES,
                     nullptr);
  }

  z_frag_entry_t get_frag_entry_x();
  z_frag_entry_t get_frag_entry_s();

  void update_frag_entry() {
    z_frag_entry_t entry = get_frag_entry_x();
    entry.update(*this);
  }

  void set_version_0() {
    mlog_write_ulint(frame() + OFFSET_VERSION, 0, MLOG_1BYTE, m_mtr);
  }

  flst_node_t *addr2ptr_x(fil_addr_t &addr) {
    space_id_t space = dict_index_get_space(m_index);
    const page_size_t page_size = dict_table_page_size(m_index->table);
    return (fut_get_ptr(space, page_size, addr, RW_X_LATCH, m_mtr));
  }

  flst_node_t *addr2ptr_s(fil_addr_t &addr) {
    space_id_t space = dict_index_get_space(m_index);
    const page_size_t page_size = dict_table_page_size(m_index->table);
    return (fut_get_ptr(space, page_size, addr, RW_S_LATCH, m_mtr));
  }

  void set_frag_entry(const fil_addr_t &addr) const {
    ut_a(addr.boffset < get_page_size());
    return (flst_write_addr(frame() + OFFSET_FRAG_ENTRY, addr, m_mtr));
  }

  /** Obtain the file address of the fragment entry that denotes the
  current fragment page.
  @return the file address of the fragment entry. */
  fil_addr_t get_frag_entry() const {
    return (flst_read_addr(frame() + OFFSET_FRAG_ENTRY, m_mtr));
  }

  void set_frag_entry_null() const {
    return (flst_write_addr(frame() + OFFSET_FRAG_ENTRY, fil_addr_null, m_mtr));
  }

  ulint get_n_dir_entries() const {
    byte *ptr = frame() + get_page_size() - OFFSET_PAGE_DIR_ENTRY_COUNT;
    return (mach_read_from_2(ptr));
  }

  void set_n_dir_entries(ulint n) const {
    byte *ptr = frame() + get_page_size() - OFFSET_PAGE_DIR_ENTRY_COUNT;
    mlog_write_ulint(ptr, n, MLOG_2BYTES, m_mtr);
  }

  bool is_border_frag(const frag_node_t &node) const {
    return (slots_end_ptr() == node.end_ptr());
  }

  byte *slots_end_ptr() const {
    ulint n = get_n_dir_entries();
    byte *first = frame() + get_page_size() - OFFSET_PAGE_DIR_ENTRY_COUNT;
    byte *ptr = first - (n * SIZE_OF_PAGE_DIR_ENTRY);
    return (ptr);
  }

  paddr_t frag_id_to_addr(ulint frag_id) const {
    byte *first = frame() + get_page_size() - OFFSET_PAGE_DIR_ENTRY_FIRST;
    byte *ptr = first - (frag_id * SIZE_OF_PAGE_DIR_ENTRY);
    return (mach_read_from_2(ptr));
  }

  ulint get_nth_dir_entry(ulint frag_id) {
    byte *first = frame() + get_page_size() - OFFSET_PAGE_DIR_ENTRY_FIRST;
    byte *ptr = first - (frag_id * SIZE_OF_PAGE_DIR_ENTRY);
    return (mach_read_from_2(ptr));
  }

  void set_nth_dir_entry(ulint frag_id, paddr_t val) {
    byte *first = frame() + get_page_size() - OFFSET_PAGE_DIR_ENTRY_FIRST;
    byte *ptr = first - (frag_id * SIZE_OF_PAGE_DIR_ENTRY);
    mlog_write_ulint(ptr, val, MLOG_2BYTES, m_mtr);
  }

  ulint init_last_dir_entry() {
    ulint n = get_n_dir_entries();
    set_nth_dir_entry(n - 1, 0);
    return (n - 1);
  }

  void incr_n_dir_entries() const {
    byte *ptr = frame() + get_page_size() - OFFSET_PAGE_DIR_ENTRY_COUNT;
    ulint n = mach_read_from_2(ptr);
    ut_a(n < FRAG_ID_NULL);
    mlog_write_ulint(ptr, n + 1, MLOG_2BYTES, m_mtr);
  }

  void decr_n_dir_entries() const {
    byte *ptr = frame() + get_page_size() - OFFSET_PAGE_DIR_ENTRY_COUNT;
    ulint n = mach_read_from_2(ptr);
    ut_a(n > 0);
    mlog_write_ulint(ptr, n - 1, MLOG_2BYTES, m_mtr);
  }

  ulint get_page_size() const {
    const page_size_t page_size = dict_table_page_size(m_index->table);
    return (page_size.physical());
  }

  ulint space_used_by_dir() const {
    ulint n = get_n_dir_entries();
    return (n * SIZE_OF_PAGE_DIR_ENTRY);
  }

  ulint locate_free_slot() {
    ulint n = get_n_dir_entries();

    for (ulint frag_id = 0; frag_id < n; frag_id++) {
      ulint paddr = get_nth_dir_entry(frag_id);

      if (paddr == 0) {
        return (frag_id);
      }
    }

    return (FRAG_ID_NULL);
  }

  /** Allocate a fragment id.
  @return On success, return fragment id.
  @return On failure, return FRAG_ID_NULL. */
  ulint alloc_frag_id() {
    ulint id = locate_free_slot();

    if (id == FRAG_ID_NULL) {
      return (alloc_dir_entry());
    }

    return (id);
  }

  std::ostream &print_frag_id(std::ostream &out) {
    ulint n = get_n_dir_entries();
    out << "FRAG IDS: " << std::endl;

    for (ulint frag_id = 0; frag_id < n; frag_id++) {
      out << "id=" << frag_id << ", addr=" << frag_id_to_addr(frag_id)
          << std::endl;
    }

    return (out);
  }

  /** Grow the frag directory by one entry.
  @return the fragment identifier that was newly added. */
  ulint alloc_dir_entry();

  /** Set the next page. */
  void set_page_next(page_no_t page_no) {
    mlog_write_ulint(frame() + FIL_PAGE_NEXT, page_no, MLOG_4BYTES, m_mtr);
  }

  /** Allocate the fragment page.
  @param[in]	hint	hint page number for allocation.
  @param[in]	bulk	true if bulk operation (OPCODE_INSERT_BULK)
                          false otherwise.
  @return the allocated buffer block. */
  buf_block_t *alloc(page_no_t hint, bool bulk);

  /** Free the fragment page along with its entry. */
  void dealloc(z_first_page_t &first, mtr_t *alloc_mtr);

  buf_block_t *load_x(page_no_t page_no) {
    page_id_t page_id(dict_index_get_space(m_index), page_no);
    page_size_t page_size(dict_table_page_size(m_index->table));
    m_block = buf_page_get(page_id, page_size, RW_X_LATCH, m_mtr);
    return (m_block);
  }

  void merge_free_frags() {
    plist_base_node_t free_lst = free_list();
    frag_node_t frag(free_lst.get_first_node(), m_mtr);
    frag_node_t next = frag.get_next_frag();

    while (!next.is_null() && frag.merge(next)) {
      free_lst.remove(next.m_node);
      next = frag.get_next_frag();
    }
  }

  void merge_free_frags(frag_node_t &frag) {
    ut_ad(!frag.is_null());
    plist_base_node_t free_lst = free_list();
    frag_node_t next = frag.get_next_frag();

    while (!next.is_null() && frag.merge(next)) {
      free_lst.remove(next.m_node);
      next = frag.get_next_frag();
    }
  }

  bool validate_lists() const {
    plist_base_node_t free_lst = free_list();
    plist_base_node_t frag_lst = frag_list();
    plist_node_t free_node = free_lst.get_first_node();

    while (!free_node.is_null()) {
      plist_node_t frag_node = frag_lst.get_first_node();

      while (!frag_node.is_null()) {
        ut_ad(frag_node.addr() != free_node.addr());
        frag_node = frag_node.get_next_node();
      }

      free_node = free_node.get_next_node();
    }
    return (true);
  }

  void insert_into_free_list(frag_node_t &frag) {
    ut_ad(frag.get_frag_id() == FRAG_ID_NULL);

    plist_base_node_t free_lst = free_list();

    plist_node_t node = free_lst.get_first_node();
    plist_node_t prev_node(m_mtr);

    while (!node.is_null()) {
      ut_ad(frag.addr() != node.addr());
      if (frag.addr() < node.addr()) {
        break;
      }
      prev_node = node;
      node = node.get_next_node();
    }

    free_lst.insert_before(node, frag.m_node);

    if (prev_node.is_null()) {
      merge_free_frags();
    } else {
      frag_node_t prev_frag(prev_node, m_mtr);
      merge_free_frags(prev_frag);
    }
  }

  /** Insert the given fragment node into the fragment list.
  @param[in,out]	frag	the fragment node to be inserted.*/
  void insert_into_frag_list(frag_node_t &frag) {
    plist_base_node_t frag_lst = frag_list();
    plist_node_t node = frag_lst.get_first_node();

    while (!node.is_null()) {
      ut_ad(frag.addr() != node.addr());
      if (frag.addr() < node.addr()) {
        break;
      }
      node = node.get_next_node();
    }

    frag_lst.insert_before(node, frag.m_node);
  }

  /** Split one free fragment into two. This is not splitting a
  fragment page.  This is just splitting one free fragment into two.
  When we want to allocate one fragment, we identify a big enough free
  fragment and split it into two - one will be the allocated portion and
  other will become a free fragment.
  @param[in]  free_frag  the free fragment that will be split.
  @param[in]  size  the payload size in bytes. */
  void split_free_frag(frag_node_t &free_frag, ulint size) {
    ut_ad(size < free_frag.payload());
    const ulint old_total_len = free_frag.get_total_len();
    plist_base_node_t free_lst = free_list();

    /* Locate the next fragment */
    byte *p2 = free_frag.data_begin() + size;

    ulint remain =
        free_frag.get_total_len() - frag_node_t::header_size() - size;

    ut_a(remain >= frag_node_t::OFFSET_DATA);

    free_frag.set_total_len(frag_node_t::header_size() + size);

    frag_node_t frag2(frame(), p2, remain, m_mtr);
    frag2.set_total_len(remain);
    frag2.set_frag_id_null();
    free_lst.insert_after(free_frag.m_node, frag2.m_node);

    ut_a(free_frag.get_total_len() + frag2.get_total_len() == old_total_len);

    ut_ad(validate_lists());
  }

  frag_node_t get_frag_node(frag_id_t id) const {
    ut_ad(id != FRAG_ID_NULL);

    paddr_t off = frag_id_to_addr(id);
    byte *f = frame();
    return (frag_node_t(f, f + off));
  }

  void dealloc_fragment(ulint frag_id) {
    ut_ad(frag_id != FRAG_ID_NULL);

    paddr_t off = frag_id_to_addr(frag_id);
    byte *f = frame();
    frag_node_t frag(f, f + off, m_mtr);
    dealloc_fragment(frag);
    dealloc_frag_id(frag_id);

    /* Update the index entry. */
    update_frag_entry();
  }

  /** Allocate a fragment with the given payload.
  @param[in]  size  the payload size.
  @param[in]  entry the index entry of the given frag page.
  @return the frag_id of the allocated fragment.
  @return FRAG_ID_NULL if fragment could not be allocated. */
  frag_id_t alloc_fragment(ulint size, z_frag_entry_t &entry);

  plist_base_node_t free_list() const {
    byte *f = frame();
    return (plist_base_node_t(f, f + OFFSET_FREE_LIST, m_mtr));
  }

  plist_base_node_t frag_list() const {
    byte *f = frame();
    return (plist_base_node_t(f, f + OFFSET_FRAGS_LIST, m_mtr));
  }

  void set_page_type() {
    byte *ptr = frame() + FIL_PAGE_TYPE;
    mlog_write_ulint(ptr, FIL_PAGE_TYPE_ZLOB_FRAG, MLOG_2BYTES, m_mtr);
  }

  page_type_t get_page_type() const {
    return (mach_read_from_2(frame() + FIL_PAGE_TYPE));
  }

  const char *get_page_type_str() const {
    page_type_t type = get_page_type();
    ut_a(type == FIL_PAGE_TYPE_ZLOB_FRAG);
    return ("FIL_PAGE_TYPE_ZLOB_FRAG");
  }

  /** The maximum free space available in a fragment page. Adjustment
  needs to be done with the frag_node_t::overhead().*/
  ulint payload() { return (z_frag_page_t::max_payload(m_index)); }

  /** The maximum free space available in a fragment page. Adjustment
  needs to be done with the frag_node_t::overhead().*/
  static ulint max_payload(dict_index_t *index) {
    page_size_t page_size(dict_table_page_size(index->table));
    return (page_size.physical() - OFFSET_FRAGS_BEGIN -
            OFFSET_PAGE_DIR_ENTRY_COUNT);
  }

  /** Determine if the given length of data can fit into a fragment
  page.
  @param[in]   index   the clust index into which LOB is inserted.
  @param[in]   data_size  The length of data to operate.
  @return true if data can fit into fragment page, false otherwise. */
  static bool can_data_fit(dict_index_t *index, ulint data_size);

  /** Get the frag page number. */
  page_no_t get_page_no() const { return (m_block->get_page_no()); }

  byte *frame() const { return (buf_block_get_frame(m_block)); }

  std::ostream &print(std::ostream &out) const {
    print_free_list(out);
    print_frag_list(out);
    print_frags_in_order(out);
    print_page_dir(out);
    return (out);
  }

  /** Get the total amount of stored data in this page. */
  ulint get_total_stored_data() const;

  /** Get the total cumulative free space in this page. */
  ulint get_total_free_len() const;

  /** Get the big free space in this page. */
  ulint get_big_free_len() const;

  /** Get the number of fragments in this frag page. */
  ulint get_n_frags() const {
    plist_base_node_t frag_lst = frag_list();
    return (frag_lst.get_len());
  }

  std::ostream &print_frags_in_order(std::ostream &out) const;

  std::ostream &print_free_list(std::ostream &out) const {
    if (m_block == nullptr) {
      return (out);
    }

    plist_base_node_t free_lst = free_list();
    out << "[Free List: " << free_lst << "]" << std::endl;

    for (plist_node_t cur = free_lst.get_first_node(); !cur.is_null();
         cur = cur.get_next_node()) {
      frag_node_t frag(cur, m_mtr);
      out << frag << std::endl;
    }
    return (out);
  }

  std::ostream &print_frag_list(std::ostream &out) const {
    if (m_block == nullptr) {
      return (out);
    }

    plist_base_node_t frag_lst = frag_list();
    out << "[Frag List: " << frag_lst << "]" << std::endl;

    for (plist_node_t cur = frag_lst.get_first_node(); !cur.is_null();
         cur = cur.get_next_node()) {
      frag_node_t frag(cur, m_mtr);
      out << frag << std::endl;
    }
    return (out);
  }

  std::ostream &print_page_dir(std::ostream &out) const {
    if (m_block == nullptr) {
      return (out);
    }

    ulint n = get_n_dir_entries();

    for (ulint frag_id = 0; frag_id < n; ++frag_id) {
      paddr_t off = frag_id_to_addr(frag_id);
      out << "[frag_id=" << frag_id << ", addr=" << off << "]" << std::endl;
    }

    return (out);
  }

  void set_mtr(mtr_t *mtr) { m_mtr = mtr; }

  void set_index(dict_index_t *index) { m_index = index; }

  void set_block_null() { m_block = nullptr; }

  /** Determine if the given fragment node is the last fragment
  node adjacent to the directory.
  @return true if it is last fragment node, false otherwise. */
  bool is_last_frag(const frag_node_t &node) const {
    return (node.end_ptr() == slots_end_ptr());
  }

 private:
  fil_addr_t get_frag_entry_addr() const {
    return (flst_read_addr(frame() + OFFSET_FRAG_ENTRY, m_mtr));
  }

  void dealloc_fragment(frag_node_t &frag) {
    plist_base_node_t frag_lst = frag_list();
    frag_lst.remove(frag.m_node);
    frag.set_frag_id_null();
    insert_into_free_list(frag);
  }

  /** Deallocate all the free slots from the end of the page
  directory. */
  void dealloc_frag_id();

  /** Deallocate the given fragment id.
  @param[in] frag_id The fragment that needs to be deallocated. */
  void dealloc_frag_id(ulint frag_id) {
    set_nth_dir_entry(frag_id, 0);
    dealloc_frag_id();
  }

  buf_block_t *m_block;
  mtr_t *m_mtr;
  dict_index_t *m_index;
};

/** Insert one chunk of input.  The maximum size of a chunk is Z_CHUNK_SIZE.
@param[in]  index      clustered index in which LOB is inserted.
@param[in]  first      the first page of the LOB.
@param[in]  trx        transaction doing the insertion.
@param[in]  ref        LOB reference in the clust rec.
@param[in]  blob       the uncompressed LOB to be inserted.
@param[in]  len        length of the blob.
@param[out] out_entry  the newly inserted index entry. can be NULL.
@param[in]  mtr        the mini transaction
@param[in]  bulk       true if it is bulk operation, false otherwise.
@return DB_SUCCESS on success, error code on failure. */
dberr_t z_insert_chunk(dict_index_t *index, z_first_page_t &first, trx_t *trx,
                       ref_t ref, byte *blob, ulint len,
                       z_index_entry_t *out_entry, mtr_t *mtr, bool bulk);

}  // namespace lob

#endif  // lob0impl_h
