/*****************************************************************************

Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
/** @file btr/btr0mtib.cc

 Multi Threaded Index Build (MTIB) using BUF_BLOCK_MEMORY and dedicated
 Bulk_flusher threads.

 Created 09/Feb/2023 Annamalai Gurusami
 *******************************************************/

#ifndef _WIN32
#include <sys/uio.h>
#endif /* _WIN32 */

#include "arch0arch.h"
#include "btr0btr.h"
#include "btr0cur.h"
#include "btr0mtib.h"
#include "btr0pcur.h"
#include "buf0buddy.h"
#include "ibuf0ibuf.h"
#include "lob0lob.h"
#include "log0chkp.h"
#include "log0log.h"
#include "os0thread-create.h"
#include "page0page.h"
#include "scope_guard.h"
#include "trx0trx.h"
#include "ut0test.h"
#include "ut0ut.h"

namespace Btree_multi {

#ifdef UNIV_DEBUG
static bool g_slow_io_debug = false;
void bulk_load_enable_slow_io_debug() { g_slow_io_debug = true; }
void bulk_load_disable_slow_io_debug() { g_slow_io_debug = false; }
#endif /* UNIV_DEBUG */

void Bulk_flusher::start(space_id_t space_id, size_t flusher_number,
                         size_t queue_size) {
  m_space_id = space_id;
  m_id = flusher_number;
  m_max_queue_size = queue_size;

  std::thread flush_thread([this]() {
    auto pfs_index = static_cast<PSI_thread_seqnum>(m_id);
    Runnable runnable{bulk_flusher_thread_key, pfs_index};
    runnable([this]() { return run(); });
  });
  m_flush_thread = std::move(flush_thread);
}

dberr_t Bulk_flusher::get_error() const {
  std::lock_guard<std::mutex> guard(m_mutex);
  return m_error;
}

void Bulk_flusher::set_error(dberr_t error_code) {
  if (error_code == DB_SUCCESS || is_error()) {
    return;
  }
  std::lock_guard<std::mutex> guard(m_mutex);
  m_is_error.store(true);
  m_error = error_code;
}

Bulk_flusher::~Bulk_flusher() {
  if (m_flush_thread.joinable()) {
    wait_to_stop();
  }
  ut_ad(m_priv_queue.empty());
  {
    std::lock_guard<std::mutex> guard(m_mutex);
    ut_ad(m_queue.empty());
  }
}

void Bulk_flusher::wait_to_stop() {
  ut_ad(m_flush_thread.joinable());
  m_stop = true;
  m_flush_thread.join();
}

void Bulk_flusher::do_work(fil_node_t *node, void *iov, size_t iov_size) {
  for (auto &page_extent : m_priv_queue) {
#ifdef UNIV_DEBUG
    if (g_slow_io_debug) {
      std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
#endif /* UNIV_DEBUG */
    m_pages_flushed += page_extent->used_pages();
    if (!is_error()) {
      auto err = page_extent->flush(node, iov, iov_size);
      set_error(err);
    }
    page_extent->destroy();
    Page_extent::drop(page_extent);
  }
  m_priv_queue.clear();
}

dberr_t Bulk_flusher::check_and_notify() const {
  std::unique_lock lk(m_mutex);

  if (m_is_error.load()) {
    return m_error;
  }
  lk.unlock();
  m_condition.notify_one();
  return DB_SUCCESS;
}

void Bulk_flusher::add(Page_extent *page_extent,
                       std::function<void()> &fn_wait_begin,
                       std::function<void()> &fn_wait_end) {
  const size_t max_queue_size = get_max_queue_size();
  std::unique_lock lk(m_mutex);

  if (m_queue.size() >= max_queue_size) {
    if (fn_wait_begin) {
      fn_wait_begin();
    }
    m_condition.wait(lk, [&]() { return m_queue.size() < max_queue_size; });

    if (fn_wait_end) {
      fn_wait_end();
    }
  }
  m_queue.push_back(page_extent);

  /* If queue is full, wake up the flusher thread. */
  if ((m_queue.size() + 1) >= max_queue_size) {
    lk.unlock();
    m_condition.notify_one();
  }
}

bool Bulk_flusher::is_work_available() {
  bool work_available = false;
  {
    std::lock_guard<std::mutex> guard(m_mutex);
    if (!m_queue.empty()) {
      std::copy(m_queue.begin(), m_queue.end(),
                std::back_inserter(m_priv_queue));
      m_queue.clear();
      ut_ad(m_queue.empty());
      work_available = true;
    }
  }
  m_condition.notify_one();
  return work_available;
}

dberr_t Bulk_flusher::run() {
  /* We only have single file tablespace right now. */
  fil_node_t *file_node = nullptr;
  page_no_t first_page = 0;

  /* We keep the IO state open for the entire duration of bulk flush to avoid
  acquiring the shard mutex frequently. It could be released at some frequency
  in future, if required. */
  auto db_error = fil_prepare_file_for_io(m_space_id, first_page, &file_node);
  bool file_prepared = (db_error == DB_SUCCESS);

  /* Flusher sets the error and continue consuming the pages, waiting for
  stop request in case of an error. */
  set_error(db_error);

  void *iov = nullptr;
  size_t iov_size = 0;

#ifdef UNIV_LINUX
  /* Allocate buffer for vector IO */
  iov_size = FSP_EXTENT_SIZE;
  iov = static_cast<void *>(ut::malloc_withkey(
      UT_NEW_THIS_FILE_PSI_KEY, sizeof(struct iovec) * iov_size));
#endif /* UNIV_LINUX */

  m_n_sleep = 0;
  m_wait_time = std::chrono::microseconds::zero();

  auto consume = [&]() {
    while (is_work_available() || !m_priv_queue.empty()) {
      do_work(file_node, iov, iov_size);
    }
  };

  for (;;) {
    /* Keep consuming till work queue is empty. */
    consume();

    /* Check and exit if asked. */
    if (should_i_stop()) {
      break;
    }
    /* Wait and return back to work. */
    wait();
  }

  /* Consume any left over and exit. */
  consume();

  if (file_prepared) {
    fil_complete_write(m_space_id, file_node);
  }

  if (iov != nullptr) {
    ut::free(iov);
  }
  ut_ad(m_priv_queue.empty());
  info();
  return db_error;
}

void Bulk_flusher::wait() {
  m_n_sleep++;
  auto start_time = std::chrono::steady_clock::now();
  {
    std::unique_lock lk(m_mutex);
    m_condition.wait_for(lk, s_sleep_duration,
                         [&]() { return !m_queue.empty(); });
  }
  auto end_time = std::chrono::steady_clock::now();

  auto time_micro = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);
  m_wait_time += time_micro;
}

#ifdef UNIV_DEBUG
static void check_page(dict_index_t *index, const page_no_t page_no) {
  const page_id_t page_id(index->space, page_no);
  const page_size_t page_size = dict_table_page_size(index->table);
  const bool is_dirty_ok = false;
  buf_page_force_evict(page_id, page_size, is_dirty_ok);

  mtr_t mtr;
  mtr.start();
  mtr.x_lock(dict_index_get_lock(index), UT_LOCATION_HERE);

  auto block = btr_block_get(page_id, page_size, RW_X_LATCH, UT_LOCATION_HERE,
                             index, &mtr);
  const bool check_lsn = true;
  const bool skip_checksum = fsp_is_checksum_disabled(index->space);

  ut_ad(block->get_page_zip() == nullptr);
  auto buf = buf_block_get_frame(block);

  ut_ad(!ut::is_zeros(buf, page_size.physical()));

  auto reporter = BlockReporter(check_lsn, buf, page_size, skip_checksum);
  const bool is_corrupted = reporter.is_corrupted();
  ut_ad(!is_corrupted);

  mtr.commit();
  buf_page_force_evict(page_id, page_size, is_dirty_ok);
}
#endif /* UNIV_DEBUG */

bool Page_load::is_corrupted() const {
  const page_size_t page_size = dict_table_page_size(m_index->table);
  const bool skip_checksum = fsp_is_checksum_disabled(m_index->space);
  const bool check_lsn = true;

  ut_ad(m_block->get_page_zip() == nullptr);
  auto frame = buf_block_get_frame(m_block);
  auto reporter = BlockReporter(check_lsn, frame, page_size, skip_checksum);

  const bool is_corrupted = reporter.is_corrupted();
  ut_ad(!is_corrupted);

  return is_corrupted;
}

void Page_load::init_for_writing() {
  ut_ad(m_block->is_memory());
  ut_ad(m_mtr == nullptr);

  const space_id_t space_id = m_index->space;
  const bool skip_checksum = fsp_is_checksum_disabled(space_id);
  const bool skip_lsn_check = false;
  const lsn_t page_lsn = log_get_lsn(*log_sys);
  auto buf_pool = buf_pool_get(m_block->page.id);

  if (!fsp_is_system_temporary(space_id) && buf_pool->is_tracking()) {
    const bool force = true;
    buf_page_t *bpage = reinterpret_cast<buf_page_t *>(m_block);
    ut_ad(page_lsn >= buf_pool->track_page_lsn);
    arch_page_sys->track_page(bpage, buf_pool->track_page_lsn, page_lsn, force);
  }

  ut_ad(m_block->get_page_zip() == nullptr);
  auto frame = buf_block_get_frame(m_block);

  buf_flush_init_for_writing(m_block, frame, nullptr, page_lsn, skip_checksum,
                             skip_lsn_check);
  ut_ad(!is_corrupted());
}

#ifdef UNIV_LINUX
dberr_t Page_extent::bulk_flush_linux(fil_node_t *node, struct iovec *iov,
                                      size_t iov_size) {
  dberr_t err{DB_SUCCESS};
  const page_no_t n_pages = m_page_loads.size();
  ut_ad(n_pages > 0);

#ifdef UNIV_DEBUG
  const bool is_tpc = m_btree_load->is_tpc_enabled();
  ut_ad(!is_tpc);
#endif /* UNIV_DEBUG */

  ut_ad(iov_size >= n_pages);

  if (iov_size < n_pages) {
    ib::error(ER_BULK_LOADER_INFO,
              "Flush Error: number of pages exceeds extent size");
    return DB_FAIL;
  }

  const size_t page_size = m_page_loads[0]->get_page_size();

  size_t i = 0;
  for (auto &page_load : m_page_loads) {
    ut_ad(page_load->is_memory());
    page_load->init_for_writing();
    auto buf = page_load->get_page();

    iov[i].iov_base = buf;
    ut_ad(iov[i].iov_base != nullptr);
    iov[i].iov_len = page_size; /* Physical page size */

    ut_ad(!ut::is_zeros(iov[i].iov_base, iov[i].iov_len));
#ifdef UNIV_DEBUG
    const page_no_t disk_page_no = mach_read_from_4(buf + FIL_PAGE_OFFSET);
    ut_ad(disk_page_no == page_load->get_page_no());
    m_btree_load->track_page_flush(disk_page_no);
#endif /* UNIV_DEBUG */
    i++;
  }
  page_no_t min_page_no = m_range.first;
  const os_offset_t offset = min_page_no * page_size;
  const ssize_t req_bytes = n_pages * page_size;
  ut_ad(node->is_open);
  ut_a(node != nullptr);

  ssize_t n = pwritev(node->handle.m_file, iov, n_pages, offset);
  if (n != req_bytes) {
    ib::error(ER_INNODB_IO_WRITE_FAILED, node->name);
    err = DB_IO_ERROR;
  }
  ut_ad(n == req_bytes);
  return err;
}
#endif /* UNIV_LINUX */

dberr_t Page_extent::flush_one_by_one(fil_node_t *node) {
  dberr_t err{DB_SUCCESS};

  const space_id_t space_id = m_page_loads[0]->space();
  const dict_index_t *index = m_btree_load->index();

  fil_space_t *space = fil_space_acquire(space_id);
  const bool is_space_encrypted = space->is_encrypted();

  page_no_t page_no = m_range.first;
  ut_ad(node == space->get_file_node(&page_no));
  ut_a(node != nullptr);

  const std::string file_name = node->name;

  IORequest request(IORequest::WRITE);
  request.block_size(node->block_size);

  const size_t physical_page_size = m_page_loads[0]->get_page_size();

  for (auto &page_load : m_page_loads) {
    ut_ad(page_load->is_memory());

    file::Block *compressed_block = nullptr;
    file::Block *e_block = nullptr;

    size_t page_size = physical_page_size;
    page_load->init_for_writing();
    ut_ad(page_load->get_page_no() == page_no);

    const os_offset_t offset = page_no * physical_page_size;

    void *buf = page_load->get_page();
    ut_ad(buf != nullptr);

    ut_ad(!ut::is_zeros(buf, physical_page_size));
    {
      ulint buflen = physical_page_size;
      /* Transparent page compression (TPC) is disabled if punch hole is not
      supported. A similar check is done in Fil_shard::do_io(). */
      const bool do_compression = space->is_compressed() &&
                                  IORequest::is_punch_hole_supported() &&
                                  node->punch_hole;

      if (do_compression) {
        /* @note Compression needs to be done before encryption. */
        /* The page size must be a multiple of the OS punch hole size. */
        ut_ad(buflen % request.block_size() == 0);

        request.compression_algorithm(space->compression_type);
        compressed_block = os_file_compress_page(request, buf, &buflen);
        page_size = buflen;
        ut_ad(page_size <= physical_page_size);
      }

      if (is_space_encrypted) {
        request.get_encryption_info().set(space->m_encryption_metadata);
        e_block = os_file_encrypt_page(request, buf, buflen);
      }
    }

    ut_ad(!ut::is_zeros(buf, page_size));
    ut_a(node->is_open);
    ut_a(node->size >= page_no);

    SyncFileIO sync_file_io(node->handle.m_file, buf, page_size, offset);
    err = sync_file_io.execute_with_retry(request);
    if (err != DB_SUCCESS) {
      break;
    }
#ifdef UNIV_DEBUG
    if (err == DB_SUCCESS) {
      const page_no_t disk_page_no =
          mach_read_from_4(static_cast<byte *>(buf) + FIL_PAGE_OFFSET);
      ut_ad(disk_page_no == page_load->get_page_no());
      m_btree_load->track_page_flush(disk_page_no);
    }
#endif /* UNIV_DEBUG */
    if (compressed_block != nullptr) {
      file::Block::free(compressed_block);
      const size_t hole_offset = offset + page_size;
      const size_t hole_size = physical_page_size - page_size;
      ut_ad(hole_size < physical_page_size);
      dberr_t err =
          os_file_punch_hole(node->handle.m_file, hole_offset, hole_size);
      if (err != DB_SUCCESS) {
        LogErr(WARNING_LEVEL, ER_IB_BULK_FLUSHER_PUNCH_HOLE, index->table_name,
               index->name(), (size_t)space_id, (size_t)page_no,
               physical_page_size, hole_size, file_name.c_str(), (size_t)err);
      }
    }
    if (e_block != nullptr) {
      file::Block::free(e_block);
    }
    page_no++;
  }

  fil_space_release(space);
  return err;
}

dberr_t Page_extent::bulk_flush(fil_node_t *node, void *iov [[maybe_unused]],
                                size_t iov_size [[maybe_unused]]) {
#ifdef UNIV_LINUX
  return bulk_flush_linux(node, static_cast<struct iovec *>(iov), iov_size);
#else
  return flush_one_by_one(node);
#endif /* UNIV_LINUX */
}

struct Page_load_compare {
  bool operator()(const Page_load *l_page_load, const Page_load *r_page_load) {
    return l_page_load->get_page_no() < r_page_load->get_page_no();
  }
};

dberr_t Page_extent::flush(fil_node_t *node, void *iov, size_t iov_size) {
  dberr_t err{DB_SUCCESS};

  /* No need to flush any pages if index build has been interrupted. */
  if (m_btree_load->is_interrupted()) {
    return DB_INTERRUPTED;
  }

  const page_no_t n_pages = m_page_loads.size();
  if (n_pages == 0) {
    /* Nothing to do. */
    return err;
  }

  std::sort(m_page_loads.begin(), m_page_loads.end(), Page_load_compare());

#ifdef UNIV_DEBUG
  for (size_t i = m_range.first, j = 0;
       i < m_range.second && j < m_page_loads.size(); ++i, ++j) {
    ut_ad(i == m_page_loads[j]->get_page_no());
  }
#endif /* UNIV_DEBUG */

  for (auto &page_load : m_page_loads) {
    ut_ad(page_load->verify_space_id());
    const page_no_t page_no = page_load->get_page_no();
    /* In the debug build we assert, but in the release build we report a
    internal failure. */
    ut_ad(page_no >= m_range.first);
    ut_ad(page_no < m_range.second);
    if (page_no < m_range.first || page_no >= m_range.second) {
      /* The page_no is out of range for the given extent. Report error. */
      return DB_FAIL;
    }
  }

  /* Remove any old copies in the buffer pool. */
  m_btree_load->force_evict(m_range);

  if (m_btree_load->is_tpc_enabled() || m_btree_load->is_tpe_enabled()) {
    err = flush_one_by_one(node);
  } else {
    err = bulk_flush(node, iov, iov_size);
  }

  /* Remove any old copies in the buffer pool. Should not be dirty. */
  const bool is_dirty_ok = false;
  m_btree_load->force_evict(m_range, is_dirty_ok);

#ifdef UNIV_DEBUG
  if (err == DB_SUCCESS) {
    dict_index_t *index = m_page_loads[0]->index();
    for (page_no_t i = m_range.first; i < n_pages; ++i) {
      check_page(index, i);
    }
  }
#endif /* UNIV_DEBUG */
  return err;
}

void Page_extent::destroy_cached() {
  for (auto page_load : m_cached_page_loads) {
    ut_ad(page_load->is_cached());
    ut::delete_(page_load);
  }
  m_cached_page_loads.clear();
}

dberr_t Page_extent::destroy() {
  for (auto page_load : m_page_loads) {
    page_load->free();
    Page_load::drop(page_load);
  }
  m_page_loads.clear();

  return DB_SUCCESS;
}

dberr_t Level_ctx::alloc_page_num(page_no_t &page_no) {
  if (m_extent_full) {
    dberr_t err = alloc_extent();
    if (err != DB_SUCCESS) {
      return err;
    }
  }
  page_no = m_page_extent->alloc();
  if (page_no == FIL_NULL) {
    dberr_t err = alloc_extent();
    if (err != DB_SUCCESS) {
      return err;
    }
    ut_ad(m_page_extent->is_valid());
    page_no = m_page_extent->alloc();
    ut_ad(page_no != FIL_NULL);
  }
  if (m_page_extent->is_fully_used()) {
    m_extent_full = true;
  }
  m_stat_n_pages++;
  m_btree_load->m_stat_n_pages++;
  ut_ad(page_no != 0);
  ut_ad(page_no != FIL_NULL);
#ifdef UNIV_DEBUG
  m_pages_allocated.push_back(page_no);
#endif /* UNIV_DEBUG */
  return DB_SUCCESS;
}

#ifdef UNIV_DEBUG
bool Level_ctx::is_page_tracked(const page_no_t &page_no) const {
  return std::find(m_pages_allocated.begin(), m_pages_allocated.end(),
                   page_no) != m_pages_allocated.end();
}
#endif /* UNIV_DEBUG */

dberr_t Level_ctx::alloc_extent() {
  ut_ad(m_extent_full);

  if (!load_extent_from_cache()) {
    const bool is_leaf = (m_level == 0);
    const bool skip_track = false;
    m_page_extent = Page_extent::create(m_btree_load, is_leaf, skip_track);
  }

  dberr_t err = m_btree_load->alloc_extent(m_page_extent->m_range, m_level);
  if (err != DB_SUCCESS) {
    return err;
  }
  m_page_extent->init();
  ut_ad(m_page_extent->is_valid());
  ut_ad(!m_page_extent->is_fully_used());
  ++m_stat_n_extents;
  m_extent_full = false;
  return err;
}

Level_ctx *Level_ctx::create(dict_index_t *index, size_t level,
                             Btree_load *btree_load) {
  Level_ctx *lvl_ctx = ut::new_withkey<Level_ctx>(UT_NEW_THIS_FILE_PSI_KEY,
                                                  index, level, btree_load);

  return lvl_ctx;
}

void Level_ctx::destroy(Level_ctx *ctx) {
  if (ctx == nullptr) {
    return;
  }
  /* Free cached extents. */
  for (auto cached_extent : ctx->m_cached_extents) {
    ut_ad(cached_extent->m_page_loads.empty());
    cached_extent->destroy_cached();
    ut::delete_(cached_extent);
  }
  ctx->m_cached_extents.clear();
  ut::delete_(ctx);
}

Page_load *Page_load::create(Btree_load *btree_load, Page_extent *page_extent) {
  ut_ad(page_extent->is_valid());
  auto index = btree_load->index();
  auto page_load =
      ut::new_withkey<Page_load>(UT_NEW_THIS_FILE_PSI_KEY, index, btree_load);
  page_load->set_page_extent(page_extent);
  btree_load->add_to_bulk_flusher();
  return page_load;
}

void Page_load::drop(Page_load *page_load) {
  if (page_load == nullptr || page_load->is_cached()) {
    return;
  }
  ut::delete_(page_load);
}

Page_load *Level_ctx::create_page_load() {
  ut_ad(m_page_extent->is_valid());
  const trx_id_t trx_id = m_btree_load->get_trx_id();
  auto page_load = Page_load::create(m_btree_load, m_page_extent);
  page_load->set_trx_id(trx_id);
  page_load->set_page_no(FIL_NULL);
  page_load->set_level(m_level);
  page_load->set_flush_observer(nullptr);
  page_load->set_level_ctx(this);
  ut_ad(m_page_extent != nullptr);
  ut_ad(m_page_extent->is_valid());
  return page_load;
}

void Level_ctx::free_page_load() {
  Page_load::drop(m_page_load);
  m_page_load = nullptr;
}

dberr_t Btree_load::alloc_extent(Page_range_t &page_range, size_t level) {
  bool is_leaf = (level == 0);
  auto err = m_allocator.allocate(is_leaf, false, page_range, m_fn_wait_begin,
                                  m_fn_wait_end);
  if (err != DB_SUCCESS) {
    return err;
  }
  ut_ad(page_range.first != 0);
  ut_ad(page_range.first != FIL_NULL);
  ut_ad(page_range.second != 0);
  ut_ad(page_range.second != FIL_NULL);
  return DB_SUCCESS;
}

bool Level_ctx::load_extent_from_cache() {
  /* Wait for 1 sec in total with increasing wait interval. */
  size_t max_retry = 30;
  size_t trial = 0;

  for (trial = 0; trial < max_retry; ++trial) {
    for (auto extent : m_cached_extents) {
      if (extent->is_free()) {
        extent->set_state(false);
        m_page_extent = extent;
        m_page_extent->reset_cached_page_loads();
        /* We don't do track_extent(). The extents are directly added
        to flush queue after page_commit if found full. */
        if (trial > 0) {
          ib::info(ER_BULK_LOADER_INFO)
              << "Found cached Extent. Retry count: " << trial;
        }
        return true;
      }
    }
    if (trial < 10) {
      /* First 10 ms: sleep for 1 ms and check. */
      std::this_thread::sleep_for(std::chrono::milliseconds{1});
    } else if (trial < 20) {
      /* 10 ms to 100 ms: sleep for 10 ms and check. */
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
    } else {
      /* 100 ms to 1 s: sleep for 100 ms and check. */
      std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
  }
  ib::info(ER_BULK_LOADER_INFO) << "Failed to find free cached Page Extent";
  return false;
}

Page_load *Level_ctx::get_page_load_from_cache() {
  auto &page_loads = m_page_extent->m_cached_page_loads;
  auto &cache_index = m_page_extent->m_next_cached_page_load_index;

  if (cache_index < page_loads.size()) {
    auto page_load = page_loads[cache_index];
    ++cache_index;

    page_load->set_page_no(FIL_NULL);
    return page_load;
  }

  if (m_page_extent->is_cached()) {
    ib::info(ER_BULK_LOADER_INFO) << "Failed to find free cached Page Load";
  }
  return nullptr;
}

void Level_ctx::build_page_cache() {
  auto &page_loads = m_page_extent->m_cached_page_loads;
  size_t num_page_loads = FSP_EXTENT_SIZE;

  page_loads.clear();
  for (size_t index = 0; index < num_page_loads; index++) {
    auto page_load = create_page_load();
    /* Mark the Page Load as cached. Should not be freed after flush. */
    page_load->set_cached();
    page_loads.push_back(page_load);
  }
  m_page_extent->m_next_cached_page_load_index = 0;
}

void Level_ctx::build_extent_cache() {
  /* Currently we cache elements twice the maximum flush queue size. The
  cached elements can be reused after the extent is flushed. The flush queue
  is common for all the levels but allocating max for each levels ensure that
  a free element is always available. */
  auto cache_size = 2 + 2 * m_btree_load->get_max_flush_queue_size();
  const bool is_leaf = (m_level == 0);
  const bool skip_track = true;

  for (size_t index = 0; index < cache_size; index++) {
    auto page_extent = Page_extent::create(m_btree_load, is_leaf, skip_track);
    /* Mark the extent as cached. Should not be freed after flush. */
    page_extent->set_cached();

    /* Mark the cached entry as free to be used. */
    bool free = true;
    page_extent->set_state(free);

    m_cached_extents.push_back(page_extent);

    m_page_extent = page_extent;
    build_page_cache();
  }
  m_page_extent = nullptr;
}

dberr_t Level_ctx::init() {
  dberr_t er{DB_SUCCESS};
  build_extent_cache();

  if (!load_extent_from_cache()) {
    const bool is_leaf = (m_level == 0);
    const bool skip_track = false;
    m_page_extent = Page_extent::create(m_btree_load, is_leaf, skip_track);
  }

  if (m_page_extent == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  er = m_btree_load->alloc_extent(m_page_extent->m_range, m_level);
  if (er != DB_SUCCESS) {
    return er;
  }

  m_page_extent->init();
  m_extent_full = false;

  ut_ad(m_page_load == nullptr);
  m_page_load = get_page_load_from_cache();

  if (m_page_load == nullptr) {
    m_page_load = create_page_load();
  }

  page_no_t new_page_no = m_page_extent->alloc();

#ifdef UNIV_DEBUG
  m_pages_allocated.push_back(new_page_no);
#endif /* UNIV_DEBUG */

  if (m_page_extent->is_fully_used()) {
    m_extent_full = true;
  }

  er = m_page_load->init_mem(new_page_no, m_page_extent);
  if (er != DB_SUCCESS) {
    return er;
  }

  return er;
}

[[nodiscard]] buf_block_t *Level_ctx::alloc(
    const page_no_t new_page_no) noexcept {
  const page_id_t new_page_id(m_index->space, new_page_no);
  const page_size_t page_size = dict_table_page_size(m_index->table);

  buf_pool_t *buf_pool = buf_pool_get(new_page_id);
  buf_block_t *block = buf_block_alloc(buf_pool);
  const page_id_t page_id(m_index->space, new_page_no);

  block->page.reset_page_id(page_id);
  block->page.set_page_size(page_size);

  page_t *new_page = buf_block_get_frame(block);
  mach_write_to_4(new_page + FIL_PAGE_OFFSET, block->page.id.page_no());

  ut_ad(!page_size.is_compressed());
  fsp_init_file_page_low(block);

  ut_ad(buf_block_get_page_zip(block) == nullptr);
  ut_ad(!dict_index_is_spatial(m_index));

  page_create_low(block, dict_table_is_comp(m_index->table), FIL_PAGE_INDEX);
  btr_page_set_level(new_page, nullptr, m_level, nullptr);

  btr_page_set_next(new_page, nullptr, FIL_NULL, nullptr);
  btr_page_set_prev(new_page, nullptr, FIL_NULL, nullptr);
  btr_page_set_index_id(new_page, nullptr, m_index->id, nullptr);
  return block;
}

void Page_load::set_page_no(const page_no_t page_no) {
  ut_ad(m_block == nullptr || m_block->is_memory());
  m_page_no = page_no;
  if (m_block != nullptr) {
    m_block->page.id.set_page_no(page_no);
    mach_write_to_4(m_page + FIL_PAGE_OFFSET, m_block->page.id.page_no());
  }
}

Page_load::Page_load(dict_index_t *index, Btree_load *btree_load)
    : m_index(index),
      m_is_comp(dict_table_is_comp(index->table)),
      m_btree_load(btree_load) {
  m_is_cached.store(false);
}

dberr_t Page_load::init_mem(const page_no_t page_no,
                            Page_extent *page_extent) noexcept {
  ut_ad(page_extent != nullptr);
  ut_ad(page_no >= page_extent->m_range.first);
  ut_ad(page_no < page_extent->m_range.second);
  ut_ad(m_heap == nullptr || is_cached());
  ut_ad(m_page_no == FIL_NULL);

  m_page_extent = page_extent;
  m_mtr = nullptr;

  if (m_heap == nullptr) {
    m_heap = mem_heap_create(1024, UT_LOCATION_HERE);
  } else {
    /* For cached page loader, reuse the same heap. */
    ut_a(is_cached());
    mem_heap_empty(m_heap);
  }

  /* Going to use BUF_BLOCK_MEMORY.  Allocate a new page. */
  auto new_block = m_level_ctx->alloc(page_no);

  ut_ad(buf_block_get_page_zip(new_block) == nullptr);
  ut_ad(!dict_index_is_spatial(m_index));
  ut_ad(!dict_index_is_sdi(m_index));

  auto new_page = buf_block_get_frame(new_block);
  auto new_page_no = page_get_page_no(new_page);

  btr_page_set_next(new_page, nullptr, FIL_NULL, nullptr);
  btr_page_set_prev(new_page, nullptr, FIL_NULL, nullptr);
  btr_page_set_index_id(new_page, nullptr, m_index->id, nullptr);

  if (dict_index_is_sec_or_ibuf(m_index) && !m_index->table->is_temporary() &&
      page_is_leaf(new_page)) {
    page_update_max_trx_id(new_block, nullptr, m_trx_id, nullptr);
  }

  m_block = new_block;
  m_page = new_page;
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

  m_heap_top = page_header_get_ptr(new_page, PAGE_HEAP_TOP);
  m_rec_no = page_header_get_field(new_page, PAGE_N_RECS);

  m_last_slotted_rec = page_get_infimum_rec(m_page);
  m_slotted_rec_no = 0;

  m_modified = true;

  ut_d(m_total_data = 0);

  ut_ad(is_memory());
  ut_ad(m_level_ctx->is_page_tracked(m_page_no));

  return DB_SUCCESS;
}

dberr_t Page_load::reinit() noexcept {
  btr_page_set_level(m_page, nullptr, m_level, m_mtr);
  page_create_empty(m_block, m_index, m_mtr);

  m_cur_rec = page_get_infimum_rec(m_page);
  m_free_space = page_get_free_space_of_empty(m_is_comp);

  m_heap_top = page_header_get_ptr(m_page, PAGE_HEAP_TOP);
  m_rec_no = page_header_get_field(m_page, PAGE_N_RECS);
  m_last_slotted_rec = page_get_infimum_rec(m_page);
  m_slotted_rec_no = 0;

  m_modified = true;

  btr_page_set_next(m_page, nullptr, FIL_NULL, m_mtr);
  btr_page_set_prev(m_page, nullptr, FIL_NULL, m_mtr);
  btr_page_set_index_id(m_page, nullptr, m_index->id, m_mtr);

  return DB_SUCCESS;
}

dberr_t Page_load::alloc() noexcept {
  ut_ad(m_page_no == FIL_NULL);
  mtr_t alloc_mtr;
  mtr_t mtr;
  mtr.start();

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
    mtr.commit();
    return DB_OUT_OF_FILE_SPACE;
  }

  /* Allocate a new page. */
  auto new_block =
      btr_page_alloc(m_index, 0, FSP_UP, m_level, &alloc_mtr, &mtr);

  auto new_page = buf_block_get_frame(new_block);

  if (n_reserved > 0) {
    fil_space_release_free_extents(m_index->space, n_reserved);
  }

  m_page_no = new_block->page.id.page_no();
  alloc_mtr.commit();

  ut_ad(buf_block_get_page_zip(new_block) == nullptr);
  ut_ad(!dict_index_is_spatial(m_index));

  page_create(new_block, &mtr, dict_table_is_comp(m_index->table),
              FIL_PAGE_INDEX);

  btr_page_set_level(new_page, nullptr, m_level, &mtr);
  btr_page_set_index_id(new_page, nullptr, m_index->id, &mtr);

  mtr.commit();
  return DB_SUCCESS;
}

void Page_load::reset() noexcept {
  ut_a(m_mtr != nullptr);
  ut_a(!m_mtr->is_active());
  ut_a(m_page_no != FIL_NULL);

  m_mtr->~mtr_t(); /* Call dtor before freeing heap. */
  mem_heap_free(m_heap);
  m_heap = nullptr;
  /* m_index is not modified. */
  /* m_trx_id is not modified. */
  m_block = nullptr;
  m_page = nullptr;
  m_cur_rec = nullptr;
  m_page_no = FIL_NULL;
  /* m_level is not changed. */
  /* m_is_comp is not modified. */
  m_heap_top = nullptr;
  m_rec_no = 0;
  m_free_space = 0;
  m_reserved_space = 0;
  ut_d(m_total_data = 0);
  /* m_modify_clock is not modified. */
  /* m_flush_observer is not modified. */
  m_last_slotted_rec = nullptr;
  m_slotted_rec_no = 0;
  m_modified = false;
  /* m_btree_load is not modified. */
  /* m_level_ctx is not modified. */
  /* m_page_extent is not modified. */
  /* m_is_cached is not modified. */
}

dberr_t Page_load::init() noexcept {
  /* Call this function only when mtr is to be used. */
  ut_ad(m_page_no != FIL_NULL);
  ut_ad(m_heap == nullptr);

  m_heap = mem_heap_create(1024, UT_LOCATION_HERE);

  auto mtr_alloc = mem_heap_alloc(m_heap, sizeof(mtr_t));
  auto mtr = new (mtr_alloc) mtr_t();
  mtr->start();

  if (m_flush_observer != nullptr) {
    mtr->set_log_mode(MTR_LOG_NO_REDO);
    mtr->set_flush_observer(m_flush_observer);
  }
  m_mtr = mtr;
  m_mtr->set_modified();

  if (!dict_index_is_online_ddl(m_index)) {
    mtr->x_lock(dict_index_get_lock(m_index), UT_LOCATION_HERE);
  }

  page_id_t page_id(dict_index_get_space(m_index), m_page_no);
  page_size_t page_size(dict_table_page_size(m_index->table));

  auto new_block =
      buf_page_get(page_id, page_size, RW_X_LATCH, UT_LOCATION_HERE, m_mtr);
  auto new_page = buf_block_get_frame(new_block);
  auto new_page_no = page_get_page_no(new_page);

  ut_ad(m_page_no == new_page_no);

  ut_ad(page_dir_get_n_heap(new_page) == PAGE_HEAP_NO_USER_LOW);

  btr_page_set_level(new_page, nullptr, m_level, m_mtr);

  m_block = new_block;
  m_page = new_page;
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
  ut_ad(verify_space_id());

  const auto rec_size = rec_offs_size(offsets);
  const auto slot_size = page_dir_calc_reserved_space(m_rec_no + 1) -
                         page_dir_calc_reserved_space(m_rec_no);
  const auto need_space = rec_size + slot_size;

  if (m_free_space < need_space) {
    /* Not enough space to insert this record. */
    return DB_FAIL;
  }

#ifdef UNIV_DEBUG
  /* Check whether records are in order. */
  if (!page_rec_is_infimum(m_cur_rec)) {
    auto old_rec = m_cur_rec;

    auto old_offsets = rec_get_offsets(
        old_rec, m_index, nullptr, ULINT_UNDEFINED, UT_LOCATION_HERE, &m_heap);

    ulint n_fields;
    const bool is_spatial = page_is_spatial_non_leaf(old_rec, m_index);
    const bool is_mvi = m_index->is_multi_value();
    const int cmp = cmp_rec_rec(rec, old_rec, offsets, old_offsets, m_index,
                                is_spatial, &n_fields);
    ut_ad(cmp > 0 || (is_mvi && cmp >= 0));
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
  ut_ad(big_rec == nullptr);
  IF_ENABLED("ddl_btree_build_insert_return_interrupt", return DB_INTERRUPTED;)

  /* The memory allocated for temporary record can be reset immediately. We do
  it to avoid repeated malloc because of cumulative allocation of record buffer
  memory. Ideally we should get rid of this allocation and write directly to
  data page. */
  auto saved_top = mem_heap_get_heap_top(m_heap);
  /* Convert tuple to record. */
  auto rec_mem = static_cast<byte *>(mem_heap_alloc(m_heap, rec_size));

  auto rec = rec_convert_dtuple_to_rec(rec_mem, m_index, tuple);

  Rec_offsets offsets{};

  offsets = rec_get_offsets(rec, m_index, offsets, ULINT_UNDEFINED,
                            UT_LOCATION_HERE, &m_heap);

  /* Insert the record.*/
  auto err = insert(rec, offsets);

  if (err != DB_SUCCESS) {
    return err;
  }

  ut_ad(m_modified);
  mem_heap_free_heap_top(m_heap, saved_top);
  return err;
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
  ut_d(const bool check_min_rec = false;);
  ut_ad(page_validate(m_page, m_index, check_min_rec));
}

dberr_t Page_load::commit() noexcept {
  /* It is assumed that finish() was called before commit */
  ut_a(!m_modified);
  ut_ad(page_validate(m_page, m_index));
  ut_a(m_rec_no > 0);
  ut_ad(!is_memory() || m_level_ctx->is_page_tracked(m_page_no));

  /* Set no free space left and no buffered changes in ibuf. */
  if (!m_index->is_clustered() && !m_index->table->is_temporary() &&
      page_is_leaf(m_page)) {
    ibuf_set_bitmap_for_bulk_load(m_block, ddl::fill_factor == 100);
  }
  ut_ad(btr_page_get_index_id(m_page) == m_index->id);
  if (m_mtr != nullptr) {
    m_mtr->commit();
  } else {
    ut_ad(m_page_extent != nullptr);
    m_page_extent->append(this);
    if (m_page_extent->is_fully_used()) {
      m_page_extent = nullptr;
    }
  }
  if (m_btree_load != nullptr) {
    if ((m_level + 1) != m_btree_load->m_last_page_nos.size()) {
      m_btree_load->m_last_page_nos[m_level] = get_page_no();
    }
  }
  return DB_SUCCESS;
}

void Page_load::rollback() noexcept {}

dtuple_t *Page_load::get_node_ptr(mem_heap_t *heap) noexcept {
  /* Create node pointer */
  auto first_rec = page_rec_get_next(page_get_infimum_rec(m_page));
  ut_a(page_rec_is_user_rec(first_rec));

  auto node_ptr =
      dict_index_build_node_ptr(m_index, first_rec, m_page_no, heap, m_level);

  return node_ptr;
}

void Page_load::print_child_page_nos() noexcept {
  if (m_level == 0) {
    return;
  }

  Rec_offsets offsets{};
  auto inf_rec = page_get_infimum_rec(m_page);
  auto rec = page_rec_get_next_const(inf_rec);
  ut_ad(page_rec_is_user_rec(rec));
  do {
    offsets = rec_get_offsets(rec, m_index, offsets, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &(m_heap));
    rec = page_rec_get_next_const(rec);
  } while (!page_rec_is_supremum(rec));
}

size_t Page_load::copy_all(const page_t *src_page) noexcept {
  auto inf_rec = page_get_infimum_rec(src_page);
  auto first_rec = page_rec_get_next_const(inf_rec);
  ut_ad(page_rec_is_user_rec(first_rec));
  const size_t n_recs = copy_records(first_rec);
  ut_ad(m_modified);
  return n_recs;
}

size_t Page_load::copy_to(std::vector<Page_load *> &to_pages) {
  auto src_page = get_page();
  auto inf_rec = page_get_infimum_rec(src_page);
  auto first_rec = page_rec_get_next_const(inf_rec);
  const size_t n_recs = page_get_n_recs(src_page);
  const size_t n_pages = to_pages.size();
  const size_t rec_per_page = (n_recs + n_pages) / n_pages;
  Rec_offsets offsets{};
  const rec_t *rec = first_rec;

  /* Total number of records inserted so far. */
  size_t rec_count = 0;
  size_t i = 0;
  do {
    offsets = rec_get_offsets(rec, m_index, offsets, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &m_heap);
    ut_a(i < to_pages.size());
    to_pages[i]->insert(rec, offsets);
    rec = page_rec_get_next_const(rec);
    if (++rec_count % rec_per_page == 0) {
      ++i;
    }
    ut_a(rec_count <= n_recs);
  } while (!page_rec_is_supremum(rec));

  if (is_min_rec_flag()) {
    to_pages[0]->set_min_rec_flag();
  }
  return rec_count;
}

size_t Page_load::copy_records(const rec_t *first_rec) noexcept {
  Rec_offsets offsets{};
  const rec_t *rec = first_rec;

  size_t n_recs{};
  ut_ad(page_rec_is_user_rec(rec));

  do {
    offsets = rec_get_offsets(rec, m_index, offsets, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &m_heap);
    insert(rec, offsets);
    rec = page_rec_get_next_const(rec);
    n_recs++;
  } while (!page_rec_is_supremum(rec));

  ut_ad(m_rec_no > 0);
  return n_recs;
}

void Page_load::set_next(page_no_t next_page_no) noexcept {
  btr_page_set_next(m_page, nullptr, next_page_no, m_mtr);
}

void Page_load::set_prev(page_no_t prev_page_no) noexcept {
  btr_page_set_prev(m_page, nullptr, prev_page_no, m_mtr);
}

page_no_t Page_load::get_prev() noexcept {
  return btr_page_get_prev(m_page, m_mtr);
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
  if (m_rec_no >= 2 && (m_free_space - required_space < m_reserved_space)) {
    return false;
  }

  return true;
}

bool Page_load::need_ext(const dtuple_t *tuple,
                         size_t rec_size) const noexcept {
  return page_zip_rec_needs_ext(rec_size, m_is_comp, dtuple_get_n_fields(tuple),
                                m_block->page.size);
}

#ifdef UNIV_DEBUG
bool Page_load::is_index_locked() noexcept {
  return (m_mtr == nullptr) ? false
                            : (dict_index_is_online_ddl(m_index) &&
                               m_mtr->memo_contains_flagged(
                                   dict_index_get_lock(m_index),
                                   MTR_MEMO_X_LOCK | MTR_MEMO_SX_LOCK));
}
#endif /* UNIV_DEBUG */

dberr_t Btree_load::page_commit(Page_load *page_loader,
                                Page_load *next_page_loader,
                                bool insert_father) noexcept {
  /* Set page links */
  if (next_page_loader != nullptr) {
    ut_ad(page_loader->get_level() == next_page_loader->get_level());
    const page_no_t cur_page_no = page_loader->get_page_no();
    const page_no_t next_page_no = next_page_loader->get_page_no();
    page_loader->set_next(next_page_no);
    next_page_loader->set_prev(cur_page_no);
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

Btree_load::Btree_load(dict_index_t *index, trx_t *trx, size_t loader_num,
                       size_t flush_queue_size,
                       Bulk_extent_allocator &allocator) noexcept
    : m_index(index),
      m_trx(trx),
      m_allocator(allocator),
      m_compare_key(m_index, nullptr, !m_index->is_clustered()),
      m_loader_num(loader_num),
      m_page_size(dict_table_page_size(m_index->table)) {
  ut_d(fil_space_inc_redo_skipped_count(m_index->space));
  ut_d(m_index_online = m_index->online_status);
  m_bulk_flusher.start(m_index->space, m_loader_num, flush_queue_size);
}

trx_id_t Btree_load::get_trx_id() const { return m_trx->id; }

Btree_load::~Btree_load() noexcept {
  ut_d(fil_space_dec_redo_skipped_count(m_index->space));
  for (auto level_ctx : m_level_ctxs) {
    Level_ctx::destroy(level_ctx);
  }
  mem_heap_free(m_heap_order);
}

dberr_t Btree_load::prepare_space(Page_load *&page_loader, size_t level,
                                  size_t rec_size) noexcept {
  if (page_loader->is_space_available(rec_size)) {
    return DB_SUCCESS;
  }

  /* Finish page modifications. */
  page_loader->finish();

  IF_ENABLED("ddl_btree_build_oom", return DB_OUT_OF_MEMORY;)

  auto lvl_ctx = m_level_ctxs[level];
  page_no_t new_page_no = FIL_NULL;

  auto err = lvl_ctx->alloc_page_num(new_page_no);
  if (err != DB_SUCCESS) {
    return err;
  }
  ut_ad(new_page_no != FIL_NULL);

  /* Create a sibling page_loader. */
  auto sibling_page_loader = lvl_ctx->get_page_load_from_cache();

  if (sibling_page_loader == nullptr) {
    sibling_page_loader = lvl_ctx->create_page_load();
  }

  if (sibling_page_loader == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  {
    auto err =
        sibling_page_loader->init_mem(new_page_no, lvl_ctx->m_page_extent);

    if (err != DB_SUCCESS) {
      Page_load::drop(sibling_page_loader);
      return err;
    }
  }

  /* It is unsafe to access uncached page extent after commit. */
  auto page_extent = page_loader->m_page_extent;
  bool extent_cached = (page_extent != nullptr && page_extent->is_cached());

  /* Commit page bulk. */
  {
    auto err = page_commit(page_loader, sibling_page_loader, true);

    if (err != DB_SUCCESS) {
      sibling_page_loader->finish();
      sibling_page_loader->rollback();
      Page_load::drop(sibling_page_loader);
      return err;
    }
  }

  /* Set new page bulk to page_loaders. */
  ut_a(sibling_page_loader->get_level() <= m_root_level);
  lvl_ctx->set_current_page_load(sibling_page_loader);
  page_loader = sibling_page_loader;
  m_last_page_nos[level] = new_page_no;

  /* If the cached extent for the page is full, add to flush queue. */
  if (extent_cached && page_extent->is_page_loads_full()) {
    ut_ad(!is_extent_tracked(page_extent));
    ut_ad(sibling_page_loader->m_page_extent != page_extent);
    add_to_bulk_flusher(page_extent);
  }
  return DB_SUCCESS;
}

void Btree_load::add_to_bulk_flusher(Page_extent *page_extent) {
  m_bulk_flusher.add(page_extent, m_fn_wait_begin, m_fn_wait_end);
}

void Btree_load::add_to_bulk_flusher(bool finish) {
  const size_t n = m_extents_tracked.size();
  for (size_t i = 0; i < n; ++i) {
    auto page_extent = m_extents_tracked.front();
    m_extents_tracked.pop_front();
    if (page_extent->is_page_loads_full() || finish) {
      m_bulk_flusher.add(page_extent, m_fn_wait_begin, m_fn_wait_end);
    } else {
      m_extents_tracked.push_back(page_extent);
    }
  }
}

dberr_t Btree_load::insert(Page_load *page_loader, dtuple_t *tuple,
                           big_rec_t *big_rec, size_t rec_size) noexcept {
  ut_ad(big_rec == nullptr);
  auto err = page_loader->insert(tuple, big_rec, rec_size);
  return err;
}

dberr_t Btree_load::insert(dtuple_t *tuple, size_t level) noexcept {
  bool is_left_most{};
  dberr_t err{DB_SUCCESS};

  /* Check if data is inserted in sorted order . */
  if (m_check_order && level == 0) {
    if (m_prev_tuple != nullptr) {
      const auto cmp = m_compare_key(m_prev_tuple->fields, tuple->fields);
      if (cmp > 0) {
        return DB_DATA_NOT_SORTED;
      }
      if (cmp == 0) {
        return DB_DUPLICATE_KEY;
      }
    }
    mem_heap_empty(m_heap_order);
    m_prev_tuple = tuple->deep_copy(m_heap_order);
  }

  if (is_new_level(level)) {
    IF_ENABLED("ddl_btree_build_oom", return DB_OUT_OF_MEMORY;)

    auto lvl_ctx = Level_ctx::create(m_index, level, this);
    if (lvl_ctx == nullptr) {
      return DB_OUT_OF_MEMORY;
    }

    err = lvl_ctx->init();
    if (err != DB_SUCCESS) {
      return err;
    }

    auto page_loader = lvl_ctx->get_page_load();

    DEBUG_SYNC_C("bulk_load_insert");

    m_level_ctxs.push_back(lvl_ctx);
    ut_a(level + 1 == m_level_ctxs.size());
    m_root_level = level;
    is_left_most = true;
    const page_no_t root_page_no = page_loader->get_page_no();
    m_first_page_nos.push_back(root_page_no);
    m_last_page_nos.push_back(root_page_no);
  }

  auto page_loader = get_level(level)->get_page_load();

  if (is_left_most && level > 0 && page_loader->get_rec_no() == 0) {
    /* The node pointer must be marked as the predefined minimum
    record,     as there is no lower alphabetical limit to records in
    the leftmost node of a level: */
    const auto info_bits = dtuple_get_info_bits(tuple) | REC_INFO_MIN_REC_FLAG;
    dtuple_set_info_bits(tuple, info_bits);
  }

  auto rec_size = rec_get_converted_size(m_index, tuple);

  if (page_loader->need_ext(tuple, rec_size)) {
    /* The record is so big that we have to store some fields
    externally on separate database pages. */
    return DB_BULK_TOO_BIG_RECORD;
  }

  err = prepare_space(page_loader, level, rec_size);

  if (err == DB_SUCCESS) {
    err = insert(page_loader, tuple, nullptr, rec_size);
  }
  return err;
}

dberr_t Btree_load::finalize_page_loads(bool is_err,
                                        page_no_t &last_page_no) noexcept {
  ut_a(last_page_no == FIL_NULL);
  ut_a(m_root_level + 1 == m_level_ctxs.size());
  dberr_t err = DB_SUCCESS;

  /* Finish all page bulks */
  for (size_t level = 0; level <= m_root_level; level++) {
    auto lvl_ctx = get_level(level);
    auto page_loader = lvl_ctx->get_page_load();
    /* It is unsafe to access uncached page extent after commit. */
    auto page_extent = page_loader->m_page_extent;
    bool extent_cached = (page_extent != nullptr && page_extent->is_cached());

    if (!is_err) {
      page_loader->finish();
      err = page_commit(page_loader, nullptr, level != m_root_level);
      if (err != DB_SUCCESS) {
        is_err = true;
      }
    } else {
      lvl_ctx->free_page_load();
    }
    if (extent_cached && !page_extent->m_page_loads.empty()) {
      /* Add the last extent to flush queue. */
      add_to_bulk_flusher(page_extent);
    }
  }
  last_page_no = m_last_page_nos[m_root_level];
  return err;
}

dberr_t Btree_load::load_root_page(page_no_t last_page_no) noexcept {
  ut_ad(last_page_no != FIL_NULL);

  page_id_t page_id(dict_index_get_space(m_index), last_page_no);
  page_size_t page_size(dict_table_page_size(m_index->table));
  page_no_t page_no = dict_index_get_page(m_index);

  auto observer = m_trx->flush_observer;
  ut_a(observer != nullptr);

  /* Load the correct root page. */
  Page_load page_loader(m_index, get_trx_id(), page_no, m_root_level, observer,
                        this);
  mtr_t mtr;
  mtr.start();
  mtr.x_lock(dict_index_get_lock(m_index), UT_LOCATION_HERE);

  auto last_block = btr_block_get(page_id, page_size, RW_X_LATCH,
                                  UT_LOCATION_HERE, m_index, &mtr);

  auto last_page = buf_block_get_frame(last_block);

#ifdef UNIV_DEBUG
  {
    auto buf = static_cast<byte *>(buf_block_get_frame(last_block));
    const bool is_zero = ut::is_zeros(buf, page_size.physical());
    ut_ad(!is_zero);
  }
#endif /* UNIV_DEBUG */

  /* Copy last page to root page. */
  auto err = page_loader.init();

  if (err == DB_SUCCESS) {
    size_t n_recs = page_loader.copy_all(last_page);
    ut_a(n_recs > 0);
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

#ifdef UNIV_DEBUG
std::string Btree_load::print_pages_in_level(const size_t level) const {
  std::ostringstream sout;
  Scoped_heap local_heap(2048, UT_LOCATION_HERE);
  ulint *offsets{};
  mem_heap_t *heap = local_heap.get();
  sout << "[level=" << level << ", pages=[";
  page_no_t page_no = m_first_page_nos[level];
  const page_size_t page_size(dict_table_page_size(m_index->table));
  size_t total_rows = 0;
  mtr_t mtr;
  size_t i{};
  while (page_no != FIL_NULL) {
    sout << "{page_no=" << page_no << ", ";
    mtr.start();
    const page_id_t page_id(m_index->space, page_no);
    buf_block_t *block = btr_block_get(page_id, page_size, RW_S_LATCH,
                                       UT_LOCATION_HERE, m_index, &mtr);
    const page_no_t next_page_no = block->get_next_page_no();
    byte *frame = buf_block_get_frame(block);
    byte *infimum = page_get_infimum_rec(frame);
    byte *supremum = page_get_supremum_rec(frame);
    byte *first_rec = page_rec_get_next(infimum);
    bool is_min_rec = rec_get_info_bits(first_rec, page_is_comp(frame)) &
                      REC_INFO_MIN_REC_FLAG;
    ut_ad(level == 0 || i > 0 || is_min_rec);
    if (unlikely(is_min_rec)) {
      sout << "min_rec, ";
    }
    rec_t *rec = infimum;
    size_t nth_rec = 0;
    while ((rec = page_rec_get_next(rec)) != supremum) {
      is_min_rec =
          rec_get_info_bits(rec, page_is_comp(frame)) & REC_INFO_MIN_REC_FLAG;
      if (level > 0) {
        offsets = rec_get_offsets(rec, m_index, offsets, ULINT_UNDEFINED,
                                  UT_LOCATION_HERE, &heap);
        auto child = btr_node_ptr_get_child_page_no(rec, offsets);
        sout << "child=" << child << ", ";
      }
      ut_ad(nth_rec == 0 || !is_min_rec);
      nth_rec++;
    }
    size_t n_recs = page_header_get_field(frame, PAGE_N_RECS);
    total_rows += n_recs;
    sout << "n_recs=" << n_recs << "}";
    page_no = next_page_no;
    mtr.commit();
    ++i;
  }
  sout << "], total_rows=" << total_rows << "]";
  return sout.str();
}

bool Btree_load::validate_index(dict_index_t *index) {
  mtr_t mtr;
  mtr_start(&mtr);
  mtr_s_lock(dict_index_get_lock(index), &mtr, UT_LOCATION_HERE);

  auto size_in_pages = btr_get_size(index, BTR_TOTAL_SIZE, &mtr);
  mtr_commit(&mtr);

  /* Skip validate index for large trees. For debug version, there is not much
  point testing very large data. The validation takes too long time and results
  in timeout. Explicit check table would do the job. For 16k page size 4k pages
  is 4K x 16K = 64M */
  if (size_in_pages > 4 * 1024) {
    return true;
  }
  return btr_validate_index(index, nullptr, false);
}

#endif /* UNIV_DEBUG */

dberr_t Btree_load::finish(bool is_err, const bool subtree) noexcept {
  ut_ad(!m_index->table->is_temporary());
  /* Assert that the index online status has not changed */

  ut_ad(m_index->online_status == m_index_online || is_err);
  if (m_level_ctxs.empty()) {
    /* The table is empty. The root page of the index tree
    is already in a consistent state. No need to flush. */
    return DB_SUCCESS;
  }

  page_no_t last_page_no{FIL_NULL};

  auto err = finalize_page_loads(is_err, last_page_no);
  if (err != DB_SUCCESS) {
    is_err = true;
  }

  add_to_bulk_flusher(true);
  m_bulk_flusher.wait_to_stop();

  if (!is_err) {
    /* Flusher could have stopped flushing pages in case of an interruption.
    Need to check here before proceeding to ensure all pages are flushed. */
    if (is_interrupted()) {
      err = DB_INTERRUPTED;
      is_err = true;

    } else if (m_bulk_flusher.is_error()) {
      /* Check for other errors in bulk flusher. */
      err = m_bulk_flusher.get_error();
      is_err = true;
    }
  }

  /* @note After this point, the bulk loaded pages can be accessed using
  regular mtr via buffer pool. */

  if (!is_err && !subtree) {
    err = load_root_page(last_page_no);
    ut_ad(validate_index(m_index));
  }

  /* Ensure that remaining pages modified without redo log is flushed here. */
  ut_d(dict_sync_check check(true));
  ut_ad(!sync_check_iterate(check));
  return err;
}

std::ostream &Btree_load::print_left_pages(std::ostream &out) const {
  out << "[Leftmost Pages: ";
  for (const page_no_t page_no : m_first_page_nos) {
    out << page_no << ",";
  }
  out << "]";
  return out;
}

std::ostream &Btree_load::print_right_pages(std::ostream &out) const {
  out << "[Rightmost Pages: ";
  for (const page_no_t page_no : m_last_page_nos) {
    out << page_no << ",";
  }
  out << "]";
  return out;
}

#ifdef UNIV_DEBUG
void Btree_load::print_tree_pages() const {
  std::ostringstream sout;
  sout << "Tree: " << std::endl;
  for (size_t level = m_first_page_nos.size(); level > 0; --level) {
    sout << print_pages_in_level(level - 1) << std::endl;
  }
  TLOG(sout.str());
}
#endif /* UNIV_DEBUG */

void Page_load::set_min_rec_flag() { set_min_rec_flag(m_mtr); }

bool Page_load::is_min_rec_flag() const {
  rec_t *first_rec = page_rec_get_next(page_get_infimum_rec(m_page));
  return rec_get_info_bits(first_rec, page_is_comp(m_page)) &
         REC_INFO_MIN_REC_FLAG;
}

void Page_load::set_min_rec_flag(mtr_t *mtr) {
  if (m_level == 0) {
    /* REC_INFO_MIN_REC_FLAG must be set only in non-leaf pages. */
    return;
  }
  const page_no_t left_sibling = get_prev();
  ut_a(left_sibling == FIL_NULL);
  rec_t *first_rec = page_rec_get_next(page_get_infimum_rec(m_page));
  btr_set_min_rec_mark(first_rec, mtr);
}

void Btree_load::force_evict(const Page_range_t &range,
                             const bool dirty_is_ok) {
  const space_id_t space_id = dict_index_get_space(m_index);
  const page_size_t page_size(dict_table_page_size(m_index->table));

  for (page_no_t p_no = range.first; p_no < range.second; ++p_no) {
    const page_id_t page_id(space_id, p_no);
    buf_page_force_evict(page_id, page_size, dirty_is_ok);
  }
}

void Btree_load::get_root_page_stat(Page_stat &stat) {
  const page_no_t subtree_root = get_subtree_root();
  ut_ad(subtree_root != FIL_NULL);
  mtr_t mtr;
  mtr.start();
  const space_id_t space_id = dict_index_get_space(m_index);
  const page_id_t page_id(space_id, subtree_root);
  const page_size_t page_size(dict_table_page_size(m_index->table));

  buf_block_t *block = btr_block_get(page_id, page_size, RW_S_LATCH,
                                     UT_LOCATION_HERE, m_index, &mtr);

  const auto page = buf_block_get_frame(block);
  stat.m_data_size = page_get_data_size(page);
  stat.m_n_recs = page_get_n_recs(page);
  mtr.commit();
}

void Page_load::free() {
  ut_ad(m_block->is_memory());
  buf_block_free(m_block);
  m_block = nullptr;
}

void Btree_load::track_extent(Page_extent *page_extent) {
  for (auto iter : m_extents_tracked) {
    ut_ad(iter != page_extent);
    if (page_extent == iter) {
      /* Ignore if already registered. */
      return;
    }
  }
  m_extents_tracked.push_back(page_extent);
}

bool Btree_load_compare::operator()(const Btree_load *l_btree,
                                    const Btree_load *r_btree) {
  const space_id_t space_id = dict_index_get_space(m_index);
  const page_size_t page_size(dict_table_page_size(m_index->table));
  const size_t l_root_level = l_btree->get_root_level();
  const size_t r_root_level = r_btree->get_root_level();

  ut_ad(!r_btree->m_first_page_nos.empty());
  ut_ad(!l_btree->m_first_page_nos.empty());

  const page_no_t l_page_no = l_btree->m_last_page_nos[l_root_level];
  const page_no_t r_page_no = r_btree->m_first_page_nos[r_root_level];
  const page_id_t l_page_id(space_id, l_page_no);
  const page_id_t r_page_id(space_id, r_page_no);
  Scoped_heap local_heap(2048, UT_LOCATION_HERE);
  mtr_t local_mtr;
  local_mtr.start();
  buf_block_t *l_block = buf_page_get(l_page_id, page_size, RW_X_LATCH,
                                      UT_LOCATION_HERE, &local_mtr);
  buf_block_t *r_block = buf_page_get(r_page_id, page_size, RW_X_LATCH,
                                      UT_LOCATION_HERE, &local_mtr);

  byte *l_frame = buf_block_get_frame(l_block);
  byte *r_frame = buf_block_get_frame(r_block);

  rec_t *r_rec = page_rec_get_next(page_get_infimum_rec(r_frame));
  rec_t *l_rec = page_rec_get_next(page_get_infimum_rec(l_frame));

  auto heap = local_heap.get();

  ulint *l_offsets = rec_get_offsets(l_rec, m_index, nullptr, ULINT_UNDEFINED,
                                     UT_LOCATION_HERE, &heap);
  ulint *r_offsets = rec_get_offsets(r_rec, m_index, nullptr, ULINT_UNDEFINED,
                                     UT_LOCATION_HERE, &heap);

  const bool spatial_index_non_leaf = false;
  const bool cmp_btree_recs = false;
  ulint matched_fields{};
  int rec_order =
      cmp_rec_rec(l_rec, r_rec, l_offsets, r_offsets, m_index,
                  spatial_index_non_leaf, &matched_fields, cmp_btree_recs);
  local_mtr.commit();
  return (rec_order < 0);
}

dberr_t Btree_load::init() {
  m_heap_order = mem_heap_create(16 * 1024, UT_LOCATION_HERE);
  if (m_heap_order == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  return DB_SUCCESS;
}

void Bulk_flusher::info() {
  const size_t sleep_duration = s_sleep_duration.count();
  const size_t total_sleep_ms = m_wait_time.count() / 1000;
  ib::info(ER_IB_BULK_FLUSHER_INFO, m_n_sleep, sleep_duration, total_sleep_ms,
           m_pages_flushed);
}

void Bulk_extent_allocator::Extent_cache::init(size_t max_range) {
  ut_ad(max_range <= S_MAX_RANGES);
  m_max_range = std::min(max_range, S_MAX_RANGES);

  m_num_allocated.store(0);
  m_num_consumed.store(0);
}

uint64_t Bulk_extent_allocator::init(dict_table_t *table, trx_t *trx,
                                     size_t size, size_t num_threads,
                                     bool in_pages) {
  m_table = table;
  m_concurrency = num_threads;
  m_trx = trx;
  auto size_extent = size / (FSP_EXTENT_SIZE * UNIV_PAGE_SIZE);

  /* We try to cache about eight extents per thread. The value is further
  adjusted based on total load size. */
  size_t cache_size = 8 * num_threads;

  /* Cached extent would cause extra allocation. We define two limits here.
  cache_min: About 1.5% of actual data size to load */
  size_t cache_min = size_extent / 64;
  cache_size = std::max(cache_min, cache_size);

  /* cache_max: For small data loads, we allow not beyond 6% */
  size_t cache_max = 4 * cache_min;
  cache_size = std::min(cache_max, cache_size);

  /* Switch to page based allocation without cache if the number is less than
  two extents per thread. */
  if (in_pages || cache_size < 2 * num_threads || cache_size < 4) {
    std::ostringstream mesg_strm_1;
    mesg_strm_1 << "Innodb:  Allocate by Page, cache: " << cache_size
                << " [min: " << cache_min << ", max: " << cache_max
                << "] threads: " << num_threads;
    ib::info(ER_BULK_LOADER_INFO, mesg_strm_1.str().c_str());

    m_type = Type::PAGE;
    return 0;
  }
  m_type = Type::EXTENT;

  auto extend_size = std::min(S_BULK_EXTEND_SIZE_MAX, cache_size);
  uint64_t extend_bytes = extend_size * FSP_EXTENT_SIZE * UNIV_PAGE_SIZE;

  /* Divide between leaf and non-leaf extents. */
  const size_t min_size = 2;
  size_t non_leaf_size = cache_size / 16;
  non_leaf_size = std::max(min_size, non_leaf_size);

  size_t leaf_size =
      cache_size > non_leaf_size ? (cache_size - non_leaf_size) : 0;
  leaf_size = std::max(min_size, leaf_size);

  /* Hard limit: 2K * 1M = 2G for each of leaf and non-leaf segments. */
  leaf_size = std::min(leaf_size, S_MAX_RANGES);
  non_leaf_size = std::min(non_leaf_size, S_MAX_RANGES);

  m_leaf_extents.init(leaf_size);
  m_non_leaf_extents.init(non_leaf_size);

  m_consumer_wait_count = 0;
  m_allocator_wait_count = 0;

  m_consumer_wait_time = std::chrono::microseconds::zero();
  m_allocator_wait_time = std::chrono::microseconds::zero();

  std::ostringstream mesg_strm_2;
  mesg_strm_2 << "Innodb:  Allocate by Extent, cache: " << cache_size
              << " [min: " << cache_min << ", max: " << cache_max
              << "] [leaf: " << leaf_size << ", non-leaf: " << non_leaf_size
              << "] threads: " << num_threads << " Extend: " << extend_size;
  ib::info(ER_BULK_LOADER_INFO, mesg_strm_2.str().c_str());

  return extend_bytes;
}

void Bulk_extent_allocator::start() {
  std::thread alloc_thread([this]() {
    Runnable runnable{bulk_alloc_thread_key, 0};
    runnable([this]() { return run(); });
  });
  m_thread = std::move(alloc_thread);
}

void Bulk_extent_allocator::stop() {
  if (!m_thread.joinable()) {
    return;
  }
  std::unique_lock lk(m_mutex);
  m_stop = true;
  lk.unlock();

  m_allocator_condition.notify_one();
  m_thread.join();

  const size_t allocator_wait_ms = m_allocator_wait_time.count() / 1000;
  const size_t consumer_wait_ms = m_consumer_wait_time.count() / 1000;

  std::ostringstream mesg_strm_1;
  mesg_strm_1 << "Innodb: Allocated, Leaf: " << m_leaf_extents.m_num_consumed
              << " | " << m_leaf_extents.m_num_allocated
              << " Non Leaf: " << m_non_leaf_extents.m_num_consumed << " | "
              << m_non_leaf_extents.m_num_allocated
              << ", Wait Stat allocator: " << allocator_wait_ms << " ms | "
              << m_allocator_wait_count << " consumer(s): " << consumer_wait_ms
              << " ms | " << m_consumer_wait_count;
  ib::info(ER_BULK_LOADER_INFO, mesg_strm_1.str().c_str());
}

bool Bulk_extent_allocator::is_interrupted() {
  return (m_trx != nullptr && trx_is_interrupted(m_trx));
}

dberr_t Bulk_extent_allocator::allocate_page(bool is_leaf,
                                             Page_range_t &range) {
  auto index = m_table->first_index();
  const space_id_t space_id = index->space;

  log_free_check();
  mtr_t mtr;
  mtr.start();
  mtr.x_lock(dict_index_get_lock(index), UT_LOCATION_HERE);

  const page_no_t n_pages = 1;
  const ulint n_ext = 1;
  ulint n_reserved;

  if (!fsp_reserve_free_extents(&n_reserved, space_id, n_ext, FSP_NORMAL, &mtr,
                                n_pages)) {
    mtr.commit();
    return DB_OUT_OF_FILE_SPACE;
  }

  page_t *root = btr_root_get(index, &mtr);

  size_t header_offset = is_leaf ? PAGE_BTR_SEG_LEAF : PAGE_BTR_SEG_TOP;
  fseg_header_t *seg_header = root + PAGE_HEADER + header_offset;

  const page_size_t page_size = dict_table_page_size(index->table);

  fil_space_t *space = fil_space_acquire(space_id);

  fseg_inode_t *inode = fseg_inode_get(seg_header, space_id, page_size, &mtr);

  page_no_t page_no = fseg_alloc_page_no(space, page_size, inode, FIL_NULL,
                                         FSP_NO_DIR, &mtr IF_DEBUG(, true));
  fil_space_release(space);

  range.first = page_no;
  range.second = range.first + 1;

  mtr.commit();

  if (n_reserved > 0) {
    fil_space_release_free_extents(space_id, n_reserved);
  }
  return DB_SUCCESS;
}

dberr_t Bulk_extent_allocator::allocate_extent(bool is_leaf, mtr_t &mtr,
                                               Page_range_t &range) {
  auto index = m_table->first_index();
  return btr_extent_alloc(index, is_leaf, range, &mtr);
}

dberr_t Bulk_extent_allocator::allocate(bool is_leaf, bool alloc_page,
                                        Page_range_t &range,
                                        std::function<void()> &fn_wait_begin,
                                        std::function<void()> &fn_wait_end) {
  if (m_type == Type::PAGE || alloc_page) {
    return allocate_page(is_leaf, range);
  }

  auto &extents = is_leaf ? m_leaf_extents : m_non_leaf_extents;

  std::unique_lock lk(m_mutex);
  bool trigger = false;

  if (extents.get_range(range, trigger)) {
    if (trigger) {
      lk.unlock();
      m_allocator_condition.notify_one();
    }
    return DB_SUCCESS;
  }

  ut_ad(extents.is_empty());

  if (fn_wait_begin) {
    fn_wait_begin();
  }
  auto start_time = std::chrono::steady_clock::now();

  auto stop_wait = [&]() { return (!extents.is_empty()); };
  std::chrono::milliseconds sleep_duration{10};

  for (;;) {
    if (!extents.is_empty()) {
      break;
    }
    /* Allocator is stopped only at the end. */
    ut_ad(!m_stop);

    if (is_interrupted() || m_stop) {
      return DB_INTERRUPTED;
    }
    if (m_error != DB_SUCCESS) {
      return m_error;
    }
    m_consumer_condition.wait_for(lk, sleep_duration, stop_wait);
  }

  if (fn_wait_end) {
    fn_wait_end();
  }
  trigger = false;
  bool success = extents.get_range(range, trigger);
  ut_ad(success);

  auto end_time = std::chrono::steady_clock::now();

  auto time_micro = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);
  m_consumer_wait_time += time_micro;
  m_consumer_wait_count++;

  if (trigger) {
    lk.unlock();
    m_allocator_condition.notify_one();
  }
  return success ? DB_SUCCESS : DB_ERROR;
}

bool Bulk_extent_allocator::Extent_cache::get_range(Page_range_t &range,
                                                    bool &alloc_trigger) {
  alloc_trigger = false;
  if (is_empty()) {
    return false;
  }
  ++m_num_consumed;

  size_t index = m_num_consumed % m_max_range;

  ut_a(index < S_MAX_RANGES);
  range = m_ranges[index];

  ut_ad(m_num_consumed <= m_num_allocated);
  auto free = m_num_allocated - m_num_consumed;
  alloc_trigger = (free < m_max_range / 2);

  return true;
}

void Bulk_extent_allocator::Extent_cache::set_range(size_t index,
                                                    Page_range_t &range) {
  ut_a(m_max_range <= S_MAX_RANGES);

  size_t range_index = (m_num_allocated + index) % m_max_range;

  m_ranges[range_index] = range;
}

bool Bulk_extent_allocator::Extent_cache::check(size_t &num_alloc,
                                                size_t &num_free) const {
  num_alloc = 0;
  num_free = 0;

  ut_ad(m_num_allocated >= m_num_consumed);
  if (m_num_allocated < m_num_consumed) {
    return false;
  }
  auto free = m_num_allocated - m_num_consumed;
  ut_ad(m_max_range >= free);

  if (free < m_max_range) {
    num_alloc = m_max_range - free;
  }
  num_free = free;
  return true;
}

bool Bulk_extent_allocator::check(size_t &n_leaf, size_t &n_non_leaf,
                                  bool &trigger) {
  std::lock_guard<std::mutex> lk(m_mutex);
  /* Check if asked to stop. */
  if (m_stop) {
    return true;
  }
  /* If consumer should be triggered. */
  size_t free_leaf = 0;
  size_t free_non_leaf = 0;

  if (!m_leaf_extents.check(n_leaf, free_leaf) ||
      !m_non_leaf_extents.check(n_non_leaf, free_non_leaf)) {
    m_error = DB_ERROR;
    return true;
  }
  trigger = (free_leaf < m_concurrency || free_non_leaf < m_concurrency);
  return false;
}

void Bulk_extent_allocator::allocator_wait() const {
  auto stop_wait = [&]() -> bool {
    return (!m_leaf_extents.is_full() || !m_non_leaf_extents.is_full());
  };
  auto start_time = std::chrono::steady_clock::now();

  std::chrono::milliseconds sleep_duration{100};
  {
    std::unique_lock lk(m_mutex);
    m_allocator_condition.wait_for(lk, sleep_duration, stop_wait);
  }
  auto end_time = std::chrono::steady_clock::now();

  auto time_micro = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);
  m_allocator_wait_time += time_micro;
  m_allocator_wait_count++;
}

dberr_t Bulk_extent_allocator::allocate_extents(bool is_leaf,
                                                size_t num_extents) {
  if (num_extents == 0) {
    return DB_SUCCESS;
  }
  auto index = m_table->first_index();
  const space_id_t space_id = index->space;

  log_free_check();
  mtr_t mtr;
  mtr.start();
  mtr.x_lock(dict_index_get_lock(index), UT_LOCATION_HERE);

  const page_no_t n_pages = 1;
  ulint n_reserved = 0;

  auto on_exit = [&]() {
    mtr.commit();
    if (n_reserved > 0) {
      fil_space_release_free_extents(space_id, n_reserved);
    }
  };

  bool success = fsp_reserve_free_extents(&n_reserved, space_id, num_extents,
                                          FSP_NORMAL, &mtr, n_pages);
  if (!success || n_reserved != num_extents) {
    /* In error case, n_reserved is already set without actually reserving. */
    if (!success) {
      n_reserved = 0;
    }
    on_exit();
    return DB_OUT_OF_FILE_SPACE;
  }

  auto &extents = is_leaf ? m_leaf_extents : m_non_leaf_extents;
  dberr_t err = DB_SUCCESS;

  for (size_t index = 1; index <= num_extents; index++) {
    Page_range_t range;
    err = allocate_extent(is_leaf, mtr, range);
    if (err != DB_SUCCESS) {
      break;
    }
    extents.set_range(index, range);
  }
  on_exit();
  return err;
}

dberr_t Bulk_extent_allocator::run() {
  dberr_t err = DB_SUCCESS;

  for (;;) {
    size_t num_leaf = 0;
    size_t num_non_leaf = 0;
    bool trigger = false;

    if (check(num_leaf, num_non_leaf, trigger)) {
      break;
    }
    if (num_leaf == 0 && num_non_leaf == 0) {
      allocator_wait();
      continue;
    }
    const size_t MAX_ALLOC_IN_STEP = 128;
    num_non_leaf = std::min(num_non_leaf, MAX_ALLOC_IN_STEP);
    num_leaf = std::min(num_leaf, MAX_ALLOC_IN_STEP);

    err = allocate_extents(false, num_non_leaf);
    if (err != DB_SUCCESS) {
      break;
    }
    err = allocate_extents(true, num_leaf);
    if (err != DB_SUCCESS) {
      break;
    }

    /* Allow the extents to be consumed. */
    std::unique_lock lk(m_mutex);
    m_leaf_extents.m_num_allocated += num_leaf;
    m_non_leaf_extents.m_num_allocated += num_non_leaf;
    lk.unlock();

    if (trigger) {
      m_consumer_condition.notify_all();
    }
  }

  std::lock_guard<std::mutex> lk(m_mutex);
  ut_ad(m_stop || err != DB_SUCCESS);

  if (m_stop) {
    return err;
  }

  /* Should not exit without setting error. */
  if (err == DB_SUCCESS) {
    err = DB_ERROR;
  }
  m_error = err;
  return err;
}

bool Btree_load::is_tpc_enabled() const {
  const space_id_t space_id = m_index->space;
  fil_space_t *space = fil_space_acquire(space_id);
  const bool is_tpc = space->is_compressed();
  const page_size_t page_size(space->flags);
  fil_space_release(space);
  return is_tpc && !page_size.is_compressed() &&
         IORequest::is_punch_hole_supported();
}

bool Btree_load::is_tpe_enabled() const {
  const space_id_t space_id = m_index->space;
  fil_space_t *space = fil_space_acquire(space_id);
  bool is_tpe = space->is_encrypted();
  fil_space_release(space);
  return is_tpe;
}

dberr_t Btree_load::Merger::merge(bool sort) {
  remove_empty_subtrees();

  /* All sub-trees were empty. Nothing to do. */
  if (m_btree_loads.empty()) {
    return DB_SUCCESS;
  }

  if (sort) {
    Btree_load_compare cmp_obj(m_index);
    std::sort(m_btree_loads.begin(), m_btree_loads.end(), cmp_obj);
  }

  dberr_t err = DB_SUCCESS;

  for (size_t j = 1; j < m_btree_loads.size(); ++j) {
    size_t i = j - 1;

    Btree_load *l_btree = m_btree_loads[i];
    Btree_load *r_btree = m_btree_loads[j];

    err = l_btree->check_key_overlap(r_btree);

    if (err != DB_SUCCESS) {
      break;
    }
  }

  switch (err) {
    case DB_SUCCESS:
      break;

    case DB_DUPLICATE_KEY:
      my_error(ER_DUP_ENTRY_WITH_KEY_NAME, MYF(0), "", m_index->name());
      return err;

    case DB_DATA_NOT_SORTED:
    default:
      my_error(ER_LOAD_BULK_DATA_UNSORTED, MYF(0), "");
      return err;
  }
  ut_d(validate_boundaries());

  size_t highest_level;
  err = subtree_link_levels(highest_level);

  if (err == DB_SUCCESS) {
    err = add_root_for_subtrees(highest_level);
  }

  ut_ad(err != DB_SUCCESS || Btree_load::validate_index(m_index));
  return err;
}

void Btree_load::Merger::remove_empty_subtrees() {
  for (auto iter = m_btree_loads.begin(); iter != m_btree_loads.end();) {
    auto btree_load = *iter;
    if (btree_load->m_first_page_nos.empty()) {
      ut::delete_(btree_load);
      iter = m_btree_loads.erase(iter);
    } else {
      ++iter;
    }
  }
}

#ifdef UNIV_DEBUG
void Btree_load::Merger::validate_boundaries() {
  for (auto btree_load : m_btree_loads) {
    ut_ad(!btree_load->m_first_page_nos.empty());
    ut_ad(!btree_load->m_last_page_nos.empty());
    ut_ad(btree_load->m_first_page_nos.size() ==
          btree_load->m_last_page_nos.size());
  }
}
#endif /* UNIV_DEBUG */

dberr_t Btree_load::Merger::subtree_link_levels(size_t &highest_level) {
  mtr_t *mtr;
  Scoped_heap local_heap(2048, UT_LOCATION_HERE);
  using stl_alloc_t = mem_heap_allocator<Btree_load *>;
  stl_alloc_t local_alloc(local_heap.get());
  auto mtr_heap = local_heap.alloc(sizeof(mtr_t));
  mtr = new (mtr_heap) mtr_t();
  highest_level = 0;
  const space_id_t space_id = dict_index_get_space(m_index);
  const page_size_t page_size(dict_table_page_size(m_index->table));
  using List = std::list<Btree_load *, stl_alloc_t>;
  auto from_list_raw = local_heap.alloc(sizeof(List));
  if (from_list_raw == nullptr) {
    return DB_OUT_OF_MEMORY;
  }
  List *from_list = new (from_list_raw) List(local_alloc);
  auto to_list_raw = local_heap.alloc(sizeof(List));
  if (to_list_raw == nullptr) {
    return DB_OUT_OF_MEMORY;
  }
  List *to_list = new (to_list_raw) List(local_alloc);
  /* Populate the from list.  Also calculate the highest level. */
  for (auto btree_load : m_btree_loads) {
    const size_t root_level = btree_load->get_root_level();
    const size_t tree_height = root_level + 1;

    ib::info(ER_IB_BULK_LOAD_SUBTREE_INFO, (size_t)space_id,
             m_index->table_name, m_index->name(), tree_height,
             btree_load->m_stat_n_extents, btree_load->m_stat_n_pages);

    if (root_level > highest_level) {
      highest_level = root_level;
    }
    from_list->push_back(btree_load);
#ifdef UNIV_DEBUG
    for (size_t cur_level = 0; cur_level < root_level; ++cur_level) {
      const page_no_t leftmost = btree_load->m_first_page_nos[cur_level];
      const page_no_t rightmost = btree_load->m_last_page_nos[cur_level];
      ut_ad(rightmost != leftmost);
    }
    {
      const page_no_t leftmost = btree_load->m_first_page_nos[root_level];
      const page_no_t rightmost = btree_load->m_last_page_nos[root_level];
      ut_ad(rightmost == leftmost);
    }
#endif /* UNIV_DEBUG */
  }

  /** Loop till all subtrees are at same level or only one subtree remaining.*/
  const size_t MAX_LOOP = from_list->size();

  for (size_t n_loop = 0;; ++n_loop) {
    if (n_loop >= MAX_LOOP) {
      std::ostringstream sout;
      sout << "{From list size: " << from_list->size() << ", {";
      for (auto &b_load : *from_list) {
        sout << b_load->get_root_level() << ", ";
      }
      sout << "}}";
      LogErr(ERROR_LEVEL, ER_IB_BULK_LOAD_MERGE_FAIL,
             "Btree_load::Merger::subtree_link_levels()", m_index->table_name,
             m_index->name(), sout.str().c_str());
      ut_d(const bool bulk_load_merge_failed = false);
      ut_ad(bulk_load_merge_failed);
      return DB_FAIL;
    }

    /* There is only one subtree.  */
    if (from_list->size() == 1) {
      break;
    }

    while (!from_list->empty()) {
      Btree_load *subtree_1 = from_list->front();
      from_list->pop_front();
      if (from_list->empty()) {
        to_list->push_back(subtree_1);
        break;
      }
      Btree_load *subtree_2 = from_list->front();
      from_list->pop_front();

      /* All keys in subtree_1 must be less than all keys in subtree_2 */
      const size_t level_1 = subtree_1->get_root_level();
      const size_t level_2 = subtree_2->get_root_level();
      const size_t level = std::min(level_1, level_2);

      for (size_t cur_level = 0; cur_level <= level; cur_level++) {
        const page_no_t l_page_no = subtree_1->m_last_page_nos[cur_level];
        const page_no_t r_page_no = subtree_2->m_first_page_nos[cur_level];

        const page_id_t l_page_id(space_id, l_page_no);
        const page_id_t r_page_id(space_id, r_page_no);

        mtr->start();
        buf_block_t *l_block = buf_page_get(l_page_id, page_size, RW_X_LATCH,
                                            UT_LOCATION_HERE, mtr);
        buf_block_t *r_block = buf_page_get(r_page_id, page_size, RW_X_LATCH,
                                            UT_LOCATION_HERE, mtr);

#ifdef UNIV_DEBUG
        const page_type_t l_type = l_block->get_page_type();
        const page_type_t r_type = r_block->get_page_type();
        ut_a(l_type == FIL_PAGE_INDEX);
        ut_a(r_type == FIL_PAGE_INDEX);
#endif /* UNIV_DEBUG */

        byte *l_frame = buf_block_get_frame(l_block);
        byte *r_frame = buf_block_get_frame(r_block);

        ut_ad(buf_block_get_page_zip(l_block) == nullptr);
        ut_ad(buf_block_get_page_zip(r_block) == nullptr);

#ifdef UNIV_DEBUG
        /* Siblings need to be at the same level. */
        ulint l_level = btr_page_get_level(l_frame);
        ulint r_level = btr_page_get_level(r_frame);
        ut_ad(l_level == r_level);
#endif /* UNIV_DEBUG */

        btr_page_set_next(l_frame, nullptr, r_page_no, mtr);
        btr_page_set_prev(r_frame, nullptr, l_page_no, mtr);

        rec_t *first_rec = page_rec_get_next(page_get_infimum_rec(r_frame));

        btr_unset_min_rec_mark(r_block, first_rec, mtr);

#ifdef UNIV_DEBUG
        {
          rec_t *l_rec = page_rec_get_prev(page_get_supremum_rec(l_frame));
          rec_t *r_rec = first_rec;
          auto heap = local_heap.get();

          ulint *l_offsets =
              rec_get_offsets(l_rec, m_index, nullptr, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &heap);
          ulint *r_offsets =
              rec_get_offsets(r_rec, m_index, nullptr, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &heap);

          const bool spatial_index_non_leaf = false;
          const bool cmp_btree_recs = false;
          ulint matched_fields{};
          int rec_order = cmp_rec_rec(l_rec, r_rec, l_offsets, r_offsets,
                                      m_index, spatial_index_non_leaf,
                                      &matched_fields, cmp_btree_recs);
          ut_ad(rec_order <= 0);
        }
#endif /* UNIV_DEBUG */

        mtr->commit();
      }
      if (level_1 == level_2) {
        to_list->push_back(subtree_1);
        from_list->push_front(subtree_2);
      } else if (level_1 < level_2) {
        const page_no_t l_page_no = subtree_1->m_last_page_nos[level_1];
        const page_no_t r_page_no = subtree_2->m_first_page_nos[level_1 + 1];

        const page_id_t l_page_id(space_id, l_page_no);
        const page_id_t r_page_id(space_id, r_page_no);

        /* Load the two pages. */
        mtr->start();
        buf_block_t *l_block = buf_page_get(l_page_id, page_size, RW_X_LATCH,
                                            UT_LOCATION_HERE, mtr);
        buf_block_t *r_block = buf_page_get(r_page_id, page_size, RW_X_LATCH,
                                            UT_LOCATION_HERE, mtr);

        byte *l_frame = buf_block_get_frame(l_block);
        byte *r_frame = buf_block_get_frame(r_block);

        auto r_first_rec = page_rec_get_next(page_get_infimum_rec(r_frame));
        btr_unset_min_rec_mark(r_block, r_first_rec, mtr);

        /* Obtain node ptr of left page. */
        auto l_first_rec = page_rec_get_next(page_get_infimum_rec(l_frame));
        ut_a(page_rec_is_user_rec(l_first_rec));
#ifdef UNIV_DEBUG
        TLOG("Creating node_ptr with child: " << l_page_no);
#endif /* UNIV_DEBUG */
        auto node_ptr = dict_index_build_node_ptr(
            m_index, l_first_rec, l_page_no, local_heap.get(), level_1);

        /* Insert node ptr into higher right page. */
        page_cur_t page_cur;
        page_cur_set_before_first(r_block, &page_cur);

        ulint *offsets{};
        mem_heap_t *heap = local_heap.get();
        rec_t *insert_rec = page_cur_tuple_insert(&page_cur, node_ptr, m_index,
                                                  &offsets, &heap, mtr);
        IF_DEBUG(bool split = false;);
        if (unlikely(insert_rec == nullptr)) {
          subtree_2->split_leftmost(r_block, level_1 + 1, node_ptr, mtr,
                                    highest_level);
          IF_DEBUG(split = true;);
        } else {
          btr_set_min_rec_mark(insert_rec, mtr);
#ifdef UNIV_DEBUG
          rec_t *next_rec = page_rec_get_next(insert_rec);
          const page_no_t right_page_no = btr_page_get_next(l_frame, mtr);
          ulint *node_ptr_offsets =
              rec_get_offsets(next_rec, m_index, nullptr, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &heap);
          const page_no_t right_child_no =
              btr_node_ptr_get_child_page_no(next_rec, node_ptr_offsets);
          ut_ad(right_page_no == right_child_no);
#endif /* UNIV_DEBUG */
        }
        mtr->commit();
        from_list->push_front(subtree_2);

        for (size_t cur_level = 0; cur_level <= level_1; cur_level++) {
          subtree_2->m_first_page_nos[cur_level] =
              subtree_1->m_first_page_nos[cur_level];
        }

#ifdef UNIV_DEBUG
        if (split) {
          subtree_2->print_tree_pages();
        } else {
          TLOG("SPLIT LEFTMOST did not happen");
        }
#endif /* UNIV_DEBUG */

        ut::delete_(subtree_1);
      } else if (level_1 > level_2) {
        /* Left subtree is taller. */
        const page_no_t l_page_no = subtree_1->m_last_page_nos[level_2 + 1];
        const page_no_t r_page_no = subtree_2->m_first_page_nos[level_2];

        const page_id_t l_page_id(space_id, l_page_no);
        const page_id_t r_page_id(space_id, r_page_no);

        /* Load the two pages. */
        mtr->start();
        buf_block_t *l_block = buf_page_get(l_page_id, page_size, RW_X_LATCH,
                                            UT_LOCATION_HERE, mtr);
        buf_block_t *r_block = buf_page_get(r_page_id, page_size, RW_X_LATCH,
                                            UT_LOCATION_HERE, mtr);

        byte *r_frame = buf_block_get_frame(r_block);

        /* Obtain node ptr of right page. */
        auto r_first_rec = page_rec_get_next(page_get_infimum_rec(r_frame));
        ut_a(page_rec_is_user_rec(r_first_rec));
        btr_unset_min_rec_mark(r_block, r_first_rec, mtr);

        auto node_ptr = dict_index_build_node_ptr(
            m_index, r_first_rec, r_page_no, local_heap.get(), level_2);

        /* Insert node ptr into higher left page. */
        page_cur_t page_cur;
        page_cur_search(l_block, m_index, node_ptr, &page_cur);

        ulint *offsets{};
        mem_heap_t *heap = local_heap.get();
        rec_t *inserted = page_cur_tuple_insert(&page_cur, node_ptr, m_index,
                                                &offsets, &heap, mtr);
        if (unlikely(inserted == nullptr)) {
          subtree_1->split_rightmost(l_block, level_2 + 1, node_ptr, mtr,
                                     highest_level);
        }
        mtr->commit();
        from_list->push_front(subtree_1);
        for (size_t cur_level = 0; cur_level <= level_2; cur_level++) {
          subtree_1->m_last_page_nos[cur_level] =
              subtree_2->m_last_page_nos[cur_level];
        }
        ut::delete_(subtree_2);
      }
    }
    std::swap(from_list, to_list);

    /* Check if all subtrees are same level. */
    const bool same_level =
        std::all_of(from_list->begin(), from_list->end(),
                    [highest_level](Btree_load *load) {
                      return load->get_root_level() == highest_level;
                    });

    if (same_level) {
      ut_ad(std::is_sorted(from_list->begin(), from_list->end(),
                           Btree_load_compare(m_index)));
      break;
    }
  }

  m_btree_loads.clear();
  while (!from_list->empty()) {
    Btree_load *subtree = from_list->front();
    from_list->pop_front();
    m_btree_loads.push_back(subtree);
  }
  return DB_SUCCESS;
}

dberr_t Btree_load::Merger::add_root_for_subtrees(const size_t highest_level) {
  /* This function uses mtr with MTR_LOG_NO_REDO and a flush observer. */
  dberr_t err{DB_SUCCESS};

  if (m_btree_loads.empty()) {
    return DB_SUCCESS;
  }

  std::vector<dtuple_t *> all_node_ptrs;
  size_t total_node_ptrs_size{0};
  ut_ad(std::is_sorted(m_btree_loads.begin(), m_btree_loads.end(),
                       Btree_load_compare(m_index)));
  const page_no_t root_page_no = dict_index_get_page(m_index);
  auto observer = m_trx->flush_observer;
  ut_a(observer != nullptr);

  size_t n_subtrees = 0;
  size_t n_root_data = 0;
  size_t n_root_recs = 0;
  for (size_t i = 0; i < m_btree_loads.size(); ++i) {
    const auto level = m_btree_loads[i]->get_root_level();
    if (level == highest_level) {
      n_subtrees++;
      Page_stat page_stat;
      m_btree_loads[i]->get_root_page_stat(page_stat);
      n_root_data += page_stat.m_data_size;
      n_root_recs += page_stat.m_n_recs;
    }
  }

  const size_t slot_size = page_dir_calc_reserved_space(n_root_recs);
  const size_t need_space = n_root_data + slot_size;
  const size_t max_free = get_max_free();
  const bool level_incr = (n_subtrees > 1) && (need_space >= max_free);
  size_t new_root_level = level_incr ? highest_level + 1 : highest_level;

  Page_load root_load(m_index, m_trx->id, root_page_no, new_root_level,
                      observer);

  /* Do not disable redo log for this mtr; it is used to free a page below.*/
  mtr_t mtr;
  mtr.start();
  mtr.x_lock(dict_index_get_lock(m_index), UT_LOCATION_HERE);

  auto guard = create_scope_guard([&mtr]() { mtr.commit(); });

  if (!level_incr) {
    err = root_load.init();
    if (err != DB_SUCCESS) {
      return err;
    }
  }

  bool min_rec = true;
  for (size_t i = 0; i < m_btree_loads.size(); ++i) {
    const page_no_t subtree_root = m_btree_loads[i]->get_subtree_root();
    const size_t tree_level = m_btree_loads[i]->get_root_level();

    if (tree_level != highest_level) {
      /* Skip smaller sub-trees.  */
      continue;
    }

    const page_id_t page_id(dict_index_get_space(m_index), subtree_root);
    page_size_t page_size(dict_table_page_size(m_index->table));

    buf_block_t *subtree_block = btr_block_get(page_id, page_size, RW_X_LATCH,
                                               UT_LOCATION_HERE, m_index, &mtr);
    auto subtree_page = buf_block_get_frame(subtree_block);
    auto first_rec = page_rec_get_next(page_get_infimum_rec(subtree_page));
    ut_a(page_rec_is_user_rec(first_rec));

    if (highest_level > 0) {
      min_rec ? btr_set_min_rec_mark(first_rec, &mtr)
              : btr_unset_min_rec_mark(subtree_block, first_rec, &mtr);
    }

    if (level_incr) {
      auto node_ptr = dict_index_build_node_ptr(
          m_index, first_rec, subtree_root, m_tuple_heap.get(), highest_level);
      auto rec_size = rec_get_converted_size(m_index, node_ptr);

      if (min_rec) {
        node_ptr->set_min_rec_flag();
      }

      all_node_ptrs.push_back(node_ptr);
      total_node_ptrs_size += rec_size;
    } else {
      /* Copy the records from subtree root to actual root. */
      (void)root_load.copy_all(subtree_page);

      /* Remove the subtree root. */
      btr_page_free_low(m_index, subtree_block, highest_level, &mtr);
    }
    min_rec = false;
  }
  if (level_incr) {
    while (total_node_ptrs_size > max_free) {
      err =
          insert_node_ptrs(all_node_ptrs, total_node_ptrs_size, new_root_level);
      if (err != DB_SUCCESS) {
        return err;
      }
      new_root_level++;
    }

    root_load.set_level(new_root_level);
    auto err = root_load.init();
    ut_a(err == DB_SUCCESS);

    for (auto node_ptr : all_node_ptrs) {
      auto rec_size = rec_get_converted_size(m_index, node_ptr);
      err = root_load.insert(node_ptr, nullptr, rec_size);
      if (err != DB_SUCCESS) {
        return err;
      }
    }
  }
  root_load.set_next(FIL_NULL);
  root_load.set_prev(FIL_NULL);
  root_load.set_min_rec_flag();
  root_load.finish();

  mtr.commit();
  guard.commit();
  root_load.commit();
  return err;
}

void Btree_load::Merger::link_right_sibling(const page_no_t l_page_no,
                                            const page_no_t r_page_no) {
  ut_ad(l_page_no != FIL_NULL);

  const space_id_t space_id = dict_index_get_space(m_index);
  const page_id_t l_page_id(space_id, l_page_no);
  const page_size_t page_size(dict_table_page_size(m_index->table));
  auto observer = m_trx->flush_observer;
  ut_a(observer != nullptr);
  mtr_t mtr;

  mtr.start();
  mtr.set_log_mode(MTR_LOG_NO_REDO);
  mtr.set_flush_observer(observer);
  buf_block_t *l_block =
      buf_page_get(l_page_id, page_size, RW_X_LATCH, UT_LOCATION_HERE, &mtr);

#ifdef UNIV_DEBUG
  const page_type_t l_type = l_block->get_page_type();
  ut_a(l_type == FIL_PAGE_INDEX);
#endif /* UNIV_DEBUG */

  byte *l_frame = buf_block_get_frame(l_block);
  ut_ad(buf_block_get_page_zip(l_block) == nullptr);
  btr_page_set_next(l_frame, nullptr, r_page_no, &mtr);
  mtr_commit(&mtr);
}

dberr_t Btree_load::Merger::insert_node_ptrs(
    std::vector<dtuple_t *> &all_node_ptrs, size_t &total_node_ptrs_size,
    size_t level) {
  dberr_t err{DB_SUCCESS};
  std::vector<dtuple_t *> next_node_ptrs;
  size_t next_size{};
  auto observer = m_trx->flush_observer;
  ut_a(observer != nullptr);

  size_t need_space = total_node_ptrs_size;
  const size_t max_free = get_max_free();
  ut_a(need_space > max_free);

  /* Track the number of records (node pointers) inserted. */
  size_t n_recs{0};

  /* Allocate one page here. */
  auto page_load =
      ut::new_withkey<Page_load>(UT_NEW_THIS_FILE_PSI_KEY, m_index, m_trx->id,
                                 FIL_NULL, level, observer, nullptr);

  auto guard = create_scope_guard([&page_load]() { ut::delete_(page_load); });

  page_no_t prev_page_no{FIL_NULL};
  page_load->alloc();
  err = page_load->init();
  ut_a(err == DB_SUCCESS);

  page_load->set_prev(FIL_NULL);
  page_load->set_next(FIL_NULL);

  /* Lambda function to call once a page is loaded with rows. */
  auto page_completed = [&]() -> void {
    page_load->finish();

    std::vector<Page_load *> page_loads;
    page_load->commit();
    page_loads.push_back(page_load);

    /* Save the node pointer of the current page. */
    for (auto page_load_i : page_loads) {
      dtuple_t *next_node_ptr = page_load_i->get_node_ptr(m_tuple_heap.get());
      next_node_ptrs.push_back(next_node_ptr);
      next_size += rec_get_converted_size(m_index, next_node_ptr);
    }

    /* Link the siblings by updating FIL_PAGE_NEXT of left sibling. */
    if (prev_page_no != FIL_NULL) {
      link_right_sibling(prev_page_no, page_loads.front()->get_page_no());
    }
    prev_page_no = page_loads.back()->get_page_no();
  };

  all_node_ptrs[0]->set_min_rec_flag();

  for (auto node_ptr : all_node_ptrs) {
    /* Insert the node pointer into the current page.  Node pointers cannot
    have external fields, so nullptr is passed. */
    auto rec_size = rec_get_converted_size(m_index, node_ptr);
    big_rec_t *big_rec{};
    err = page_load->insert(node_ptr, big_rec, rec_size);
    n_recs++;

    if (n_recs == 1) {
      page_load->set_min_rec_flag();
    }

    if (err == DB_FAIL) {
      /* The current page has been populated with required number of records/
      node pointers, so take necessary action to proceed with the next page. */
      page_completed();

      /* Allocate next page. */
      page_load->reset();
      page_load->alloc();

      const dberr_t err2 = page_load->init();
      ut_a(err2 == DB_SUCCESS);
      page_load->set_prev(prev_page_no);
      page_load->set_next(FIL_NULL);
      err = page_load->insert(node_ptr, big_rec, rec_size);
    }

    if (err != DB_SUCCESS) {
      break;
    }
  }

  if (err != DB_SUCCESS) {
    return err;
  }

  page_completed();

  /* Update the function arguments with the new values. */
  all_node_ptrs.swap(next_node_ptrs);
  total_node_ptrs_size = next_size;
  return err;
}

dberr_t Btree_load::check_key_overlap(const Btree_load *r_btree) const {
  const Btree_load *l_btree = this;
  const space_id_t space_id = dict_index_get_space(m_index);
  const page_size_t page_size(dict_table_page_size(m_index->table));

  ut_ad(!r_btree->m_first_page_nos.empty());
  ut_ad(!l_btree->m_first_page_nos.empty());

  const page_no_t l_page_no = l_btree->m_last_page_nos[0];
  const page_no_t r_page_no = r_btree->m_first_page_nos[0];
  const page_id_t l_page_id(space_id, l_page_no);
  const page_id_t r_page_id(space_id, r_page_no);
  Scoped_heap local_heap(2048, UT_LOCATION_HERE);
  mtr_t local_mtr;
  local_mtr.start();
  buf_block_t *l_block = buf_page_get(l_page_id, page_size, RW_S_LATCH,
                                      UT_LOCATION_HERE, &local_mtr);
  buf_block_t *r_block = buf_page_get(r_page_id, page_size, RW_S_LATCH,
                                      UT_LOCATION_HERE, &local_mtr);

  byte *l_frame = buf_block_get_frame(l_block);
  byte *r_frame = buf_block_get_frame(r_block);

  rec_t *r_rec = page_rec_get_next(page_get_infimum_rec(r_frame));
  rec_t *l_rec = page_rec_get_prev(page_get_supremum_rec(l_frame));

  auto heap = local_heap.get();

  ulint *l_offsets = rec_get_offsets(l_rec, m_index, nullptr, ULINT_UNDEFINED,
                                     UT_LOCATION_HERE, &heap);
  ulint *r_offsets = rec_get_offsets(r_rec, m_index, nullptr, ULINT_UNDEFINED,
                                     UT_LOCATION_HERE, &heap);

  const bool spatial_index_non_leaf = false;
  const bool cmp_btree_recs = false;
  ulint matched_fields{};
  int rec_order =
      cmp_rec_rec(l_rec, r_rec, l_offsets, r_offsets, m_index,
                  spatial_index_non_leaf, &matched_fields, cmp_btree_recs);
  local_mtr.commit();

  if (rec_order < 0) {
    return DB_SUCCESS;
  }

  if (rec_order == 0) {
    return DB_DUPLICATE_KEY;
  }

  return DB_DATA_NOT_SORTED;
}

Level_ctx::~Level_ctx() {}

Page_load::~Page_load() noexcept {
  if (m_heap != nullptr) {
    /* mtr is allocated using heap. */
    if (m_mtr != nullptr) {
      ut_a(!m_mtr->is_active());
      m_mtr->~mtr_t();
    }
    mem_heap_free(m_heap);
  }
}

bool Btree_load::is_interrupted() const {
  return (m_trx != nullptr && trx_is_interrupted(m_trx));
}

#ifdef UNIV_DEBUG
bool Page_load::verify_space_id() const {
  const space_id_t space_id_1 = m_index->space;
  const space_id_t space_id_2 = m_block->page.id.space();
  const auto page = buf_block_get_frame(m_block);
  const space_id_t space_id_3 = page_get_space_id(page);
  ut_ad(space_id_1 == space_id_2);
  ut_ad(space_id_2 == space_id_3);
  ut_ad(space_id_1 == space_id_3);
  return true;
}
#endif /* UNIV_DEBUG */

void Btree_load::split_rightmost(buf_block_t *block, size_t level,
                                 dtuple_t *node_ptr, mtr_t *mtr,
                                 size_t &highest_level) {
  /* This split is only to be used while merging subtrees. */
  ut_a(level > 0);
  Scoped_heap local_heap(2048, UT_LOCATION_HERE);
  const page_size_t page_size = m_index->get_page_size();
  node_ptr->unset_min_rec_flag();
  /* Allocate a new page at the given level. */
  auto new_block = btr_page_alloc(m_index, 0, FSP_NO_DIR, level, mtr, mtr);
  auto new_page = buf_block_get_frame(new_block);
  auto new_page_zip = buf_block_get_page_zip(new_block);
  auto new_page_no = new_block->page.id.page_no();
  btr_page_create(new_block, new_page_zip, m_index, level, mtr);
  auto page_no = block->page.id.page_no();
  auto page = buf_block_get_frame(block);
  auto page_zip = buf_block_get_page_zip(block);
  /* Set the next node and previous node fields of new page */
  btr_page_set_next(new_page, new_page_zip, FIL_NULL, mtr);
  btr_page_set_prev(new_page, new_page_zip, page_no, mtr);
  /* Set the next page field of old page. */
  btr_page_set_next(page, page_zip, new_page_no, mtr);
  /* Insert given node_ptr to the new page. */
  page_cur_t page_cur;
  page_cur_set_before_first(new_block, &page_cur);
  ulint *offsets{};
  mem_heap_t *heap = local_heap.get();
  rec_t *inserted =
      page_cur_tuple_insert(&page_cur, node_ptr, m_index, &offsets, &heap, mtr);
  ut_a(inserted != nullptr);
  const size_t root_level = get_root_level();
  /* Obtain node pointer of new rightmost page. */
  auto first_rec = page_rec_get_next(page_get_infimum_rec(new_page));
  ut_a(page_rec_is_user_rec(first_rec));
  auto new_node_ptr = dict_index_build_node_ptr(m_index, first_rec, new_page_no,
                                                local_heap.get(), level);
  if (level == root_level) {
    /* Add a new level */
    page_no_t old_root = m_last_page_nos[level];
    const page_id_t old_root_pageid(m_index->space, old_root);
    /* Allocate a new page at the given level. */
    auto new_root_block =
        btr_page_alloc(m_index, 0, FSP_NO_DIR, level + 1, mtr, mtr);
    auto new_root_page = buf_block_get_frame(new_root_block);
    auto new_root_page_zip = buf_block_get_page_zip(new_root_block);
    auto new_root_page_no = new_root_block->page.id.page_no();
    m_last_page_nos.push_back(new_root_page_no);
    m_first_page_nos.push_back(new_root_page_no);
    btr_page_create(new_root_block, new_root_page_zip, m_index, level + 1, mtr);
    /* Set the next node and previous node fields of new page */
    btr_page_set_next(new_root_page, new_root_page_zip, FIL_NULL, mtr);
    btr_page_set_prev(new_root_page, new_root_page_zip, FIL_NULL, mtr);
    auto old_root_block = buf_page_get(old_root_pageid, page_size, RW_X_LATCH,
                                       UT_LOCATION_HERE, mtr);
    auto old_root_first_rec = page_rec_get_next(
        page_get_infimum_rec(buf_block_get_frame(old_root_block)));
    ut_a(page_rec_is_user_rec(old_root_first_rec));
    auto old_root_node_ptr = dict_index_build_node_ptr(
        m_index, old_root_first_rec, old_root, local_heap.get(), level + 1);
    /* Update the last_page_nos */
    m_last_page_nos[level] = new_page_no;
    page_cur_t page_cur;
    page_cur_set_before_first(new_root_block, &page_cur);
    ulint *offsets{};
    mem_heap_t *heap = local_heap.get();
    inserted = page_cur_tuple_insert(&page_cur, old_root_node_ptr, m_index,
                                     &offsets, &heap, mtr);
    btr_set_min_rec_mark(inserted, mtr);
    ut_a(inserted != nullptr);
    page_cur_move_to_next(&page_cur);
    inserted = page_cur_tuple_insert(&page_cur, new_node_ptr, m_index, &offsets,
                                     &heap, mtr);
    btr_unset_min_rec_mark(new_root_block, inserted, mtr);
    ut_a(inserted != nullptr);
    m_root_level++;
    if (m_root_level > highest_level) {
      highest_level = m_root_level;
    }
  } else {
    /* Obtain the parent node. */
    const page_no_t parent_page_no = m_last_page_nos[level + 1];
    const page_id_t parent_pageid(m_index->space, parent_page_no);
    auto parent_block = buf_page_get(parent_pageid, page_size, RW_X_LATCH,
                                     UT_LOCATION_HERE, mtr);
    /* Insert new node_ptr to the parent page. */
    page_cur_t page_cur;
    page_cur_search(parent_block, m_index, new_node_ptr, &page_cur);
    ulint *offsets{};
    mem_heap_t *heap = local_heap.get();
    inserted = page_cur_tuple_insert(&page_cur, new_node_ptr, m_index, &offsets,
                                     &heap, mtr);
    if (inserted == nullptr) {
      split_rightmost(parent_block, 1 + level, new_node_ptr, mtr,
                      highest_level);
    }
  }
  m_last_page_nos[level] = new_page_no;
}

void Btree_load::split_leftmost(buf_block_t *&block, size_t level,
                                dtuple_t *node_ptr, mtr_t *mtr,
                                size_t &highest_level) {
  /* Note: This is not really a split operation. */
  /* This split is only to be used while merging subtrees. */
  ut_a(level > 0);
  Scoped_heap local_heap(2048, UT_LOCATION_HERE);
  mem_heap_t *heap = local_heap.get();
  const page_size_t page_size = m_index->get_page_size();
  node_ptr->set_min_rec_flag();

  ulint *offsets{};

  /* First record of the block that is full. */
  auto first_rec_full_block =
      page_rec_get_next(page_get_infimum_rec(buf_block_get_frame(block)));

  btr_unset_min_rec_mark(block, first_rec_full_block, mtr);

  /* Allocate a new page at the given level. */
  auto new_block = btr_page_alloc(m_index, 0, FSP_NO_DIR, level, mtr, mtr);
  auto new_page = buf_block_get_frame(new_block);
  auto new_page_zip = buf_block_get_page_zip(new_block);
  auto new_page_no = new_block->page.id.page_no();
  btr_page_create(new_block, new_page_zip, m_index, level, mtr);

  auto page_no = block->page.id.page_no();
  auto page = buf_block_get_frame(block);
  auto page_zip = buf_block_get_page_zip(block);

  /* Node pointer of the full block. */
  auto node_ptr_of_full_block = dict_index_build_node_ptr(
      m_index, first_rec_full_block, page_no, local_heap.get(), level);

  /* Set the next node and previous node fields of new page */
  btr_page_set_next(new_page, new_page_zip, page_no, mtr);
  btr_page_set_prev(new_page, new_page_zip, FIL_NULL, mtr);

  /* Set the prev page field of old page. */
  btr_page_set_prev(page, page_zip, new_page_no, mtr);

  /* Insert given node_ptr to the new page. */
  page_cur_t page_cur;
  page_cur_set_before_first(new_block, &page_cur);

  rec_t *inserted =
      page_cur_tuple_insert(&page_cur, node_ptr, m_index, &offsets, &heap, mtr);
  ut_a(inserted != nullptr);
  btr_set_min_rec_mark(inserted, mtr);

  m_first_page_nos[level] = new_page_no;
  const size_t root_level = get_root_level();

  /* Obtain node pointer of new leftmost page. */
#ifdef UNIV_DEBUG
  TLOG("Creating node_ptr with child: " << new_page_no);
#endif /* UNIV_DEBUG */
  auto new_node_ptr = dict_index_build_node_ptr(m_index, inserted, new_page_no,
                                                local_heap.get(), level);
  new_node_ptr->set_min_rec_flag();
  if (level == root_level) {
    /* Add a new level */
    page_no_t old_root = m_first_page_nos[level];
    const page_id_t old_root_pageid(m_index->space, old_root);
    /* Allocate a new page at the given level. */
    auto new_root_block =
        btr_page_alloc(m_index, 0, FSP_NO_DIR, level + 1, mtr, mtr);
    auto new_root_page = buf_block_get_frame(new_root_block);
    auto new_root_page_zip = buf_block_get_page_zip(new_root_block);
    auto new_root_page_no = new_root_block->page.id.page_no();
    m_last_page_nos.push_back(new_root_page_no);
    m_first_page_nos.push_back(new_root_page_no);
    btr_page_create(new_root_block, new_root_page_zip, m_index, level + 1, mtr);

    /* Set the next node and previous node fields of new page */
    btr_page_set_next(new_root_page, new_root_page_zip, FIL_NULL, mtr);
    btr_page_set_prev(new_root_page, new_root_page_zip, FIL_NULL, mtr);

    auto old_root_block = buf_page_get(old_root_pageid, page_size, RW_X_LATCH,
                                       UT_LOCATION_HERE, mtr);
    auto old_root_first_rec = page_rec_get_next(
        page_get_infimum_rec(buf_block_get_frame(old_root_block)));
    ut_a(page_rec_is_user_rec(old_root_first_rec));

#ifdef UNIV_DEBUG
    TLOG("Creating node_ptr with child (old_root): " << old_root);
#endif /* UNIV_DEBUG */
    auto old_root_node_ptr = dict_index_build_node_ptr(
        m_index, old_root_first_rec, old_root, local_heap.get(), level + 1);

    /* Update the first_page_nos */
    m_first_page_nos[level] = new_page_no;

    page_cur_t page_cur;
    page_cur_set_before_first(new_root_block, &page_cur);

    ulint *offsets{};
    mem_heap_t *heap = local_heap.get();
    inserted = page_cur_tuple_insert(&page_cur, new_node_ptr, m_index, &offsets,
                                     &heap, mtr);
    btr_set_min_rec_mark(inserted, mtr);
    ut_a(inserted != nullptr);
    page_cur_move_to_next(&page_cur);

    inserted = page_cur_tuple_insert(&page_cur, old_root_node_ptr, m_index,
                                     &offsets, &heap, mtr);
    btr_unset_min_rec_mark(new_root_block, inserted, mtr);
    ut_a(inserted != nullptr);
    m_root_level++;
    if (m_root_level > highest_level) {
      highest_level = m_root_level;
    }
  } else {
    /* Obtain the parent node. */
    const page_no_t parent_page_no = m_first_page_nos[level + 1];
    const page_id_t parent_pageid(m_index->space, parent_page_no);
    auto parent_block = buf_page_get(parent_pageid, page_size, RW_X_LATCH,
                                     UT_LOCATION_HERE, mtr);

    page_cur_t page_cur;
    page_cur_set_before_first(parent_block, &page_cur);
    page_cur_move_to_next(&page_cur);
    page_cur_move_to_next(&page_cur);
    rec_t *second_rec = page_cur_get_rec(&page_cur);
    page_delete_rec_list_start(second_rec, parent_block, m_index, mtr);
    page_cur_set_before_first(parent_block, &page_cur);
    ulint *offsets{};
    mem_heap_t *heap = local_heap.get();
    inserted = page_cur_tuple_insert(&page_cur, node_ptr_of_full_block, m_index,
                                     &offsets, &heap, mtr);
    if (unlikely(inserted == nullptr)) {
      split_leftmost(parent_block, 1 + level, node_ptr_of_full_block, mtr,
                     highest_level);
    }

    /* Insert new node_ptr to the parent page. */
    page_cur_set_before_first(parent_block, &page_cur);
    inserted = page_cur_tuple_insert(&page_cur, new_node_ptr, m_index, &offsets,
                                     &heap, mtr);
    if (unlikely(inserted == nullptr)) {
      split_leftmost(parent_block, 1 + level, new_node_ptr, mtr, highest_level);
    } else {
      btr_set_min_rec_mark(inserted, mtr);
    }
  }

  /* Update the left most block in the argument. */
  block = new_block;
}
} /* namespace Btree_multi */
