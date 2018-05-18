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

#include "lob0index.h"
#include "lob0first.h"
#include "lob0pages.h"
#include "trx0purge.h"
#include "trx0trx.h"

namespace lob {

/** Move the version base node from current entry to the given entry.
@param[in]	to_entry	The index entry to which the version base
                                node is moved to. */
void index_entry_t::move_version_base_node(index_entry_t &to_entry) {
  flst_base_node_t *from_node = get_versions_list();
  flst_base_node_t *to_node = to_entry.get_versions_list();
  mlog_write_string(to_node, from_node, FLST_BASE_NODE_SIZE, m_mtr);
  ut_ad(flst_get_len(from_node) == flst_get_len(to_node));
  flst_init(from_node, m_mtr);
}

/** The current index entry points to a latest LOB page.  It may or may
not have older versions.  If older version is there, bring it back to the
index list from the versions list.  Then remove the current entry from
the index list.  Move the versions list from current entry to older entry.
@param[in]  index  the clustered index containing the LOB.
@param[in]  trxid  The transaction identifier.
@param[in]  first_page  The first lob page containing index list and free
                        list.
@return the location of next entry. */
fil_addr_t index_entry_t::make_old_version_current(dict_index_t *index,
                                                   trx_id_t trxid,
                                                   first_page_t &first_page) {
  flst_base_node_t *base = first_page.index_list();
  flst_base_node_t *free_list = first_page.free_list();
  flst_base_node_t *version_list = get_versions_ptr();

  if (flst_get_len(version_list) > 0) {
    space_id_t space = dict_index_get_space(index);
    const page_size_t page_size = dict_table_page_size(index->table);

    /* Remove the old version from versions list. */
    fil_addr_t old_node_addr = flst_get_first(version_list, m_mtr);

    flst_node_t *old_node =
        fut_get_ptr(space, page_size, old_node_addr, RW_X_LATCH, m_mtr);

    flst_remove(version_list, old_node, m_mtr);

    /* Copy the version base node from current to old entry. */
    index_entry_t old_entry(old_node, m_mtr, index);
    move_version_base_node(old_entry);

    /* Insert the old version after the current node. */
    flst_insert_after(base, m_node, old_node, m_mtr);
  }

  fil_addr_t loc = purge_version(index, trxid, base, free_list);

  ut_ad(flst_validate(base, m_mtr));

  return (loc);
}

/** Purge the current index entry. An index entry points to either a FIRST
page or DATA page.  That LOB page will be freed if it is DATA page.  A FIRST
page should not be freed. */
void index_entry_t::purge(dict_index_t *index) {
  DBUG_ENTER("index_entry_t::purge");

  page_no_t page_no = get_page_no();

  buf_block_t *block = nullptr;

  block = buf_page_get(page_id_t(dict_index_get_space(index), page_no),
                       dict_table_page_size(index->table), RW_X_LATCH, m_mtr);

  page_type_t type = fil_page_get_type(block->frame);

  if (type != FIL_PAGE_TYPE_LOB_FIRST) {
    data_page_t data_page(block, m_mtr, index);
    data_page.dealloc();
  }

  set_prev_null();
  set_next_null();
  set_versions_null();
  set_page_no(FIL_NULL);
  set_trx_id(0);
  set_trx_id_modifier(0);
  set_trx_undo_no(0);
  set_data_len(0);

  DBUG_VOID_RETURN;
}

/** Purge the current entry.
@param[in]  index  the clustered index containing the LOB.
@param[in]  trxid  The transaction identifier.
@param[in]  lst    the base node of index list.
@param[in]  free_list    the base node of free list.
@return the location of the next entry. */
fil_addr_t index_entry_t::purge_version(dict_index_t *index, trx_id_t trxid,
                                        flst_base_node_t *lst,
                                        flst_base_node_t *free_list) {
  /* Save the location of next node. */
  fil_addr_t next_loc = flst_get_next_addr(m_node, m_mtr);

  /* Remove the current node from the list it belongs. */
  flst_remove(lst, m_node, m_mtr);

  /* Purge the current node. */
  purge(index);

  /* Add the current node to the free list. */
  flst_add_first(free_list, m_node, m_mtr);

  /* Return the next node location. */
  return (next_loc);
}

std::ostream &index_entry_t::print(std::ostream &out) const {
  if (!is_null()) {
    out << "[index_entry_t: node=" << (void *)m_node << ", self=" << get_self()
        << ", creator trxid=" << get_trx_id()
        << ", modifier_trxid=" << get_trx_id_modifier()
        << ", trx_undo_no=" << get_trx_undo_no()
        << ", page_no=" << get_page_no() << ", data_len=" << get_data_len()
        << ", lob version=" << get_lob_version() << ", index_id=" << m_index->id
        << ", next=" << get_next() << ", prev=" << get_prev()
        << ", versions=" << get_versions_mem() << "]";
  }
  return (out);
}

fil_addr_t index_entry_t::get_self() const {
  if (m_node == nullptr) {
    return (fil_addr_null);
  }
  page_t *frame = page_align(m_node);
  page_no_t page_no = mach_read_from_4(frame + FIL_PAGE_OFFSET);
  ulint offset = m_node - frame;
  ut_ad(offset < UNIV_PAGE_SIZE);

  return (fil_addr_t(page_no, offset));
}

void index_entry_t::read(index_entry_mem_t &entry_mem) const {
  if (is_null()) {
    entry_mem.reset();
    return;
  }

  entry_mem.m_self = get_self();
  entry_mem.m_prev = get_prev();
  entry_mem.m_next = get_next();
  entry_mem.m_versions = get_versions_mem();
  entry_mem.m_trx_id = get_trx_id();
  entry_mem.m_trx_id_modifier = get_trx_id_modifier();
  entry_mem.m_undo_no = get_trx_undo_no();
  entry_mem.m_undo_no_modifier = get_trx_undo_no_modifier();
  entry_mem.m_page_no = get_page_no();
  entry_mem.m_data_len = get_data_len();
}

/** Load the index entry available in the given file address.
Take x-latch on the index page.
@param[in]	addr	the file address of the index entry.
@return the buffer block containing the index entry. */
buf_block_t *index_entry_t::load_x(const fil_addr_t &addr) {
  ut_ad(m_mtr != nullptr);
  ut_ad(m_index != nullptr);

  m_block = nullptr;
  const space_id_t space_id = dict_index_get_space(m_index);
  const page_size_t page_size = dict_table_page_size(m_index->table);

  m_node = fut_get_ptr(space_id, page_size, addr, RW_X_LATCH, m_mtr, &m_block);

  return (m_block);
}

/** Load the index entry available in the given file address.
Take s-latch on the index page.
@param[in]	addr	the file location of index entry.
@return the buffer block. */
buf_block_t *index_entry_t::load_s(const fil_addr_t &addr) {
  ut_ad(m_mtr != nullptr);
  ut_ad(m_index != nullptr);

  m_block = nullptr;
  const space_id_t space_id = dict_index_get_space(m_index);
  const page_size_t page_size = dict_table_page_size(m_index->table);

  m_node = fut_get_ptr(space_id, page_size, addr, RW_S_LATCH, m_mtr, &m_block);

  return (m_block);
}

void index_entry_mem_t::reset() {
  m_self = fil_addr_null;
  m_prev = fil_addr_null;
  m_next = fil_addr_null;
  m_versions.reset();
  m_trx_id = 0;
  m_trx_id_modifier = 0;
  m_undo_no = 0;
  m_undo_no_modifier = 0;
  m_page_no = FIL_NULL;
  m_data_len = 0;
}

/** Print this object into the given output stream.
@param[in]	out	the output stream.
@return the output stream. */
std::ostream &index_entry_mem_t::print(std::ostream &out) const {
  out << "[index_entry_mem_t: m_self=" << m_self << ", m_prev=" << m_prev
      << ", m_next=" << m_next << ", m_versions=" << m_versions
      << ", m_trx_id=" << m_trx_id
      << ", m_trx_id_modifier=" << m_trx_id_modifier
      << ", m_undo_no=" << m_undo_no
      << ", m_undo_no_modifier=" << m_undo_no_modifier
      << ", m_page_no=" << m_page_no << ", m_data_len=" << m_data_len << "]";
  return (out);
}

}; /* namespace lob */
