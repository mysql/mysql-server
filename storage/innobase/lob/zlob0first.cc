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

#include "zlob0first.h"
#include "trx0trx.h"
#include "zlob0index.h"
#include "zlob0read.h"

namespace lob {

/** Given the page size, what is the number of index entries the first page
can contain. */
ulint z_first_page_t::get_n_index_entries() const {
  ut_ad(m_index != nullptr);

  const page_size_t page_size(dict_table_page_size(m_index->table));

  ulint len = page_size.physical();
  switch (len) {
    case KB16:
      /* For a page size of 16KB, there are 100 index entries in
      the first page of the zlob. */
      return (100);
    case 8192:
      /* 8KB. */
      return (80);
    case 4096:
      /* 4KB. */
      return (40);
    case 2048:
      return (20);
    case 1024:
      return (5);
    default:
      ut_error;
  }
}

/** Given the page size, what is the number of frag entries the first page
can contain. */
ulint z_first_page_t::get_n_frag_entries() const {
  ut_ad(m_index != nullptr);

  DBUG_EXECUTE_IF("innodb_zlob_first_use_only_1_frag_entries", return (1););

  const page_size_t page_size(dict_table_page_size(m_index->table));
  ulint len = page_size.physical();
  switch (len) {
    case KB16:
      /* For a page size of 16KB, there are 200 frag entries in
      the first page of the zlob. */
      return (200);
    case 8192:
      return (100);
    case 4096:
      return (40);
    case 2048:
      return (20);
    case 1024:
      return (5);
    default:
      ut_error;
  }
}

buf_block_t *z_first_page_t::alloc(bool bulk) {
  ut_ad(m_block == nullptr);

  page_no_t hint = FIL_NULL;
  m_block = alloc_lob_page(m_index, m_mtr, hint, bulk);

  if (m_block == nullptr) {
    return (nullptr);
  }

  init();

  ut_ad(m_block->get_page_type() == FIL_PAGE_TYPE_ZLOB_FIRST);
  return (m_block);
}

/** Print the index entries. */
std::ostream &z_first_page_t::print_index_entries(std::ostream &out) const {
  flst_base_node_t *flst = index_list();
  fil_addr_t node_loc = flst_get_first(flst, m_mtr);

  space_id_t space = dict_index_get_space(m_index);
  const page_size_t page_size = dict_table_page_size(m_index->table);

  out << "Index Entries: " << flst_bnode_t(flst, m_mtr) << std::endl;

  while (!fil_addr_is_null(node_loc)) {
    flst_node_t *node =
        fut_get_ptr(space, page_size, node_loc, RW_X_LATCH, m_mtr);
    z_index_entry_t entry(node, m_mtr, m_index);
    out << entry << std::endl;

    flst_base_node_t *vers = entry.get_versions_list();
    fil_addr_t ver_loc = flst_get_first(vers, m_mtr);

    uint32_t depth = 0;
    while (!fil_addr_is_null(ver_loc)) {
      depth++;

      for (uint32_t i = 0; i < depth; ++i) {
        out << "+";
      }
      flst_node_t *ver_node = addr2ptr_x(ver_loc);
      z_index_entry_t vers_entry(ver_node, m_mtr, m_index);
      out << vers_entry << std::endl;
      ver_loc = vers_entry.get_next();
    }

    node_loc = entry.get_next();
  }

  return (out);
}

/** Print the frag entries. */
std::ostream &z_first_page_t::print_frag_entries(std::ostream &out) const {
  flst_base_node_t *flst = frag_list();
  fil_addr_t node_loc = flst_get_first(flst, m_mtr);
  space_id_t space = dict_index_get_space(m_index);
  const page_size_t page_size = dict_table_page_size(m_index->table);

  out << "Frag Entries: " << flst_bnode_t(flst, m_mtr) << std::endl;

  while (!fil_addr_is_null(node_loc)) {
    flst_node_t *node =
        fut_get_ptr(space, page_size, node_loc, RW_X_LATCH, m_mtr);
    z_frag_entry_t entry(node, m_mtr);
    out << entry << std::endl;
    node_loc = entry.get_next();
  }

  return (out);
}

/** Allocate one index entry.  If there is no free index entry, allocate
an index page (a page full of z_index_entry_t objects) and service the
request.
@return the allocated index entry. */
z_index_entry_t z_first_page_t::alloc_index_entry(bool bulk) {
  flst_base_node_t *free_lst = free_list();
  fil_addr_t first_loc = flst_get_first(free_lst, m_mtr);

  if (fil_addr_is_null(first_loc)) {
    z_index_page_t page(m_mtr, m_index);
    page.alloc(*this, bulk);
    first_loc = flst_get_first(free_lst, m_mtr);
  }

  if (fil_addr_is_null(first_loc)) {
    return (z_index_entry_t());
  }

  flst_node_t *first_ptr = addr2ptr_x(first_loc);
  z_index_entry_t entry(first_ptr, m_mtr);
  entry.remove(free_lst);

  return (entry);
}

/** Allocate one frag page entry.  If there is no free frag entry, allocate
an frag node page (a page full of z_frag_entry_t objects) and service the
request.
@return the allocated frag entry. */
z_frag_entry_t z_first_page_t::alloc_frag_entry(bool bulk) {
  flst_base_node_t *free_lst = free_frag_list();
  flst_base_node_t *used_lst = frag_list();

  fil_addr_t first_loc = flst_get_first(free_lst, m_mtr);

  if (fil_addr_is_null(first_loc)) {
    z_frag_node_page_t page(m_mtr, m_index);
    page.alloc(*this, bulk);
    first_loc = flst_get_first(free_lst, m_mtr);
  }

  if (fil_addr_is_null(first_loc)) {
    return (z_frag_entry_t());
  }

  flst_node_t *first_ptr = addr2ptr_x(first_loc);
  z_frag_entry_t entry(first_ptr, m_mtr);
  entry.remove(free_lst);
  entry.push_front(used_lst);
  return (entry);
}

frag_id_t z_first_page_t::alloc_fragment(bool bulk, ulint len,
                                         z_frag_page_t &frag_page,
                                         z_frag_entry_t &entry) {
  ut_ad(m_mtr != nullptr);

  frag_id_t frag_id = FRAG_ID_NULL;

  frag_page.set_mtr(m_mtr);
  frag_page.set_index(m_index);
  frag_page.set_block_null();

  const page_no_t first_page_no = get_page_no();

  /* Make sure that there will be some extra space for page directory
  entry and meta data.  Adding a margin to provide for this.  This is
  for exact fit. */
  const ulint look_size = len + frag_node_t::header_size();

  ut_ad(look_size <= z_frag_page_t::max_payload(m_index));

  flst_base_node_t *frag_lst = frag_list();

  /* Iterate through the list of frag entries in the page. */
  fil_addr_t loc = flst_get_first(frag_lst, m_mtr);

  while (!fil_addr_is_null(loc)) {
    flst_node_t *node = addr2ptr_x(loc);
    entry.reset(node);

    ulint big_free = entry.get_big_free_len();

    if (big_free >= look_size) {
      /* Double check if the information in the index
      entry matches with the fragment page. If not, update
      the index entry. */
      frag_page.load_x(entry.get_page_no());

      const ulint big_free_len_1 = frag_page.get_big_free_len();
      const ulint big_free_len_2 = entry.get_big_free_len();

      if (big_free_len_1 == big_free_len_2) {
        frag_id = frag_page.alloc_fragment(len, entry);
        if (frag_id != FRAG_ID_NULL) {
          break;
        }
      } else {
        entry.update(frag_page);

        /* Check again */
        big_free = entry.get_big_free_len();

        if (big_free >= look_size) {
          frag_id = frag_page.alloc_fragment(len, entry);
          if (frag_id != FRAG_ID_NULL) {
            break;
          }
        }
      }
    }

    loc = flst_get_next_addr(node, m_mtr);
    entry.reset(nullptr);
  }

  if (frag_id != FRAG_ID_NULL) {
    return (frag_id);
  }

  if (fil_addr_is_null(loc)) {
    /* Need to allocate a new fragment page. */
    buf_block_t *tmp_block = frag_page.alloc(*this, first_page_no + 1, bulk);

    if (tmp_block == nullptr) {
      return (FRAG_ID_NULL);
    }

    entry = alloc_frag_entry(bulk);

    if (entry.is_null()) {
      return (FRAG_ID_NULL);
    }

    entry.set_page_no(frag_page.get_page_no());
    frag_page.set_frag_entry(entry.get_self_addr());

    /* Update the index entry with new space information. */
    entry.update(frag_page);
  }

#ifdef UNIV_DEBUG
  /* Adding more checks to ensure that an alloc fragment doesn't fail
  for the selected fragment page. */
  fil_addr_t addr1 = frag_page.get_frag_entry();
  fil_addr_t addr2 = entry.get_self_addr();
  ut_ad(addr1.is_equal(addr2));

  const ulint big_free_len_1 = frag_page.get_big_free_len();
  const ulint big_free_len_2 = entry.get_big_free_len();
  ut_ad(big_free_len_1 == big_free_len_2);

  ut_ad(big_free_len_1 >= look_size);
  ut_ad(big_free_len_1 > len);
#endif /* UNIV_DEBUG */

  frag_id = frag_page.alloc_fragment(len, entry);

  ut_ad(frag_id != FRAG_ID_NULL);

  return (frag_id);
}

/** Print the page. */
std::ostream &z_first_page_t::print(std::ostream &out) const {
  print_index_entries(out);
  print_frag_entries(out);
  return (out);
}

/** Free all the z_frag_page_t pages. All the z_frag_page_t pages are
singly linked to each other.  The head of the list is maintained in the
first page. */
size_t z_first_page_t::free_all_frag_node_pages() {
  size_t n_pages_freed = 0;
  mtr_t local_mtr;
  mtr_start(&local_mtr);
  local_mtr.set_log_mode(m_mtr->get_log_mode());

  load_x(&local_mtr);

  while (true) {
    page_no_t page_no = get_frag_node_page_no();
    if (page_no == FIL_NULL) {
      break;
    }

    z_frag_node_page_t frag_node_page(&local_mtr, m_index);
    frag_node_page.load_x(page_no);
    page_no_t next_page = frag_node_page.get_next_page_no();

    /* Make all changes to the first page using local_mtr. */
    set_frag_node_page_no(next_page, &local_mtr);
    frag_node_page.dealloc();
    n_pages_freed++;

    ut_ad(!local_mtr.conflicts_with(m_mtr));
    restart_mtr(&local_mtr);
  }
  ut_ad(!local_mtr.conflicts_with(m_mtr));
  mtr_commit(&local_mtr);

  return (n_pages_freed);
}

/** Free all the index pages. */
size_t z_first_page_t::free_all_index_pages() {
  size_t n_pages_freed = 0;
  mtr_t local_mtr;
  mtr_start(&local_mtr);
  local_mtr.set_log_mode(m_mtr->get_log_mode());

  load_x(&local_mtr);
  while (true) {
    page_no_t page_no = get_index_page_no();
    if (page_no == FIL_NULL) {
      break;
    }
    z_index_page_t index_page(&local_mtr, m_index);
    index_page.load_x(page_no);
    page_no_t next_page = index_page.get_next_page_no();

    /* Make all changes to the first page using local_mtr. */
    set_index_page_no(next_page, &local_mtr);

    index_page.dealloc();
    n_pages_freed++;
    ut_ad(!local_mtr.conflicts_with(m_mtr));
    restart_mtr(&local_mtr);
  }
  ut_ad(!local_mtr.conflicts_with(m_mtr));
  mtr_commit(&local_mtr);
  return (n_pages_freed);
}

ulint z_first_page_t::size_of_index_entries() const {
  return (z_index_entry_t::SIZE * get_n_index_entries());
}

void z_first_page_t::init_index_entries() {
  flst_base_node_t *flst = free_list();
  ulint n = get_n_index_entries();
  for (ulint i = 0; i < n; ++i) {
    flst_node_t *ptr = frame() + OFFSET_INDEX_BEGIN;
    ptr += (i * z_index_entry_t::SIZE);
    z_index_entry_t entry(ptr, m_mtr);
    entry.init();
    entry.push_back(flst);
  }
}

void z_first_page_t::load_entry_s(fil_addr_t &addr, z_index_entry_t &entry) {
  entry.load_s(addr);
}

void z_first_page_t::load_entry_x(fil_addr_t &addr, z_index_entry_t &entry) {
  entry.load_x(addr);
}

/** Deallocate the first page of a compressed LOB. */
void z_first_page_t::dealloc() {
  ut_ad(m_mtr != nullptr);
  btr_page_free_low(m_index, m_block, ULINT_UNDEFINED, m_mtr);
  m_block = nullptr;
}

buf_block_t *z_first_page_t::load_x(const page_id_t &page_id,
                                    const page_size_t &page_size) {
  m_block =
      buf_page_get(page_id, page_size, RW_X_LATCH, UT_LOCATION_HERE, m_mtr);

#ifdef UNIV_DEBUG
  /* Dump the page into the log file, if the page type is not matching
  one of the first page types. */
  page_type_t page_type = get_page_type();

  switch (page_type) {
    case FIL_PAGE_TYPE_ZBLOB:
    case FIL_PAGE_TYPE_ZLOB_FIRST:
      /* Valid first page type for compressed LOB.*/
      break;
    default:
      ut_print_buf(std::cout, m_block->frame, page_size.physical());
      ut_error;
  }
#endif /* UNIV_DEBUG */

  return (m_block);
}

/** Increment the LOB version by 1. */
uint32_t z_first_page_t::incr_lob_version() {
  ut_ad(m_mtr != nullptr);

  const uint32_t cur = get_lob_version();
  const uint32_t val = cur + 1;
  mlog_write_ulint(frame() + OFFSET_LOB_VERSION, val, MLOG_4BYTES, m_mtr);

  return (val);
}

/** When the bit is set, the LOB is not partially updatable anymore.
Enable the bit.
@param[in]      trx     the current transaction.*/
void z_first_page_t::mark_cannot_be_partially_updated(trx_t *trx) {
  const trx_id_t trxid = (trx == nullptr) ? 0 : trx->id;
  const undo_no_t undo_no = (trx == nullptr) ? 0 : (trx->undo_no - 1);

#ifdef LOB_DEBUG
  std::cout << "thread=" << std::this_thread::get_id()
            << ", ZLOB first page=" << get_page_id()
            << ", mark_cannot_be_partially_updated()" << std::endl;
#endif /* LOB_DEBUG */

  uint8_t flags = get_flags();
  flags |= 0x01;
  mlog_write_ulint(frame() + OFFSET_FLAGS, flags, MLOG_1BYTE, m_mtr);

  set_last_trx_id(trxid);
  set_last_trx_undo_no(undo_no);
}

size_t z_first_page_t::free_all_data_pages() {
  size_t n_pages_freed = 0;
  mtr_t local_mtr;
  mtr_start(&local_mtr);
  local_mtr.set_log_mode(m_mtr->get_log_mode());
  load_x(&local_mtr);

  flst_base_node_t *flst = index_list();
  fil_addr_t node_loc = flst_get_first(flst, &local_mtr);

  z_index_entry_t cur_entry(&local_mtr, m_index);

  while (!fil_addr_is_null(node_loc)) {
    flst_node_t *node = addr2ptr_x(node_loc, &local_mtr);
    cur_entry.reset(node);
    n_pages_freed += cur_entry.free_data_pages(&local_mtr);

    flst_base_node_t *vers = cur_entry.get_versions_list();
    fil_addr_t ver_loc = flst_get_first(vers, &local_mtr);

    while (!fil_addr_is_null(ver_loc)) {
      flst_node_t *ver_node = addr2ptr_x(ver_loc, &local_mtr);
      z_index_entry_t vers_entry(ver_node, &local_mtr, m_index);
      n_pages_freed += vers_entry.free_data_pages(&local_mtr);
      ver_loc = vers_entry.get_next();

      ut_ad(!local_mtr.conflicts_with(m_mtr));
      restart_mtr(&local_mtr);
      node = addr2ptr_x(node_loc, &local_mtr);
      cur_entry.reset(node);
    }

    node_loc = cur_entry.get_next();
    cur_entry.reset(nullptr);
    ut_ad(!local_mtr.conflicts_with(m_mtr));
    restart_mtr(&local_mtr);
  }
  flst_init(flst, &local_mtr);
  flst_base_node_t *free_flst = free_list();
  flst_init(free_flst, &local_mtr);
  ut_ad(!local_mtr.conflicts_with(m_mtr));
  mtr_commit(&local_mtr);

  return (n_pages_freed);
}

#ifdef UNIV_DEBUG
bool z_first_page_t::validate_low() {
  mtr_t local_mtr;
  mtr_start(&local_mtr);
  local_mtr.set_log_mode(m_mtr->get_log_mode());
  load_x(&local_mtr);

  ut_ad(get_page_type() == FIL_PAGE_TYPE_ZLOB_FIRST);

  flst_base_node_t *flst = index_list();
  fil_addr_t node_loc = flst_get_first(flst, &local_mtr);

  z_index_entry_t cur_entry(&local_mtr, m_index);

  while (!fil_addr_is_null(node_loc)) {
    flst_node_t *node = addr2ptr_x(node_loc, &local_mtr);
    cur_entry.reset(node);

    ut_ad(z_validate_strm(m_index, cur_entry, &local_mtr));

    flst_base_node_t *vers = cur_entry.get_versions_list();
    fil_addr_t ver_loc = flst_get_first(vers, &local_mtr);

    while (!fil_addr_is_null(ver_loc)) {
      flst_node_t *ver_node = addr2ptr_x(ver_loc, &local_mtr);
      z_index_entry_t vers_entry(ver_node, &local_mtr, m_index);
      ut_ad(z_validate_strm(m_index, vers_entry, &local_mtr));
      ver_loc = vers_entry.get_next();
      restart_mtr(&local_mtr);
      node = addr2ptr_x(node_loc, &local_mtr);
      cur_entry.reset(node);
    }

    node_loc = cur_entry.get_next();
    cur_entry.reset(nullptr);

    restart_mtr(&local_mtr);
  }

  mtr_commit(&local_mtr);
  return (true);
}
#endif /* UNIV_DEBUG */

void z_first_page_t::import(trx_id_t trx_id) {
  set_trx_id_no_redo(trx_id);
  set_last_trx_id_no_redo(trx_id);

  ulint n = get_n_index_entries();
  for (ulint i = 0; i < n; ++i) {
    flst_node_t *ptr = frame() + OFFSET_INDEX_BEGIN;
    ptr += (i * z_index_entry_t::SIZE);
    z_index_entry_t entry(ptr);
    entry.set_trx_id_no_redo(trx_id);
    entry.set_trx_id_modifier_no_redo(trx_id);
  }
}

size_t z_first_page_t::free_all_frag_pages_old() {
  size_t n_pages_freed = 0;
  mtr_t local_mtr;
  mtr_start(&local_mtr);
  local_mtr.set_log_mode(m_mtr->get_log_mode());
  load_x(&local_mtr);

  /* There is no list of fragment pages maintained.  We have to identify the
  list of fragment pages from the following two lists. */
  flst_base_node_t *frag_lst = frag_list();
  flst_base_node_t *free_frag_lst = free_frag_list();

  std::vector<flst_base_node_t *> two_list = {frag_lst, free_frag_lst};

  for (auto cur_lst : two_list) {
    while (flst_get_len(cur_lst) > 0) {
      fil_addr_t loc = flst_get_first(cur_lst, &local_mtr);
      flst_node_t *node = addr2ptr_x(loc, &local_mtr);
      z_frag_entry_t entry(node, &local_mtr);
      page_no_t frag_page_no = entry.get_page_no();
      loc = entry.get_next();
      entry.remove(cur_lst);

      if (frag_page_no == FIL_NULL) {
        continue;
      }

      /* Multiple entries can point to the same fragment page.  So scan through
      the list and remove all entries pointing to the same fragment page. */
      while (!fil_addr_is_null(loc)) {
        node = addr2ptr_x(loc, &local_mtr);
        z_frag_entry_t entry2(node, &local_mtr);

        loc = entry2.get_next();
        if (frag_page_no == entry2.get_page_no()) {
          entry2.set_page_no(FIL_NULL);
          entry2.remove(cur_lst);
        }
      }

      /* Free the fragment page. */
      entry.free_frag_page(&local_mtr, m_index);
      n_pages_freed++;
      ut_ad(!local_mtr.conflicts_with(m_mtr));
      restart_mtr(&local_mtr);
    }
  }
  flst_init(frag_lst, &local_mtr);
  flst_init(free_frag_lst, &local_mtr);

  ut_ad(!local_mtr.conflicts_with(m_mtr));
  mtr_commit(&local_mtr);
  return (n_pages_freed);
}

size_t z_first_page_t::free_all_frag_pages() {
  size_t n_pages_freed = 0;
  if (get_frag_page_no() == 0) {
    n_pages_freed = free_all_frag_pages_old();
  } else {
    n_pages_freed = free_all_frag_pages_new();
  }
  return (n_pages_freed);
}

size_t z_first_page_t::free_all_frag_pages_new() {
  size_t n_pages_freed = 0;
  mtr_t local_mtr;
  mtr_start(&local_mtr);
  local_mtr.set_log_mode(m_mtr->get_log_mode());
  load_x(&local_mtr);

  while (true) {
    page_no_t page_no = get_frag_page_no(&local_mtr);
    if (page_no == FIL_NULL) {
      break;
    }
    z_frag_page_t frag_page(&local_mtr, m_index);
    frag_page.load_x(page_no);
    page_no_t next_page = frag_page.get_next_page_no();
    set_frag_page_no(&local_mtr, next_page);
    frag_page.dealloc();
    n_pages_freed++;
    ut_ad(!local_mtr.conflicts_with(m_mtr));
    restart_mtr(&local_mtr);
  }
  ut_ad(!local_mtr.conflicts_with(m_mtr));
  mtr_commit(&local_mtr);
  return (n_pages_freed);
}

size_t z_first_page_t::destroy() {
  size_t n_pages_freed = make_empty();
  dealloc();
  n_pages_freed++;
  return (n_pages_freed);
}

size_t z_first_page_t::make_empty() {
  size_t n_pages_freed = 0;
  n_pages_freed += free_all_data_pages();
  n_pages_freed += free_all_frag_pages();
  n_pages_freed += free_all_frag_node_pages();
  n_pages_freed += free_all_index_pages();
  return (n_pages_freed);
}

#ifdef UNIV_DEBUG
bool z_first_page_t::verify_frag_page_no() {
  mtr_t local_mtr;
  mtr_start(&local_mtr);
  page_no_t page_no = get_frag_page_no();

  /* If the page_no is 0, then FIL_PAGE_PREV is not used to store the list of
  fragment pages.  So modifying it is not allowed and hence verification is
  not needed. */
  ut_ad(page_no != 0);

  if (page_no == FIL_NULL) {
    return (true);
  }

  z_frag_page_t frag_page(&local_mtr, m_index);
  frag_page.load_x(page_no);
  page_type_t ptype = frag_page.get_page_type();
  mtr_commit(&local_mtr);

  ut_ad(ptype == FIL_PAGE_TYPE_ZLOB_FRAG);
  return (ptype == FIL_PAGE_TYPE_ZLOB_FRAG);
}
#endif /* UNIV_DEBUG */

} /* namespace lob */
