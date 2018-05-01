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

#include <list>
#include "btr0btr.h"
#include "buf0buf.h"
#include "dict0dict.h"
#include "lob0first.h"
#include "lob0index.h"
#include "lob0lob.h"
#include "table.h"
#include "trx0trx.h"

namespace lob {

/** Allocate one LOB page.
@param[in]	index	the index in which LOB exists.
@param[in]	lob_mtr	the mini-transaction context.
@param[in]	hint	the hint page number for allocation.
@param[in]	bulk	true if operation is OPCODE_INSERT_BULK,
                        false otherwise.
@return the allocated block of the BLOB page. */
buf_block_t *alloc_lob_page(dict_index_t *index, mtr_t *lob_mtr, page_no_t hint,
                            bool bulk) {
  ulint r_extents;
  mtr_t mtr_bulk;
  mtr_t *alloc_mtr;
  buf_block_t *block = nullptr;

  space_id_t space_id = dict_index_get_space(index);

  ut_ad(fsp_check_tablespace_size(space_id));

  if (bulk) {
    mtr_start(&mtr_bulk);
    alloc_mtr = &mtr_bulk;
  } else {
    alloc_mtr = lob_mtr;
  }

  if (!fsp_reserve_free_extents(&r_extents, space_id, 1, FSP_BLOB, alloc_mtr,
                                1)) {
    alloc_mtr->commit();
    return (nullptr);
  }

  block = btr_page_alloc(index, hint, FSP_NO_DIR, 0, alloc_mtr, lob_mtr);

  fil_space_release_free_extents(space_id, r_extents);

  if (bulk) {
    alloc_mtr->commit();
  }

  return (block);
}

dberr_t get_affected_index_entries(const ref_t &ref, dict_index_t *index,
                                   const Binary_diff &bdiff,
                                   List_iem_t &entries, mtr_t *mtr) {
  page_no_t first_page_no = ref.page_no();
  space_id_t space_id = ref.space_id();
  const page_size_t page_size = dict_table_page_size(index->table);
  const page_id_t first_page_id(space_id, first_page_no);
  size_t offset = bdiff.offset();
  ulint data_len = 0;

  if (!dict_table_has_atomic_blobs(index->table)) {
    /* For compact and redundant row format, remove the local
    prefix length from the offset. */

    ut_ad(offset >= DICT_ANTELOPE_MAX_INDEX_COL_LEN);
    offset -= DICT_ANTELOPE_MAX_INDEX_COL_LEN;
  }

  /* Currently only working with uncompressed LOB */
  ut_ad(!page_size.is_compressed());

  first_page_t first_page(mtr, index);
  first_page.load_x(first_page_id, page_size);

#ifdef UNIV_DEBUG
  {
    ulint page_type = first_page.get_page_type();
    ut_ad(page_type == FIL_PAGE_TYPE_LOB_FIRST);
  }
#endif /* UNIV_DEBUG */

  /* Obtain the first index entry. */
  flst_base_node_t *base_node = first_page.index_list();
  fil_addr_t node_loc = flst_get_first(base_node, mtr);

  buf_block_t *block = nullptr;
  index_entry_t entry(mtr, index);

  while (!fil_addr_is_null(node_loc)) {
    if (block == nullptr) {
      block = entry.load_x(node_loc);
    } else if (block->page.id.page_no() != node_loc.page) {
      block = entry.load_x(node_loc);
    } else {
      /* Next entry in the same page. */
      ut_ad(block == entry.get_block());
      entry.reset(node_loc);
    }

    /* Get the amount of data */
    data_len = entry.get_data_len();

    if (offset < data_len) {
      index_entry_mem_t entry_mem;
      entry.read(entry_mem);
      entries.push_back(entry_mem);

      size_t remain = data_len - offset;

      if (bdiff.length() > remain) {
        block = entry.next();

        if (block != nullptr) {
          entry.read(entry_mem);
          entries.push_back(entry_mem);
        }
      }

      break;
    }

    offset -= data_len;

    /* The next node should not be the same as the current node. */
    ut_ad(!node_loc.is_equal(entry.get_next()));

    node_loc = entry.get_next();
  }

  ut_ad(entries.size() == 1 || entries.size() == 2);

  return (DB_SUCCESS);
}

/** Get information about the given LOB.
@param[in]	ref	the LOB reference.
@param[in]	index	the clustered index to which LOB belongs.
@param[out]	lob_version	the lob version number.
@param[out]	last_trx_id	the trx_id that modified the lob last.
@param[out]	last_undo_no	the trx undo no that modified the lob last.
@param[out]	page_type	the page type of first lob page.
@param[in]	mtr		the mini transaction context.
@return always returns DB_SUCCESS. */
dberr_t get_info(ref_t &ref, dict_index_t *index, ulint &lob_version,
                 trx_id_t &last_trx_id, undo_no_t &last_undo_no,
                 ulint &page_type, mtr_t *mtr) {
  page_no_t first_page_no = ref.page_no();
  space_id_t space_id = ref.space_id();
  const page_size_t page_size = dict_table_page_size(index->table);
  const page_id_t first_page_id(space_id, first_page_no);

  /* Currently only working with uncompressed LOB */
  ut_ad(!page_size.is_compressed());

  first_page_t first_page(mtr, index);
  first_page.load_x(first_page_id, page_size);

  page_type = first_page.get_page_type();
  lob_version = first_page.get_lob_version();
  last_trx_id = first_page.get_last_trx_id();
  last_undo_no = first_page.get_last_trx_undo_no();

  return (DB_SUCCESS);
}

}; /* namespace lob */
