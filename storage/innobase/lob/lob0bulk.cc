/*****************************************************************************

Copyright (c) 2024, Oracle and/or its affiliates.

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

#include "lob0bulk.h"
#include "btr0mtib.h"

namespace lob {

namespace bulk {

using ::bulk::flst_add_last;

void first_page_t::append_to_index(flst_node_t *node) {
  flst_base_node_t *index_lst = index_list();
  flst_add_last(index_lst, node, m_blocks);
}

void first_page_t::init_lists() {
  byte *free_lst = free_list();
  byte *index_lst = index_list();

  ::bulk::flst_init(index_lst);
  ::bulk::flst_init(free_lst);

  ulint nc = node_count();

  byte *cur = nodes_begin();
  for (ulint i = 0; i < nc; ++i) {
    index_entry_t entry(cur);
    entry.init();
    ::bulk::flst_add_last(free_lst, cur, m_blocks);
    cur += index_entry_t::SIZE;
  }
}

void first_page_t::init(Btree_multi::Page_load *page_load) {
  ut_ad(page_load != nullptr);
  ut_ad(page_load->is_memory());
  ut_ad(page_load->is_leaf());
  ut_ad(page_load->get_page_no() != FIL_NULL);
  ut_ad(m_mtr == nullptr);

  m_page_load = page_load;
  m_block = page_load->get_block();
  m_blocks.push_back(m_block);

  set_page_type();

  set_version_0();
  set_data_len(0);
  set_trx_id(0);
  init_lists();

  flst_node_t *node = alloc_index_entry();
  ut_a(node != nullptr);

  set_last_trx_id(page_load->get_trx_id());
  init_lob_version();

  m_index_entry.reset(node);
  m_index_entry.set_versions_null();
  m_index_entry.set_trx_id(get_trx_id());
  m_index_entry.set_trx_id_modifier(get_trx_id());
  m_index_entry.set_trx_undo_no(0);
  m_index_entry.set_trx_undo_no_modifier(0);
  m_index_entry.set_page_no(get_page_no());
  m_index_entry.set_data_len(0);
  m_index_entry.set_lob_version(1);

  flst_base_node_t *idx_list = index_list();
  flst_add_last(idx_list, node, m_blocks);
}

flst_node_t *first_page_t::alloc_index_entry() {
  ut_ad(m_mtr == nullptr);

  flst_base_node_t *f_list = free_list();

  fil_addr_t node_addr = ::bulk::flst_get_first(f_list);
  if (fil_addr_is_null(node_addr)) {
    return nullptr;
  }
  flst_node_t *node = ::bulk::fut_get_ptr(node_addr, m_blocks);
  ::bulk::flst_remove(f_list, node, m_blocks);
  return node;
}

trx_id_t first_page_t::get_trx_id() const { return m_page_load->get_trx_id(); }

page_no_t first_page_t::get_page_no() const {
  return m_page_load->get_page_no();
}

void first_page_t::set_next_page(page_no_t page_no) {
  mach_write_ulint(frame() + FIL_PAGE_NEXT, page_no, MLOG_4BYTES);
}

void node_page_t::set_page_type() {
  mach_write_ulint(frame() + FIL_PAGE_TYPE, FIL_PAGE_TYPE_LOB_INDEX,
                   MLOG_2BYTES);
}

void node_page_t::set_version_0() {
  mach_write_ulint(frame() + OFFSET_VERSION, 0, MLOG_1BYTE);
}

void node_page_t::set_next_page(page_no_t page_no) {
  mach_write_ulint(frame() + FIL_PAGE_NEXT, page_no, MLOG_4BYTES);
}

void node_page_t::init(Btree_multi::Page_load *node_page,
                       first_page_t &first_page) {
  ut_ad(node_page != nullptr);

  m_page_load = node_page;
  m_block = node_page->get_block();

  first_page.get_blocks_cache().push_back(m_block);

  set_page_type();
  set_version_0();
  set_next_page(first_page.get_next_page());

  first_page.set_next_page(m_page_load->get_page_no());

  ulint lob_metadata_len = payload();
  ulint node_count = lob_metadata_len / index_entry_t::SIZE;

  flst_base_node_t *free_list = first_page.free_list();
  byte *cur = nodes_begin();

  /* Populate the free list with empty index entry nodes. */
  for (ulint i = 0; i < node_count; ++i) {
    flst_add_last(free_list, cur, first_page.get_blocks_cache());
    cur += index_entry_t::SIZE;
  }
}

void data_page_t::set_page_type() {
  mach_write_ulint(frame() + FIL_PAGE_TYPE, FIL_PAGE_TYPE_LOB_DATA,
                   MLOG_2BYTES);
}

void data_page_t::set_version_0() {
  mach_write_ulint(frame() + OFFSET_VERSION, 0, MLOG_1BYTE);
}

void data_page_t::set_next_page_null() {
  mach_write_ulint(frame() + FIL_PAGE_NEXT, FIL_NULL, MLOG_4BYTES);
}

void data_page_t::init(Btree_multi::Page_load *data_page) {
  ut_ad(data_page != nullptr);

  m_page_load = data_page;
  m_block = data_page->get_block();
  ut_ad(m_block->is_memory());

  set_page_type();
  set_version_0();
  set_next_page_null();
}

} /* namespace bulk */

} /* namespace lob */
