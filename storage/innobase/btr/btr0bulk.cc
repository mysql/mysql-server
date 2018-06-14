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

  ut_ad(m_heap == NULL);
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
    bool success;
    success = fsp_reserve_free_extents(&n_reserved, m_index->space, 1,
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
      btr_page_set_level(new_page, NULL, m_level, mtr);
    }

    btr_page_set_next(new_page, NULL, FIL_NULL, mtr);
    btr_page_set_prev(new_page, NULL, FIL_NULL, mtr);

    btr_page_set_index_id(new_page, NULL, m_index->id, mtr);
  } else {
    page_id_t page_id(dict_index_get_space(m_index), m_page_no);
    page_size_t page_size(dict_table_page_size(m_index->table));

    new_block = btr_block_get(page_id, page_size, RW_X_LATCH, m_index, mtr);

    new_page = buf_block_get_frame(new_block);
    new_page_zip = buf_block_get_page_zip(new_block);
    new_page_no = page_get_page_no(new_page);
    ut_ad(m_page_no == new_page_no);

    ut_ad(page_dir_get_n_heap(new_page) == PAGE_HEAP_NO_USER_LOW);

    btr_page_set_level(new_page, NULL, m_level, mtr);
  }

  if (dict_index_is_sec_or_ibuf(m_index) && !m_index->table->is_temporary() &&
      page_is_leaf(new_page)) {
    page_update_max_trx_id(new_block, NULL, m_trx_id, mtr);
  }

  m_mtr = mtr;
  m_block = new_block;
  m_block->skip_flush_check = true;
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

  ut_d(m_total_data = 0);
  page_header_set_field(m_page, NULL, PAGE_HEAP_TOP, UNIV_PAGE_SIZE - 1);

  return (DB_SUCCESS);
}

/** Insert a record in the page.
@param[in]	rec		record
@param[in]	offsets		record offsets */
void PageBulk::insert(const rec_t *rec, ulint *offsets) {
  ulint rec_size;

  ut_ad(m_heap != NULL);

  rec_size = rec_offs_size(offsets);

#ifdef UNIV_DEBUG
  /* Check whether records are in order. */
  if (!page_rec_is_infimum(m_cur_rec)) {
    rec_t *old_rec = m_cur_rec;
    ulint *old_offsets =
        rec_get_offsets(old_rec, m_index, NULL, ULINT_UNDEFINED, &m_heap);

    ut_ad(cmp_rec_rec(rec, old_rec, offsets, old_offsets, m_index) > 0);
  }

  m_total_data += rec_size;
#endif /* UNIV_DEBUG */

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
    rec_set_n_owned_new(insert_rec, NULL, 0);
    rec_set_heap_no_new(insert_rec, PAGE_HEAP_NO_USER_LOW + m_rec_no);
  } else {
    rec_set_n_owned_old(insert_rec, 0);
    rec_set_heap_no_old(insert_rec, PAGE_HEAP_NO_USER_LOW + m_rec_no);
  }

  /* 4. Set member variables. */
  ulint slot_size;
  slot_size = page_dir_calc_reserved_space(m_rec_no + 1) -
              page_dir_calc_reserved_space(m_rec_no);

  ut_ad(m_free_space >= rec_size + slot_size);
  ut_ad(m_heap_top + rec_size < m_page + UNIV_PAGE_SIZE);

  m_free_space -= rec_size + slot_size;
  m_heap_top += rec_size;
  m_rec_no += 1;
  m_cur_rec = insert_rec;
}

/** Mark end of insertion to the page. Scan all records to set page dirs,
and set page header members.
Note: we refer to page_copy_rec_list_end_to_created_page. */
void PageBulk::finish() {
  ut_ad(m_rec_no > 0);

#ifdef UNIV_DEBUG
  ut_ad(m_total_data + page_dir_calc_reserved_space(m_rec_no) <=
        page_get_free_space_of_empty(m_is_comp));

  /* To pass the debug tests we have to set these dummy values
  in the debug version */
  page_dir_set_n_slots(m_page, NULL, UNIV_PAGE_SIZE / 2);
#endif

  ulint count = 0;
  ulint n_recs = 0;
  ulint slot_index = 0;
  rec_t *insert_rec = page_rec_get_next(page_get_infimum_rec(m_page));
  page_dir_slot_t *slot = NULL;

  /* Set owner & dir. */
  do {
    count++;
    n_recs++;

    if (count == (PAGE_DIR_SLOT_MAX_N_OWNED + 1) / 2) {
      slot_index++;

      slot = page_dir_get_nth_slot(m_page, slot_index);

      page_dir_slot_set_rec(slot, insert_rec);
      page_dir_slot_set_n_owned(slot, NULL, count);

      count = 0;
    }

    insert_rec = page_rec_get_next(insert_rec);
  } while (!page_rec_is_supremum(insert_rec));

  if (slot_index > 0 && (count + 1 + (PAGE_DIR_SLOT_MAX_N_OWNED + 1) / 2 <=
                         PAGE_DIR_SLOT_MAX_N_OWNED)) {
    /* We can merge the two last dir slots. This operation is
    here to make this function imitate exactly the equivalent
    task made using page_cur_insert_rec, which we use in database
    recovery to reproduce the task performed by this function.
    To be able to check the correctness of recovery, it is good
    that it imitates exactly. */

    count += (PAGE_DIR_SLOT_MAX_N_OWNED + 1) / 2;

    page_dir_slot_set_n_owned(slot, NULL, 0);

    slot_index--;
  }

  slot = page_dir_get_nth_slot(m_page, 1 + slot_index);
  page_dir_slot_set_rec(slot, page_get_supremum_rec(m_page));
  page_dir_slot_set_n_owned(slot, NULL, count + 1);

  ut_ad(!dict_index_is_spatial(m_index));
  page_dir_set_n_slots(m_page, NULL, 2 + slot_index);
  page_header_set_ptr(m_page, NULL, PAGE_HEAP_TOP, m_heap_top);
  page_dir_set_n_heap(m_page, NULL, PAGE_HEAP_NO_USER_LOW + m_rec_no);
  page_header_set_field(m_page, NULL, PAGE_N_RECS, m_rec_no);

  page_header_set_ptr(m_page, NULL, PAGE_LAST_INSERT, m_cur_rec);
  page_header_set_field(m_page, NULL, PAGE_DIRECTION, PAGE_RIGHT);
  page_header_set_field(m_page, NULL, PAGE_N_DIRECTION, 0);

  m_block->skip_flush_check = false;
}

/** Commit inserts done to the page
@param[in]	success		Flag whether all inserts succeed. */
void PageBulk::commit(bool success) {
  if (success) {
    ut_ad(page_validate(m_page, m_index));

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
  ut_ad(m_page_zip != NULL);

  return (
      page_zip_compress(m_page_zip, m_page, m_index, page_zip_level, m_mtr));
}

/** Get node pointer
@return node pointer */
dtuple_t *PageBulk::getNodePtr() {
  rec_t *first_rec;
  dtuple_t *node_ptr;

  /* Create node pointer */
  first_rec = page_rec_get_next(page_get_infimum_rec(m_page));
  ut_a(page_rec_is_user_rec(first_rec));
  node_ptr =
      dict_index_build_node_ptr(m_index, first_rec, m_page_no, m_heap, m_level);

  return (node_ptr);
}

/** Get split rec in left page.We split a page in half when compresssion fails,
and the split rec will be copied to right page.
@return split rec */
rec_t *PageBulk::getSplitRec() {
  rec_t *rec;
  ulint *offsets;
  ulint total_used_size;
  ulint total_recs_size;
  ulint n_recs;

  ut_ad(m_page_zip != NULL);
  ut_ad(m_rec_no >= 2);

  ut_ad(page_get_free_space_of_empty(m_is_comp) > m_free_space);
  total_used_size = page_get_free_space_of_empty(m_is_comp) - m_free_space;

  total_recs_size = 0;
  n_recs = 0;
  offsets = NULL;
  rec = page_get_infimum_rec(m_page);

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
  }

  return (rec);
}

/** Copy all records after split rec including itself.
@param[in]	split_rec	split rec */
void PageBulk::copyIn(rec_t *split_rec) {
  rec_t *rec = split_rec;
  ulint *offsets = NULL;

  ut_ad(m_rec_no == 0);
  ut_ad(page_rec_is_user_rec(rec));

  do {
    offsets =
        rec_get_offsets(rec, m_index, offsets, ULINT_UNDEFINED, &(m_heap));

    insert(rec, offsets);

    rec = page_rec_get_next(rec);
  } while (!page_rec_is_supremum(rec));

  ut_ad(m_rec_no > 0);
}

/** Remove all records after split rec including itself.
@param[in]	split_rec	split rec	*/
void PageBulk::copyOut(rec_t *split_rec) {
  rec_t *rec;
  rec_t *last_rec;
  ulint n;

  /* Suppose before copyOut, we have 5 records on the page:
  infimum->r1->r2->r3->r4->r5->supremum, and r3 is the split rec.

  after copyOut, we have 2 records on the page:
  infimum->r1->r2->supremum. slot ajustment is not done. */

  rec = page_rec_get_next(page_get_infimum_rec(m_page));
  last_rec = page_rec_get_prev(page_get_supremum_rec(m_page));
  n = 0;

  while (rec != split_rec) {
    rec = page_rec_get_next(rec);
    n++;
  }

  ut_ad(n > 0);

  /* Set last record's next in page */
  ulint *offsets = NULL;
  rec = page_rec_get_prev(split_rec);
  offsets = rec_get_offsets(rec, m_index, offsets, ULINT_UNDEFINED, &(m_heap));
  page_rec_set_next(rec, page_get_supremum_rec(m_page));

  /* Set related members */
  m_cur_rec = rec;
  m_heap_top = rec_get_end(rec, offsets);

  offsets =
      rec_get_offsets(last_rec, m_index, offsets, ULINT_UNDEFINED, &(m_heap));

  m_free_space += rec_get_end(last_rec, offsets) - m_heap_top +
                  page_dir_calc_reserved_space(m_rec_no) -
                  page_dir_calc_reserved_space(n);
  ut_ad(m_free_space > 0);
  m_rec_no = n;

#ifdef UNIV_DEBUG
  m_total_data -= rec_get_end(last_rec, offsets) - m_heap_top;
#endif /* UNIV_DEBUG */
}

/** Set next page
@param[in]	next_page_no	next page no */
void PageBulk::setNext(page_no_t next_page_no) {
  btr_page_set_next(m_page, NULL, next_page_no, m_mtr);
}

/** Set previous page
@param[in]	prev_page_no	previous page no */
void PageBulk::setPrev(page_no_t prev_page_no) {
  btr_page_set_prev(m_page, NULL, prev_page_no, m_mtr);
}

/** Check if required space is available in the page for the rec to be inserted.
We check fill factor & padding here.
@param[in]	rec_size	required length
@return true	if space is available */
bool PageBulk::isSpaceAvailable(ulint rec_size) {
  ulint slot_size;
  ulint required_space;

  slot_size = page_dir_calc_reserved_space(m_rec_no + 1) -
              page_dir_calc_reserved_space(m_rec_no);

  required_space = rec_size + slot_size;

  if (required_space > m_free_space) {
    ut_ad(m_rec_no > 0);
    return false;
  }

  /* Fillfactor & Padding apply to both leaf and non-leaf pages.
  Note: we keep at least 2 records in a page to avoid B-tree level
  growing too high. */
  if (m_rec_no >= 2 && ((m_page_zip == NULL &&
                         m_free_space - required_space < m_reserved_space) ||
                        (m_page_zip != NULL &&
                         m_free_space - required_space < m_padding_space))) {
    return (false);
  }

  return (true);
}

/** Check whether the record needs to be stored externally.
@return false if the entire record can be stored locally on the page  */
bool PageBulk::needExt(const dtuple_t *tuple, ulint rec_size) {
  return (page_zip_rec_needs_ext(
      rec_size, m_is_comp, dtuple_get_n_fields(tuple), m_block->page.size));
}

/** Store external record
Since the record is not logged yet, so we don't log update to the record.
the blob data is logged first, then the record is logged in bulk mode.
@param[in]	big_rec		external recrod
@param[in]	offsets		record offsets
@return	error code */
dberr_t PageBulk::storeExt(const big_rec_t *big_rec, ulint *offsets) {
  /* Note: not all fileds are initialized in btr_pcur. */
  btr_pcur_t btr_pcur;
  btr_pcur.pos_state = BTR_PCUR_IS_POSITIONED;
  btr_pcur.latch_mode = BTR_MODIFY_LEAF;
  btr_pcur.btr_cur.index = m_index;

  page_cur_t *page_cur = &btr_pcur.btr_cur.page_cur;
  page_cur->index = m_index;
  page_cur->rec = m_cur_rec;
  page_cur->offsets = offsets;
  page_cur->block = m_block;

  dberr_t err = lob::btr_store_big_rec_extern_fields(nullptr, &btr_pcur, NULL,
                                                     offsets, big_rec, m_mtr,
                                                     lob::OPCODE_INSERT_BULK);

  ut_ad(page_offset(m_cur_rec) == page_offset(page_cur->rec));

  /* Reset m_block and m_cur_rec from page cursor, because
  block may be changed during blob insert. */
  m_block = page_cur->block;
  m_cur_rec = page_cur->rec;
  m_page = buf_block_get_frame(m_block);

  return (err);
}

/** Release block by commiting mtr
Note: log_free_check requires holding no lock/latch in current thread. */
void PageBulk::release() {
  ut_ad(!dict_index_is_spatial(m_index));

  /* We fix the block because we will re-pin it soon. */
  buf_block_buf_fix_inc(m_block, __FILE__, __LINE__);

  /* No other threads can modify this block. */
  m_modify_clock = buf_block_get_modify_clock(m_block);

  mtr_commit(m_mtr);
}

/** Start mtr and latch the block */
void PageBulk::latch() {
  ibool ret;

  mtr_start(m_mtr);
  mtr_x_lock(dict_index_get_lock(m_index), m_mtr);
  mtr_set_log_mode(m_mtr, MTR_LOG_NO_REDO);
  mtr_set_flush_observer(m_mtr, m_flush_observer);

  /* TODO: need a simple and wait version of buf_page_optimistic_get. */
  ret = buf_page_optimistic_get(RW_X_LATCH, m_block, m_modify_clock, __FILE__,
                                __LINE__, m_mtr);
  /* In case the block is S-latched by page_cleaner. */
  if (!ret) {
    page_id_t page_id(dict_index_get_space(m_index), m_page_no);
    page_size_t page_size(dict_table_page_size(m_index->table));

    m_block = buf_page_get_gen(page_id, page_size, RW_X_LATCH, m_block,
                               BUF_GET_IF_IN_POOL, __FILE__, __LINE__, m_mtr);
    ut_ad(m_block != NULL);
  }

  buf_block_buf_fix_dec(m_block);

  ut_ad(m_cur_rec > m_page && m_cur_rec < m_heap_top);
}

/** Split a page
@param[in]	page_bulk	page to split
@param[in]	next_page_bulk	next page
@return	error code */
dberr_t BtrBulk::pageSplit(PageBulk *page_bulk, PageBulk *next_page_bulk) {
  ut_ad(page_bulk->getPageZip() != NULL);

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
  rec_t *split_rec = page_bulk->getSplitRec();
  new_page_bulk.copyIn(split_rec);
  page_bulk->copyOut(split_rec);

  /* 4. commit the splitted page. */
  err = pageCommit(page_bulk, &new_page_bulk, true);
  if (err != DB_SUCCESS) {
    pageAbort(&new_page_bulk);
    return (err);
  }

  /* 5. commit the new page. */
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
  page_bulk->finish();

  /* Set page links */
  if (next_page_bulk != NULL) {
    ut_ad(page_bulk->getLevel() == next_page_bulk->getLevel());

    page_bulk->setNext(next_page_bulk->getPageNo());
    next_page_bulk->setPrev(page_bulk->getPageNo());
  } else {
    /** Suppose a page is released and latched again, we need to
    mark it modified in mini-transaction.  */
    page_bulk->setNext(FIL_NULL);
  }

  /* Compress page if it's a compressed table. */
  if (page_bulk->getPageZip() != NULL && !page_bulk->compress()) {
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

/** Release all latches */
void BtrBulk::release() {
  ut_ad(m_root_level + 1 == m_page_bulks->size());

  for (ulint level = 0; level <= m_root_level; level++) {
    PageBulk *page_bulk = m_page_bulks->at(level);

    page_bulk->release();
  }
}

/** Re-latch all latches */
void BtrBulk::latch() {
  ut_ad(m_root_level + 1 == m_page_bulks->size());

  for (ulint level = 0; level <= m_root_level; level++) {
    PageBulk *page_bulk = m_page_bulks->at(level);
    page_bulk->latch();
  }
}

/** Insert a tuple to page in a level
@param[in]	tuple	tuple to insert
@param[in]	level	B-tree level
@return error code */
dberr_t BtrBulk::insert(dtuple_t *tuple, ulint level) {
  bool is_left_most = false;
  dberr_t err = DB_SUCCESS;

  ut_ad(m_heap != NULL);

  /* Check if we need to create a PageBulk for the level. */
  if (level + 1 > m_page_bulks->size()) {
    PageBulk *new_page_bulk = UT_NEW_NOKEY(
        PageBulk(m_index, m_trx_id, FIL_NULL, level, m_flush_observer));
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
  big_rec_t *big_rec = NULL;
  rec_t *rec = NULL;
  ulint *offsets = NULL;

  if (page_bulk->needExt(tuple, rec_size)) {
    /* The record is so big that we have to store some fields
    externally on separate database pages */
    big_rec = dtuple_convert_big_rec(m_index, 0, tuple, &n_ext);

    if (big_rec == NULL) {
      return (DB_TOO_BIG_RECORD);
    }

    rec_size = rec_get_converted_size(m_index, tuple, n_ext);
  }

  if (page_bulk->getPageZip() != NULL && page_zip_is_too_big(m_index, tuple)) {
    err = DB_TOO_BIG_RECORD;
    goto func_exit;
  }

  if (!page_bulk->isSpaceAvailable(rec_size)) {
    /* Create a sibling page_bulk. */
    PageBulk *sibling_page_bulk;
    sibling_page_bulk = UT_NEW_NOKEY(
        PageBulk(m_index, m_trx_id, FIL_NULL, level, m_flush_observer));
    err = sibling_page_bulk->init();
    if (err != DB_SUCCESS) {
      UT_DELETE(sibling_page_bulk);
      goto func_exit;
    }

    /* Commit page bulk. */
    err = pageCommit(page_bulk, sibling_page_bulk, true);
    if (err != DB_SUCCESS) {
      pageAbort(sibling_page_bulk);
      UT_DELETE(sibling_page_bulk);
      goto func_exit;
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
        err = DB_INTERRUPTED;
        goto func_exit;
      }

      /* Wake up page cleaner to flush dirty pages. */
      srv_inc_activity_count();
      os_event_set(buf_flush_event);

      logFreeCheck();
    }
  }

  DBUG_EXECUTE_IF("BtrBulk_insert_inject_error", err = DB_INTERRUPTED;
                  goto func_exit;);

  /* Convert tuple to rec. */
  rec = rec_convert_dtuple_to_rec(
      static_cast<byte *>(mem_heap_alloc(page_bulk->m_heap, rec_size)), m_index,
      tuple, n_ext);
  offsets = rec_get_offsets(rec, m_index, offsets, ULINT_UNDEFINED,
                            &(page_bulk->m_heap));

  page_bulk->insert(rec, offsets);

  if (big_rec != NULL) {
    ut_ad(m_index->is_clustered());
    ut_ad(page_bulk->getLevel() == 0);
    ut_ad(page_bulk == m_page_bulks->at(0));

    /* Release all latched but leaf node. */
    for (ulint level = 1; level <= m_root_level; level++) {
      PageBulk *page_bulk = m_page_bulks->at(level);

      page_bulk->release();
    }

    err = page_bulk->storeExt(big_rec, offsets);

    /* Latch */
    for (ulint level = 1; level <= m_root_level; level++) {
      PageBulk *page_bulk = m_page_bulks->at(level);
      page_bulk->latch();
    }
  }

func_exit:
  if (big_rec != NULL) {
    dtuple_convert_back_big_rec(m_index, tuple, big_rec);
  }

  return (err);
}

/** Btree bulk load finish. We commit the last page in each level
and copy the last page in top level to the root page of the index
if no error occurs.
@param[in]	err	whether bulk load was successful until now
@return error code  */
dberr_t BtrBulk::finish(dberr_t err) {
  page_no_t last_page_no = FIL_NULL;

  ut_ad(!m_index->table->is_temporary());

  if (m_page_bulks->size() == 0) {
    /* The table is empty. The root page of the index tree
    is already in a consistent state. No need to flush. */
    return (err);
  }

  ut_ad(m_root_level + 1 == m_page_bulks->size());

  /* Finish all page bulks */
  for (ulint level = 0; level <= m_root_level; level++) {
    PageBulk *page_bulk = m_page_bulks->at(level);

    last_page_no = page_bulk->getPageNo();

    if (err == DB_SUCCESS) {
      err = pageCommit(page_bulk, NULL, level != m_root_level);
    }

    if (err != DB_SUCCESS) {
      pageAbort(page_bulk);
    }

    UT_DELETE(page_bulk);
  }

  if (err == DB_SUCCESS) {
    rec_t *first_rec;
    mtr_t mtr;
    buf_block_t *last_block;
    page_t *last_page;
    page_id_t page_id(dict_index_get_space(m_index), last_page_no);
    page_size_t page_size(dict_table_page_size(m_index->table));
    page_no_t root_page_no = dict_index_get_page(m_index);
    PageBulk root_page_bulk(m_index, m_trx_id, root_page_no, m_root_level,
                            m_flush_observer);

    mtr_start(&mtr);
    mtr_x_lock(dict_index_get_lock(m_index), &mtr);

    ut_ad(last_page_no != FIL_NULL);
    last_block = btr_block_get(page_id, page_size, RW_X_LATCH, m_index, &mtr);
    last_page = buf_block_get_frame(last_block);
    first_rec = page_rec_get_next(page_get_infimum_rec(last_page));
    ut_ad(page_rec_is_user_rec(first_rec));

    /* Copy last page to root page. */
    err = root_page_bulk.init();
    if (err != DB_SUCCESS) {
      mtr_commit(&mtr);
      return (err);
    }
    root_page_bulk.copyIn(first_rec);

    /* Remove last page. */
    btr_page_free_low(m_index, last_block, m_root_level, &mtr);

    /* Do not flush the last page. */
    last_block->page.flush_observer = NULL;

    mtr_commit(&mtr);

    err = pageCommit(&root_page_bulk, NULL, false);
    ut_ad(err == DB_SUCCESS);
  }

#ifdef UNIV_DEBUG
  dict_sync_check check(true);

  ut_ad(!sync_check_iterate(check));
#endif /* UNIV_DEBUG */

  ut_ad(err != DB_SUCCESS || btr_validate_index(m_index, NULL, false));
  return (err);
}
