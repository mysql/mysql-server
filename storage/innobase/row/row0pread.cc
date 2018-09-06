/*****************************************************************************

Copyright (c) 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file row/row0pread.cc
Parallel read implementation

Created 2018-01-27 by Sunny Bains */

#include <vector>

#include "btr0pcur.h"
#include "dict0dict.h"
#include "row0pread.h"
#include "row0row.h"
#include "row0vers.h"
#include "ut0new.h"

/** Context. */
template <typename T, typename R>
struct Reader<T, R>::Ctx {
  /** Constructor.
  @param[in]    id      Thread ID.
  @param[in]    range   Range that the thread has to read. */
  Ctx(size_t id, const Range &range) : m_id(id), m_range(range) {}

  /** Destructor. */
  ~Ctx();

  /** Destroy the persistent cursor. */
  static void destroy(R &row);

  /** Context ID. */
  size_t m_id{std::numeric_limits<size_t>::max()};

  /** Error during parallel read. */
  dberr_t m_err{DB_SUCCESS};

  /** Range to read in this contxt. */
  Range m_range{};
};

// Doxygen gets confused by the explicit specializations.

//! @cond

template <>
void Reader<Key_reader, Key_reader_row>::Ctx::destroy(Key_reader_row &row) {
  if (row.m_heap != nullptr) {
    if (row.m_pcur != nullptr) {
      row.m_pcur->free_rec_buf();
      /* Created with placement new on the heap. */
      call_destructor(row.m_pcur);
    }
    mem_heap_free(row.m_heap);
    row.m_heap = nullptr;
  }
}

/** Specialised destructor for Key_reader_row. */
template <>
Reader<Key_reader, Key_reader_row>::Ctx::~Ctx() {
  destroy(m_range.second);
}

/** Specialised destructor for page_no_t. */
template <>
Reader<Phy_reader, page_no_t>::Ctx::~Ctx() {}

template <>
void Reader<Phy_reader, page_no_t>::Ctx::destroy(page_no_t &) {}

//! @endcond

/** Persistent cursor wrapper around btr_pcur_t */
class PCursor {
 public:
  /** Constructor.
  @param[in,out]  pcur  Persistent cursor in use.
  @param[in]      mtr   Mini transaction used by the persistent cursor. */
  PCursor(btr_pcur_t *pcur, mtr_t *mtr) : m_mtr(mtr), m_pcur(pcur) {}

  /** Check if are threads waiting on the index latch. Yield the latch
  so that other threads can progress. */
  void yield();

  /** Move to the next block.
  @param[in]  index  Index being traversed.
  @return DB_SUCCESS or error code. */
  dberr_t move_to_next_block(dict_index_t *index)
      MY_ATTRIBUTE((warn_unused_result));

  /** Restore the cursor position. */
  void restore_position() {
    auto relative = m_pcur->m_rel_pos;

    auto equal =
        m_pcur->restore_position(BTR_SEARCH_LEAF, m_mtr, __FILE__, __LINE__);

    if (relative == BTR_PCUR_ON) {
      if (!equal) {
        page_cur_move_to_next(m_pcur->get_page_cur());
      }
    } else {
      ut_ad(relative == BTR_PCUR_AFTER ||
            relative == BTR_PCUR_AFTER_LAST_IN_TREE);
    }
  }

 private:
  /** Mini transaction. */
  mtr_t *m_mtr{};

  /** Persistent cursor. */
  btr_pcur_t *m_pcur{};
};

template <typename T, typename R>
Reader<T, R>::Reader(dict_table_t *table, trx_t *trx, dict_index_t *index,
                     size_t n_threads)
    : m_table(table),
      m_index(index),
      m_is_compact(dict_table_is_comp(table)),
      m_trx(trx),
      m_page_size(dict_tf_to_fsp_flags(table->flags)),
      m_n_threads(n_threads) {
  m_n_completed = 0;

  m_event = os_event_create("Parallel reader");
#ifdef UNIV_DEBUG
  bool found = false;

  for (auto index = UT_LIST_GET_FIRST(m_table->indexes); index != nullptr;
       index = UT_LIST_GET_NEXT(indexes, index)) {
    if (index == m_index) {
      found = true;
      break;
    }
  }

  ut_ad(found);
#endif /* UNIV_DEBUG */
}

template <typename T, typename R>
Reader<T, R>::~Reader() {
  os_event_destroy(m_event);
}

Key_reader::~Key_reader() {}

Phy_reader::~Phy_reader() {}

template <typename T, typename R>
buf_block_t *Reader<T, R>::block_get_s_latched(const page_id_t &page_id,
                                               mtr_t *mtr) const {
  return (btr_block_get(page_id, m_page_size, RW_S_LATCH, m_index, mtr));
}

page_no_t Phy_reader::iterate_recs(size_t id, Phy_reader::Ctx &ctx, mtr_t *mtr,
                                   const buf_block_t *block, Phy_reader::F &f) {
  ut_ad(page_is_leaf(buf_block_get_frame(block)));

  page_cur_t cursor;

  page_cur_set_before_first(block, &cursor);

  /* Skip the infimum record. */
  page_cur_move_to_next(&cursor);

  for (auto rec = page_cur_get_rec(&cursor); !page_rec_is_supremum(rec);
       rec = page_cur_get_rec(&cursor)) {
    if (!rec_get_deleted_flag(rec, m_is_compact)) {
      ctx.m_err = f(id, block, rec);

      if (ctx.m_err != DB_SUCCESS) {
        return (FIL_NULL);
      }
    }

    page_cur_move_to_next(&cursor);
  }

  return (btr_page_get_next(page_cur_get_page(&cursor), mtr));
}

dberr_t Phy_reader::traverse(size_t id, Phy_reader::Ctx &ctx,
                             Phy_reader::F &f) {
  const auto end = ctx.m_range.second;
  page_id_t page_id(m_index->space, ctx.m_range.first);

  mtr_t mtr;

  while (page_id.page_no() != end && page_id.page_no() != FIL_NULL) {
    mtr.start();

    mtr.set_log_mode(MTR_LOG_NO_REDO);

    auto block = block_get_s_latched(page_id, &mtr);

    ut_ad(page_is_leaf(buf_block_get_frame(block)));

    page_id.set_page_no(iterate_recs(id, ctx, &mtr, block, f));

    mtr.commit();
  }

  return (ctx.m_err);
}

Phy_reader::Ranges Phy_reader::create_ranges(
    const Phy_reader::Subtrees &subtrees) {
  Ranges ranges;

  for (size_t i = 0; i < subtrees.size(); ++i) {
    auto leaf = subtrees[i].back();

    if (i < subtrees.size() - 1) {
      auto next = subtrees[i + 1].back();

      ranges.push_back(Range(leaf, next));
    } else {
      ranges.push_back(Range(leaf, FIL_NULL));
    }
  }

  return (ranges);
}

void PCursor::yield() {
  /* We should always yield on a block boundary. */
  ut_ad(m_pcur->is_after_last_on_page());

  /* Store the cursor position on the last user record on
  the page. */

  m_pcur->move_to_prev_on_page();

  m_pcur->store_position(m_mtr);

  m_mtr->commit();

  /* Yield so that another thread can proceed. */
  os_thread_yield();

  m_mtr->start();

  m_mtr->set_log_mode(MTR_LOG_NO_REDO);

  /* Restore position on the record, or its predecessor if the record
  was purged meanwhile. */

  restore_position();

  if (!m_pcur->is_after_last_on_page()) {
    /* Move to the successor of the saved record. */
    m_pcur->move_to_next_on_page();
  }
}

dberr_t PCursor::move_to_next_block(dict_index_t *index) {
  ut_ad(m_pcur->is_after_last_on_page());

  if (rw_lock_get_waiters(dict_index_get_lock(index))) {
    /* There are waiters on the clustered index tree lock. Store and restore
    the cursor position, and yield so that scanning a large table will not
    starve other threads. */

    yield();

    /* It's possible that the restore places the cursor in the middle of
    the block. We need to account for that too. */

    if (m_pcur->is_on_user_rec()) {
      return (DB_SUCCESS);
    }
  }

  auto cur = m_pcur->get_page_cur();
  auto next_page_no = btr_page_get_next(page_cur_get_page(cur), m_mtr);

  if (next_page_no == FIL_NULL) {
    m_mtr->commit();

    return (DB_END_OF_INDEX);
  }

  auto block = page_cur_get_block(cur);
  const auto &page_id = block->page.id;

  block = btr_block_get(page_id_t(page_id.space(), next_page_no),
                        block->page.size, BTR_SEARCH_LEAF, index, m_mtr);

  btr_leaf_page_release(page_cur_get_block(cur), BTR_SEARCH_LEAF, m_mtr);

  page_cur_set_before_first(block, cur);

  /* Skip the infimum record. */
  page_cur_move_to_next(cur);

  /* Page can't be empty unless it is a root page. */
  ut_ad(!page_cur_is_after_last(cur));

  return (DB_SUCCESS);
}

bool Key_reader::check_visibility(const rec_t *&rec, ulint *&offsets,
                                  mem_heap_t *&heap, mtr_t *mtr) {
  ut_ad(m_trx->read_view == nullptr || MVCC::is_view_active(m_trx->read_view));

  if (m_trx->read_view != nullptr) {
    auto view = m_trx->read_view;

    if (m_index->is_clustered()) {
      trx_id_t rec_trx_id;

      if (m_index->trx_id_offset > 0) {
        rec_trx_id = trx_read_trx_id(rec + m_index->trx_id_offset);
      } else {
        rec_trx_id = row_get_rec_trx_id(rec, m_index, offsets);
      }

      if (m_trx->isolation_level > TRX_ISO_READ_UNCOMMITTED &&
          !view->changes_visible(rec_trx_id, m_table->name)) {
        rec_t *old_vers;

        row_vers_build_for_consistent_read(rec, mtr, m_index, &offsets, view,
                                           &heap, heap, &old_vers, nullptr,
                                           nullptr);

        rec = old_vers;

        if (rec == nullptr) {
          return (false);
        }
      }
    } else {
      trx_id_t max_trx_id = page_get_max_trx_id(page_align(rec));

      ut_ad(max_trx_id > 0);

      if (!view->sees(max_trx_id)) {
        return (false);
      }
    }
  }

  if (rec_get_deleted_flag(rec, m_is_compact)) {
    /* This record was deleted in the latest committed version, or it was
    deleted and then reinserted-by-update before purge kicked in. Skip it. */

    return (false);
  }

  ut_ad(!rec_offs_any_null_extern(rec, offsets));

  return (true);
}

void Key_reader::build_row(const rec_t *rec, Key_reader::Row &row, bool copy) {
  auto heap = row.m_heap;

  row.m_offsets =
      rec_get_offsets(rec, m_index, nullptr, ULINT_UNDEFINED, &row.m_heap);

  auto rec_len = rec_offs_size(row.m_offsets);

  if (copy) {
    rec_t *copy_rec;
    copy_rec = static_cast<rec_t *>(mem_heap_alloc(heap, rec_len));
    memcpy(copy_rec, rec, rec_len);
    row.m_rec = copy_rec;
  } else {
    row.m_rec = rec;
  }

  auto tuple = row_rec_to_index_entry_low(row.m_rec, m_index, row.m_offsets,
                                          &row.m_n_ext, heap);

  dtuple_set_n_fields_cmp(tuple, dict_index_get_n_unique_in_tree(m_index));

  row.m_tuple = tuple;
}

Key_reader::Row Key_reader::open_cursor(page_no_t page_no) {
  Row row;

  row.m_heap = mem_heap_create(sizeof(btr_pcur_t) + (srv_page_size / 2));

  row.m_page_no = page_no;

  page_id_t page_id(m_index->space, page_no);

  mtr_t mtr;

  mtr.start();

  mtr.set_log_mode(MTR_LOG_NO_REDO);

  auto block = block_get_s_latched(page_id, &mtr);

  ut_ad(page_is_leaf(buf_block_get_frame(block)));

  page_cur_t cursor;

  page_cur_set_before_first(block, &cursor);

  /* Skip the infimum record. */
  page_cur_move_to_next(&cursor);

  auto rec = page_cur_get_rec(&cursor);

  if (page_rec_is_supremum(rec)) {
    /* Empty page, only root page can be empty. */
    ut_a(block->page.id.page_no() == m_index->page);
    mtr.commit();
    return (row);
  }

  void *ptr = mem_heap_alloc(row.m_heap, sizeof(btr_pcur_t));

  ::new (ptr) btr_pcur_t();

  row.m_pcur = reinterpret_cast<btr_pcur_t *>(ptr);

  row.m_pcur->init();

  row.m_pcur->set_fetch_type(Page_fetch::SCAN);

  /* Make a copy of the rec. */
  build_row(rec, row, true);

  mtr.commit();

  constexpr auto GE = PAGE_CUR_GE;

  ut_ad(rw_lock_own(dict_index_get_lock(m_index), RW_LOCK_SX));

  /* We acquire the index->lock in SX mode in partition(). */
  constexpr auto PAR_SX = BTR_PARALLEL_READ_INIT;

  mtr.start();

  mtr.set_log_mode(MTR_LOG_NO_REDO);

  row.m_pcur->open_on_user_rec(m_index, row.m_tuple, GE, PAR_SX, &mtr, __FILE__,
                               __LINE__);

  row.m_pcur->store_position(&mtr);

  mtr.commit();

  return (row);
}

dberr_t Key_reader::traverse(size_t id, Key_reader::Ctx &ctx,
                             Key_reader::F &f) {
  mtr_t mtr;
  auto &from = ctx.m_range.first;
  const auto &end_tuple = ctx.m_range.second.m_tuple;
  PCursor pcursor(from.m_pcur, &mtr);
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;

  rec_offs_init(offsets_);

  mtr.start();

  mtr.set_log_mode(MTR_LOG_NO_REDO);

  mem_heap_t *heap = mem_heap_create(srv_page_size * 2);

  pcursor.restore_position();

  for (;;) {
    auto pcur = from.m_pcur;
    auto cur = pcur->get_page_cur();

    if (page_cur_is_after_last(cur)) {
      mem_heap_empty(heap);

      ctx.m_err = pcursor.move_to_next_block(m_index);

      if (ctx.m_err != DB_SUCCESS) {
        ut_ad(ctx.m_err == DB_END_OF_INDEX);
        ctx.m_err = DB_SUCCESS;
        break;
      }

      ut_ad(!page_cur_is_before_first(cur));
    }

    const rec_t *rec = page_cur_get_rec(cur);

    offsets = rec_get_offsets(rec, m_index, offsets, ULINT_UNDEFINED, &heap);

    auto skip = !check_visibility(rec, offsets, heap, &mtr);

    auto block = page_cur_get_block(cur);

    if (rec != nullptr && end_tuple != nullptr) {
      auto ret = cmp_dtuple_rec(end_tuple, rec, m_index, offsets);

      /* Note: The range creation doesn't use MVCC. Therefore it's possible
      that the range boundary entry could have been deleted. */
      if (ret <= 0) {
        mtr.commit();
        break;
      }
    }

    if (!skip) {
      ctx.m_err = f(id, block, rec);
    }

    page_cur_move_to_next(cur);

    if (ctx.m_err != DB_SUCCESS) {
      mtr.commit();
      break;
    }
  }

  ut_ad(!mtr.is_active());

  mem_heap_free(heap);

  return (ctx.m_err);
}

Key_reader::Ranges Key_reader::create_ranges(
    const Key_reader::Subtrees &subtrees) {
  ut_ad(rw_lock_own(dict_index_get_lock(m_index), RW_LOCK_SX));

  Ranges ranges;

  /* Create the start cursor. Remember to free this explicitly. */
  auto row = open_cursor(subtrees[0].back());

  if (row.m_pcur == nullptr) {
    Ctx::destroy(row);
    /* Index is empty. */
    return (ranges);
  }

  ranges.push_back(Range(row, Row()));

  for (size_t i = 1; i < subtrees.size(); ++i) {
    auto leaf = subtrees[i].back();

    auto next_row = open_cursor(leaf);

    ranges.back().second = next_row;

    ranges.push_back(Range(next_row, Row()));
  }

  return (ranges);
}

template <typename T, typename R>
void Reader<T, R>::worker(size_t id, Queue &ctxq, Function &f) {
  dberr_t err = DB_SUCCESS;

  for (;;) {
    Ctx *ctx;
    size_t n_completed = 0;
    int64_t sig_count = os_event_reset(m_event);

    while (ctxq.dequeue(ctx)) {
      err = static_cast<T *>(this)->traverse(id, *ctx, f);

      ++n_completed;

      if (err != DB_SUCCESS) {
        break;
      }
    }

    if (err != DB_SUCCESS) {
      break;
    }

    m_n_completed.fetch_add(n_completed, std::memory_order_relaxed);

    if (m_n_completed.load() == m_ctxs.size()) {
      /* Wakeup other worker threads before exiting */
      os_event_set(m_event);
      break;
    }

    constexpr auto FOREVER = OS_SYNC_INFINITE_TIME;

    os_event_wait_time_low(m_event, FOREVER, sig_count);
  }

  ut_a(err != DB_SUCCESS || m_n_completed == m_ctxs.size());
}

template <typename T, typename R>
page_no_t Reader<T, R>::left_child(const buf_block_t *block) const {
  ut_ad(rw_lock_own(dict_index_get_lock(m_index), RW_LOCK_SX));

  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;

  rec_offs_init(offsets_);

  page_cur_t cur;

  page_cur_set_before_first(block, &cur);

  /* Skip the infimum record. */
  page_cur_move_to_next(&cur);

  const auto rec = page_cur_get_rec(&cur);

  mem_heap_t *heap = nullptr;

  offsets = rec_get_offsets(rec, m_index, offsets, ULINT_UNDEFINED, &heap);

  auto page_no = btr_node_ptr_get_child_page_no(rec, offsets);

  if (heap != nullptr) {
    mem_heap_free(heap);
  }

  return (page_no);
}

template <typename T, typename R>
typename Reader<T, R>::Pages Reader<T, R>::left_leaf(page_no_t page_no,
                                                     mtr_t *mtr) const {
  ut_ad(rw_lock_own(dict_index_get_lock(m_index), RW_LOCK_SX));

  Pages q;
  page_id_t page_id(m_index->space, page_no);

  q.reserve(8);

  /* Follow the left most pointer down on each page. */
  for (;;) {
    auto block = block_get_s_latched(page_id, mtr);

    q.push_back(page_id.page_no());

    if (page_is_leaf(buf_block_get_frame(block))) {
      break;
    }

    page_id.set_page_no(left_child(block));
  }

  return (q);
}

template <typename T, typename R>
void Reader<T, R>::iterate_internal_blocks(page_no_t page_no,
                                           Subtrees &subtrees) const {
  ut_ad(rw_lock_own(dict_index_get_lock(m_index), RW_LOCK_SX));

  page_id_t page_id(m_index->space, page_no);

  /* Scan the entire internal node level.
  Note: The previous page latch can be released before next page latch
  is acquired because:
   1. There is an SX latch on dict_index_t::lock
   2. we are doing read only operations */
  while (page_id.page_no() != FIL_NULL) {
    mtr_t mtr;

    mtr.start();

    auto block = block_get_s_latched(page_id, &mtr);

    ut_ad(!page_is_leaf(buf_block_get_frame(block)));

    page_cur_t cursor;

    page_cur_set_before_first(block, &cursor);

    /* Skip the infimum record. */
    page_cur_move_to_next(&cursor);

    subtrees.push_back(left_leaf(left_child(block), &mtr));

    /* Move the cursor to the next page in the list. */
    page_no_t next;

    next = btr_page_get_next(page_cur_get_page(&cursor), &mtr);

    page_id.set_page_no(next);

    mtr.commit();
  }
}

template <typename T, typename R>
typename Reader<T, R>::Ranges Reader<T, R>::partition() {
  mtr_t mtr;

  rw_lock_sx_lock(dict_index_get_lock(m_index));

  mtr.start();

  mtr.set_log_mode(MTR_LOG_NO_REDO);

  /* Fetch the index root page. */
  auto block = btr_root_block_get(m_index, RW_S_LATCH, &mtr);
  page_no_t page_no = dict_index_get_page(m_index);

  bool is_leaf = page_is_leaf(buf_block_get_frame(block));

  mtr.commit();

  Subtrees subtrees;

  if (is_leaf) {
    /* Single node in the btree. */

    Pages pages;

    pages.push_back(page_no);

    subtrees.push_back(pages);
  } else {
    mtr.start();

    mtr.set_log_mode(MTR_LOG_NO_REDO);

    /* Fetch the start page number of each level. */
    Pages levels = left_leaf(page_no, &mtr);

    mtr.commit();

    /* Find a B-Tree level which has enough sub-trees
    to scan in parallel. */
    for (size_t i = 0; i < levels.size() - 1; ++i) {
      subtrees.clear();

      iterate_internal_blocks(levels[i], subtrees);

      if (subtrees.size() >= m_n_threads) {
        break;
      }
    }
  }

  auto ptr = static_cast<T *>(this)->create_ranges(subtrees);

  rw_lock_sx_unlock(dict_index_get_lock(m_index));

  return (ptr);
}

template <typename T, typename R>
dberr_t Reader<T, R>::read(Function &&f) {
  auto partitions = partition();

  if (partitions.empty()) {
    /* Index is empty. */
    return (DB_SUCCESS);
  }

  ut_ad(m_ctxs.empty());

  size_t id = 0;

  dberr_t err = DB_SUCCESS;

  for (auto range : partitions) {
    m_ctxs.push_back(UT_NEW_NOKEY(Ctx(id, range)));

    if (m_ctxs.back() == nullptr) {
      err = DB_OUT_OF_MEMORY;
      break;
    }

    ++id;
  }

  if (err != DB_SUCCESS) {
    auto &ctx = m_ctxs.front();
    ctx->destroy(ctx->m_range.first);

    for (auto &ctx : m_ctxs) {
      UT_DELETE(ctx);
    }

    return (err);
  }

  m_n_threads = std::min(partitions.size(), m_n_threads);

  Queue ctxq(ut_2_power_up(m_ctxs.size() + 1));

  std::vector<std::thread> threads;

  for (size_t i = 0; i < m_n_threads; ++i) {
    auto worker = &Reader<T, R>::worker;

    threads.emplace_back(worker, this, i, std::ref(ctxq), std::ref(f));
  }

  for (auto &ctx : m_ctxs) {
    while (!ctxq.enqueue(ctx)) {
      os_thread_yield();
    }

    os_event_set(m_event);
  }

  for (auto &t : threads) {
    t.join();
  }

  auto &ctx = m_ctxs.front();
  ctx->destroy(ctx->m_range.first);

  for (auto &ctx : m_ctxs) {
    if (ctx->m_err != DB_SUCCESS) {
      /* Note: We return the error of the last context. We can't return
      multiple values. The expectation is that the callback function has
      the error state per thread. */
      err = ctx->m_err;
    }

    UT_DELETE(ctx);
  }

  m_ctxs.clear();

  return (err);
}

// Explicit specialization.
template class Reader<Phy_reader, page_no_t>;
template class Reader<Key_reader, Key_reader_row>;
