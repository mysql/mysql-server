/*****************************************************************************

Copyright (c) 2014, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file btr/btr0bulk.cc
 The B-tree bulk load

 Created 03/11/2014 Shaohua Wang
 *******************************************************/

#include "btr0bulk.h"
#include "btr0btr.h"
#include "btr0cur.h"
#include "btr0pcur.h"
#include "ibuf0ibuf.h"
#include "lob0lob.h"

/** Innodb B-tree index fill factor for bulk load. */
long innobase_fill_factor;

/** Initialize members, allocate page if needed and start mtr.
Note: we commit all mtrs on failure.
@return error code. */
dberr_t PageBulk::init() {
  mtr_t *mtr;
  buf_block_t *new_block;
  page_t *new_page;
  page_zip_des_t *new_page_zip;
  page_no_t new_page_no;

  ut_ad(m_heap == nullptr);

  m_heap = mem_heap_create(1000);

  mtr = static_cast<mtr_t *>(mem_heap_alloc(m_heap, sizeof(mtr_t)));
  mtr_start(mtr);
  mtr_x_lock(dict_index_get_lock(m_index), mtr);
  mtr_set_log_mode(mtr, MTR_LOG_NO_REDO);
  mtr_set_flush_observer(mtr, m_flush_observer);

  if (m_page_no == FIL_NULL) {
    mtr_t alloc_mtr;

    /* We commit redo log for allocation by a separate mtr,
    because we don't guarantee pages are committed following
    the allocation order, and we will always generate redo log
    for page allocation, even when creating a new tablespace. */
    mtr_start(&alloc_mtr);

    ulint n_reserved;
    bool success = fsp_reserve_free_extents(&n_reserved, m_index->space, 1,
                                            FSP_NORMAL, &alloc_mtr);
    if (!success) {
      mtr_commit(&alloc_mtr);
      mtr_commit(mtr);
      return (DB_OUT_OF_FILE_SPACE);
    }

    /* Allocate a new page. */
    new_block = btr_page_alloc(m_index, 0, FSP_UP, m_level, &alloc_mtr, mtr);

    if (n_reserved > 0) {
      fil_space_release_free_extents(m_index->space, n_reserved);
    }

    mtr_commit(&alloc_mtr);

    new_page = buf_block_get_frame(new_block);
    new_page_zip = buf_block_get_page_zip(new_block);
    new_page_no = page_get_page_no(new_page);

    ut_ad(!dict_index_is_spatial(m_index));
    ut_ad(!dict_index_is_sdi(m_index));

    if (new_page_zip) {
      page_create_zip(new_block, m_index, m_level, 0, mtr, FIL_PAGE_INDEX);
    } else {
      ut_ad(!dict_index_is_spatial(m_index));
      page_create(new_block, mtr, dict_table_is_comp(m_index->table),
                  FIL_PAGE_INDEX);
      btr_page_set_level(new_page, nullptr, m_level, mtr);
    }

    btr_page_set_next(new_page, nullptr, FIL_NULL, mtr);
    btr_page_set_prev(new_page, nullptr, FIL_NULL, mtr);

    btr_page_set_index_id(new_page, nullptr, m_index->id, mtr);
  } else {
    page_id_t page_id(dict_index_get_space(m_index), m_page_no);
    page_size_t page_size(dict_table_page_size(m_index->table));

    new_block = btr_block_get(page_id, page_size, RW_X_LATCH, m_index, mtr);

    new_page = buf_block_get_frame(new_block);
    new_page_zip = buf_block_get_page_zip(new_block);
    new_page_no = page_get_page_no(new_page);
    ut_ad(m_page_no == new_page_no);

    ut_ad(page_dir_get_n_heap(new_page) == PAGE_HEAP_NO_USER_LOW);

    btr_page_set_level(new_page, nullptr, m_level, mtr);
  }

  if (dict_index_is_sec_or_ibuf(m_index) && !m_index->table->is_temporary() &&
      page_is_leaf(new_page)) {
    page_update_max_trx_id(new_block, nullptr, m_trx_id, mtr);
  }

  m_mtr = mtr;
  m_block = new_block;
  m_page = new_page;
  m_page_zip = new_page_zip;
  m_page_no = new_page_no;
  m_cur_rec = page_get_infimum_rec(new_page);
  ut_ad(m_is_comp == !!page_is_comp(new_page));
  m_free_space = page_get_free_space_of_empty(m_is_comp);

  if (innobase_fill_factor == 100 && m_index->is_clustered()) {
    /* Keep default behavior compatible with 5.6 */
    m_reserved_space = dict_index_get_space_reserve();
  } else {
    m_reserved_space = UNIV_PAGE_SIZE * (100 - innobase_fill_factor) / 100;
  }

  m_padding_space =
      UNIV_PAGE_SIZE - dict_index_zip_pad_optimal_page_size(m_index);
  m_heap_top = page_header_get_ptr(new_page, PAGE_HEAP_TOP);
  m_rec_no = page_header_get_field(new_page, PAGE_N_RECS);

  m_last_slotted_rec = page_get_infimum_rec(m_page);
  m_slotted_rec_no = 0;

  m_modified = true;

  ut_d(m_total_data = 0);

  return (DB_SUCCESS);
}

/** Insert a tuple in the page.
@param[in]  tuple     tuple to insert
@param[in]  big_rec   external record
@param[in]  rec_size  record size
@param[in]  n_ext     number of externally stored columns
@return error code */
dberr_t PageBulk::insert(const dtuple_t *tuple, const big_rec_t *big_rec,
                         ulint rec_size, ulint n_ext) {
  ulint *offsets = nullptr;

  DBUG_EXECUTE_IF("BtrBulk_insert_inject_error", return DB_INTERRUPTED;);

  /* Convert tuple to record. */
  byte *rec_mem = static_cast<byte *>(mem_heap_alloc(m_heap, rec_size));

  rec_t *rec = rec_convert_dtuple_to_rec(rec_mem, m_index, tuple, n_ext);
  offsets = rec_get_offsets(rec, m_index, offsets, ULINT_UNDEFINED, &m_heap);

  /* Insert the record.*/
  insert(rec, offsets);
  ut_ad(m_modified);

  dberr_t err = DB_SUCCESS;

  if (big_rec) {
    /* The page must be valid as MTR may be committed
    during LOB insertion. */
    finish();
    err = storeExt(big_rec, offsets);
  }

  return err;
}

/** Insert a record in the page.
@param[in]	rec		record
@param[in]	offsets		record offsets */
void PageBulk::insert(const rec_t *rec, ulint *offsets) {
  ulint rec_size;

  ut_ad(m_heap != nullptr);

  rec_size = rec_offs_size(offsets);

#ifdef UNIV_DEBUG
  /* Check whether records are in order. */
  if (!page_rec_is_infimum(m_cur_rec)) {
    rec_t *old_rec = m_cur_rec;
    ulint *old_offsets =
        rec_get_offsets(old_rec, m_index, nullptr, ULINT_UNDEFINED, &m_heap);

    ut_ad(cmp_rec_rec(rec, old_rec, offsets, old_offsets, m_index) > 0);
  }

  m_total_data += rec_size;
#endif /* UNIV_DEBUG */

  /* 0. Mark space for record as used (checked e.g. in page_rec_set_next). */
  page_header_set_ptr(m_page, nullptr, PAGE_HEAP_TOP, m_heap_top + rec_size);

  /* 1. Copy the record to page. */
  rec_t *insert_rec = rec_copy(m_heap_top, rec, offsets);
  rec_offs_make_valid(insert_rec, m_index, offsets);

  /* 2. Insert the record in the linked list. */
  rec_t *next_rec = page_rec_get_next(m_cur_rec);

  page_rec_set_next(insert_rec, next_rec);
  page_rec_set_next(m_cur_rec, insert_rec);

  /* 3. Set the n_owned field in the inserted record to zero,
  and set the heap_no field. */
  if (m_is_comp) {
    rec_set_n_owned_new(insert_rec, nullptr, 0);
    rec_set_heap_no_new(insert_rec, PAGE_HEAP_NO_USER_LOW + m_rec_no);
  } else {
    rec_set_n_owned_old(insert_rec, 0);
    rec_set_heap_no_old(insert_rec, PAGE_HEAP_NO_USER_LOW + m_rec_no);
  }

  /* 4. Set member variables. */
  ulint slot_size = page_dir_calc_reserved_space(m_rec_no + 1) -
                    page_dir_calc_reserved_space(m_rec_no);

  ut_ad(m_free_space >= rec_size + slot_size);
  ut_ad(m_heap_top + rec_size < m_page + UNIV_PAGE_SIZE);

  m_free_space -= rec_size + slot_size;
  m_heap_top += rec_size;
  m_rec_no += 1;
  m_cur_rec = insert_rec;

  m_modified = true;
}

/** Mark end of insertion to the page. Scan records to set page dirs,
and set page header members. The scan is incremental (slots and records
which assignment could be "finalized" are not checked again. Check the
m_slotted_rec_no usage, note it could be reset in some cases like
during split.
Note: we refer to page_copy_rec_list_end_to_created_page. */
void PageBulk::finish() {
  ut_ad(!dict_index_is_spatial(m_index));

  if (!m_modified) {
    return;
  }

  ut_ad(m_total_data + page_dir_calc_reserved_space(m_rec_no) <=
        page_get_free_space_of_empty(m_is_comp));

  ulint n_rec_to_assign = m_rec_no - m_slotted_rec_no;

  /* Fill slots for non-supremum records if possible.
   * Slot for supremum record could store up to
   * PAGE_DIR_SLOT_MAX_N_OWNED-1 records. */
  while (n_rec_to_assign >= PAGE_DIR_SLOT_MAX_N_OWNED) {
    static constexpr ulint RECORDS_PER_SLOT =
        (PAGE_DIR_SLOT_MAX_N_OWNED + 1) / 2;

    for (ulint i = 0; i < RECORDS_PER_SLOT; ++i) {
      m_last_slotted_rec = page_rec_get_next(m_last_slotted_rec);
    }
    m_slotted_rec_no += RECORDS_PER_SLOT;

    /* Reserve next slot (must be done before slot is used). */
    auto n_slots = page_dir_get_n_slots(m_page);
    page_dir_set_n_slots(m_page, nullptr, n_slots + 1);

    /* Fill the slot data. */
    auto slot = page_dir_get_nth_slot(m_page, n_slots - 1);
    page_dir_slot_set_rec(slot, m_last_slotted_rec);
    page_dir_slot_set_n_owned(slot, nullptr, RECORDS_PER_SLOT);

    n_rec_to_assign -= RECORDS_PER_SLOT;
  }

  /* Assign remaining records to slot with supremum record. */
  auto n_slots = page_dir_get_n_slots(m_page);
  auto slot = page_dir_get_nth_slot(m_page, n_slots - 1);
  auto sup_rec = page_get_supremum_rec(m_page);

  page_dir_slot_set_rec(slot, sup_rec);
  page_dir_slot_set_n_owned(slot, nullptr, n_rec_to_assign + 1);

  page_header_set_ptr(m_page, nullptr, PAGE_HEAP_TOP, m_heap_top);
  page_dir_set_n_heap(m_page, nullptr, PAGE_HEAP_NO_USER_LOW + m_rec_no);
  page_header_set_field(m_page, nullptr, PAGE_N_RECS, m_rec_no);

  page_header_set_ptr(m_page, nullptr, PAGE_LAST_INSERT, m_cur_rec);
  page_header_set_field(m_page, nullptr, PAGE_DIRECTION, PAGE_RIGHT);
  page_header_set_field(m_page, nullptr, PAGE_N_DIRECTION, 0);

  m_modified = false;
}

/** Commit inserts done to the page
@param[in]	success		Flag whether all inserts succeed. */
void PageBulk::commit(bool success) {
  /* It is assumed that finish() was called before commit */
  ut_ad(!m_modified);
  ut_ad(page_validate(m_page, m_index));

  if (success) {
    ut_ad(m_rec_no > 0);

    /* Set no free space left and no buffered changes in ibuf. */
    if (!m_index->is_clustered() && !m_index->table->is_temporary() &&
        page_is_leaf(m_page)) {
      ibuf_set_bitmap_for_bulk_load(m_block, innobase_fill_factor == 100);
    }
  }

  mtr_commit(m_mtr);
}

/** Compress a page of compressed table
@return	true	compress successfully or no need to compress
@return	false	compress failed. */
bool PageBulk::compress() {
  ut_ad(!m_modified);
  ut_ad(m_page_zip != nullptr);

  return (
      page_zip_compress(m_page_zip, m_page, m_index, page_zip_level, m_mtr));
}

/** Get node pointer
@return node pointer */
dtuple_t *PageBulk::getNodePtr() {
  /* Create node pointer */
  rec_t *first_rec = page_rec_get_next(page_get_infimum_rec(m_page));
  ut_a(page_rec_is_user_rec(first_rec));
  dtuple_t *node_ptr =
      dict_index_build_node_ptr(m_index, first_rec, m_page_no, m_heap, m_level);

  return (node_ptr);
}

/** Split the page records between this and given bulk.
 * @param new_page_bulk  The new bulk to store split records. */
void PageBulk::split(PageBulk &new_page_bulk) {
  auto split_point = getSplitRec();

  new_page_bulk.copyRecords(split_point.m_rec);
  splitTrim(split_point);

  ut_ad(new_page_bulk.m_modified);
  ut_ad(m_modified);
}

/** Get page split point. We split a page in half when compression
fails, and the split record and all following records should be copied
to the new page.
@return split record descriptor */
PageBulk::SplitPoint PageBulk::getSplitRec() {
  ut_ad(m_page_zip != nullptr);
  ut_ad(m_rec_no >= 2);

  ut_ad(page_get_free_space_of_empty(m_is_comp) > m_free_space);

  ulint total_used_size =
      page_get_free_space_of_empty(m_is_comp) - m_free_space;

  ulint total_recs_size = 0;
  ulint n_recs = 0;
  ulint *offsets = nullptr;

  rec_t *rec = page_get_infimum_rec(m_page);
  do {
    rec = page_rec_get_next(rec);
    ut_ad(page_rec_is_user_rec(rec));

    offsets =
        rec_get_offsets(rec, m_index, offsets, ULINT_UNDEFINED, &(m_heap));
    total_recs_size += rec_offs_size(offsets);
    n_recs++;
  } while (total_recs_size + page_dir_calc_reserved_space(n_recs) <
           total_used_size / 2);

  /* Keep at least one record on left page */
  if (page_rec_is_infimum(page_rec_get_prev(rec))) {
    rec = page_rec_get_next(rec);
    ut_ad(page_rec_is_user_rec(rec));
  } else {
    /* rec is to be moved, and this is used as number of records
     * before split */
    n_recs--;
  }

  return (SplitPoint{rec, n_recs});
}

/** Copy all records from page.
@param[in]  src_page  Page with records to copy. */
void PageBulk::copyAll(const page_t *src_page) {
  auto inf_rec = page_get_infimum_rec(src_page);
  auto first_rec = page_rec_get_next_const(inf_rec);

  ut_ad(page_rec_is_user_rec(first_rec));

  copyRecords(first_rec);

  ut_ad(m_modified);
}

/** Copy given and all following records.
@param[in]  first_rec  first record to copy */
void PageBulk::copyRecords(const rec_t *first_rec) {
  const rec_t *rec = first_rec;
  ulint *offsets = nullptr;

  ut_ad(m_rec_no == 0);
  ut_ad(page_rec_is_user_rec(rec));

  do {
    offsets =
        rec_get_offsets(rec, m_index, offsets, ULINT_UNDEFINED, &(m_heap));

    insert(rec, offsets);

    rec = page_rec_get_next_const(rec);
  } while (!page_rec_is_supremum(rec));

  ut_ad(m_rec_no > 0);
}

/** Remove all records after split rec including itself.
@param[in]  split_point  split point descriptor */
void PageBulk::splitTrim(const SplitPoint &split_point) {
  /* Suppose before copyOut, we have 5 records on the page:
  infimum->r1->r2->r3->r4->r5->supremum, and r3 is the split rec.

  after copyOut, we have 2 records on the page:
  infimum->r1->r2->supremum. slot adjustment is not done. */

  /* Set number of user records. */
  ulint new_rec_no = split_point.m_n_rec_before;
  ut_ad(new_rec_no > 0);

  /* Set last record's next in page */
  rec_t *new_last_user_rec = page_rec_get_prev(split_point.m_rec);
  page_rec_set_next(new_last_user_rec, page_get_supremum_rec(m_page));

  /* Set related members */
  auto old_heap_top = m_heap_top;

  ulint *offsets = nullptr;
  offsets = rec_get_offsets(new_last_user_rec, m_index, offsets,
                            ULINT_UNDEFINED, &(m_heap));
  m_heap_top = rec_get_end(new_last_user_rec, offsets);

  m_free_space +=
      (old_heap_top - m_heap_top) + (page_dir_calc_reserved_space(m_rec_no) -
                                     page_dir_calc_reserved_space(new_rec_no));
  ut_ad(m_free_space > 0);

  m_cur_rec = new_last_user_rec;
  m_rec_no = new_rec_no;

#ifdef UNIV_DEBUG
  m_total_data -= old_heap_top - m_heap_top;
#endif /* UNIV_DEBUG */

  /* Invalidate all slots except infimum. */
  ulint n_slots = page_dir_get_n_slots(m_page);
  for (ulint slot_idx = 1; slot_idx < n_slots; ++slot_idx) {
    auto slot = page_dir_get_nth_slot(m_page, slot_idx);
    page_dir_slot_set_n_owned(slot, nullptr, 0);
  }
  page_dir_set_n_slots(m_page, nullptr, 2);

  /* No records assigned to slots. */
  m_last_slotted_rec = page_get_infimum_rec(m_page);
  m_slotted_rec_no = 0;

  m_modified = true;
}

/** Set next page
@param[in]	next_page_no	next page no */
void PageBulk::setNext(page_no_t next_page_no) {
  btr_page_set_next(m_page, nullptr, next_page_no, m_mtr);
}

/** Set previous page
@param[in]	prev_page_no	previous page no */
void PageBulk::setPrev(page_no_t prev_page_no) {
  btr_page_set_prev(m_page, nullptr, prev_page_no, m_mtr);
}

/** Check if required space is available in the page for the rec to be inserted.
We check fill factor & padding here.
@param[in]	rec_size	required length
@return true	if space is available */
bool PageBulk::isSpaceAvailable(ulint rec_size) const {
  ulint slot_size = page_dir_calc_reserved_space(m_rec_no + 1) -
                    page_dir_calc_reserved_space(m_rec_no);

  ulint required_space = rec_size + slot_size;

  if (required_space > m_free_space) {
    ut_ad(m_rec_no > 0);
    return (false);
  }

  /* Fillfactor & Padding apply to both leaf and non-leaf pages.
  Note: we keep at least 2 records in a page to avoid B-tree level
  growing too high. */
  if (m_rec_no >= 2 && ((m_page_zip == nullptr &&
                         m_free_space - required_space < m_reserved_space) ||
                        (m_page_zip != nullptr &&
                         m_free_space - required_space < m_padding_space))) {
    return (false);
  }

  return (true);
}

/** Check whether the record needs to be stored externally.
@return false if the entire record can be stored locally on the page */
bool PageBulk::needExt(const dtuple_t *tuple, ulint rec_size) const {
  return (page_zip_rec_needs_ext(
      rec_size, m_is_comp, dtuple_get_n_fields(tuple), m_block->page.size));
}

/** Store external record
Since the record is not logged yet, so we don't log update to the record.
the blob data is logged first, then the record is logged in bulk mode.
@param[in]	big_rec		external record
@param[in]	offsets		record offsets
@return	error code */
dberr_t PageBulk::storeExt(const big_rec_t *big_rec, ulint *offsets) {
  ut_ad(m_index->is_clustered());

  /* Note: not all fields are initialized in btr_pcur. */
  btr_pcur_t btr_pcur;
  btr_pcur.m_pos_state = BTR_PCUR_IS_POSITIONED;
  btr_pcur.m_latch_mode = BTR_MODIFY_LEAF;
  btr_pcur.m_btr_cur.index = m_index;

  page_cur_t *page_cur = &btr_pcur.m_btr_cur.page_cur;
  page_cur->index = m_index;
  page_cur->rec = m_cur_rec;
  page_cur->offsets = offsets;
  page_cur->block = m_block;

  dberr_t err = lob::btr_store_big_rec_extern_fields(
      nullptr, &btr_pcur, nullptr, offsets, big_rec, m_mtr,
      lob::OPCODE_INSERT_BULK);

  ut_ad(page_offset(m_cur_rec) == page_offset(page_cur->rec));

  /* Reset m_block and m_cur_rec from page cursor, because
  block may be changed during blob insert. */
  m_block = page_cur->block;
  m_cur_rec = page_cur->rec;
  m_page = buf_block_get_frame(m_block);

  return (err);
}

/** Release block by committing mtr
Note: log_free_check requires holding no lock/latch in current thread. */
void PageBulk::release() {
  /* Make sure page is valid before it is released. */
  if (m_modified) {
    finish();
    ut_ad(!m_modified);
  }
  ut_ad(page_validate(m_page, m_index));

  ut_ad(!dict_index_is_spatial(m_index));

  /* We fix the block because we will re-pin it soon. */
  buf_block_buf_fix_inc(m_block, __FILE__, __LINE__);

  /* No other threads can modify this block. */
  m_modify_clock = buf_block_get_modify_clock(m_block);

  mtr_commit(m_mtr);
}

/** Start mtr and latch the block */
void PageBulk::latch() {
  mtr_start(m_mtr);
  mtr_x_lock(dict_index_get_lock(m_index), m_mtr);
  mtr_set_log_mode(m_mtr, MTR_LOG_NO_REDO);
  mtr_set_flush_observer(m_mtr, m_flush_observer);

  /* TODO: need a simple and wait version of buf_page_optimistic_get. */
  auto ret =
      buf_page_optimistic_get(RW_X_LATCH, m_block, m_modify_clock,
                              Page_fetch::NORMAL, __FILE__, __LINE__, m_mtr);
  /* In case the block is S-latched by page_cleaner. */
  if (!ret) {
    page_id_t page_id(dict_index_get_space(m_index), m_page_no);
    page_size_t page_size(dict_table_page_size(m_index->table));

    m_block =
        buf_page_get_gen(page_id, page_size, RW_X_LATCH, m_block,
                         Page_fetch::IF_IN_POOL, __FILE__, __LINE__, m_mtr);
    ut_ad(m_block != nullptr);
  }

  buf_block_buf_fix_dec(m_block);

  ut_ad(m_cur_rec > m_page && m_cur_rec < m_heap_top);
}

/** Split a page
@param[in]	page_bulk	page to split
@param[in]	next_page_bulk	next page
@return	error code */
dberr_t BtrBulk::pageSplit(PageBulk *page_bulk, PageBulk *next_page_bulk) {
  ut_ad(page_bulk->isTableCompressed());

  /* 1. Check if we have only one user record on the page. */
  if (page_bulk->getRecNo() <= 1) {
    return (DB_TOO_BIG_RECORD);
  }

  /* 2. create a new page. */
  PageBulk new_page_bulk(m_index, m_trx_id, FIL_NULL, page_bulk->getLevel(),
                         m_flush_observer);
  dberr_t err = new_page_bulk.init();
  if (err != DB_SUCCESS) {
    return (err);
  }

  /* 3. copy the upper half to new page. */
  page_bulk->split(new_page_bulk);

  /* 4. finish page bulk modifications. */
  page_bulk->finish();
  new_page_bulk.finish();

  /* 5. commit the split page. */
  err = pageCommit(page_bulk, &new_page_bulk, true);
  if (err != DB_SUCCESS) {
    pageAbort(&new_page_bulk);
    return (err);
  }

  /* 6. commit the new page. */
  err = pageCommit(&new_page_bulk, next_page_bulk, true);
  if (err != DB_SUCCESS) {
    pageAbort(&new_page_bulk);
    return (err);
  }

  return (err);
}

/** Commit(finish) a page. We set next/prev page no, compress a page of
compressed table and split the page if compression fails, insert a node
pointer to father page if needed, and commit mini-transaction.
@param[in]	page_bulk	page to commit
@param[in]	next_page_bulk	next page
@param[in]	insert_father	false when page_bulk is a root page and
                                true when it's a non-root page
@return	error code */
dberr_t BtrBulk::pageCommit(PageBulk *page_bulk, PageBulk *next_page_bulk,
                            bool insert_father) {
  /* Set page links */
  if (next_page_bulk != nullptr) {
    ut_ad(page_bulk->getLevel() == next_page_bulk->getLevel());

    page_bulk->setNext(next_page_bulk->getPageNo());
    next_page_bulk->setPrev(page_bulk->getPageNo());
  } else {
    /** Suppose a page is released and latched again, we need to
    mark it modified in mini-transaction.  */
    page_bulk->setNext(FIL_NULL);
  }

  /* Compress page if it's a compressed table. */
  if (page_bulk->isTableCompressed() && !page_bulk->compress()) {
    return (pageSplit(page_bulk, next_page_bulk));
  }

  /* Insert node pointer to father page. */
  if (insert_father) {
    dtuple_t *node_ptr = page_bulk->getNodePtr();
    dberr_t err = insert(node_ptr, page_bulk->getLevel() + 1);

    if (err != DB_SUCCESS) {
      return (err);
    }
  }

  /* Commit mtr. */
  page_bulk->commit(true);

  return (DB_SUCCESS);
}

/** Log free check */
void BtrBulk::logFreeCheck() {
  if (log_needs_free_check()) {
    release();

    log_free_check();

    latch();
  }
}

/** Constructor
@param[in]  index   B-tree index
@param[in]  trx_id    transaction id
@param[in]  observer  flush observer */
BtrBulk::BtrBulk(dict_index_t *index, trx_id_t trx_id, FlushObserver *observer)
    : m_index(index),
      m_trx_id(trx_id),
      m_root_level(0),
      m_flush_observer(observer),
      m_page_bulks(nullptr) {
  ut_ad(m_flush_observer != nullptr);
#ifdef UNIV_DEBUG
  fil_space_inc_redo_skipped_count(m_index->space);
#endif /* UNIV_DEBUG */
}

/** Destructor */
BtrBulk::~BtrBulk() {
  if (m_page_bulks) {
    UT_DELETE(m_page_bulks);
  }

#ifdef UNIV_DEBUG
  fil_space_dec_redo_skipped_count(m_index->space);
#endif /* UNIV_DEBUG */
}

/** Initialization
@note Must be called right after constructor. */
dberr_t BtrBulk::init() {
  ut_ad(m_page_bulks == nullptr);

  m_page_bulks = UT_NEW_NOKEY(page_bulk_vector());
  if (m_page_bulks == nullptr) {
    return (DB_OUT_OF_MEMORY);
  }

  return (DB_SUCCESS);
}

/** Release all latches */
void BtrBulk::release() {
  ut_ad(m_page_bulks);
  ut_ad(m_root_level + 1 == m_page_bulks->size());

  for (ulint level = 0; level <= m_root_level; level++) {
    PageBulk *page_bulk = m_page_bulks->at(level);

    page_bulk->release();
  }
}

/** Re-latch all latches */
void BtrBulk::latch() {
  ut_ad(m_page_bulks);
  ut_ad(m_root_level + 1 == m_page_bulks->size());

  for (ulint level = 0; level <= m_root_level; level++) {
    PageBulk *page_bulk = m_page_bulks->at(level);
    page_bulk->latch();
  }
}

/** Prepare space to insert a tuple.
@param[in,out]  page_bulk   page bulk that will be used to store the record.
                            It may be replaced if there is not enough space
                            to hold the record.
@param[in]  level           B-tree level
@param[in]  rec_size        record size
@return error code */
dberr_t BtrBulk::prepareSpace(PageBulk *&page_bulk, ulint level,
                              ulint rec_size) {
  if (page_bulk->isSpaceAvailable(rec_size)) {
    return (DB_SUCCESS);
  }

  /* Finish page modifications. */
  page_bulk->finish();

  DBUG_EXECUTE_IF("ib_btr_bulk_prepare_space_error",
                  { return (DB_OUT_OF_MEMORY); });

  /* Create a sibling page_bulk. */
  PageBulk *sibling_page_bulk = UT_NEW_NOKEY(
      PageBulk(m_index, m_trx_id, FIL_NULL, level, m_flush_observer));
  if (sibling_page_bulk == nullptr) {
    return (DB_OUT_OF_MEMORY);
  }

  auto init_err = sibling_page_bulk->init();
  if (init_err != DB_SUCCESS) {
    UT_DELETE(sibling_page_bulk);
    return (init_err);
  }

  /* Commit page bulk. */
  auto commit_err = pageCommit(page_bulk, sibling_page_bulk, true);
  if (commit_err != DB_SUCCESS) {
    pageAbort(sibling_page_bulk);
    UT_DELETE(sibling_page_bulk);
    return (commit_err);
  }

  /* Set new page bulk to page_bulks. */
  ut_ad(sibling_page_bulk->getLevel() <= m_root_level);
  m_page_bulks->at(level) = sibling_page_bulk;

  UT_DELETE(page_bulk);
  page_bulk = sibling_page_bulk;

  /* Important: log_free_check whether we need a checkpoint. */
  if (page_is_leaf(sibling_page_bulk->getPage())) {
    /* Check whether trx is interrupted */
    if (m_flush_observer->check_interrupted()) {
      return (DB_INTERRUPTED);
    }

    /* Wake up page cleaner to flush dirty pages. */
    srv_inc_activity_count();
    os_event_set(buf_flush_event);

    logFreeCheck();
  }

  return (DB_SUCCESS);
}

/** Insert a tuple to a page.
@param[in]  page_bulk   page bulk object
@param[in]  tuple       tuple to insert
@param[in]  big_rec     big record vector, could be nullptr if there is no
                        data to be stored externally.
@param[in]  rec_size    record size
@param[in]  n_ext       number of externally stored columns
@return error code */
dberr_t BtrBulk::insert(PageBulk *page_bulk, dtuple_t *tuple,
                        big_rec_t *big_rec, ulint rec_size, ulint n_ext) {
  dberr_t err = DB_SUCCESS;

  if (big_rec != nullptr) {
    ut_ad(m_index->is_clustered());
    ut_ad(page_bulk->getLevel() == 0);
    ut_ad(page_bulk == m_page_bulks->at(0));

    /* Release all latched but leaf node. */
    for (ulint level = 1; level <= m_root_level; level++) {
      PageBulk *level_page_bulk = m_page_bulks->at(level);

      level_page_bulk->release();
    }
  }

  err = page_bulk->insert(tuple, big_rec, rec_size, n_ext);

  if (big_rec != nullptr) {
    /* Restore latches */
    for (ulint level = 1; level <= m_root_level; level++) {
      PageBulk *level_page_bulk = m_page_bulks->at(level);
      level_page_bulk->latch();
    }
  }

  return (err);
}

/** Insert a tuple to page in a level
@param[in]	tuple	tuple to insert
@param[in]	level	B-tree level
@return error code */
dberr_t BtrBulk::insert(dtuple_t *tuple, ulint level) {
  bool is_left_most = false;
  dberr_t err = DB_SUCCESS;

  ut_ad(m_page_bulks != nullptr);

  /* Check if we need to create a PageBulk for the level. */
  if (level + 1 > m_page_bulks->size()) {
    PageBulk *new_page_bulk = UT_NEW_NOKEY(
        PageBulk(m_index, m_trx_id, FIL_NULL, level, m_flush_observer));
    if (new_page_bulk == nullptr) {
      return (DB_OUT_OF_MEMORY);
    }

    err = new_page_bulk->init();
    if (err != DB_SUCCESS) {
      return (err);
    }

    m_page_bulks->push_back(new_page_bulk);
    ut_ad(level + 1 == m_page_bulks->size());
    m_root_level = level;

    is_left_most = true;
  }

  ut_ad(m_page_bulks->size() > level);

  PageBulk *page_bulk = m_page_bulks->at(level);

  if (is_left_most && level > 0 && page_bulk->getRecNo() == 0) {
    /* The node pointer must be marked as the predefined minimum
    record,	as there is no lower alphabetical limit to records in
    the leftmost node of a level: */
    dtuple_set_info_bits(tuple,
                         dtuple_get_info_bits(tuple) | REC_INFO_MIN_REC_FLAG);
  }

  ulint n_ext = 0;
  ulint rec_size = rec_get_converted_size(m_index, tuple, n_ext);
  big_rec_t *big_rec = nullptr;

  if (page_bulk->needExt(tuple, rec_size)) {
    /* The record is so big that we have to store some fields
    externally on separate database pages */
    big_rec = dtuple_convert_big_rec(m_index, 0, tuple, &n_ext);
    if (big_rec == nullptr) {
      return (DB_TOO_BIG_RECORD);
    }

    rec_size = rec_get_converted_size(m_index, tuple, n_ext);
  }

  if (page_bulk->isTableCompressed() && page_zip_is_too_big(m_index, tuple)) {
    err = DB_TOO_BIG_RECORD;
    goto func_exit;
  }

  err = prepareSpace(page_bulk, level, rec_size);
  if (err != DB_SUCCESS) {
    goto func_exit;
  }

  DBUG_EXECUTE_IF("ib_btr_bulk_insert_inject_error", {
    static int rec_cnt = 0;
    if (++rec_cnt == 10) {
      err = DB_TOO_BIG_RECORD;
      rec_cnt = 0;
      goto func_exit;
    }
  });

  err = insert(page_bulk, tuple, big_rec, rec_size, n_ext);

func_exit:
  if (big_rec != nullptr) {
    dtuple_convert_back_big_rec(m_index, tuple, big_rec);
  }

  return (err);
}

dberr_t BtrBulk::finishAllPageBulks(dberr_t err, page_no_t &last_page_no) {
  ut_ad(m_root_level + 1 == m_page_bulks->size());

  last_page_no = FIL_NULL;

  /* Finish all page bulks */
  for (ulint level = 0; level <= m_root_level; level++) {
    PageBulk *page_bulk = m_page_bulks->at(level);

    page_bulk->finish();

    last_page_no = page_bulk->getPageNo();

    if (err == DB_SUCCESS) {
      err = pageCommit(page_bulk, nullptr, level != m_root_level);
    }

    if (err != DB_SUCCESS) {
      pageAbort(page_bulk);
    }

    UT_DELETE(page_bulk);
  }

  return (err);
}

/** Btree bulk load finish. We commit the last page in each level
and copy the last page in top level to the root page of the index
if no error occurs.
@param[in]	err	whether bulk load was successful until now
@return error code  */
dberr_t BtrBulk::finish(dberr_t err) {
  ut_ad(m_page_bulks);
  ut_ad(!m_index->table->is_temporary());

  page_no_t last_page_no = FIL_NULL;

  if (m_page_bulks->size() == 0) {
    /* The table is empty. The root page of the index tree
    is already in a consistent state. No need to flush. */
    return (err);
  }

  err = finishAllPageBulks(err, last_page_no);

  if (err == DB_SUCCESS) {
    ut_ad(last_page_no != FIL_NULL);

    page_id_t last_page_id(dict_index_get_space(m_index), last_page_no);
    page_size_t page_size(dict_table_page_size(m_index->table));
    page_no_t root_page_no = dict_index_get_page(m_index);
    PageBulk root_page_bulk(m_index, m_trx_id, root_page_no, m_root_level,
                            m_flush_observer);

    mtr_t mtr;
    mtr_start(&mtr);
    mtr_x_lock(dict_index_get_lock(m_index), &mtr);

    buf_block_t *last_block =
        btr_block_get(last_page_id, page_size, RW_X_LATCH, m_index, &mtr);
    page_t *last_page = buf_block_get_frame(last_block);

    /* Copy last page to root page. */
    err = root_page_bulk.init();
    if (err == DB_SUCCESS) {
      root_page_bulk.copyAll(last_page);
      root_page_bulk.finish();

      /* Remove last page. */
      btr_page_free_low(m_index, last_block, m_root_level, &mtr);

      /* Do not flush the last page. */
      last_block->page.flush_observer = nullptr;

      mtr_commit(&mtr);

      err = pageCommit(&root_page_bulk, nullptr, false);
      ut_ad(err == DB_SUCCESS);
    } else {
      mtr_commit(&mtr);
    }
  }

#ifdef UNIV_DEBUG
  dict_sync_check check(true);

  ut_ad(!sync_check_iterate(check));
#endif /* UNIV_DEBUG */

  ut_ad(err != DB_SUCCESS || btr_validate_index(m_index, nullptr, false));
  return (err);
}
