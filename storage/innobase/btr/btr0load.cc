/*****************************************************************************

Copyright (c) 2014, 2022, Oracle and/or its affiliates.

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

/** @file btr/btr0load.cc
 The B-tree bulk load

 Created 03/11/2014 Shaohua Wang
 *******************************************************/

#include "btr0load.h"
#include "btr0btr.h"
#include "btr0cur.h"
#include "btr0pcur.h"
#include "ibuf0ibuf.h"
#include "lob0lob.h"
#include "log0chkp.h"

namespace ddl {
/** Innodb B-tree index fill factor for bulk load. */
long fill_factor;
}  // namespace ddl

/** The proper function call sequence of Page_load is as below:
-- Page_load::init
-- Page_load::insert
-- Page_load::finish
-- Page_load::compress(COMPRESSED table only)
-- Page_load::page_split(COMPRESSED table only)
-- Page_load::commit */
class Page_load : private ut::Non_copyable {
  using Rec_offsets = ulint *;

  /** Page split point descriptor. */
  struct Split_point {
    /** Record being the point of split.
    All records before this record should stay on current on page.
    This record and all following records should be moved to new page. */
    rec_t *m_rec{};

    /** Number of records before this record. */
    size_t m_n_rec_before{};
  };

 public:
  /** Constructor
  @param[in]    index                     B-tree index
  @param[in]    trx_id                  Transaction id
  @param[in]    page_no                 Page number
  @param[in]    level                     Page level
  @param[in]    observer                Flush observer */
  Page_load(dict_index_t *index, trx_id_t trx_id, page_no_t page_no,
            size_t level, Flush_observer *observer) noexcept
      : m_index(index),
        m_trx_id(trx_id),
        m_page_no(page_no),
        m_level(level),
        m_is_comp(dict_table_is_comp(index->table)),
        m_flush_observer(observer) {
    ut_ad(!dict_index_is_spatial(m_index));
  }

  /** Destructor */
  ~Page_load() noexcept {
    if (m_heap != nullptr) {
      /* mtr is allocated using heap. */
      if (m_mtr != nullptr) {
        ut_a(!m_mtr->is_active());
        m_mtr->~mtr_t();
      }
      mem_heap_free(m_heap);
    }
  }

 private:
  /** Initialize members and allocate page if needed and start mtr.
  @note Must be called and only once right after constructor.
  @return error code */
  [[nodiscard]] dberr_t init() noexcept;

  /** Insert a tuple in the page.
  @param[in]  tuple             Tuple to insert
  @param[in]  big_rec           External record
  @param[in]  rec_size          Record size
  @return error code */
  [[nodiscard]] dberr_t insert(const dtuple_t *tuple, const big_rec_t *big_rec,
                               size_t rec_size) noexcept;

  /** Mark end of insertion to the page. Scan records to set page dirs,
  and set page header members. The scan is incremental (slots and records
  which assignment could be "finalized" are not checked again. Check the
  m_slotted_rec_no usage, note it could be reset in some cases like
  during split.
  Note: we refer to page_copy_rec_list_end_to_created_page. */
  void finish() noexcept;

  /** Commit mtr for a page */
  void commit() noexcept;

  /** Commit mtr for a page */
  void rollback() noexcept;

  /** Compress if it is compressed table
  @return       true    compress successfully or no need to compress
  @return       false   compress failed. */
  [[nodiscard]] bool compress() noexcept;

  /** Check whether the record needs to be stored externally.
  @return false if the entire record can be stored locally on the page */
  [[nodiscard]] bool need_ext(const dtuple_t *tuple,
                              size_t rec_size) const noexcept;

  /** Get node pointer
  @return node pointer */
  [[nodiscard]] dtuple_t *get_node_ptr() noexcept;

  /** Split the page records between this and given bulk.
  @param new_page_load  The new bulk to store split records. */
  void split(Page_load &new_page_load) noexcept;

  /** Copy all records from page.
  @param[in]  src_page          Page with records to copy. */
  void copy_all(const page_t *src_page) noexcept;

  /** Set next page
  @param[in]    next_page_no        Next page no */
  void set_next(page_no_t next_page_no) noexcept;

  /** Set previous page
  @param[in]    prev_page_no        Previous page no */
  void set_prev(page_no_t prev_page_no) noexcept;

  /** Release block by committing mtr */
  inline void release() noexcept;

  /** Start mtr and latch block */
  inline void latch() noexcept;

  /** Check if required space is available in the page for the rec
  to be inserted.       We check fill factor & padding here.
  @param[in]    rec_size                Required space
  @return true  if space is available */
  [[nodiscard]] inline bool is_space_available(size_t rec_size) const noexcept;

  /** Get page no */
  [[nodiscard]] page_no_t get_page_no() const noexcept { return m_page_no; }

  /** Get page level */
  [[nodiscard]] size_t get_level() const noexcept { return m_level; }

  /** Get record no */
  [[nodiscard]] size_t get_rec_no() const { return m_rec_no; }

  /** Get page */
  [[nodiscard]] const page_t *get_page() const noexcept { return m_page; }

  /** Check if table is compressed.
  @return true if table is compressed, false otherwise. */
  [[nodiscard]] bool is_table_compressed() const noexcept {
    return m_page_zip != nullptr;
  }

#ifdef UNIV_DEBUG
  /** Check if index is X locked
  @return true if index is locked. */
  bool is_index_locked() noexcept;
#endif /* UNIV_DEBUG */

  /** Get page split point. We split a page in half when compression
  fails, and the split record and all following records should be copied
  to the new page.
  @return split record descriptor */
  [[nodiscard]] Split_point get_split_rec() noexcept;

  /** Copy given and all following records.
  @param[in]  first_rec         First record to copy */
  void copy_records(const rec_t *first_rec) noexcept;

  /** Remove all records after split rec including itself.
  @param[in]  split_point       Split point descriptor */
  void split_trim(const Split_point &split_point) noexcept;

  /** Insert a record in the page, check for duplicates too.
  @param[in]  rec               Record
  @param[in]  offsets           Record offsets
  @return DB_SUCCESS or error code. */
  dberr_t insert(const rec_t *rec, Rec_offsets offsets) noexcept;

  /** Store external record
  Since the record is not logged yet, so we don't log update to the record.
  the blob data is logged first, then the record is logged in bulk mode.
  @param[in]  big_rec           External record
  @param[in]  offsets           Record offsets
  @return error code */
  [[nodiscard]] dberr_t store_ext(const big_rec_t *big_rec,
                                  Rec_offsets offsets) noexcept;

 private:
  /** Memory heap for internal allocation */
  mem_heap_t *m_heap{};

  /** The index B-tree */
  dict_index_t *m_index{};

  /** The min-transaction */
  mtr_t *m_mtr{};

  /** The transaction id */
  trx_id_t m_trx_id{};

  /** The buffer block */
  buf_block_t *m_block{};

  /** The page */
  page_t *m_page{};

  /** The page zip descriptor */
  page_zip_des_t *m_page_zip{};

  /** The current rec, just before the next insert rec */
  rec_t *m_cur_rec{};

  /** The page no */
  page_no_t m_page_no{};

  /** The page level in B-tree */
  size_t m_level{};

  /** Flag: is page in compact format */
  const bool m_is_comp{};

  /** The heap top in page for next insert */
  byte *m_heap_top{};

  /** User record no */
  size_t m_rec_no{};

  /** The free space left in the page */
  size_t m_free_space{};

  /** The reserved space for fill factor */
  size_t m_reserved_space{};

  /** The padding space for compressed page */
  size_t m_padding_space{};

  /** Total data in the page */
  IF_DEBUG(size_t m_total_data{};)

  /** The modify clock value of the buffer block
  when the block is re-pinned */
  uint64_t m_modify_clock{};

  /** Flush observer */
  Flush_observer *m_flush_observer{};

  /** Last record assigned to a slot. */
  rec_t *m_last_slotted_rec{};

  /** Number of records assigned to slots. */
  size_t m_slotted_rec_no{};

  /** Page modified flag. */
  bool m_modified{};

  friend class Btree_load;
};

dberr_t Page_load::init() noexcept {
  page_t *new_page;
  page_no_t new_page_no;
  buf_block_t *new_block;
  page_zip_des_t *new_page_zip;

  ut_ad(m_heap == nullptr);

  m_heap = mem_heap_create(1024, UT_LOCATION_HERE);

  auto mtr_alloc = mem_heap_alloc(m_heap, sizeof(mtr_t));

  auto mtr = new (mtr_alloc) mtr_t();

  mtr->start();

  if (!dict_index_is_online_ddl(m_index)) {
    mtr->x_lock(dict_index_get_lock(m_index), UT_LOCATION_HERE);
  }

  mtr->set_log_mode(MTR_LOG_NO_REDO);
  mtr->set_flush_observer(m_flush_observer);

  if (m_page_no == FIL_NULL) {
    mtr_t alloc_mtr;

    /* We commit redo log for allocation by a separate mtr,
    because we don't guarantee pages are committed following
    the allocation order, and we will always generate redo log
    for page allocation, even when creating a new tablespace. */
    alloc_mtr.start();

    ulint n_reserved;
    bool success = fsp_reserve_free_extents(&n_reserved, m_index->space, 1,
                                            FSP_NORMAL, &alloc_mtr);
    if (!success) {
      alloc_mtr.commit();
      mtr->commit();
      return DB_OUT_OF_FILE_SPACE;
    }

    /* Allocate a new page. */
    new_block = btr_page_alloc(m_index, 0, FSP_UP, m_level, &alloc_mtr, mtr);

    if (n_reserved > 0) {
      fil_space_release_free_extents(m_index->space, n_reserved);
    }

    alloc_mtr.commit();

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

    new_block = btr_block_get(page_id, page_size, RW_X_LATCH, UT_LOCATION_HERE,
                              m_index, mtr);

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
  ut_ad(m_is_comp == page_is_comp(new_page));
  m_free_space = page_get_free_space_of_empty(m_is_comp);

  if (ddl::fill_factor == 100 && m_index->is_clustered()) {
    /* Keep default behavior compatible with 5.6 */
    m_reserved_space = dict_index_get_space_reserve();
  } else {
    m_reserved_space = UNIV_PAGE_SIZE * (100 - ddl::fill_factor) / 100;
  }

  m_padding_space =
      UNIV_PAGE_SIZE - dict_index_zip_pad_optimal_page_size(m_index);
  m_heap_top = page_header_get_ptr(new_page, PAGE_HEAP_TOP);
  m_rec_no = page_header_get_field(new_page, PAGE_N_RECS);

  m_last_slotted_rec = page_get_infimum_rec(m_page);
  m_slotted_rec_no = 0;

  m_modified = true;

  ut_d(m_total_data = 0);

  return DB_SUCCESS;
}

dberr_t Page_load::insert(const rec_t *rec, Rec_offsets offsets) noexcept {
  ut_ad(m_heap != nullptr);

  auto rec_size = rec_offs_size(offsets);

#ifdef UNIV_DEBUG
  /* Check whether records are in order. */
  if (!page_rec_is_infimum(m_cur_rec)) {
    auto old_rec = m_cur_rec;

    auto old_offsets = rec_get_offsets(
        old_rec, m_index, nullptr, ULINT_UNDEFINED, UT_LOCATION_HERE, &m_heap);

    ut_ad(cmp_rec_rec(rec, old_rec, offsets, old_offsets, m_index,
                      page_is_spatial_non_leaf(old_rec, m_index)) > 0 ||
          (m_index->is_multi_value() &&
           cmp_rec_rec(rec, old_rec, offsets, old_offsets, m_index,
                       page_is_spatial_non_leaf(old_rec, m_index)) >= 0));
  }

  m_total_data += rec_size;
#endif /* UNIV_DEBUG */

  /* 0. Mark space for record as used (checked e.g. in page_rec_set_next). */
  page_header_set_ptr(m_page, nullptr, PAGE_HEAP_TOP, m_heap_top + rec_size);

  /* 1. Copy the record to page. */
  auto insert_rec = rec_copy(m_heap_top, rec, offsets);
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
  auto slot_size = page_dir_calc_reserved_space(m_rec_no + 1) -
                   page_dir_calc_reserved_space(m_rec_no);

  ut_ad(m_free_space >= rec_size + slot_size);
  ut_ad(m_heap_top + rec_size < m_page + UNIV_PAGE_SIZE);

  m_free_space -= rec_size + slot_size;
  m_heap_top += rec_size;
  m_rec_no += 1;
  m_cur_rec = insert_rec;

  m_modified = true;

  return DB_SUCCESS;
}

dberr_t Page_load::insert(const dtuple_t *tuple, const big_rec_t *big_rec,
                          size_t rec_size) noexcept {
  IF_ENABLED("ddl_btree_build_insert_return_interrupt", return DB_INTERRUPTED;)

  /* Convert tuple to record. */
  auto rec_mem = static_cast<byte *>(mem_heap_alloc(m_heap, rec_size));

  auto rec = rec_convert_dtuple_to_rec(rec_mem, m_index, tuple);

  Rec_offsets offsets{};

  offsets = rec_get_offsets(rec, m_index, offsets, ULINT_UNDEFINED,
                            UT_LOCATION_HERE, &m_heap);

  /* Insert the record.*/
  const auto err = insert(rec, offsets);

  if (err != DB_SUCCESS) {
    return err;
  }

  ut_ad(m_modified);

  if (big_rec != nullptr) {
    /* The page must be valid as MTR may be committed during LOB insertion. */
    finish();
    return store_ext(big_rec, offsets);
  } else {
    return DB_SUCCESS;
  }
}

void Page_load::finish() noexcept {
  ut_ad(!dict_index_is_spatial(m_index));

  if (!m_modified) {
    return;
  }

  ut_ad(m_total_data + page_dir_calc_reserved_space(m_rec_no) <=
        page_get_free_space_of_empty(m_is_comp));

  auto n_rec_to_assign = m_rec_no - m_slotted_rec_no;

  /* Fill slots for non-supremum records if possible.
  Slot for supremum record could store up to
  PAGE_DIR_SLOT_MAX_N_OWNED-1 records. */
  while (n_rec_to_assign >= PAGE_DIR_SLOT_MAX_N_OWNED) {
    static constexpr size_t RECORDS_PER_SLOT =
        (PAGE_DIR_SLOT_MAX_N_OWNED + 1) / 2;

    for (size_t i = 0; i < RECORDS_PER_SLOT; ++i) {
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

void Page_load::commit() noexcept {
  /* It is assumed that finish() was called before commit */
  ut_a(!m_modified);
  ut_a(page_validate(m_page, m_index));
  ut_a(m_rec_no > 0);

  /* Set no free space left and no buffered changes in ibuf. */
  if (!m_index->is_clustered() && !m_index->table->is_temporary() &&
      page_is_leaf(m_page)) {
    ibuf_set_bitmap_for_bulk_load(m_block, ddl::fill_factor == 100);
  }

  m_mtr->commit();
}

void Page_load::rollback() noexcept {
  /* It is assumed that finish() was called before commit */
  ut_a(!m_modified);
  ut_a(page_validate(m_page, m_index));

  m_mtr->commit();
}

bool Page_load::compress() noexcept {
  ut_ad(!m_modified);
  ut_ad(m_page_zip != nullptr);

  return page_zip_compress(m_page_zip, m_page, m_index, page_zip_level, m_mtr);
}

dtuple_t *Page_load::get_node_ptr() noexcept {
  /* Create node pointer */
  auto first_rec = page_rec_get_next(page_get_infimum_rec(m_page));
  ut_a(page_rec_is_user_rec(first_rec));

  auto node_ptr =
      dict_index_build_node_ptr(m_index, first_rec, m_page_no, m_heap, m_level);

  return node_ptr;
}

void Page_load::split(Page_load &new_page_loader) noexcept {
  auto split_point = get_split_rec();

  new_page_loader.copy_records(split_point.m_rec);
  split_trim(split_point);

  ut_ad(new_page_loader.m_modified);
  ut_ad(m_modified);
}

Page_load::Split_point Page_load::get_split_rec() noexcept {
  ut_a(m_rec_no >= 2);
  ut_a(m_page_zip != nullptr);
  ut_a(page_get_free_space_of_empty(m_is_comp) > m_free_space);

  auto total_used_size = page_get_free_space_of_empty(m_is_comp) - m_free_space;

  size_t n_recs{};
  Rec_offsets offsets{};
  size_t total_recs_size{};

  auto rec = page_get_infimum_rec(m_page);

  do {
    rec = page_rec_get_next(rec);
    ut_ad(page_rec_is_user_rec(rec));

    offsets = rec_get_offsets(rec, m_index, offsets, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &m_heap);
    total_recs_size += rec_offs_size(offsets);
    n_recs++;
  } while (total_recs_size + page_dir_calc_reserved_space(n_recs) <
           total_used_size / 2);

  /* Keep at least one record on left page */
  if (page_rec_is_infimum(page_rec_get_prev(rec))) {
    rec = page_rec_get_next(rec);
    ut_ad(page_rec_is_user_rec(rec));
  } else {
    /* rec is to be moved, and this is used as number of records before split */
    --n_recs;
  }

  return Split_point{rec, n_recs};
}

void Page_load::copy_all(const page_t *src_page) noexcept {
  auto inf_rec = page_get_infimum_rec(src_page);
  auto first_rec = page_rec_get_next_const(inf_rec);

  ut_ad(page_rec_is_user_rec(first_rec));

  copy_records(first_rec);

  ut_ad(m_modified);
}

void Page_load::copy_records(const rec_t *first_rec) noexcept {
  Rec_offsets offsets{};
  const rec_t *rec = first_rec;

  ut_ad(m_rec_no == 0);
  ut_ad(page_rec_is_user_rec(rec));

  do {
    offsets = rec_get_offsets(rec, m_index, offsets, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &m_heap);

    insert(rec, offsets);

    rec = page_rec_get_next_const(rec);
  } while (!page_rec_is_supremum(rec));

  ut_ad(m_rec_no > 0);
}

void Page_load::split_trim(const Split_point &split_point) noexcept {
  /* Suppose before copyOut, we have 5 records on the page:
  infimum->r1->r2->r3->r4->r5->supremum, and r3 is the split rec.

  after copyOut, we have 2 records on the page:
  infimum->r1->r2->supremum. slot adjustment is not done. */

  /* Set number of user records. */
  auto new_rec_no = split_point.m_n_rec_before;
  ut_a(new_rec_no > 0);

  /* Set last record's next in page */
  rec_t *new_last_user_rec = page_rec_get_prev(split_point.m_rec);
  page_rec_set_next(new_last_user_rec, page_get_supremum_rec(m_page));

  /* Set related members */
  auto old_heap_top = m_heap_top;

  Rec_offsets offsets{};
  offsets = rec_get_offsets(new_last_user_rec, m_index, offsets,
                            ULINT_UNDEFINED, UT_LOCATION_HERE, &m_heap);
  m_heap_top = rec_get_end(new_last_user_rec, offsets);

  m_free_space +=
      (old_heap_top - m_heap_top) + (page_dir_calc_reserved_space(m_rec_no) -
                                     page_dir_calc_reserved_space(new_rec_no));
  ut_ad(m_free_space > 0);

  m_cur_rec = new_last_user_rec;
  m_rec_no = new_rec_no;

  ut_d(m_total_data -= old_heap_top - m_heap_top);

  /* Invalidate all slots except infimum. */
  auto n_slots = page_dir_get_n_slots(m_page);

  for (size_t slot_idx = 1; slot_idx < n_slots; ++slot_idx) {
    auto slot = page_dir_get_nth_slot(m_page, slot_idx);
    page_dir_slot_set_n_owned(slot, nullptr, 0);
  }

  page_dir_set_n_slots(m_page, nullptr, 2);

  /* No records assigned to slots. */
  m_last_slotted_rec = page_get_infimum_rec(m_page);
  m_slotted_rec_no = 0;

  m_modified = true;
}

void Page_load::set_next(page_no_t next_page_no) noexcept {
  btr_page_set_next(m_page, nullptr, next_page_no, m_mtr);
}

void Page_load::set_prev(page_no_t prev_page_no) noexcept {
  btr_page_set_prev(m_page, nullptr, prev_page_no, m_mtr);
}

bool Page_load::is_space_available(size_t rec_size) const noexcept {
  auto slot_size = page_dir_calc_reserved_space(m_rec_no + 1) -
                   page_dir_calc_reserved_space(m_rec_no);

  auto required_space = rec_size + slot_size;

  if (required_space > m_free_space) {
    ut_a(m_rec_no > 0);
    return false;
  }

  /* Fillfactor & Padding apply to both leaf and non-leaf pages.
  Note: we keep at least 2 records in a page to avoid B-tree level
  growing too high. */
  if (m_rec_no >= 2 && ((m_page_zip == nullptr &&
                         m_free_space - required_space < m_reserved_space) ||
                        (m_page_zip != nullptr &&
                         m_free_space - required_space < m_padding_space))) {
    return false;
  }

  return true;
}

bool Page_load::need_ext(const dtuple_t *tuple,
                         size_t rec_size) const noexcept {
  return page_zip_rec_needs_ext(rec_size, m_is_comp, dtuple_get_n_fields(tuple),
                                m_block->page.size);
}

dberr_t Page_load::store_ext(const big_rec_t *big_rec,
                             Rec_offsets offsets) noexcept {
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

  return err;
}

void Page_load::release() noexcept {
  /* Make sure page is valid before it is released. */
  if (m_modified) {
    finish();
    ut_ad(!m_modified);
  }

  ut_ad(page_validate(m_page, m_index));
  ut_ad(!dict_index_is_spatial(m_index));
  ut_ad(m_block->page.buf_fix_count > 0);

  /* We fix the block because we will re-pin it soon. */
  buf_block_buf_fix_inc(m_block, UT_LOCATION_HERE);

  m_modify_clock = m_block->get_modify_clock(IF_DEBUG(true));

  m_mtr->commit();
}

/** Start mtr and latch the block */
void Page_load::latch() noexcept {
  m_mtr->start();

  if (!dict_index_is_online_ddl(m_index)) {
    m_mtr->x_lock(dict_index_get_lock(m_index), UT_LOCATION_HERE);
  }

  m_mtr->set_log_mode(MTR_LOG_NO_REDO);
  m_mtr->set_flush_observer(m_flush_observer);

  ut_a(m_block->page.buf_fix_count > 0);

  /* TODO: need a simple and wait version of buf_page_optimistic_get. */

  /* Since we using the mtr_t mechanism, we have to follow the existing
  rules. We are going to write to the page, for which we need an X latch
  on the page. mtr_t infrastructure uses the X latch to determine
  if the page was dirtied or not. We have to hack around the fix count
  rules to make it work. */
  auto success =
      buf_page_optimistic_get(RW_X_LATCH, m_block, m_modify_clock,
                              Page_fetch::NORMAL, __FILE__, __LINE__, m_mtr);

  /* In case the block is S-latched by page_cleaner. */
  if (!success) {
    page_id_t page_id(dict_index_get_space(m_index), m_page_no);
    page_size_t page_size(dict_table_page_size(m_index->table));

    m_block = buf_page_get_gen(page_id, page_size, RW_X_LATCH, m_block,
                               Page_fetch::IF_IN_POOL, UT_LOCATION_HERE, m_mtr);
    ut_ad(m_block != nullptr);
  }

  buf_block_buf_fix_dec(m_block);

  /* The caller is going to use the m_block, so it needs to be buffer-fixed
  even after the decrement above. This works like this:
  release(){
    // Initially buf_fix_count == N > 0
    ++buf_fix_count; // N + 1
    mtr.commit() {
      --buf_fix_count // N
    }
  }
  // At the end buf_fix_count == N > 0
  latch(){
    // Initially buf_fix_count == M > 0
    buf_page_get_gen/buf_page_optimistic_get internally(){
      ++buf_fix_count // M+1
    }
    --buf_fix_count // M
  }
  / At the end buf_fix_count == M > 0 */

  ut_a(m_block->page.buf_fix_count > 0);
  ut_a(m_cur_rec > m_page && m_cur_rec < m_heap_top);
}

#ifdef UNIV_DEBUG
bool Page_load::is_index_locked() noexcept {
  return dict_index_is_online_ddl(m_index) &&
         m_mtr->memo_contains_flagged(dict_index_get_lock(m_index),
                                      MTR_MEMO_X_LOCK | MTR_MEMO_SX_LOCK);
}
#endif /* UNIV_DEBUG */

dberr_t Btree_load::page_split(Page_load *page_loader,
                               Page_load *next_page_loader) noexcept {
  ut_ad(page_loader->is_table_compressed());
  dberr_t err{DB_SUCCESS};

  /* 1. Check if we have only one user record on the page. */
  if (page_loader->get_rec_no() <= 1) {
    return DB_TOO_BIG_RECORD;
  }

  /* 2. create a new page. */
  Page_load new_page_loader(m_index, m_trx_id, FIL_NULL,
                            page_loader->get_level(), m_flush_observer);

  err = new_page_loader.init();

  if (err != DB_SUCCESS) {
    return err;
  }

  /* 3. copy the upper half to new page. */
  page_loader->split(new_page_loader);

  /* 4. finish page bulk modifications. */
  page_loader->finish();
  new_page_loader.finish();

  /* 5. commit the split page. */
  err = page_commit(page_loader, &new_page_loader, true);
  if (err != DB_SUCCESS) {
    new_page_loader.rollback();
    return err;
  }

  /* 6. commit the new page. */
  err = page_commit(&new_page_loader, next_page_loader, true);
  if (err != DB_SUCCESS) {
    new_page_loader.rollback();
    return err;
  }

  return err;
}

dberr_t Btree_load::page_commit(Page_load *page_loader,
                                Page_load *next_page_loader,
                                bool insert_father) noexcept {
  /* Set page links */
  if (next_page_loader != nullptr) {
    ut_ad(page_loader->get_level() == next_page_loader->get_level());

    page_loader->set_next(next_page_loader->get_page_no());
    next_page_loader->set_prev(page_loader->get_page_no());
  } else {
    /* Suppose a page is released and latched again, we need to
    mark it modified in mini-transaction.  */
    page_loader->set_next(FIL_NULL);
  }

  /* Assert that no locks are held during bulk load operation
  in case of a online ddl operation. Insert thread acquires index->lock
  to check the online status of index. During bulk load index,
  there are no concurrent insert or reads and hence, there is no
  need to acquire a lock in that case. */
  ut_ad(!page_loader->is_index_locked());

  IF_ENABLED("ddl_btree_build_sleep",
             std::this_thread::sleep_for(std::chrono::seconds{1});)

  /* Compress page if it's a compressed table. */
  if (page_loader->is_table_compressed() && !page_loader->compress()) {
    return page_split(page_loader, next_page_loader);
  }

  /* Insert node pointer to father page. */
  if (insert_father) {
    auto node_ptr = page_loader->get_node_ptr();
    const dberr_t err = insert(node_ptr, page_loader->get_level() + 1);

    if (err != DB_SUCCESS) {
      return err;
    }
  }

  /* Commit mtr. */
  page_loader->commit();

  return DB_SUCCESS;
}

void Btree_load::log_free_check() noexcept {
  if (log_free_check_is_required()) {
    release();

    ::log_free_check();

    latch();
  }
}

Btree_load::Btree_load(dict_index_t *index, trx_id_t trx_id,
                       Flush_observer *observer) noexcept
    : m_index(index), m_trx_id(trx_id), m_flush_observer(observer) {
  ut_a(m_flush_observer != nullptr);
  ut_d(fil_space_inc_redo_skipped_count(m_index->space));
  ut_d(m_index_online = m_index->online_status);
}

Btree_load::~Btree_load() noexcept {
  ut_d(fil_space_dec_redo_skipped_count(m_index->space));
}

void Btree_load::release() noexcept {
  auto page_loader = m_page_loaders[0];
  page_loader->release();
}

void Btree_load::latch() noexcept {
  if (m_n_recs == 0) {
    /* Nothing to latch. */
    return;
  }
  auto page_loader = m_page_loaders[0];
  page_loader->latch();
}

dberr_t Btree_load::prepare_space(Page_load *&page_loader, size_t level,
                                  size_t rec_size) noexcept {
  if (page_loader->is_space_available(rec_size)) {
    return DB_SUCCESS;
  }

  /* Finish page modifications. */
  page_loader->finish();

  IF_ENABLED("ddl_btree_build_oom", return DB_OUT_OF_MEMORY;)

  /* Create a sibling page_loader. */
  auto sibling_page_loader =
      ut::new_withkey<Page_load>(UT_NEW_THIS_FILE_PSI_KEY, m_index, m_trx_id,
                                 FIL_NULL, level, m_flush_observer);

  if (sibling_page_loader == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  {
    auto err = sibling_page_loader->init();

    if (err != DB_SUCCESS) {
      ut::delete_(sibling_page_loader);
      return err;
    }
  }

  /* Commit page bulk. */
  {
    auto err = page_commit(page_loader, sibling_page_loader, true);

    if (err != DB_SUCCESS) {
      sibling_page_loader->rollback();
      ut::delete_(sibling_page_loader);
      return err;
    }
  }

  /* Set new page bulk to page_loaders. */
  ut_a(sibling_page_loader->get_level() <= m_root_level);

  m_page_loaders[level] = sibling_page_loader;

  ut::delete_(page_loader);

  page_loader = sibling_page_loader;

  /* Important: log_free_check whether we need a checkpoint. */
  if (page_is_leaf(sibling_page_loader->get_page())) {
    /* Wake up page cleaner to flush dirty pages. */
    srv_inc_activity_count();
    os_event_set(buf_flush_event);

    log_free_check();
  }

  return DB_SUCCESS;
}

dberr_t Btree_load::insert(Page_load *page_loader, dtuple_t *tuple,
                           big_rec_t *big_rec, size_t rec_size) noexcept {
  if (big_rec != nullptr) {
    ut_a(m_index->is_clustered());
    ut_a(page_loader->get_level() == 0);
    ut_a(page_loader == m_page_loaders[0]);
  }
  auto err = page_loader->insert(tuple, big_rec, rec_size);
  return err;
}

dberr_t Btree_load::insert(dtuple_t *tuple, size_t level) noexcept {
  bool is_left_most{};
  dberr_t err{DB_SUCCESS};

  /* Check if we need to create a Page_load for the level. */
  if (level + 1 > m_page_loaders.size()) {
    auto page_loader =
        ut::new_withkey<Page_load>(UT_NEW_THIS_FILE_PSI_KEY, m_index, m_trx_id,
                                   FIL_NULL, level, m_flush_observer);

    if (page_loader == nullptr) {
      return DB_OUT_OF_MEMORY;
    }

    err = page_loader->init();

    if (err != DB_SUCCESS) {
      return err;
    }

    DEBUG_SYNC_C("bulk_load_insert");

    m_page_loaders.push_back(page_loader);

    ut_a(level + 1 == m_page_loaders.size());

    m_root_level = level;

    is_left_most = true;

    if (level > 0) {
      page_loader->release();
    }
  }

  ut_a(m_page_loaders.size() > level);

  auto page_loader = m_page_loaders[level];
  if (level > 0) {
    page_loader->latch();
  }

  if (is_left_most && level > 0 && page_loader->get_rec_no() == 0) {
    /* The node pointer must be marked as the predefined minimum
    record,     as there is no lower alphabetical limit to records in
    the leftmost node of a level: */
    const auto info_bits = dtuple_get_info_bits(tuple) | REC_INFO_MIN_REC_FLAG;
    dtuple_set_info_bits(tuple, info_bits);
  }

  big_rec_t *big_rec{};
  auto rec_size = rec_get_converted_size(m_index, tuple);

  if (page_loader->need_ext(tuple, rec_size)) {
    /* The record is so big that we have to store some fields
    externally on separate database pages */
    big_rec = dtuple_convert_big_rec(m_index, nullptr, tuple);
    if (big_rec == nullptr) {
      if (level > 0) {
        page_loader->release();
      }
      return DB_TOO_BIG_RECORD;
    }

    rec_size = rec_get_converted_size(m_index, tuple);
  }

  if (page_loader->is_table_compressed() &&
      page_zip_is_too_big(m_index, tuple)) {
    err = DB_TOO_BIG_RECORD;
  } else {
    err = prepare_space(page_loader, level, rec_size);

    if (err == DB_SUCCESS) {
      IF_ENABLED(
          "ddl_btree_build_too_big_record", static int rec_cnt = 0;

          if (++rec_cnt == 10) {
            rec_cnt = 0;
            if (big_rec != nullptr) {
              dtuple_convert_back_big_rec(tuple, big_rec);
            }
            if (level > 0) {
              page_loader->release();
            }
            return DB_TOO_BIG_RECORD;
          })

      err = insert(page_loader, tuple, big_rec, rec_size);
    }
  }

  if (big_rec != nullptr) {
    dtuple_convert_back_big_rec(tuple, big_rec);
  }
  if (level > 0) {
    page_loader->release();
  }
  return err;
}

dberr_t Btree_load::finalize_page_loads(dberr_t err,
                                        page_no_t &last_page_no) noexcept {
  ut_a(last_page_no == FIL_NULL);
  ut_a(m_root_level + 1 == m_page_loaders.size());

  /* Finish all page bulks */
  for (size_t level = 0; level <= m_root_level; level++) {
    auto page_loader = m_page_loaders[level];
    if (level > 0) {
      page_loader->latch();
    }
    page_loader->finish();

    last_page_no = page_loader->get_page_no();

    if (err == DB_SUCCESS) {
      err = page_commit(page_loader, nullptr, level != m_root_level);
    }

    if (err != DB_SUCCESS) {
      page_loader->rollback();
    }

    ut::delete_(page_loader);
  }

  return err;
}

dberr_t Btree_load::load_root_page(page_no_t last_page_no) noexcept {
  ut_ad(last_page_no != FIL_NULL);

  page_id_t page_id(dict_index_get_space(m_index), last_page_no);
  page_size_t page_size(dict_table_page_size(m_index->table));
  page_no_t page_no = dict_index_get_page(m_index);

  Page_load page_loader(m_index, m_trx_id, page_no, m_root_level,
                        m_flush_observer);

  mtr_t mtr;

  mtr.start();
  mtr.x_lock(dict_index_get_lock(m_index), UT_LOCATION_HERE);

  auto last_block = btr_block_get(page_id, page_size, RW_X_LATCH,
                                  UT_LOCATION_HERE, m_index, &mtr);

  auto last_page = buf_block_get_frame(last_block);

  /* Copy last page to root page. */
  auto err = page_loader.init();

  if (err == DB_SUCCESS) {
    page_loader.copy_all(last_page);
    page_loader.finish();

    /* Remove last page. */
    btr_page_free_low(m_index, last_block, m_root_level, &mtr);

    /* Do not flush the last page. */
    last_block->page.m_flush_observer = nullptr;

    mtr.commit();

    err = page_commit(&page_loader, nullptr, false);
    ut_a(err == DB_SUCCESS);
  } else {
    mtr.commit();
  }

  return err;
}

dberr_t Btree_load::finish(dberr_t err) noexcept {
  ut_ad(!m_index->table->is_temporary());

  /* Assert that the index online status has not changed */
  ut_ad(m_index->online_status == m_index_online);

  if (m_page_loaders.empty()) {
    /* The table is empty. The root page of the index tree
    is already in a consistent state. No need to flush. */
    return err;
  }

  page_no_t last_page_no{FIL_NULL};

  err = finalize_page_loads(err, last_page_no);

  if (err == DB_SUCCESS) {
    err = load_root_page(last_page_no);
  }

  ut_d(dict_sync_check check(true));
  ut_ad(!sync_check_iterate(check));

  return err;
}

/** The transaction interrupted check is expensive, we check after this
many rows. */
static constexpr uint64_t TRX_INTERRUPTED_CHECK = 25000;

dberr_t Btree_load::build(Cursor &cursor) noexcept {
  dberr_t err;
  dtuple_t *dtuple{};
  uint64_t interrupt_check{};

  while ((err = cursor.fetch(dtuple)) == DB_SUCCESS) {
    if (cursor.duplicates_detected()) {
      err = DB_DUPLICATE_KEY;
      break;
    }

    err = insert(dtuple, 0);

    if (err != DB_SUCCESS) {
      return err;
    }

    err = cursor.next();

    if (err != DB_SUCCESS) {
      break;
    }

    ++m_n_recs;

    IF_ENABLED("ddl_btree_load_interrupt",
               interrupt_check = TRX_INTERRUPTED_CHECK;);

    if (!(interrupt_check++ % TRX_INTERRUPTED_CHECK) &&
        m_flush_observer->check_interrupted()) {
      err = DB_INTERRUPTED;
      break;
    }
  }
  return err == DB_END_OF_INDEX ? DB_SUCCESS : err;
}
