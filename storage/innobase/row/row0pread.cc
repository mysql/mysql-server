/*****************************************************************************

Copyright (c) 2018, 2019, Oracle and/or its affiliates. All Rights Reserved.

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

#include <array>

#include "btr0pcur.h"
#include "dict0dict.h"
#include "os0thread-create.h"
#include "row0mysql.h"
#include "row0pread.h"
#include "row0row.h"
#include "row0vers.h"
#include "ut0new.h"

#ifdef UNIV_PFS_THREAD
mysql_pfs_key_t parallel_read_thread_key;
mysql_pfs_key_t parallel_read_ahead_thread_key;
#endif /* UNIV_PFS_THREAD */

std::atomic_size_t Parallel_reader::s_active_threads{};

/** Tree depth at which we decide to split blocks further. */
static constexpr size_t SPLIT_THRESHOLD{3};

/** Size of the read ahead request queue. */
static constexpr size_t MAX_READ_AHEAD_REQUESTS{128};

/** Maximum number of read ahead threads to spawn. Partitioned tables
can have 1000s of partitions. We don't want to spawn dedicated threads
per scan context. */
constexpr static size_t MAX_READ_AHEAD_THREADS{2};

std::string Parallel_reader::Scan_range::to_string() const {
  std::ostringstream os;

  os << "m_start: ";
  if (m_start != nullptr) {
    m_start->print(os);
  } else {
    os << "null";
  }
  os << ", m_end: ";
  if (m_end != nullptr) {
    m_end->print(os);
  } else {
    os << "null";
  }
  return (os.str());
}

Parallel_reader::Scan_ctx::Iter::~Iter() {
  if (m_heap == nullptr) {
    return;
  }

  if (m_pcur != nullptr) {
    m_pcur->free_rec_buf();
    /* Created with placement new on the heap. */
    call_destructor(m_pcur);
  }

  mem_heap_free(m_heap);
  m_heap = nullptr;
}

Parallel_reader::Ctx::~Ctx() {}
Parallel_reader::Scan_ctx::~Scan_ctx() {}

Parallel_reader::~Parallel_reader() {
  mutex_destroy(&m_mutex);
  os_event_destroy(m_event);
  release_unused_threads(m_max_threads);
}

size_t Parallel_reader::available_threads(size_t n_required) {
  const auto RELAXED = std::memory_order_relaxed;
  auto active = s_active_threads.fetch_add(n_required, RELAXED);

  if (active < MAX_THREADS) {
    const auto available = MAX_THREADS - active;

    if (n_required <= available) {
      return (n_required);
    } else {
      s_active_threads.fetch_sub(n_required - available, RELAXED);
      return (available);
    }
  }

  s_active_threads.fetch_sub(n_required, RELAXED);

  return (0);
}

void Parallel_reader::Scan_ctx::index_s_lock() {
  if (m_s_locks.fetch_add(1, std::memory_order_acquire) == 0) {
    auto index = m_config.m_index;
    /* The latch can be unlocked by a thread that didn't originally lock it. */
    rw_lock_s_lock_gen(dict_index_get_lock(index), true);
  }
}

void Parallel_reader::Scan_ctx::index_s_unlock() {
  if (m_s_locks.fetch_sub(1, std::memory_order_acquire) == 1) {
    auto index = m_config.m_index;
    /* The latch can be unlocked by a thread that didn't originally lock it. */
    rw_lock_s_unlock_gen(dict_index_get_lock(index), true);
  }
}

dberr_t Parallel_reader::Ctx::split() {
  ut_ad(m_range.first->m_tuple == nullptr ||
        dtuple_validate(m_range.first->m_tuple));
  ut_ad(m_range.second->m_tuple == nullptr ||
        dtuple_validate(m_range.second->m_tuple));

  /* Setup the sub-range. */
  Scan_range scan_range(m_range.first->m_tuple, m_range.second->m_tuple);

  /* S lock so that the tree structure doesn't change while we are
  figuring out the sub-trees to scan. */
  m_scan_ctx->index_s_lock();

  auto ranges = m_scan_ctx->partition(scan_range, 1);

  if (!ranges.empty()) {
    ranges.back().second = m_range.second;
  }

  /* Create the partitioned scan execution contexts. */
  for (auto &range : ranges) {
    auto err = m_scan_ctx->create_context(range, false);

    if (err != DB_SUCCESS) {
      m_scan_ctx->index_s_unlock();
      return (err);
    }
  }

  m_scan_ctx->index_s_unlock();

  return (DB_SUCCESS);
}

Parallel_reader::Parallel_reader(size_t max_threads)
    : m_max_threads(max_threads),
      m_ctxs(),
      m_read_aheadq(ut_2_power_up(MAX_READ_AHEAD_REQUESTS)) {
  m_n_completed = 0;

  mutex_create(LATCH_ID_PARALLEL_READ, &m_mutex);

  m_event = os_event_create("Parallel reader");
}

Parallel_reader::Scan_ctx::Scan_ctx(Parallel_reader *reader, size_t id,
                                    trx_t *trx,
                                    const Parallel_reader::Config &config,
                                    F &&f)
    : m_id(id), m_config(config), m_trx(trx), m_f(f), m_reader(reader) {}

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

buf_block_t *Parallel_reader::Scan_ctx::block_get_s_latched(
    const page_id_t &page_id, mtr_t *mtr, int line) const {
  auto block = buf_page_get_gen(page_id, m_config.m_page_size, RW_S_LATCH,
                                nullptr, Page_fetch::SCAN, __FILE__, line, mtr);

  buf_block_dbg_add_level(block, SYNC_TREE_NODE);

  return (block);
}

void PCursor::yield() {
  /* We should always yield on a block boundary. */
  ut_ad(m_pcur->is_after_last_on_page());

  /* Store the cursor position on the last user record on the page. */
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
    /* There are waiters on the index tree lock. Store and restore
    the cursor position, and yield so that scanning a large table
    will not starve other threads. */

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

  block = buf_page_get_gen(page_id_t(page_id.space(), next_page_no),
                           block->page.size, RW_S_LATCH, nullptr,
                           Page_fetch::SCAN, __FILE__, __LINE__, m_mtr);

  buf_block_dbg_add_level(block, SYNC_TREE_NODE);

  btr_leaf_page_release(page_cur_get_block(cur), RW_S_LATCH, m_mtr);

  page_cur_set_before_first(block, cur);

  /* Skip the infimum record. */
  page_cur_move_to_next(cur);

  /* Page can't be empty unless it is a root page. */
  ut_ad(!page_cur_is_after_last(cur));

  return (DB_SUCCESS);
}

bool Parallel_reader::Scan_ctx::check_visibility(const rec_t *&rec,
                                                 ulint *&offsets,
                                                 mem_heap_t *&heap,
                                                 mtr_t *mtr) {
  const auto table_name = m_config.m_index->table->name;

  ut_ad(m_trx->read_view == nullptr || MVCC::is_view_active(m_trx->read_view));

  if (m_trx->read_view != nullptr) {
    auto view = m_trx->read_view;

    if (m_config.m_index->is_clustered()) {
      trx_id_t rec_trx_id;

      if (m_config.m_index->trx_id_offset > 0) {
        rec_trx_id = trx_read_trx_id(rec + m_config.m_index->trx_id_offset);
      } else {
        rec_trx_id = row_get_rec_trx_id(rec, m_config.m_index, offsets);
      }

      if (m_trx->isolation_level > TRX_ISO_READ_UNCOMMITTED &&
          !view->changes_visible(rec_trx_id, table_name)) {
        rec_t *old_vers;

        row_vers_build_for_consistent_read(rec, mtr, m_config.m_index, &offsets,
                                           view, &heap, heap, &old_vers,
                                           nullptr, nullptr);

        rec = old_vers;

        if (rec == nullptr) {
          return (false);
        }
      }
    } else {
      /* Secondary index scan not supported yet. */
      ut_error;

      auto max_trx_id = page_get_max_trx_id(page_align(rec));

      ut_ad(max_trx_id > 0);

      if (!view->sees(max_trx_id)) {
        /* FIXME: This is not sufficient. We may need to read in the cluster
        index record to be 100% sure. */
        return (false);
      }
    }
  }

  if (rec_get_deleted_flag(rec, m_config.m_is_compact)) {
    /* This record was deleted in the latest committed version, or it was
    deleted and then reinserted-by-update before purge kicked in. Skip it. */
    return (false);
  }

  ut_ad(m_trx->isolation_level == TRX_ISO_READ_UNCOMMITTED ||
        !rec_offs_any_null_extern(rec, offsets));

  return (true);
}

void Parallel_reader::Scan_ctx::copy_row(const rec_t *rec, Iter *iter) const {
  iter->m_offsets = rec_get_offsets(rec, m_config.m_index, nullptr,
                                    ULINT_UNDEFINED, &iter->m_heap);

  /* Copy the row from the page to the scan iterator. The copy should use
  memory from the iterator heap because the scan iterator owns the copy. */
  auto rec_len = rec_offs_size(iter->m_offsets);

  auto copy_rec = static_cast<rec_t *>(mem_heap_alloc(iter->m_heap, rec_len));

  memcpy(copy_rec, rec, rec_len);

  iter->m_rec = copy_rec;

  auto tuple =
      row_rec_to_index_entry_low(iter->m_rec, m_config.m_index, iter->m_offsets,
                                 &iter->m_n_ext, iter->m_heap);

  ut_ad(dtuple_validate(tuple));

  /* We have copied the entire record but we only need to compare the
  key columns when we check for boundary conditions. */
  const auto n_compare = dict_index_get_n_unique_in_tree(m_config.m_index);

  dtuple_set_n_fields_cmp(tuple, n_compare);

  iter->m_tuple = tuple;
}

std::shared_ptr<Parallel_reader::Scan_ctx::Iter>
Parallel_reader::Scan_ctx::create_persistent_cursor(
    const page_cur_t &page_cursor, mtr_t *mtr) const {
  ut_ad(index_s_own());

  std::shared_ptr<Iter> iter = std::make_shared<Iter>();

  iter->m_heap = mem_heap_create(sizeof(btr_pcur_t) + (srv_page_size / 16));

  ut_a(page_is_leaf(buf_block_get_frame(page_cursor.block)));

  auto rec = page_cursor.rec;

  const bool is_infimum = page_rec_is_infimum(rec);

  if (is_infimum) {
    rec = page_rec_get_next(rec);
  }

  if (page_rec_is_supremum(rec)) {
    /* Empty page, only root page can be empty. */
    ut_a(!is_infimum ||
         page_cursor.block->page.id.page_no() == m_config.m_index->page);
    return (iter);
  }

  void *ptr = mem_heap_alloc(iter->m_heap, sizeof(btr_pcur_t));

  ::new (ptr) btr_pcur_t();

  iter->m_pcur = reinterpret_cast<btr_pcur_t *>(ptr);

  iter->m_pcur->init();

  /* Make a copy of the rec. */
  copy_row(rec, iter.get());

  iter->m_pcur->open_on_user_rec(page_cursor, PAGE_CUR_GE,
                                 BTR_ALREADY_S_LATCHED | BTR_SEARCH_LEAF);

  iter->m_pcur->store_position(mtr);
  iter->m_pcur->set_fetch_type(Page_fetch::SCAN);

  return (iter);
}

dberr_t Parallel_reader::Ctx::traverse() {
  mtr_t mtr;

  auto &from = m_range.first;

  const auto &end_tuple = m_range.second->m_tuple;

  PCursor pcursor(from->m_pcur, &mtr);

  ulint offsets_[REC_OFFS_NORMAL_SIZE];

  ulint *offsets = offsets_;

  rec_offs_init(offsets_);

  mtr.start();

  mtr.set_log_mode(MTR_LOG_NO_REDO);

  auto heap = mem_heap_create(srv_page_size / 4);

  pcursor.restore_position();

  dberr_t err{DB_SUCCESS};

  m_start = true;

  auto index = m_scan_ctx->m_config.m_index;

  for (;;) {
    auto pcur = from->m_pcur;
    auto cur = pcur->get_page_cur();

    if (page_cur_is_after_last(cur)) {
      mem_heap_empty(heap);

      offsets = offsets_;
      rec_offs_init(offsets_);

      if (m_scan_ctx->m_config.m_read_ahead) {
        auto next_page_no = btr_page_get_next(page_cur_get_page(cur), &mtr);

        if (next_page_no != FIL_NULL && !(next_page_no % FSP_EXTENT_SIZE)) {
          m_scan_ctx->submit_read_ahead(next_page_no);
        }
      }

      err = pcursor.move_to_next_block(index);

      if (err != DB_SUCCESS) {
        ut_ad(err == DB_END_OF_INDEX);
        err = DB_SUCCESS;
        break;
      }

      ut_ad(!page_cur_is_before_first(cur));
    }

    const rec_t *rec = page_cur_get_rec(cur);

    offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED, &heap);

    auto skip = !m_scan_ctx->check_visibility(rec, offsets, heap, &mtr);

    m_block = page_cur_get_block(cur);

    if (rec != nullptr && end_tuple != nullptr) {
      auto ret = end_tuple->compare(rec, index, offsets);

      /* Note: The range creation doesn't use MVCC. Therefore it's possible
      that the range boundary entry could have been deleted. */
      if (ret <= 0) {
        mtr.commit();
        break;
      }
    }

    if (!skip) {
      m_rec = rec;
      err = m_scan_ctx->m_f(this);
      m_start = false;
    }

    page_cur_move_to_next(cur);

    if (err != DB_SUCCESS) {
      mtr.commit();
      break;
    }
  }

  ut_a(!mtr.is_active());

  mem_heap_free(heap);

  return (err);
}

void Parallel_reader::enqueue(std::shared_ptr<Ctx> ctx) {
  mutex_enter(&m_mutex);
  m_ctxs.push_back(ctx);
  mutex_exit(&m_mutex);
}

std::shared_ptr<Parallel_reader::Ctx> Parallel_reader::dequeue() {
  mutex_enter(&m_mutex);

  if (m_ctxs.empty()) {
    mutex_exit(&m_mutex);
    return (nullptr);
  }

  auto ctx = m_ctxs.front();
  m_ctxs.pop_front();

  mutex_exit(&m_mutex);

  return (ctx);
}

bool Parallel_reader::is_queue_empty() const {
  mutex_enter(&m_mutex);
  auto empty = m_ctxs.empty();
  mutex_exit(&m_mutex);
  return (empty);
}

void Parallel_reader::worker(size_t thread_id) {
  dberr_t err{DB_SUCCESS};

  if (m_start_callback) {
    err = m_start_callback(thread_id);
  }

  for (;;) {
    size_t n_completed = 0;
    int64_t sig_count = os_event_reset(m_event);

    while (err == DB_SUCCESS && !is_error_set()) {
      auto ctx = dequeue();

      if (ctx == nullptr) {
        break;
      }

      auto scan_ctx = ctx->m_scan_ctx;

      if (scan_ctx->is_error_set()) {
        break;
      }

      ctx->m_thread_id = thread_id;

      if (ctx->m_split) {
        err = ctx->split();
        /* Tell the other threads that there is work to do. */
        os_event_set(m_event);
      } else {
        err = ctx->traverse();
      }

      ++n_completed;
    }

    if (err != DB_SUCCESS || is_error_set()) {
      break;
    }

    m_n_completed.fetch_add(n_completed, std::memory_order_relaxed);

    if (m_n_completed == m_ctx_id) {
      /* Wakeup other worker threads before exiting */
      os_event_set(m_event);
      break;
    }

    constexpr auto FOREVER = OS_SYNC_INFINITE_TIME;

    os_event_wait_time_low(m_event, FOREVER, sig_count);
  }

  if (m_finish_callback) {
    err = m_finish_callback(thread_id);
  }

  if (err != DB_SUCCESS) {
    /* Set the "global" error state. */
    if (!is_error_set()) {
      set_error_state(err);
    }

    /* Wake up any sleeping threads. */
    os_event_set(m_event);
  }

  ut_a(err != DB_SUCCESS || is_error_set() ||
       (m_n_completed == m_ctx_id && is_queue_empty()));
}

page_no_t Parallel_reader::Scan_ctx::search(const buf_block_t *block,
                                            const dtuple_t *key) const {
  ut_ad(index_s_own());

  page_cur_t page_cursor;
  const auto index = m_config.m_index;

  if (key != nullptr) {
    page_cur_search(block, index, key, PAGE_CUR_LE, &page_cursor);
  } else {
    page_cur_set_before_first(block, &page_cursor);
  }

  if (page_rec_is_infimum(page_cur_get_rec(&page_cursor))) {
    page_cur_move_to_next(&page_cursor);
  }

  const auto rec = page_cur_get_rec(&page_cursor);

  mem_heap_t *heap = nullptr;

  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  auto offsets = offsets_;

  rec_offs_init(offsets_);

  offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED, &heap);

  auto page_no = btr_node_ptr_get_child_page_no(rec, offsets);

  if (heap != nullptr) {
    mem_heap_free(heap);
  }

  return (page_no);
}

page_cur_t Parallel_reader::Scan_ctx::start_range(
    page_no_t page_no, mtr_t *mtr, const dtuple_t *key,
    Savepoints &savepoints) const {
  ut_ad(index_s_own());

  auto index = m_config.m_index;
  page_id_t page_id(index->space, page_no);

  /* Follow the left most pointer down on each page. */
  for (;;) {
    auto savepoint = mtr->get_savepoint();

    auto block = block_get_s_latched(page_id, mtr, __LINE__);

    savepoints.push_back({savepoint, block});

    if (!page_is_leaf(buf_block_get_frame(block))) {
      page_id.set_page_no(search(block, key));
      continue;
    }

    page_cur_t page_cursor;

    if (key != nullptr) {
      page_cur_search(block, index, key, PAGE_CUR_GE, &page_cursor);
    } else {
      page_cur_set_before_first(block, &page_cursor);
    }

    if (page_rec_is_infimum(page_cur_get_rec(&page_cursor))) {
      page_cur_move_to_next(&page_cursor);
    }

    ut_a(!page_cur_is_after_last(&page_cursor));

    return (page_cursor);
  }

  ut_error;

  return (page_cur_t{});
}

void Parallel_reader::Scan_ctx::create_range(Ranges &ranges,
                                             page_cur_t &leaf_page_cursor,
                                             mtr_t *mtr) const {
  leaf_page_cursor.index = m_config.m_index;

  auto iter = create_persistent_cursor(leaf_page_cursor, mtr);

  /* Setup the previous range (next) to point to the current range. */
  if (!ranges.empty()) {
    ut_a(ranges.back().second->m_heap == nullptr);
    ranges.back().second = iter;
  }

  ranges.push_back(Range(iter, std::make_shared<Iter>()));
}

void Parallel_reader::Scan_ctx::create_ranges(const Scan_range &scan_range,
                                              page_no_t page_no, size_t depth,
                                              const size_t level,
                                              Ranges &ranges, mtr_t *mtr) {
  ut_ad(index_s_own());
  ut_a(max_threads() > 0);
  ut_a(page_no != FIL_NULL);

  /* Do a breadth first traversal of the B+Tree using recursion. We want to
  set up the scan ranges in one pass. This guarantees that the tree structure
  cannot change while we are creating the scan sub-ranges.

  Once we create the persistent cursor (Range) for a sub-tree we can release
  the latches on all blocks traversed for that sub-tree. */

  const auto index = m_config.m_index;

  page_id_t page_id(index->space, page_no);

  Savepoint savepoint({mtr->get_savepoint(), nullptr});

  auto block = block_get_s_latched(page_id, mtr, __LINE__);

  savepoint.second = block;

  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  auto offsets = offsets_;

  rec_offs_init(offsets_);

  page_cur_t page_cursor;

  page_cursor.index = index;

  auto start = scan_range.m_start;

  if (start != nullptr) {
    page_cur_search(block, index, start, PAGE_CUR_LE, &page_cursor);

    if (page_cur_is_after_last(&page_cursor)) {
      return;
    } else if (page_rec_is_infimum(page_cur_get_rec(&page_cursor))) {
      page_cur_move_to_next(&page_cursor);
    }
  } else {
    page_cur_set_before_first(block, &page_cursor);
    /* Skip the infimum record. */
    page_cur_move_to_next(&page_cursor);
  }

  mem_heap_t *heap{};

  const auto at_leaf = page_is_leaf(buf_block_get_frame(block));

  Savepoints savepoints{};

  while (!page_cur_is_after_last(&page_cursor)) {
    const auto rec = page_cur_get_rec(&page_cursor);

    ut_a(at_leaf || rec_get_node_ptr_flag(rec) ||
         !dict_table_is_comp(index->table));

    if (heap == nullptr) {
      heap = mem_heap_create(srv_page_size / 4);
    }

    offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED, &heap);

    const auto end = scan_range.m_end;

    if (end != nullptr && end->compare(rec, index, offsets) <= 0) {
      break;
    }

    page_cur_t leaf_page_cursor;

    if (!at_leaf) {
      auto page_no = btr_node_ptr_get_child_page_no(rec, offsets);

      if (depth < level) {
        /* Need to create a range starting at a lower level in the tree. */
        create_ranges(scan_range, page_no, depth + 1, level, ranges, mtr);
        page_cur_move_to_next(&page_cursor);
        continue;
      }

      /* Find the range start in the leaf node. */
      leaf_page_cursor = start_range(page_no, mtr, start, savepoints);
    } else {
      if (start != nullptr) {
        page_cur_search(block, index, start, PAGE_CUR_GE, &page_cursor);
        ut_a(!page_rec_is_infimum(page_cur_get_rec(&page_cursor)));
      } else {
        page_cur_set_before_first(block, &page_cursor);

        /* Skip the infimum record. */
        page_cur_move_to_next(&page_cursor);
        ut_a(!page_cur_is_after_last(&page_cursor));
      }

      /* Since we are alread at a leaf node use the current page cursor. */
      memcpy(&leaf_page_cursor, &page_cursor, sizeof(leaf_page_cursor));
    }

    ut_a(page_is_leaf(buf_block_get_frame(leaf_page_cursor.block)));

    if (!page_rec_is_supremum(page_cur_get_rec(&leaf_page_cursor))) {
      create_range(ranges, leaf_page_cursor, mtr);
    }

    /* We've created the persistent cursor, safe to release S latches on
    the blocks that are in this range (sub-tree). */
    for (auto &savepoint : savepoints) {
      mtr->release_block_at_savepoint(savepoint.first, savepoint.second);
    }

    if (m_depth == 0 && depth == 0) {
      m_depth = savepoints.size();
    }

    savepoints.clear();

    if (at_leaf) {
      break;
    }

    start = nullptr;

    page_cur_move_to_next(&page_cursor);
  }

  savepoints.push_back(savepoint);

  for (auto &savepoint : savepoints) {
    mtr->release_block_at_savepoint(savepoint.first, savepoint.second);
  }

  if (heap != nullptr) {
    mem_heap_free(heap);
  }
}

Parallel_reader::Scan_ctx::Ranges Parallel_reader::Scan_ctx::partition(
    const Scan_range &scan_range, size_t level) {
  ut_ad(index_s_own());

  mtr_t mtr;

  mtr.start();

  mtr.set_log_mode(MTR_LOG_NO_REDO);

  Ranges ranges{};

  create_ranges(scan_range, m_config.m_index->page, 0, level, ranges, &mtr);

  if (scan_range.m_end != nullptr && !ranges.empty()) {
    auto &iter = ranges.back().second;

    ut_a(iter->m_heap == nullptr);

    iter->m_heap = mem_heap_create(sizeof(btr_pcur_t) + (srv_page_size / 16));

    iter->m_tuple = dtuple_copy(scan_range.m_end, iter->m_heap);

    /* Do a deep copy. */
    for (size_t i = 0; i < dtuple_get_n_fields(iter->m_tuple); ++i) {
      dfield_dup(&iter->m_tuple->fields[i], iter->m_heap);
    }
  }

  mtr.commit();

  return (ranges);
}

dberr_t Parallel_reader::Scan_ctx::create_context(const Range &range,
                                                  bool split) {
  auto ctx_id = m_reader->m_ctx_id.fetch_add(1, std::memory_order_relaxed);

  // clang-format off

  auto ctx = std::shared_ptr<Ctx>(
      UT_NEW_NOKEY(Ctx(ctx_id, this, range)),
      [](Ctx *ctx) { UT_DELETE(ctx); });

  // clang-format on

  dberr_t err{DB_SUCCESS};

  if (ctx.get() == nullptr) {
    m_reader->m_ctx_id.fetch_sub(1, std::memory_order_relaxed);
    return (DB_OUT_OF_MEMORY);
  } else {
    ctx->m_split = split;
    m_reader->enqueue(ctx);
  }

  return (err);
}

dberr_t Parallel_reader::Scan_ctx::create_contexts(const Ranges &ranges) {
  size_t split_point{};

  ut_a(max_threads() > 0 && max_threads() <= Parallel_reader::MAX_THREADS);

  if (ranges.size() > max_threads()) {
    split_point = (ranges.size() / max_threads()) * max_threads();
  } else if (m_depth < SPLIT_THRESHOLD) {
    /* If the tree is not very deep then don't split. For smaller tables
    it is more expensive to split because we end up traversing more blocks*/
    split_point = max_threads();
  }

  ib::info() << "ranges: " << ranges.size() << " max_threads: " << max_threads()
             << " split: " << split_point << " depth: " << m_depth;

  size_t i{};

  for (auto range : ranges) {
    auto err = create_context(range, i >= split_point);

    if (err != DB_SUCCESS) {
      return (err);
    }

    ++i;
  }

  return (DB_SUCCESS);
}

void Parallel_reader::read_ahead_worker(page_no_t n_pages) {
  DBUG_EXECUTE_IF("bug28079850", set_error_state(DB_INTERRUPTED););

  while (is_active() && !is_error_set()) {
    uint64_t dequeue_count{};

    Read_ahead_request read_ahead_request;

    while (m_read_aheadq.dequeue(read_ahead_request)) {
      auto scan_ctx = read_ahead_request.m_scan_ctx;

      if (trx_is_interrupted(scan_ctx->m_trx)) {
        set_error_state(DB_INTERRUPTED);
        break;
      }

      ut_a(scan_ctx->m_config.m_read_ahead);
      ut_a(read_ahead_request.m_page_no != FIL_NULL);

      page_id_t page_id(scan_ctx->m_config.m_index->space,
                        read_ahead_request.m_page_no);

      buf_phy_read_ahead(page_id, scan_ctx->m_config.m_page_size, n_pages);

      ++dequeue_count;
    }

    m_consumed.fetch_add(dequeue_count, std::memory_order_relaxed);

    while (read_ahead_queue_empty() && is_active() && !is_error_set()) {
      os_thread_sleep(20);
    }
  }
}

void Parallel_reader::read_ahead() {
  ut_a(!m_scan_ctxs.empty());

  auto n_read_ahead_threads =
      std::min(m_scan_ctxs.size(), MAX_READ_AHEAD_THREADS);

  std::vector<IB_thread> threads;

  for (size_t i = 1; i < n_read_ahead_threads; ++i) {
    threads.emplace_back(os_thread_create(parallel_read_ahead_thread_key,
                                          &Parallel_reader::read_ahead_worker,
                                          this, FSP_EXTENT_SIZE));
    threads.back().start();
  }

  read_ahead_worker(FSP_EXTENT_SIZE);

  if (is_error_set()) {
    os_event_set(m_event);
  }

  for (auto &t : threads) {
    t.wait();
  }
}

void Parallel_reader::parallel_read() {
  ut_a(m_max_threads > 0);

  if (m_ctxs.empty()) {
    return;
  }

  std::vector<IB_thread> threads;

  for (size_t i = 0; i < m_max_threads; ++i) {
    threads.emplace_back(os_thread_create(parallel_read_thread_key,
                                          &Parallel_reader::worker, this, i));
    threads.back().start();
  }

  os_event_set(m_event);

  /* Start the read ahead threads. */
  read_ahead();

  for (auto &t : threads) {
    t.wait();
  }
}

dberr_t Parallel_reader::run() {
  if (!m_scan_ctxs.empty()) {
    parallel_read();
  }

  for (auto &scan_ctx : m_scan_ctxs) {
    if (m_err != DB_SUCCESS) {
      return (m_err);
    }
    if (scan_ctx->m_err != DB_SUCCESS) {
      /* Return the state of the first Scan context that is in state ERROR. */
      return (scan_ctx->m_err);
    }
  }

  return (DB_SUCCESS);
}

bool Parallel_reader::add_scan(trx_t *trx,
                               const Parallel_reader::Config &config,
                               Parallel_reader::F &&f) {
  // clang-format off

  auto scan_ctx = std::shared_ptr<Scan_ctx>(
      UT_NEW_NOKEY(Scan_ctx(this, m_scan_ctx_id, trx, config, std::move(f))),
      [](Scan_ctx *scan_ctx) { UT_DELETE(scan_ctx); });

  // clang-format on

  if (scan_ctx.get() == nullptr) {
    ib::error() << "Out of memory";
    return (false);
  }

  m_scan_ctxs.push_back(scan_ctx);

  scan_ctx->index_s_lock();

  ++m_scan_ctx_id;

  /* Split at the root node (level == 0). */
  auto ranges = scan_ctx->partition(config.m_scan_range, 0);

  if (ranges.empty()) {
    /* Table is empty. */
    scan_ctx->index_s_unlock();
    return (true);
  }

  auto err = scan_ctx->create_contexts(ranges);

  scan_ctx->index_s_unlock();

  return (err == DB_SUCCESS);
}
