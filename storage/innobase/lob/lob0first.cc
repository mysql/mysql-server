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

#include "lob0first.h"
#include "lob0impl.h"
#include "lob0index.h"
#include "lob0pages.h"
#include "trx0trx.h"

namespace lob {

void first_page_t::replace_inline(trx_t *trx, ulint offset, const byte *&ptr,
                                  ulint &want, mtr_t *mtr) {
  byte *old_ptr = data_begin();
  old_ptr += offset;

  ulint data_avail = get_data_len() - offset;
  ulint data_to_copy = (want > data_avail) ? data_avail : want;
  mlog_write_string(old_ptr, ptr, data_to_copy, mtr);

  ptr += data_to_copy;
  want -= data_to_copy;
}

/** Replace data in the page by making a copy-on-write.
@param[in]	trx	the current transaction.
@param[in]	offset	the location where replace operation starts.
@param[in,out]	ptr	the buffer containing new data. after the
                        call it will point to remaining data.
@param[in,out]	want	requested amount of data to be replaced.
                        after the call it will contain amount of
                        data yet to be replaced.
@param[in]	mtr	the mini-transaction context.
@return	the newly allocated buffer block.
@return	nullptr if new page could not be allocated (DB_OUT_OF_FILE_SPACE). */
buf_block_t *first_page_t::replace(trx_t *trx, ulint offset, const byte *&ptr,
                                   ulint &want, mtr_t *mtr) {
  DBUG_ENTER("first_page_t::replace");

  buf_block_t *new_block = nullptr;

  /** Allocate a new data page. */
  data_page_t new_page(mtr, m_index);
  new_block = new_page.alloc(mtr, false);

  DBUG_EXECUTE_IF("innodb_lob_first_page_replace_failed", new_block = nullptr;);

  if (new_block == nullptr) {
    DBUG_RETURN(nullptr);
  }

  byte *new_ptr = new_page.data_begin();
  byte *old_ptr = data_begin();

  DBUG_LOG("first_page_t", PrintBuffer(old_ptr, get_data_len()));
  DBUG_LOG("first_page_t", PrintBuffer(ptr, want));

  new_page.set_trx_id(trx->id);
  new_page.set_data_len(get_data_len());

  /** Copy contents from old page to new page. */
  mlog_write_string(new_ptr, old_ptr, offset, mtr);

  new_ptr += offset;
  old_ptr += offset;

  /** Copy the new data to new page. */
  ulint data_avail = get_data_len() - offset;
  ulint data_to_copy = (want > data_avail) ? data_avail : want;
  mlog_write_string(new_ptr, ptr, data_to_copy, mtr);

  new_ptr += data_to_copy;
  old_ptr += data_to_copy;
  ptr += data_to_copy;

#ifdef UNIV_DEBUG
  /* The old data that is being replaced. */
  DBUG_LOG("lob::first_page_t",
           "old data=" << PrintBuffer(old_ptr - data_to_copy, data_to_copy));
  /* The new data that replaces old data. */
  DBUG_LOG("lob::first_page_t",
           "new data=" << PrintBuffer(new_ptr - data_to_copy, data_to_copy));
#endif /* UNIV_DEBUG */

  /** Copy contents from old page to new page. */
  if (want < data_avail) {
    ut_ad(data_to_copy == want);
    ulint remain = data_avail - want;
    mlog_write_string(new_ptr, old_ptr, remain, mtr);
  }

  DBUG_LOG("first_page_t",
           PrintBuffer(new_page.data_begin(), new_page.get_data_len()));

  want -= data_to_copy;

  DBUG_RETURN(new_block);
}

std::ostream &first_page_t::print_index_entries_cache_s(
    std::ostream &out, BlockCache &cache) const {
  if (m_block == nullptr) {
    return (out);
  }

  flst_base_node_t *base = index_list();
  fil_addr_t node_loc = flst_get_first(base, m_mtr);
  ulint n_entries = flst_get_len(base);

  out << "[n_entries=" << n_entries << ", " << std::endl;
  while (!fil_addr_is_null(node_loc)) {
    flst_node_t *node = addr2ptr_s_cache(cache, node_loc);
    index_entry_t entry(node, m_mtr, m_index);

    flst_base_node_t *vers = entry.get_versions_list();
    fil_addr_t ver_loc = flst_get_first(vers, m_mtr);

    out << entry << std::endl;

    uint32_t depth = 0;
    while (!fil_addr_is_null(ver_loc)) {
      depth++;

      for (uint32_t i = 0; i < depth; ++i) {
        out << "+";
      }
      flst_node_t *ver_node = addr2ptr_s_cache(cache, ver_loc);
      index_entry_t vers_entry(ver_node, m_mtr, m_index);
      out << vers_entry << std::endl;
      ver_loc = vers_entry.get_next();
    }

    node_loc = entry.get_next();
  }

  out << "]" << std::endl;
  return (out);
}

std::ostream &first_page_t::print_index_entries(std::ostream &out) const {
  space_id_t space = dict_index_get_space(m_index);
  const page_size_t page_size = dict_table_page_size(m_index->table);

  if (m_block == nullptr) {
    return (out);
  }

  flst_base_node_t *base = index_list();
  fil_addr_t node_loc = flst_get_first(base, m_mtr);
  ulint n_entries = flst_get_len(base);

  out << "[n_entries=" << n_entries << ", " << std::endl;
  while (!fil_addr_is_null(node_loc)) {
    flst_node_t *node =
        fut_get_ptr(space, page_size, node_loc, RW_X_LATCH, m_mtr);
    index_entry_t entry(node, m_mtr, m_index);

    flst_base_node_t *vers = entry.get_versions_list();
    fil_addr_t ver_loc = flst_get_first(vers, m_mtr);

    out << entry << std::endl;

    uint32_t depth = 0;
    while (!fil_addr_is_null(ver_loc)) {
      depth++;

      for (uint32_t i = 0; i < depth; ++i) {
        out << "+";
      }
      flst_node_t *ver_node = addr2ptr_x(ver_loc);
      index_entry_t vers_entry(ver_node, m_mtr, m_index);
      out << vers_entry << std::endl;
      ver_loc = vers_entry.get_next();
    }

    node_loc = entry.get_next();
  }

  out << "]" << std::endl;
  return (out);
}

#ifdef UNIV_DEBUG
bool first_page_t::validate() const {
  flst_base_node_t *idx_list = index_list();
  ut_ad(flst_validate(idx_list, m_mtr));
  return (true);
}
#endif /* UNIV_DEBUG */

/** Allocate the first page for uncompressed LOB.
@param[in,out]	alloc_mtr	the allocation mtr.
@param[in]	is_bulk		true if it is bulk operation.
                                (OPCODE_INSERT_BULK)
return the allocated buffer block.*/
buf_block_t *first_page_t::alloc(mtr_t *alloc_mtr, bool is_bulk) {
  ut_ad(m_index != nullptr);
  ut_ad(m_block == nullptr);

  page_no_t hint = FIL_NULL;
  m_block = alloc_lob_page(m_index, alloc_mtr, hint, is_bulk);

  if (m_block == nullptr) {
    return (nullptr);
  }

  ut_ad(rw_lock_get_x_lock_count(&m_block->lock) == 1);

  /* After allocation, first set the page type. */
  set_page_type();

  set_version_0();
  set_data_len(0);
  set_trx_id(0);
  byte *free_lst = free_list();
  byte *index_lst = index_list();
  flst_init(index_lst, m_mtr);
  flst_init(free_lst, m_mtr);

  ulint nc = node_count();

  byte *cur = nodes_begin();
  for (ulint i = 0; i < nc; ++i) {
    index_entry_t entry(cur, m_mtr, m_index);
    entry.init();
    flst_add_last(free_lst, cur, m_mtr);
    cur += index_entry_t::SIZE;
  }
  ut_ad(flst_validate(free_lst, m_mtr));
  set_next_page_null();
  ut_ad(get_page_type() == FIL_PAGE_TYPE_LOB_FIRST);
  return (m_block);
}

/** Allocate one index entry.  If required an index page (of type
FIL_PAGE_TYPE_LOB_INDEX) will be allocated.
@param[in]	bulk	true if it is a bulk operation
                        (OPCODE_INSERT_BULK), false otherwise.
@return the file list node of the index entry. */
flst_node_t *first_page_t::alloc_index_entry(bool bulk) {
  flst_base_node_t *f_list = free_list();
  fil_addr_t node_addr = flst_get_first(f_list, m_mtr);
  if (fil_addr_is_null(node_addr)) {
    node_page_t node_page(m_mtr, m_index);
    buf_block_t *block = node_page.alloc(*this, bulk);

    if (block == nullptr) {
      return (nullptr);
    }

    node_addr = flst_get_first(f_list, m_mtr);
  }
  flst_node_t *node = addr2ptr_x(node_addr);
  flst_remove(f_list, node, m_mtr);
  return (node);
}

void first_page_t::free_all_index_pages() {
  space_id_t space_id = dict_index_get_space(m_index);
  page_size_t page_size(dict_table_page_size(m_index->table));

  while (true) {
    page_no_t page_no = get_next_page();

    if (page_no == FIL_NULL) {
      break;
    }

    node_page_t index_page(m_mtr, m_index);
    page_id_t page_id(space_id, page_no);
    index_page.load_x(page_id, page_size);
    page_no_t next_page = index_page.get_next_page();
    set_next_page(next_page);
    index_page.dealloc();
  }
}

/** Load the first page of LOB with x-latch.
@param[in]   page_id    the page identifier of the first page.
@param[in]   page_size  the page size information.
@return the buffer block of the first page. */
buf_block_t *first_page_t::load_x(const page_id_t &page_id,
                                  const page_size_t &page_size) {
  m_block = buf_page_get(page_id, page_size, RW_X_LATCH, m_mtr);

  ut_ad(m_block != nullptr);
#ifdef UNIV_DEBUG
  /* Dump the page into the log file, if the page type is not
  matching one of the first page types. */
  page_type_t page_type = get_page_type();
  switch (page_type) {
    case FIL_PAGE_TYPE_BLOB:
    case FIL_PAGE_TYPE_ZBLOB:
    case FIL_PAGE_TYPE_LOB_FIRST:
    case FIL_PAGE_TYPE_ZLOB_FIRST:
    case FIL_PAGE_SDI_ZBLOB:
    case FIL_PAGE_SDI_BLOB:
      /* Valid first page type.*/
      break;
    default:
      std::cerr << "Unexpected LOB first page type=" << page_type << std::endl;
      ut_print_buf(std::cerr, m_block->frame, page_size.physical());
      ut_error;
  }
#endif /* UNIV_DEBUG */
  return (m_block);
}

/** Increment the lob version number by 1. */
uint32_t first_page_t::incr_lob_version() {
  ut_ad(m_mtr != nullptr);

  const uint32_t cur = get_lob_version();
  const uint32_t val = cur + 1;
  mlog_write_ulint(frame() + OFFSET_LOB_VERSION, val, MLOG_4BYTES, m_mtr);
  return (val);
}

/** When the bit is set, the LOB is not partially updatable anymore.
Enable the bit. */
void first_page_t::mark_cannot_be_partially_updated(trx_t *trx) {
  const trx_id_t trxid = (trx == nullptr) ? 0 : trx->id;
  const undo_no_t undo_no = (trx == nullptr) ? 0 : (trx->undo_no - 1);

  uint8_t flags = get_flags();
  flags |= 0x01;
  mlog_write_ulint(frame() + OFFSET_FLAGS, flags, MLOG_1BYTE, m_mtr);

  set_last_trx_id(trxid);
  set_last_trx_undo_no(undo_no);
}

/** Read data from the first page.
@param[in]	offset	the offset from where read starts.
@param[out]	ptr	the output buffer
@param[in]	want	number of bytes to read.
@return number of bytes read. */
ulint first_page_t::read(ulint offset, byte *ptr, ulint want) {
  byte *start = data_begin();
  start += offset;
  ulint avail_data = get_data_len() - offset;

  ulint copy_len = (want < avail_data) ? want : avail_data;
  memcpy(ptr, start, copy_len);
  return (copy_len);
}

/** Write as much as possible of the given data into the page.
@param[in]	trxid	the current transaction.
@param[in]	data	the data to be written.
@param[in]	len	the length of the given data.
@return number of bytes actually written. */
ulint first_page_t::write(trx_id_t trxid, const byte *&data, ulint &len) {
  byte *ptr = data_begin();
  ulint written = (len > max_space_available()) ? max_space_available() : len;

  /* Write the data into the page. */
  mlog_write_string(ptr, data, written, m_mtr);

  set_data_len(written);
  set_trx_id(trxid);

  data += written;
  len -= written;

  return (written);
}

void first_page_t::import(trx_id_t trx_id) {
  set_trx_id_no_redo(trx_id);
  set_last_trx_id_no_redo(trx_id);

  byte *cur = nodes_begin();
  ulint nc = node_count();

  for (ulint i = 0; i < nc; ++i) {
    index_entry_t entry(cur, m_mtr, m_index);
    entry.set_trx_id_no_redo(trx_id);
    entry.set_trx_id_modifier_no_redo(trx_id);

    cur += index_entry_t::SIZE;
  }
}

void first_page_t::dealloc() {
  ut_ad(m_mtr != nullptr);
  ut_ad(get_next_page() == FIL_NULL);

  btr_page_free_low(m_index, m_block, ULINT_UNDEFINED, m_mtr);
  m_block = nullptr;
}

}  // namespace lob
