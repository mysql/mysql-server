/*****************************************************************************

Copyright (c) 1995, 2022, Oracle and/or its affiliates.

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

/** @file buf/buf0lru.cc
 The database buffer replacement algorithm

 Created 11/5/1995 Heikki Tuuri
 *******************************************************/

#include "buf0lru.h"

#include "btr0btr.h"
#include "btr0sea.h"
#include "buf0buddy.h"
#include "buf0buf.h"
#include "buf0dblwr.h"
#include "buf0flu.h"
#include "buf0rea.h"
#include "buf0stats.h"
#include "fil0fil.h"
#include "hash0hash.h"
#include "ibuf0ibuf.h"
#include "log0recv.h"
#include "log0write.h"
#include "my_dbug.h"
#include "os0event.h"
#include "os0file.h"
#include "page0zip.h"
#include "srv0mon.h"
#include "srv0srv.h"
#include "sync0rw.h"
#include "trx0trx.h"
#include "ut0byte.h"
#include "ut0rnd.h"

/** The number of blocks from the LRU_old pointer onward, including
the block pointed to, must be buf_pool->LRU_old_ratio/BUF_LRU_OLD_RATIO_DIV
of the whole LRU list length, except that the tolerance defined below
is allowed. Note that the tolerance must be small enough such that for
even the BUF_LRU_OLD_MIN_LEN long LRU list, the LRU_old pointer is not
allowed to point to either end of the LRU list. */

constexpr uint32_t BUF_LRU_OLD_TOLERANCE = 20;

/** The minimum amount of non-old blocks when the LRU_old list exists
(that is, when there are more than BUF_LRU_OLD_MIN_LEN blocks).
@see buf_LRU_old_adjust_len */
constexpr uint32_t BUF_LRU_NON_OLD_MIN_LEN = 5;
static_assert(BUF_LRU_NON_OLD_MIN_LEN < BUF_LRU_OLD_MIN_LEN,
              "BUF_LRU_NON_OLD_MIN_LEN >= BUF_LRU_OLD_MIN_LEN");
/** When dropping the search hash index entries before deleting an ibd
file, we build a local array of pages belonging to that tablespace
in the buffer pool. Following is the size of that array.
We also release buf_pool->LRU_list_mutex after scanning this many pages of the
flush_list when dropping a table. This is to ensure that other threads
are not blocked for extended period of time when using very large
buffer pools. */
static const ulint BUF_LRU_DROP_SEARCH_SIZE = 1024;

/** We scan these many blocks when looking for a clean page to evict
during LRU eviction. */
static const ulint BUF_LRU_SEARCH_SCAN_THRESHOLD = 100;

/** If we switch on the InnoDB monitor because there are too few available
frames in the buffer pool, we set this to true */
static std::atomic_bool buf_lru_switched_on_innodb_mon = false;

/** These statistics are not 'of' LRU but 'for' LRU.  We keep count of I/O
 and page_zip_decompress() operations.  Based on the statistics,
 buf_LRU_evict_from_unzip_LRU() decides if we want to evict from
 unzip_LRU or the regular LRU.  From unzip_LRU, we will only evict the
 uncompressed frame (meaning we can evict dirty blocks as well).  From
 the regular LRU, we will evict the entire block (i.e.: both the
 uncompressed and compressed data), which must be clean. */

/** @{ */

/** Number of intervals for which we keep the history of these stats.
Each interval is 1 second, defined by the rate at which
srv_error_monitor_thread() calls buf_LRU_stat_update(). */
static const ulint BUF_LRU_STAT_N_INTERVAL = 50;

/** Co-efficient with which we multiply I/O operations to equate them
with page_zip_decompress() operations. */
static const ulint BUF_LRU_IO_TO_UNZIP_FACTOR = 50;

/** Sampled values buf_LRU_stat_cur.
Not protected by any mutex.  Updated by buf_LRU_stat_update(). */
static buf_LRU_stat_t buf_LRU_stat_arr[BUF_LRU_STAT_N_INTERVAL];

/** Cursor to buf_LRU_stat_arr[] that is updated in a round-robin fashion. */
static ulint buf_LRU_stat_arr_ind;

/** Current operation counters.  Not protected by any mutex.  Cleared
by buf_LRU_stat_update(). */
buf_LRU_stat_t buf_LRU_stat_cur;

/** Running sum of past values of buf_LRU_stat_cur.
Updated by buf_LRU_stat_update().  Not Protected by any mutex. */
buf_LRU_stat_t buf_LRU_stat_sum;

/** @} */

/** @name Heuristics for detecting index scan
@{ */
/** Move blocks to "new" LRU list only if the first access was at
least this many milliseconds ago.  Not protected by any mutex or latch. */
uint buf_LRU_old_threshold;
std::chrono::milliseconds get_buf_LRU_old_threshold() {
  return std::chrono::milliseconds{buf_LRU_old_threshold};
}

/** @} */

/** Takes a block out of the LRU list and page hash table.
If the block is compressed-only (BUF_BLOCK_ZIP_PAGE),
the object will be freed.

The caller must hold buf_pool->LRU_list_mutex, the buf_page_get_mutex() mutex
and the appropriate hash_lock. This function will release the
buf_page_get_mutex() and the hash_lock.

If a compressed page is freed other compressed pages may be relocated.

@param[in]      bpage           block, must contain a file page and
                                be in a state where it can be freed; there
                                may or may not be a hash index to the page
@param[in]      zip             true if should remove also the
                                compressed page of an uncompressed page
@param[in]      ignore_content  true if should ignore page content, since it
                                could be not initialized
@retval true if BUF_BLOCK_FILE_PAGE was removed from page_hash. The
caller needs to free the page to the free list
@retval false if BUF_BLOCK_ZIP_PAGE was removed from page_hash. In
this case the block is already returned to the buddy allocator. */
[[nodiscard]] static bool buf_LRU_block_remove_hashed(buf_page_t *bpage,
                                                      bool zip,
                                                      bool ignore_content);

/** Puts a file page whose has no hash index to the free list.
@param[in,out] block            Must contain a file page and be in a state
                                where it can be freed. */
static void buf_LRU_block_free_hashed_page(buf_block_t *block) noexcept;

/** Increases LRU size in bytes with page size inline function
@param[in]      bpage           control block
@param[in]      buf_pool        buffer pool instance */
static inline void incr_LRU_size_in_bytes(buf_page_t *bpage,
                                          buf_pool_t *buf_pool) {
  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

  buf_pool->stat.LRU_bytes += bpage->size.physical();

  ut_ad(buf_pool->stat.LRU_bytes <= buf_pool->curr_pool_size);
}

/** Determines if the unzip_LRU list should be used for evicting a victim
instead of the general LRU list.
@param[in,out]  buf_pool        buffer pool instance
@return true if should use unzip_LRU */
bool buf_LRU_evict_from_unzip_LRU(buf_pool_t *buf_pool) {
  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

  /* If the unzip_LRU list is empty, we can only use the LRU. */
  if (UT_LIST_GET_LEN(buf_pool->unzip_LRU) == 0) {
    return false;
  }

  /* If unzip_LRU is at most 10% of the size of the LRU list,
  then use the LRU.  This slack allows us to keep hot
  decompressed pages in the buffer pool. */
  if (UT_LIST_GET_LEN(buf_pool->unzip_LRU) <=
      UT_LIST_GET_LEN(buf_pool->LRU) / 10) {
    return false;
  }

  /* If eviction hasn't started yet, we assume by default
  that a workload is disk bound. */
  if (buf_pool->freed_page_clock == 0) {
    return true;
  }

  /* Calculate the average over past intervals, and add the values
  of the current interval. */
  ulint io_avg =
      buf_LRU_stat_sum.io / BUF_LRU_STAT_N_INTERVAL + buf_LRU_stat_cur.io;

  ulint unzip_avg =
      buf_LRU_stat_sum.unzip / BUF_LRU_STAT_N_INTERVAL + buf_LRU_stat_cur.unzip;

  /* Decide based on our formula.  If the load is I/O bound
  (unzip_avg is smaller than the weighted io_avg), evict an
  uncompressed frame from unzip_LRU.  Otherwise we assume that
  the load is CPU bound and evict from the regular LRU. */
  return (unzip_avg <= io_avg * BUF_LRU_IO_TO_UNZIP_FACTOR);
}

/** Attempts to drop page hash index on a batch of pages belonging to a
particular space id.
@param[in]      space_id        space id
@param[in]      page_size       page size
@param[in]      arr             array of page_no
@param[in]      count           number of entries in array */
static void buf_LRU_drop_page_hash_batch(space_id_t space_id,
                                         const page_size_t &page_size,
                                         const page_no_t *arr, ulint count) {
  ut_ad(count <= BUF_LRU_DROP_SEARCH_SIZE);

  for (ulint i = 0; i < count; ++i, ++arr) {
    /* While our only caller
    buf_LRU_drop_page_hash_for_tablespace()
    is being executed for DROP TABLE or similar,
    the table cannot be evicted from the buffer pool.
    Note: this should not be executed for DROP TABLESPACE,
    because DROP TABLESPACE would be refused if tables existed
    in the tablespace, and a previous DROP TABLE would have
    already removed the AHI entries. */
    btr_search_drop_page_hash_when_freed(page_id_t(space_id, *arr), page_size);
  }
}

/** When doing a DROP TABLE/DISCARD TABLESPACE we have to drop all page
hash index entries belonging to that table. This function tries to
do that in batch. Note that this is a 'best effort' attempt and does
not guarantee that ALL hash entries will be removed.
@param[in]      buf_pool        buffer pool instance
@param[in]      space_id        space id */
static void buf_LRU_drop_page_hash_for_tablespace(buf_pool_t *buf_pool,
                                                  space_id_t space_id) {
  bool found;
  const page_size_t page_size(fil_space_get_page_size(space_id, &found));

  if (!found) {
    /* Somehow, the tablespace does not exist.  Nothing to drop. */
    ut_d(ut_error);
    ut_o(return );
  }

  page_no_t *page_arr = static_cast<page_no_t *>(ut::malloc_withkey(
      UT_NEW_THIS_FILE_PSI_KEY, sizeof(page_no_t) * BUF_LRU_DROP_SEARCH_SIZE));

  ulint num_entries = 0;

  mutex_enter(&buf_pool->LRU_list_mutex);

scan_again:
  for (buf_page_t *bpage = UT_LIST_GET_LAST(buf_pool->LRU); bpage != nullptr;
       /* No op */) {
    buf_page_t *prev_bpage = UT_LIST_GET_PREV(LRU, bpage);

    ut_a(buf_page_in_file(bpage));

    if (buf_page_get_state(bpage) != BUF_BLOCK_FILE_PAGE ||
        bpage->id.space() != space_id || bpage->was_io_fixed()) {
      /* Compressed pages are never hashed.
      Skip blocks of other tablespaces.
      Skip I/O-fixed blocks (to be dealt with later). */
    next_page:
      bpage = prev_bpage;
      continue;
    }

    buf_block_t *block = reinterpret_cast<buf_block_t *>(bpage);

    mutex_enter(&block->mutex);

    block->ahi.validate();

    bool skip = bpage->buf_fix_count > 0 || !block->ahi.index;

    mutex_exit(&block->mutex);

    if (skip) {
      /* Skip this block, because there are
      no adaptive hash index entries
      pointing to it, or because we cannot
      drop them due to the buffer-fix. */
      goto next_page;
    }

    /* Store the page number so that we can drop the hash
    index in a batch later. */
    page_arr[num_entries] = bpage->id.page_no();
    ut_a(num_entries < BUF_LRU_DROP_SEARCH_SIZE);
    ++num_entries;

    if (num_entries < BUF_LRU_DROP_SEARCH_SIZE) {
      goto next_page;
    }

    /* Array full. We release the LRU list mutex to obey
    the latching order. */
    mutex_exit(&buf_pool->LRU_list_mutex);

    buf_LRU_drop_page_hash_batch(space_id, page_size, page_arr, num_entries);

    num_entries = 0;

    mutex_enter(&buf_pool->LRU_list_mutex);

    /* Note that we released the buf_pool->LRU_list_mutex above
    after reading the prev_bpage during processing of a
    page_hash_batch (i.e.: when the array was full).
    Because prev_bpage could belong to a compressed-only
    block, it may have been relocated, and thus the
    pointer cannot be trusted. Because bpage is of type
    buf_block_t, it is safe to dereference.

    bpage can change in the LRU list. This is OK because
    this function is a 'best effort' to drop as many
    search hash entries as possible and it does not
    guarantee that ALL such entries will be dropped. */

    /* If, however, bpage has been removed from LRU list
    to the free list then we should restart the scan. */
    if (bpage != nullptr && buf_page_get_state(bpage) != BUF_BLOCK_FILE_PAGE) {
      goto scan_again;
    }
  }

  mutex_exit(&buf_pool->LRU_list_mutex);

  /* Drop any remaining batch of search hashed pages. */
  buf_LRU_drop_page_hash_batch(space_id, page_size, page_arr, num_entries);
  ut::free(page_arr);
}

/** Try to pin the block in buffer pool. Once pinned, the block cannot be moved
within flush list or removed. The dirty page can be flushed when we release the
flush list mutex. We return without pinning in that case.
@param[in,out]  buf_pool  buffer pool instance
@param[in,out]  bpage     page to remove
@return true if page could be pinned successfully. */
static bool buf_page_try_pin(buf_pool_t *buf_pool, buf_page_t *bpage) {
  /* Allow pin/unpin with NULL. */
  if (bpage == nullptr) {
    return true;
  }

  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
  ut_ad(buf_flush_list_mutex_own(buf_pool));
  ut_ad(bpage->in_flush_list);

  /* To take care of the ABA problem that the block get flushed and
  re-inserted into flush list, we can check the oldest LSN. It is
  safe to access oldest LSN with flush list mutex protection as it
  is set and reset while adding and removing from flush list. */
  auto saved_oldest_lsn = bpage->get_oldest_lsn();

  buf_flush_list_mutex_exit(buf_pool);

  /* The LRU list mutex ensures that the page descriptor cannot be freed
  for both compressed and uncompressed page. */
  BPageMutex *block_mutex = buf_page_get_mutex(bpage);
  mutex_enter(block_mutex);

  bool pinned = false;

  /* Recheck the I/O fix and the flush list presence now that we
  hold the right mutex */
  if (buf_page_get_io_fix(bpage) == BUF_IO_NONE && bpage->is_dirty() &&
      saved_oldest_lsn == bpage->get_oldest_lsn()) {
    /* "Fix" the block so that the position cannot be
    changed after we release the buffer pool and
    block mutexes. */
    buf_page_set_sticky(bpage);
    pinned = true;
    ut_ad(bpage->in_flush_list);
  }

  mutex_exit(block_mutex);
  buf_flush_list_mutex_enter(buf_pool);

  return pinned;
}

/** Unpin the block in buffer pool. Ensure that the dirty page cannot be
flushed even though we need to release the flush list mutex momentarily.
@param[in,out]  buf_pool  buffer pool instance
@param[in,out]  bpage     page to remove */
static void buf_page_unpin(buf_pool_t *buf_pool, buf_page_t *bpage) {
  /* Allow pin/unpin with NULL. */
  if (bpage == nullptr) {
    return;
  }

  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
  ut_ad(buf_flush_list_mutex_own(buf_pool));

  buf_flush_list_mutex_exit(buf_pool);

  BPageMutex *block_mutex = buf_page_get_mutex(bpage);
  mutex_enter(block_mutex);

  /* "Unfix" the block now that we have both the LRU list and block mutexes . */
  buf_page_unset_sticky(bpage);

  buf_flush_list_mutex_enter(buf_pool);

  /* Release block mutex only after re-acquiring the flush list mutex to
  avoid any window where the page could have been flushed concurrently. The
  block mutex must be acquired before flushing a page. */
  mutex_exit(block_mutex);
}

/** If we have hogged the resources for too long then release the LRU list and
flush list mutexes and do a thread yield. Set the current page to "sticky" so
that it is not relocated during the yield. If I/O is started before sticky BIT
could be set, we skip yielding. The caller should restart the scan.
@param[in,out]  buf_pool        buffer pool instance
@param[in,out]  bpage           page to remove
@param[in]      processed       number of pages processed
@param[out]     restart         if caller needs to restart scan
@return true if yielded. */
[[nodiscard]] static bool buf_flush_try_yield(buf_pool_t *buf_pool,
                                              buf_page_t *bpage,
                                              size_t processed, bool &restart) {
  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
  ut_ad(buf_flush_list_mutex_own(buf_pool));

  restart = false;

  /* Every BUF_LRU_DROP_SEARCH_SIZE iterations in the loop we release
  buf_pool->LRU_list_mutex to let other threads do their job but only if the
  block is not IO fixed. This ensures that the block stays in its position in
  the flush_list. We read io_fix without block_mutex, because we will recheck
  with block_mutex.*/
  if (bpage != nullptr && processed >= BUF_LRU_DROP_SEARCH_SIZE &&
      bpage->was_io_fix_none()) {
    if (!buf_page_try_pin(buf_pool, bpage)) {
      restart = true;
      return false;
    }

    ut_ad(bpage->in_flush_list);
    ut_d(auto oldest_lsn = bpage->get_oldest_lsn());

    /* Now it is safe to release the LRU list mutex. */
    buf_flush_list_mutex_exit(buf_pool);
    mutex_exit(&buf_pool->LRU_list_mutex);

    /* Try and force a context switch. */
    std::this_thread::yield();

    mutex_enter(&buf_pool->LRU_list_mutex);
    buf_flush_list_mutex_enter(buf_pool);

    buf_page_unpin(buf_pool, bpage);

    /* Should not have been removed from the flush list during the yield. */
    ut_ad(bpage->in_flush_list);

    /* The oldest LSN change would mean the page is removed and inserted back.
    This ABA issue is handled during pinning and should not be the case. */
    ut_ad(oldest_lsn == bpage->get_oldest_lsn());

    return true;
  }
  return false;
}

/** Check if a dirty page should be flushed or removed based on space ID and
flush observer.
@param[in]  page        dirty page in flush list
@param[in]  observer    Flush observer
@param[in]  space       Space ID
@return true, if page should considered for flush or removal. */
static inline bool check_page_flush_observer(buf_page_t *page,
                                             const Flush_observer *observer,
                                             space_id_t space) {
  /* If no flush observer then compare space ID. */
  if (observer == nullptr) {
    return (space == page->id.space());
  }
  /* Otherwise, match the flush observer pointer. */
  return (observer == page->get_flush_observer());
}

/** Attempts to remove a single page from flush list. It is fine to
skip flush if the page flush is already in progress.
@param[in,out]  buf_pool  buffer pool instance
@param[in,out]  bpage     page to remove
@return true if page could be removed successfully. */
static bool remove_page_flush_list(buf_pool_t *buf_pool, buf_page_t *bpage) {
  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
  ut_ad(buf_flush_list_mutex_own(buf_pool));

  /* It is safe to check bpage->space and bpage->io_fix while holding
  buf_pool->LRU_list_mutex only. We will repeat the check of io_fix
  under block_mutex later, this is just an optimization to avoid the
  mutex acquisition if its likely io_fix is not NONE. */
  if (bpage->was_io_fixed()) {
    /* We cannot remove this page during this scan. */
    return false;
  }
  BPageMutex *block_mutex = buf_page_get_mutex(bpage);

  /* We don't have to worry about bpage becoming a dangling pointer by a
  compressed page flush list relocation because we hold LRU mutex. */
  buf_flush_list_mutex_exit(buf_pool);
  mutex_enter(block_mutex);

  bool removed = false;
  /* Recheck the page I/O fix and the flush list presence now that we hold
  the right mutex. */
  if (buf_page_get_io_fix(bpage) == BUF_IO_NONE && bpage->is_dirty()) {
    buf_flush_remove(bpage);
    removed = true;
  }

  mutex_exit(block_mutex);
  buf_flush_list_mutex_enter(buf_pool);

  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
  return removed;
}

/** Remove all dirty pages belonging to a given tablespace inside a specific
buffer pool instance. The pages still remain a part of LRU and are evicted from
the list as they age towards the tail of the LRU. We don't check for interrupt
as we must finish the operation. Usually this function is called as a cleanup
work after an interrupt is received.
@param[in,out]  buf_pool  buffer pool instance
@param[in]      id        space id for which to remove or flush pages
@param[in]      observer  flush observer to identify specific pages
@retval DB_SUCCESS if all freed
@retval DB_FAIL if not all freed and caller should call function again. */
[[nodiscard]] static dberr_t remove_pages_flush_list(buf_pool_t *buf_pool,
                                                     space_id_t id,
                                                     Flush_observer *observer) {
  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

  buf_flush_list_mutex_enter(buf_pool);

  buf_page_t *prev = nullptr;
  size_t processed = 0;
  dberr_t error = DB_SUCCESS;

  for (buf_page_t *bpage = UT_LIST_GET_LAST(buf_pool->flush_list);
       bpage != nullptr; bpage = prev) {
    ut_a(buf_page_in_file(bpage));

    /* Save the previous page before freeing the current one. */
    prev = UT_LIST_GET_PREV(list, bpage);

    if (check_page_flush_observer(bpage, observer, id)) {
      /* Try to PIN the previous page before removing. This would let us
      continue the current iteration after removal. */
      bool pinned = buf_page_try_pin(buf_pool, prev);

      /* Try to remove the current page even if pin failed. */
      bool removed = remove_page_flush_list(buf_pool, bpage);

      if (!pinned) {
        /* We should not trust prev pointer as PIN was unsuccessful. */
        error = DB_FAIL;
        break;
      }

      if (!removed) {
        /* Currently we come back and re-check. The iteration can continue.
        In future, it is also possible to wait for the concurrent IO to complete
        to avoid re-scanning. */
        error = DB_FAIL;
      }

      buf_page_unpin(buf_pool, prev);
    }

    ++processed;

    /* Yield if we have hogged the CPU and mutexes for too long. */
    bool restart = false;
    if (buf_flush_try_yield(buf_pool, prev, processed, restart)) {
      ut_ad(!restart);
      /* Reset the batch size counter if we had to yield. */
      processed = 0;
    }

    if (restart) {
      /* The previous page is already flushed or being flushed. We need at least
      another iteration. Current iteration can continue. */
      error = DB_FAIL;
    }
  }

  buf_flush_list_mutex_exit(buf_pool);
  return error;
}

/** Flushes a single page inside a buffer pool instance.
@param[in,out]  buf_pool  buffer pool instance
@param[in,out]  bpage     page to flush
@return true if page was flushed. */
static bool flush_page_flush_list(buf_pool_t *buf_pool, buf_page_t *bpage) {
  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
  ut_ad(buf_flush_list_mutex_own(buf_pool));

  if (bpage->was_io_fixed()) {
    return false;
  }

  BPageMutex *block_mutex = buf_page_get_mutex(bpage);

  /* We don't have to worry about bpage becoming a dangling pointer by a
  compressed page flush list relocation because we hold LRU mutex. */
  buf_flush_list_mutex_exit(buf_pool);

  mutex_enter(block_mutex);
  bool flushed = false;

  if (buf_flush_ready_for_flush(bpage, BUF_FLUSH_SINGLE_PAGE)) {
    /* We trigger single page flush and async IO. However, if double write is
    used, dblwr::write() forces all single page flush to sync IO.
    1. It makes the function behaviour change from sync to async for temp
       tablespaces and if redo is disabled. The caller must not assume
       the page is flushed when we return flushed = T.
    2. For bulk flush async trigger could be better for performance and seems
       to be the case in 5.7. Need to validate if 8.0 forcing sync flush
       is intentional - No functional impact. */
    flushed = buf_flush_page(buf_pool, bpage, BUF_FLUSH_SINGLE_PAGE, false);
  }

  if (flushed) {
    /* During flush, we have already released the LRU list and block mutexes.
    Wake up possible simulated aio thread to actually post the writes to the
    operating system */
    os_aio_simulated_wake_handler_threads();
    mutex_enter(&buf_pool->LRU_list_mutex);
  } else {
    mutex_exit(block_mutex);
  }

  buf_flush_list_mutex_enter(buf_pool);

  ut_ad(!mutex_own(block_mutex));
  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

  return flushed;
}

/** Remove all dirty pages belonging to a given tablespace inside a specific
buffer pool instance when we are deleting the data file(s) of that
tablespace. The pages still remain a part of LRU and are evicted from
the list as they age towards the tail of the LRU.
@param[in,out]  buf_pool  buffer pool instance
@param[in]      id        space id for which to remove or flush pages
@param[in]      observer  flush observer
@param[in]      trx       transaction to check if the operation must be
                          interrupted, can be NULL
@retval DB_SUCCESS if all freed
@retval DB_FAIL if not all freed
@retval DB_INTERRUPTED if the transaction was interrupted */
[[nodiscard]] static dberr_t flush_pages_flush_list(buf_pool_t *buf_pool,
                                                    space_id_t id,
                                                    Flush_observer *observer,
                                                    const trx_t *trx) {
  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

  buf_flush_list_mutex_enter(buf_pool);

  buf_page_t *prev = nullptr;
  size_t processed = 0;

  dberr_t error = DB_SUCCESS;

  for (buf_page_t *bpage = UT_LIST_GET_LAST(buf_pool->flush_list);
       bpage != nullptr; bpage = prev) {
    ut_a(buf_page_in_file(bpage));

    /* Save the previous page before flushing the current one. */
    prev = UT_LIST_GET_PREV(list, bpage);

    if (check_page_flush_observer(bpage, observer, id)) {
      /* Try to PIN the previous page before flushing. This would let us
      continue the current iteration after flush. */
      bool pinned = buf_page_try_pin(buf_pool, prev);

      /* Try to flush the current page even if pin failed. */
      flush_page_flush_list(buf_pool, bpage);

      /* Currently we come back and re-check once flush is triggered. If the
      flush is unsuccessful we need to rescan too. So, we set the error for
      rescan unconditionally here. The iteration can continue if PIN was
      successful. */
      error = DB_FAIL;

      if (!pinned) {
        /* We should not trust prev pointer as PIN was unsuccessful. */
        break;
      }

      buf_page_unpin(buf_pool, prev);
    }

    ++processed;

    /* Yield if we have hogged the CPU and mutexes for too long. */
    bool restart = false;
    if (buf_flush_try_yield(buf_pool, prev, processed, restart)) {
      ut_ad(!restart);
      /* Reset the batch size counter if we had to yield. */
      processed = 0;
    }

    if (restart) {
      /* The previous page is already flushed or being flushed. We need at least
      another iteration. Current iteration can continue. */
      error = DB_FAIL;
    }

    /* The check for trx is interrupted is expensive, we want to check every
    N iterations. */
    if (processed == 0 && trx && trx_is_interrupted(trx)) {
      if (trx->flush_observer != nullptr) {
        trx->flush_observer->interrupted();
      }
      error = DB_INTERRUPTED;
      break;
    }
  }

  buf_flush_list_mutex_exit(buf_pool);
  return error;
}

/** Remove or flush all the dirty pages that belong to a given tablespace
inside a specific buffer pool instance. The pages will remain in the LRU
list and will be evicted from the LRU list as they age and move towards
the tail of the LRU list.
@param[in,out]  buf_pool        buffer pool instance
@param[in]      id              space id
@param[in]      observer        flush observer
@param[in]      flush           flush to disk if true, otherwise remove
                                the pages without flushing
@param[in]      trx             transaction to check if the operation
                                must be interrupted
@param[in]      strict          true, if no page from tablespace
                                can be in buffer pool just after flush */
static void buf_flush_dirty_pages(buf_pool_t *buf_pool, space_id_t id,
                                  Flush_observer *observer, bool flush,
                                  const trx_t *trx, bool strict) {
  dberr_t err;

  do {
    /* TODO: it should be possible to avoid locking the LRU list
    mutex here. */
    mutex_enter(&buf_pool->LRU_list_mutex);

    if (flush) {
      err = flush_pages_flush_list(buf_pool, id, observer, trx);

    } else {
      err = remove_pages_flush_list(buf_pool, id, observer);
    }

    mutex_exit(&buf_pool->LRU_list_mutex);

    ut_ad(buf_flush_validate(buf_pool));

    if (err == DB_FAIL) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    if (err == DB_INTERRUPTED && observer != nullptr) {
      ut_a(flush);

      flush = false;
      err = DB_FAIL;
    }

    /* DB_FAIL is a soft error, it means that the task wasn't
    completed, needs to be retried. */

    ut_ad(buf_flush_validate(buf_pool));

  } while (err == DB_FAIL);

  ut_ad(observer != nullptr || err == DB_INTERRUPTED || !strict ||
        buf_pool_get_dirty_pages_count(buf_pool, id, observer) == 0);
}

/** Remove all pages that belong to a given tablespace inside a specific
buffer pool instance when we are DISCARDing the tablespace.
@param[in,out]  buf_pool        buffer pool instance
@param[in]      id              space id */
static void buf_LRU_remove_all_pages(buf_pool_t *buf_pool, ulint id) {
  buf_page_t *bpage;

scan_again:
  mutex_enter(&buf_pool->LRU_list_mutex);

  auto all_freed = true;

  for (bpage = UT_LIST_GET_LAST(buf_pool->LRU); bpage != nullptr;
       /* No op */) {
    rw_lock_t *hash_lock;
    buf_page_t *prev_bpage;
    BPageMutex *block_mutex;

    ut_a(buf_page_in_file(bpage));
    ut_ad(bpage->in_LRU_list);

    prev_bpage = UT_LIST_GET_PREV(LRU, bpage);

    /* It is safe to check bpage->id.space() and bpage->io_fix
    while holding buf_pool->LRU_list_mutex only and later recheck
    while holding the buf_page_get_mutex() mutex.  */

    if (bpage->id.space() != id) {
      /* Skip this block, as it does not belong to
      the space that is being invalidated. */
      goto next_page;
    } else if (bpage->was_io_fixed()) {
      /* We cannot remove this page during this scan
      yet; maybe the system is currently reading it
      in, or flushing the modifications to the file */

      all_freed = false;
      goto next_page;
    } else {
      hash_lock = buf_page_hash_lock_get(buf_pool, bpage->id);

      rw_lock_x_lock(hash_lock, UT_LOCATION_HERE);

      block_mutex = buf_page_get_mutex(bpage);

      mutex_enter(block_mutex);

      if (bpage->id.space() != id || bpage->buf_fix_count > 0 ||
          (buf_page_get_io_fix(bpage) != BUF_IO_NONE)) {
        mutex_exit(block_mutex);

        rw_lock_x_unlock(hash_lock);

        /* We cannot remove this page during
        this scan yet; maybe the system is
        currently reading it in, or flushing
        the modifications to the file */

        all_freed = false;

        goto next_page;
      }
    }

    ut_ad(mutex_own(block_mutex));

    DBUG_PRINT("ib_buf", ("evict page " UINT32PF ":" UINT32PF " state %u",
                          bpage->id.space(), bpage->id.page_no(),
                          static_cast<unsigned>(bpage->state)));

    if (buf_page_get_state(bpage) != BUF_BLOCK_FILE_PAGE) {
      /* Do nothing, because the adaptive hash index
      covers uncompressed pages only. */
    } else if (((buf_block_t *)bpage)->ahi.index) {
      mutex_exit(&buf_pool->LRU_list_mutex);

      rw_lock_x_unlock(hash_lock);

      mutex_exit(block_mutex);

      /* Note that the following call will acquire
      and release block->lock X-latch.
      Note that the table cannot be evicted during
      the execution of ALTER TABLE...DISCARD TABLESPACE
      because MySQL is keeping the table handle open. */

      btr_search_drop_page_hash_when_freed(bpage->id, bpage->size);

      goto scan_again;
    } else {
      reinterpret_cast<buf_block_t *>(bpage)->ahi.assert_empty();
    }

    if (bpage->is_dirty()) {
      buf_flush_remove(bpage);
    }

    ut_ad(!bpage->in_flush_list);

    /* Remove from the LRU list. */

    if (buf_LRU_block_remove_hashed(bpage, true, false)) {
      buf_LRU_block_free_hashed_page((buf_block_t *)bpage);
    } else {
      ut_ad(block_mutex == &buf_pool->zip_mutex);
    }

    ut_ad(!mutex_own(block_mutex));

    /* buf_LRU_block_remove_hashed() releases the hash_lock */
    ut_ad(!rw_lock_own(hash_lock, RW_LOCK_X));
    ut_ad(!rw_lock_own(hash_lock, RW_LOCK_S));

  next_page:
    bpage = prev_bpage;
  }

  mutex_exit(&buf_pool->LRU_list_mutex);

  if (!all_freed) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    goto scan_again;
  }
}

/** Remove pages belonging to a given tablespace inside a specific
 buffer pool instance when we are deleting the data file(s) of that
 tablespace. The pages still remain a part of LRU and are evicted from
 the list as they age towards the tail of the LRU only if buf_remove
 is BUF_REMOVE_FLUSH_NO_WRITE. */
static void buf_LRU_remove_pages(
    buf_pool_t *buf_pool,    /*!< buffer pool instance */
    space_id_t id,           /*!< in: space id */
    buf_remove_t buf_remove, /*!< in: remove or flush strategy */
    const trx_t *trx,        /*!< to check if the operation must
                             be interrupted */
    bool strict)             /*!< in: true if no page from tablespace
                             can be in buffer pool just after flush */
{
  Flush_observer *observer = (trx == nullptr) ? nullptr : trx->flush_observer;

  switch (buf_remove) {
    case BUF_REMOVE_ALL_NO_WRITE:
      buf_LRU_remove_all_pages(buf_pool, id);
      break;

    case BUF_REMOVE_FLUSH_NO_WRITE:
      /* Pass trx as NULL to avoid interruption check. */
      buf_flush_dirty_pages(buf_pool, id, observer, false, nullptr, strict);
      break;

    case BUF_REMOVE_FLUSH_WRITE:
      buf_flush_dirty_pages(buf_pool, id, observer, true, trx, strict);

      if (observer == nullptr) {
        /* Ensure that all asynchronous IO is completed. */
        os_aio_wait_until_no_pending_writes();
        fil_flush(id);
      }
      break;

    case BUF_REMOVE_NONE:
      ut_error;
      break;
  }
}

void buf_LRU_flush_or_remove_pages(space_id_t id, buf_remove_t buf_remove,
                                   const trx_t *trx, bool strict) {
  /* Before we attempt to drop pages one by one we first
  attempt to drop page hash index entries in batches to make
  it more efficient. The batching attempt is a best effort
  attempt and does not guarantee that all pages hash entries
  will be dropped. We get rid of remaining page hash entries
  one by one below. */
  for (ulint i = 0; i < srv_buf_pool_instances; i++) {
    auto buf_pool = buf_pool_from_array(i);

    switch (buf_remove) {
      case BUF_REMOVE_ALL_NO_WRITE:
        buf_LRU_drop_page_hash_for_tablespace(buf_pool, id);
        break;

      case BUF_REMOVE_FLUSH_NO_WRITE:
        /* It is a DROP TABLE for a single table
        tablespace. No AHI entries exist because
        we already dealt with them when freeing up
        extents. */
      case BUF_REMOVE_FLUSH_WRITE:
        /* We allow read-only queries against the
        table, there is no need to drop the AHI entries. */
        break;

      case BUF_REMOVE_NONE:
        ut_error;
        break;
    }

    buf_LRU_remove_pages(buf_pool, id, buf_remove, trx, strict);
  }
}

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/** Insert a compressed block into buf_pool->zip_clean in the LRU order.
@param[in]      bpage   pointer to the block in question */
void buf_LRU_insert_zip_clean(buf_page_t *bpage) {
  buf_pool_t *buf_pool = buf_pool_from_bpage(bpage);

  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
  ut_ad(mutex_own(&buf_pool->zip_mutex));
  ut_ad(buf_page_get_state(bpage) == BUF_BLOCK_ZIP_PAGE);

  /* Find the first successor of bpage in the LRU list
  that is in the zip_clean list. */
  buf_page_t *b = bpage;

  do {
    b = UT_LIST_GET_NEXT(LRU, b);
  } while (b && buf_page_get_state(b) != BUF_BLOCK_ZIP_PAGE);

  /* Insert bpage before b, i.e., after the predecessor of b. */
  if (b != nullptr) {
    b = UT_LIST_GET_PREV(list, b);
  }

  if (b != nullptr) {
    UT_LIST_INSERT_AFTER(buf_pool->zip_clean, b, bpage);
  } else {
    UT_LIST_ADD_FIRST(buf_pool->zip_clean, bpage);
  }
}
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

/** Try to free an uncompressed page of a compressed block from the unzip
LRU list.  The compressed page is preserved, and it need not be clean.
@param[in]      buf_pool        buffer pool instance
@param[in]      scan_all        scan whole LRU list if true, otherwise
                                scan only srv_LRU_scan_depth / 2 blocks
@return true if freed */
static bool buf_LRU_free_from_unzip_LRU_list(buf_pool_t *buf_pool,
                                             bool scan_all) {
  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

  if (!buf_LRU_evict_from_unzip_LRU(buf_pool)) {
    return (false);
  }

  ulint scanned = 0;
  bool freed = false;

  for (buf_block_t *block = UT_LIST_GET_LAST(buf_pool->unzip_LRU);
       block != nullptr && !freed && (scan_all || scanned < srv_LRU_scan_depth);
       ++scanned) {
    buf_block_t *prev_block;

    prev_block = UT_LIST_GET_PREV(unzip_LRU, block);

    mutex_enter(&block->mutex);

    ut_ad(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);
    ut_ad(block->in_unzip_LRU_list);
    ut_ad(block->page.in_LRU_list);

    freed = buf_LRU_free_page(&block->page, false);

    if (!freed) {
      mutex_exit(&block->mutex);
    }

    block = prev_block;
  }

  if (scanned) {
    MONITOR_INC_VALUE_CUMULATIVE(MONITOR_LRU_UNZIP_SEARCH_SCANNED,
                                 MONITOR_LRU_UNZIP_SEARCH_SCANNED_NUM_CALL,
                                 MONITOR_LRU_UNZIP_SEARCH_SCANNED_PER_CALL,
                                 scanned);
  }

  return (freed);
}

/** Try to free a clean page from the common LRU list.
@param[in,out]  buf_pool        buffer pool instance
@param[in]      scan_all        scan whole LRU list if true, otherwise scan
                                only up to BUF_LRU_SEARCH_SCAN_THRESHOLD
@return true if freed */
static bool buf_LRU_free_from_common_LRU_list(buf_pool_t *buf_pool,
                                              bool scan_all) {
  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

  bool freed{};
  ulint scanned{};

  for (buf_page_t *bpage = buf_pool->lru_scan_itr.start();
       bpage != nullptr && !freed &&
       (scan_all || scanned < BUF_LRU_SEARCH_SCAN_THRESHOLD);
       ++scanned, bpage = buf_pool->lru_scan_itr.get()) {
    ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
    auto prev = UT_LIST_GET_PREV(LRU, bpage);
    auto block_mutex = buf_page_get_mutex(bpage);

    buf_pool->lru_scan_itr.set(prev);

    ut_ad(bpage->in_LRU_list);
    ut_ad(buf_page_in_file(bpage));

    const auto accessed = buf_page_is_accessed(bpage);

    if (bpage->was_stale()) {
      freed = buf_page_free_stale(buf_pool, bpage);
    } else {
      mutex_enter(block_mutex);

      if (buf_flush_ready_for_replace(bpage)) {
        freed = buf_LRU_free_page(bpage, true);
      }

      if (!freed) {
        mutex_exit(block_mutex);
      }
    }

    if (freed && accessed == std::chrono::steady_clock::time_point{}) {
      /* Keep track of pages that are evicted without
      ever being accessed. This gives us a measure of
      the effectiveness of readahead */
      ++buf_pool->stat.n_ra_pages_evicted;
    }

    ut_ad(!mutex_own(block_mutex));

    if (freed) {
      break;
    }
  }

  if (scanned) {
    MONITOR_INC_VALUE_CUMULATIVE(MONITOR_LRU_SEARCH_SCANNED,
                                 MONITOR_LRU_SEARCH_SCANNED_NUM_CALL,
                                 MONITOR_LRU_SEARCH_SCANNED_PER_CALL, scanned);
  }

  ut_ad(freed ? !mutex_own(&buf_pool->LRU_list_mutex)
              : mutex_own(&buf_pool->LRU_list_mutex));

  return (freed);
}

bool buf_LRU_scan_and_free_block(buf_pool_t *buf_pool, bool scan_all) {
  bool freed = false;
  bool use_unzip_list = UT_LIST_GET_LEN(buf_pool->unzip_LRU) > 0;

  mutex_enter(&buf_pool->LRU_list_mutex);

  if (use_unzip_list) {
    freed = buf_LRU_free_from_unzip_LRU_list(buf_pool, scan_all);
  }

  if (!freed) {
    freed = buf_LRU_free_from_common_LRU_list(buf_pool, scan_all);
  }

  if (!freed) {
    mutex_exit(&buf_pool->LRU_list_mutex);
  }

  ut_ad(!mutex_own(&buf_pool->LRU_list_mutex));

  return (freed);
}

/** Returns true if less than 25 % of the buffer pool in any instance is
 available. This can be used in heuristics to prevent huge transactions
 eating up the whole buffer pool for their locks.
 @return true if less than 25 % of buffer pool left */
bool buf_LRU_buf_pool_running_out(void) {
  bool ret = false;

  for (ulint i = 0; i < srv_buf_pool_instances && !ret; i++) {
    buf_pool_t *buf_pool;

    buf_pool = buf_pool_from_array(i);

    if (!recv_recovery_is_on() &&
        UT_LIST_GET_LEN(buf_pool->free) + UT_LIST_GET_LEN(buf_pool->LRU) <
            std::min(buf_pool->curr_size, buf_pool->old_size) / 4) {
      ret = true;
    }
  }

  return ret;
}

/** Returns a free block from the buf_pool.
The block is taken off the free list.  If it is empty, returns NULL.
@param[in]      buf_pool        buffer pool instance
@return a free control block, or NULL if the buf_block->free list is empty */
buf_block_t *buf_LRU_get_free_only(buf_pool_t *buf_pool) {
  buf_block_t *block;

  mutex_enter(&buf_pool->free_list_mutex);

  block = reinterpret_cast<buf_block_t *>(UT_LIST_GET_FIRST(buf_pool->free));

  while (block != nullptr) {
    ut_ad(block->page.in_free_list);
    ut_d(block->page.in_free_list = false);
    ut_ad(!block->page.in_flush_list);
    ut_ad(!block->page.in_LRU_list);
    ut_a(!buf_page_in_file(&block->page));
    UT_LIST_REMOVE(buf_pool->free, &block->page);
    mutex_exit(&buf_pool->free_list_mutex);

    if (!buf_get_withdraw_depth(buf_pool) ||
        !buf_block_will_withdrawn(buf_pool, block)) {
      /* found valid free block */
      /* No adaptive hash index entries may point to
      a free block. */
      block->ahi.assert_empty();

      buf_block_set_state(block, BUF_BLOCK_READY_FOR_USE);

      UNIV_MEM_ALLOC(block->frame, UNIV_PAGE_SIZE);

      ut_ad(buf_pool_from_block(block) == buf_pool);

      return (block);
    }

    /* This should be withdrawn */
    mutex_enter(&buf_pool->free_list_mutex);
    UT_LIST_ADD_LAST(buf_pool->withdraw, &block->page);
    ut_d(block->in_withdraw_list = true);

    block = reinterpret_cast<buf_block_t *>(UT_LIST_GET_FIRST(buf_pool->free));
  }

  mutex_exit(&buf_pool->free_list_mutex);

  return (block);
}

/** Checks how much of buf_pool is occupied by non-data objects like
 AHI, lock heaps etc. Depending on the size of non-data objects this
 function will either assert or issue a warning and switch on the
 status monitor. */
static void buf_LRU_check_size_of_non_data_objects(
    const buf_pool_t *buf_pool) /*!< in: buffer pool instance */
{
  if (!recv_recovery_is_on() && buf_pool->curr_size == buf_pool->old_size &&
      UT_LIST_GET_LEN(buf_pool->free) + UT_LIST_GET_LEN(buf_pool->LRU) <
          buf_pool->curr_size / 20) {
    ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_132)
        << "Over 95 percent of the buffer pool is"
           " occupied by lock heaps or the adaptive hash index!"
           " Check that your transactions do not set too many"
           " row locks. Your buffer pool size is "
        << (buf_pool->curr_size / (1024 * 1024 / UNIV_PAGE_SIZE))
        << " MB."
           " Maybe you should make the buffer pool bigger?"
           " We intentionally generate a seg fault to print"
           " a stack trace on Linux!";

  } else if (!recv_recovery_is_on() &&
             buf_pool->curr_size == buf_pool->old_size &&
             (UT_LIST_GET_LEN(buf_pool->free) +
              UT_LIST_GET_LEN(buf_pool->LRU)) < buf_pool->curr_size / 3) {
    if (!buf_lru_switched_on_innodb_mon.exchange(true)) {
      /* Over 67 % of the buffer pool is occupied by lock
      heaps or the adaptive hash index. This may be a memory
      leak! */

      ib::warn(ER_IB_MSG_133)
          << "Over 67 percent of the buffer pool is"
             " occupied by lock heaps or the adaptive hash"
             " index! Check that your transactions do not"
             " set too many row locks. Your buffer pool"
             " size is "
          << (buf_pool->curr_size / (1024 * 1024 / UNIV_PAGE_SIZE))
          << " MB. Maybe you should make the buffer pool"
             " bigger?. Starting the InnoDB Monitor to print"
             " diagnostics, including lock heap and hash"
             " index sizes.";

      srv_innodb_needs_monitoring++;
    }

  } else if (buf_lru_switched_on_innodb_mon.load()) {
    if (buf_lru_switched_on_innodb_mon.exchange(false)) {
      srv_innodb_needs_monitoring--;
    }
  }
}

/** Returns a free block from the buf_pool. The block is taken off the
free list. If free list is empty, blocks are moved from the end of the
LRU list to the free list.
This function is called from a user thread when it needs a clean
block to read in a page. Note that we only ever get a block from
the free list. Even when we flush a page or find a page in LRU scan
we put it to free list to be used.
* iteration 0:
  * get a block from free list, success:done
  * if buf_pool->try_LRU_scan is set
    * scan LRU up to srv_LRU_scan_depth to find a clean block
    * the above will put the block on free list
    * success:retry the free list
  * flush one dirty page from tail of LRU to disk
    * the above will put the block on free list
    * success: retry the free list
* iteration 1:
  * same as iteration 0 except:
    * scan whole LRU list
    * scan LRU list even if buf_pool->try_LRU_scan is not set
* iteration > 1:
  * same as iteration 1 but sleep 10ms
@param[in,out]  buf_pool        buffer pool instance
@return the free control block, in state BUF_BLOCK_READY_FOR_USE */
buf_block_t *buf_LRU_get_free_block(buf_pool_t *buf_pool) {
  buf_block_t *block = nullptr;
  bool freed = false;
  ulint n_iterations = 0;
  ulint flush_failures = 0;
  bool started_monitor = false;

  ut_ad(!mutex_own(&buf_pool->LRU_list_mutex));

  MONITOR_INC(MONITOR_LRU_GET_FREE_SEARCH);
loop:
  buf_LRU_check_size_of_non_data_objects(buf_pool);

  /* If there is a block in the free list, take it */
  block = buf_LRU_get_free_only(buf_pool);

  if (block != nullptr) {
    ut_ad(!block->page.someone_has_io_responsibility());
    ut_ad(buf_pool_from_block(block) == buf_pool);
    memset(&block->page.zip, 0, sizeof block->page.zip);

    if (started_monitor) {
      srv_innodb_needs_monitoring--;
    }

    block->page.reset_flush_observer();
    return block;
  }

  MONITOR_INC(MONITOR_LRU_GET_FREE_LOOPS);

  freed = false;
  os_rmb;
  if (buf_pool->try_LRU_scan || n_iterations > 0) {
    /* If no block was in the free list, search from the
    end of the LRU list and try to free a block there.
    If we are doing for the first time we'll scan only
    tail of the LRU list otherwise we scan the whole LRU
    list. */
    freed = buf_LRU_scan_and_free_block(buf_pool, n_iterations > 0);

    if (!freed && n_iterations == 0) {
      /* Tell other threads that there is no point
      in scanning the LRU list. This flag is set to
      true again when we flush a batch from this
      buffer pool. */
      buf_pool->try_LRU_scan = false;
      os_wmb;
    }
  }

  if (freed) {
    goto loop;
  }

  if (n_iterations > 20 && srv_buf_pool_old_size == srv_buf_pool_size) {
    ib::warn(ER_IB_MSG_134)
        << "Difficult to find free blocks in the buffer pool"
           " ("
        << n_iterations << " search iterations)! " << flush_failures
        << " failed attempts to"
           " flush a page! Consider increasing the buffer pool"
           " size. It is also possible that in your Unix version"
           " fsync is very slow, or completely frozen inside"
           " the OS kernel. Then upgrading to a newer version"
           " of your operating system may help. Look at the"
           " number of fsyncs in diagnostic info below."
           " Pending flushes (fsync) log: "
        << log_pending_flushes()
        << "; buffer pool: " << fil_n_pending_tablespace_flushes << ". "
        << os_n_file_reads << " OS file reads, " << os_n_file_writes
        << " OS file writes, " << os_n_fsyncs
        << " OS fsyncs. Starting InnoDB Monitor to print"
           " further diagnostics to the standard output.";
    if (!started_monitor) {
      started_monitor = true;
      srv_innodb_needs_monitoring++;
    }
  }

  /* If we have scanned the whole LRU and still are unable to
  find a free block then we should sleep here to let the
  page_cleaner do an LRU batch for us. */

  if (!srv_read_only_mode) {
    os_event_set(buf_flush_event);
  }

  if (n_iterations > 1) {
    MONITOR_INC(MONITOR_LRU_GET_FREE_WAITS);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  /* No free block was found: try to flush the LRU list.
  This call will flush one page from the LRU and put it on the
  free list. That means that the free block is up for grabs for
  all user threads.

  TODO: A more elegant way would have been to return the freed
  up block to the caller here but the code that deals with
  removing the block from page_hash and LRU_list is fairly
  involved (particularly in case of compressed pages). We
  can do that in a separate patch sometime in future. */

  if (!buf_flush_single_page_from_LRU(buf_pool)) {
    MONITOR_INC(MONITOR_LRU_SINGLE_FLUSH_FAILURE_COUNT);
    ++flush_failures;
  }

  srv_stats.buf_pool_wait_free.add(n_iterations, 1);

  n_iterations++;

  goto loop;
}

/** Calculates the desired number for the old blocks list.
@param[in]      buf_pool        buffer pool instance */
static size_t calculate_desired_LRU_old_size(const buf_pool_t *buf_pool) {
  return std::min(UT_LIST_GET_LEN(buf_pool->LRU) *
                      static_cast<size_t>(buf_pool->LRU_old_ratio) /
                      BUF_LRU_OLD_RATIO_DIV,
                  UT_LIST_GET_LEN(buf_pool->LRU) -
                      (BUF_LRU_OLD_TOLERANCE + BUF_LRU_NON_OLD_MIN_LEN));
}

/** Moves the LRU_old pointer so that the length of the old blocks list
is inside the allowed limits.
@param[in]      buf_pool        buffer pool instance */
static inline void buf_LRU_old_adjust_len(buf_pool_t *buf_pool) {
  ulint old_len;
  ulint new_len;

  ut_a(buf_pool->LRU_old);
  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
  ut_ad(buf_pool->LRU_old_ratio >= BUF_LRU_OLD_RATIO_MIN);
  ut_ad(buf_pool->LRU_old_ratio <= BUF_LRU_OLD_RATIO_MAX);
  static_assert(BUF_LRU_OLD_RATIO_MIN * BUF_LRU_OLD_MIN_LEN >
                    BUF_LRU_OLD_RATIO_DIV * (BUF_LRU_OLD_TOLERANCE + 5),
                "BUF_LRU_OLD_RATIO_MIN * BUF_LRU_OLD_MIN_LEN <= "
                "BUF_LRU_OLD_RATIO_DIV * (BUF_LRU_OLD_TOLERANCE + 5)");
#ifdef UNIV_LRU_DEBUG
  /* buf_pool->LRU_old must be the first item in the LRU list
  whose "old" flag is set. */
  ut_a(buf_pool->LRU_old->old);
  ut_a(!UT_LIST_GET_PREV(LRU, buf_pool->LRU_old) ||
       !UT_LIST_GET_PREV(LRU, buf_pool->LRU_old)->old);
  ut_a(!UT_LIST_GET_NEXT(LRU, buf_pool->LRU_old) ||
       UT_LIST_GET_NEXT(LRU, buf_pool->LRU_old)->old);
#endif /* UNIV_LRU_DEBUG */

  old_len = buf_pool->LRU_old_len;
  new_len = calculate_desired_LRU_old_size(buf_pool);

  for (;;) {
    buf_page_t *LRU_old = buf_pool->LRU_old;

    ut_a(LRU_old);
    ut_ad(LRU_old->in_LRU_list);
#ifdef UNIV_LRU_DEBUG
    ut_a(LRU_old->old);
#endif /* UNIV_LRU_DEBUG */

    /* Update the LRU_old pointer if necessary */

    if (old_len + BUF_LRU_OLD_TOLERANCE < new_len) {
      buf_pool->LRU_old = LRU_old = UT_LIST_GET_PREV(LRU, LRU_old);
#ifdef UNIV_LRU_DEBUG
      ut_a(!LRU_old->old);
#endif /* UNIV_LRU_DEBUG */
      old_len = ++buf_pool->LRU_old_len;
      buf_page_set_old(LRU_old, true);

    } else if (old_len > new_len + BUF_LRU_OLD_TOLERANCE) {
      buf_pool->LRU_old = UT_LIST_GET_NEXT(LRU, LRU_old);
      old_len = --buf_pool->LRU_old_len;
      buf_page_set_old(LRU_old, false);
    } else {
      return;
    }
  }
}

/** Initializes the old blocks pointer in the LRU list. This function should be
called when the LRU list grows to BUF_LRU_OLD_MIN_LEN length.
@param[in,out]  buf_pool        buffer pool instance */
static void buf_LRU_old_init(buf_pool_t *buf_pool) {
  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
  ut_a(UT_LIST_GET_LEN(buf_pool->LRU) == BUF_LRU_OLD_MIN_LEN);

  /* We first initialize all blocks in the LRU list as old and then use
  the adjust function to move the LRU_old pointer to the right
  position */

  for (buf_page_t *bpage = UT_LIST_GET_LAST(buf_pool->LRU); bpage != nullptr;
       bpage = UT_LIST_GET_PREV(LRU, bpage)) {
    ut_ad(bpage->in_LRU_list);
    ut_ad(buf_page_in_file(bpage));

    /* This loop temporarily violates the
    assertions of buf_page_set_old(). */
    bpage->old = true;
  }

  buf_pool->LRU_old = UT_LIST_GET_FIRST(buf_pool->LRU);
  buf_pool->LRU_old_len = UT_LIST_GET_LEN(buf_pool->LRU);

  buf_LRU_old_adjust_len(buf_pool);
}

/** Remove a block from the unzip_LRU list if it belonged to the list.
@param[in]      bpage   control block */
static void buf_unzip_LRU_remove_block_if_needed(buf_page_t *bpage) {
  buf_pool_t *buf_pool = buf_pool_from_bpage(bpage);

  ut_ad(buf_page_in_file(bpage));
  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

  if (buf_page_belongs_to_unzip_LRU(bpage)) {
    buf_block_t *block = reinterpret_cast<buf_block_t *>(bpage);

    ut_ad(block->in_unzip_LRU_list);
    ut_d(block->in_unzip_LRU_list = false);

    UT_LIST_REMOVE(buf_pool->unzip_LRU, block);
  }
}

/** Adjust LRU hazard pointers if needed.
@param[in] buf_pool Buffer pool instance
@param[in] bpage Control block */
void buf_LRU_adjust_hp(buf_pool_t *buf_pool, const buf_page_t *bpage) {
  buf_pool->lru_hp.adjust(bpage);
  buf_pool->lru_scan_itr.adjust(bpage);
  buf_pool->single_scan_itr.adjust(bpage);
}

/** Removes a block from the LRU list.
@param[in]      bpage   control block */
static inline void buf_LRU_remove_block(buf_page_t *bpage) {
  buf_pool_t *buf_pool = buf_pool_from_bpage(bpage);

  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

  ut_a(buf_page_in_file(bpage));

  ut_ad(bpage->in_LRU_list);

  /* Important that we adjust the hazard pointers before removing
  bpage from the LRU list. */
  buf_LRU_adjust_hp(buf_pool, bpage);

  /* If the LRU_old pointer is defined and points to just this block,
  move it backward one step */

  if (bpage == buf_pool->LRU_old) {
    /* Below: the previous block is guaranteed to exist,
    because the LRU_old pointer is only allowed to differ
    by BUF_LRU_OLD_TOLERANCE from strict
    buf_pool->LRU_old_ratio/BUF_LRU_OLD_RATIO_DIV of the LRU
    list length. */
    buf_page_t *prev_bpage = UT_LIST_GET_PREV(LRU, bpage);

    ut_a(prev_bpage);
#ifdef UNIV_LRU_DEBUG
    ut_a(!prev_bpage->old);
#endif /* UNIV_LRU_DEBUG */
    buf_pool->LRU_old = prev_bpage;
    buf_page_set_old(prev_bpage, true);

    buf_pool->LRU_old_len++;
  }

  /* Remove the block from the LRU list */
  UT_LIST_REMOVE(buf_pool->LRU, bpage);
  ut_d(bpage->in_LRU_list = false);

  buf_pool->stat.LRU_bytes -= bpage->size.physical();

  buf_unzip_LRU_remove_block_if_needed(bpage);

  /* If the LRU list is so short that LRU_old is not defined,
  clear the "old" flags and return */
  if (UT_LIST_GET_LEN(buf_pool->LRU) < BUF_LRU_OLD_MIN_LEN) {
    for (auto bpage : buf_pool->LRU) {
      /* This loop temporarily violates the
      assertions of buf_page_set_old(). */
      bpage->old = false;
    }

    buf_pool->LRU_old = nullptr;
    buf_pool->LRU_old_len = 0;

    return;
  }

  ut_ad(buf_pool->LRU_old);

  /* Update the LRU_old_len field if necessary */
  if (buf_page_is_old(bpage)) {
    buf_pool->LRU_old_len--;
  }

  /* Adjust the length of the old block list if necessary */
  buf_LRU_old_adjust_len(buf_pool);
}

/** Adds a block to the LRU list of decompressed zip pages.
@param[in]      block   control block
@param[in]      old     true if should be put to the end of the list,
                        else put to the start */
void buf_unzip_LRU_add_block(buf_block_t *block, bool old) {
  buf_pool_t *buf_pool = buf_pool_from_block(block);

  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

  ut_a(buf_page_belongs_to_unzip_LRU(&block->page));

  ut_ad(!block->in_unzip_LRU_list);
  ut_d(block->in_unzip_LRU_list = true);

  if (old) {
    UT_LIST_ADD_LAST(buf_pool->unzip_LRU, block);
  } else {
    UT_LIST_ADD_FIRST(buf_pool->unzip_LRU, block);
  }
}

/** Adds a block to the LRU list. Please make sure that the page_size is
already set when invoking the function, so that we can get correct
page_size from the buffer page when adding a block into LRU
@param[in]      bpage   control block
@param[in]      old     true if should be put to the old blocks in the LRU list,
                        else put to the start; if the LRU list is very short,
                        the block is added to the start, regardless of this
                        parameter */
static inline void buf_LRU_add_block_low(buf_page_t *bpage, bool old) {
  buf_pool_t *buf_pool = buf_pool_from_bpage(bpage);

  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

  ut_a(buf_page_in_file(bpage));
  ut_ad(!bpage->in_LRU_list);

  if (!old || (UT_LIST_GET_LEN(buf_pool->LRU) < BUF_LRU_OLD_MIN_LEN)) {
    UT_LIST_ADD_FIRST(buf_pool->LRU, bpage);

    bpage->freed_page_clock = buf_pool->freed_page_clock;
  } else {
#ifdef UNIV_LRU_DEBUG
    /* buf_pool->LRU_old must be the first item in the LRU list
    whose "old" flag is set. */
    ut_a(buf_pool->LRU_old->old);
    ut_a(!UT_LIST_GET_PREV(LRU, buf_pool->LRU_old) ||
         !UT_LIST_GET_PREV(LRU, buf_pool->LRU_old)->old);
    ut_a(!UT_LIST_GET_NEXT(LRU, buf_pool->LRU_old) ||
         UT_LIST_GET_NEXT(LRU, buf_pool->LRU_old)->old);
#endif /* UNIV_LRU_DEBUG */
    UT_LIST_INSERT_AFTER(buf_pool->LRU, buf_pool->LRU_old, bpage);

    buf_pool->LRU_old_len++;
  }

  ut_d(bpage->in_LRU_list = true);

  incr_LRU_size_in_bytes(bpage, buf_pool);

  if (UT_LIST_GET_LEN(buf_pool->LRU) > BUF_LRU_OLD_MIN_LEN) {
    ut_ad(buf_pool->LRU_old);

    /* Adjust the length of the old block list if necessary */

    buf_page_set_old(bpage, old);
    buf_LRU_old_adjust_len(buf_pool);

  } else if (UT_LIST_GET_LEN(buf_pool->LRU) == BUF_LRU_OLD_MIN_LEN) {
    /* The LRU list is now long enough for LRU_old to become
    defined: init it */

    buf_LRU_old_init(buf_pool);
  } else {
    buf_page_set_old(bpage, buf_pool->LRU_old != nullptr);
  }

  /* If this is a zipped block with decompressed frame as well
  then put it on the unzip_LRU list */
  if (buf_page_belongs_to_unzip_LRU(bpage)) {
    buf_unzip_LRU_add_block((buf_block_t *)bpage, old);
  }
}

/** Adds a block to the LRU list. Please make sure that the page_size is
 already set when invoking the function, so that we can get correct
 page_size from the buffer page when adding a block into LRU */
void buf_LRU_add_block(buf_page_t *bpage, /*!< in: control block */
                       bool old) /*!< in: true if should be put to the old
                                  blocks in the LRU list, else put to the start;
                                  if the LRU list is very short, the block is
                                  added to the start, regardless of this
                                  parameter */
{
  buf_LRU_add_block_low(bpage, old);
}

/** Moves a block to the start of the LRU list.
@param[in]      bpage   control block */
void buf_LRU_make_block_young(buf_page_t *bpage) {
  buf_pool_t *buf_pool = buf_pool_from_bpage(bpage);

  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

  if (bpage->old) {
    buf_pool->stat.n_pages_made_young++;
  }

  buf_LRU_remove_block(bpage);
  buf_LRU_add_block_low(bpage, false);
}

/** Moves a block to the end of the LRU list.
@param[in]      bpage   control block */
void buf_LRU_make_block_old(buf_page_t *bpage) {
  ut_d(buf_pool_t *buf_pool =) buf_pool_from_bpage(bpage);

  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

  buf_LRU_remove_block(bpage);
  buf_LRU_add_block_low(bpage, true);
}

bool buf_LRU_free_page(buf_page_t *bpage, bool zip) {
  auto buf_pool = buf_pool_from_bpage(bpage);
  auto block_mutex = buf_page_get_mutex(bpage);
  auto hash_lock = buf_page_hash_lock_get(buf_pool, bpage->id);

  ut_ad(bpage->in_LRU_list);
  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
  ut_ad(mutex_own(block_mutex));
  ut_ad(buf_page_in_file(bpage));

  if (!buf_page_can_relocate(bpage)) {
    /* Do not free buffer fixed and I/O-fixed blocks. */
    return (false);
  }

#ifdef UNIV_IBUF_COUNT_DEBUG
  ut_a(ibuf_count_get(bpage->id) == 0);
#endif /* UNIV_IBUF_COUNT_DEBUG */

  buf_page_t *b{};
  auto is_dirty = bpage->is_dirty();

  if (zip || bpage->zip.data == nullptr) {
    /* This would completely free the block. */
    /* Do not completely free dirty blocks. */

    if (is_dirty) {
      return (false);
    }
  } else if (is_dirty && buf_page_get_state(bpage) != BUF_BLOCK_FILE_PAGE) {
    ut_ad(buf_page_get_state(bpage) == BUF_BLOCK_ZIP_DIRTY);

    return (false);

  } else if (buf_page_get_state(bpage) == BUF_BLOCK_FILE_PAGE) {
    b = buf_page_alloc_descriptor();
    ut_a(b);
  }

  ut_ad(buf_page_in_file(bpage));
  ut_ad(bpage->in_LRU_list);
  ut_ad(bpage->in_flush_list == is_dirty);

  DBUG_PRINT("ib_buf", ("free page " UINT32PF ":" UINT32PF, bpage->id.space(),
                        bpage->id.page_no()));

  mutex_exit(block_mutex);
  DBUG_EXECUTE_IF("buf_lru_free_page_delay_block_mutex_reacquisition",
                  std::this_thread::sleep_for(std::chrono::microseconds(100)););

  rw_lock_x_lock(hash_lock, UT_LOCATION_HERE);
  mutex_enter(block_mutex);
  is_dirty = bpage->is_dirty();

  if (!buf_page_can_relocate(bpage) ||
      ((zip || bpage->zip.data == nullptr) && is_dirty)) {
    rw_lock_x_unlock(hash_lock);

    if (b != nullptr) {
      buf_page_free_descriptor(b);
    }

    return (false);
  }

  if (is_dirty && buf_page_get_state(bpage) != BUF_BLOCK_FILE_PAGE) {
    ut_ad(buf_page_get_state(bpage) == BUF_BLOCK_ZIP_DIRTY);

    rw_lock_x_unlock(hash_lock);

    if (b != nullptr) {
      buf_page_free_descriptor(b);
    }

    return (false);
  }

  if (b != nullptr) {
    new (b) buf_page_t(*bpage);
  }

  ut_ad(rw_lock_own(hash_lock, RW_LOCK_X));
  ut_ad(buf_page_can_relocate(bpage));

  if (!buf_LRU_block_remove_hashed(bpage, zip, false)) {
    mutex_exit(&buf_pool->LRU_list_mutex);

    if (b != nullptr) {
      buf_page_free_descriptor(b);
    }
    return true;
  }
  ut_ad(!mutex_own(block_mutex));

  /* buf_LRU_block_remove_hashed() releases the hash_lock */
  ut_ad(!rw_lock_own(hash_lock, RW_LOCK_X) &&
        !rw_lock_own(hash_lock, RW_LOCK_S));

  /* We have just freed a BUF_BLOCK_FILE_PAGE. If b != nullptr
  then it was a compressed page with an uncompressed frame and
  we are interested in freeing only the uncompressed frame.
  Therefore we have to reinsert the compressed page descriptor
  into the LRU and page_hash (and possibly flush_list).
  if b == nullptr then it was a regular page that has been freed */

  if (b != nullptr) {
    auto prev_b = UT_LIST_GET_PREV(LRU, b);

    rw_lock_x_lock(hash_lock, UT_LOCATION_HERE);

    mutex_enter(block_mutex);

    ut_a(!buf_page_hash_get_low(buf_pool, b->id));

    b->state = b->is_dirty() ? BUF_BLOCK_ZIP_DIRTY : BUF_BLOCK_ZIP_PAGE;

    ut_ad(b->size.is_compressed());

    UNIV_MEM_DESC(b->zip.data, b->size.physical());

    /* The fields in_page_hash and in_LRU_list of
    the to-be-freed block descriptor should have
    been cleared in
    buf_LRU_block_remove_hashed(), which
    invokes buf_LRU_remove_block(). */
    ut_ad(!bpage->in_page_hash);
    ut_ad(!bpage->in_LRU_list);

    /* bpage->state was BUF_BLOCK_FILE_PAGE because
    b != NULL. The type cast below is thus valid. */
    ut_ad(!((buf_block_t *)bpage)->in_unzip_LRU_list);

    /* The fields of bpage were copied to b before
    buf_LRU_block_remove_hashed() was invoked. */
    ut_ad(!b->in_zip_hash);
    ut_ad(b->in_page_hash);
    ut_ad(b->in_LRU_list);

    HASH_INSERT(buf_page_t, hash, buf_pool->page_hash, b->id.hash(), b);

    /* Insert b where bpage was in the LRU list. */
    if (prev_b != nullptr) {
      ut_ad(prev_b->in_LRU_list);
      ut_ad(buf_page_in_file(prev_b));

      UT_LIST_INSERT_AFTER(buf_pool->LRU, prev_b, b);

      incr_LRU_size_in_bytes(b, buf_pool);

      if (buf_page_is_old(b)) {
        buf_pool->LRU_old_len++;
        if (buf_pool->LRU_old == UT_LIST_GET_NEXT(LRU, b)) {
          buf_pool->LRU_old = b;
        }
      }

      auto lru_len = UT_LIST_GET_LEN(buf_pool->LRU);

      if (lru_len > BUF_LRU_OLD_MIN_LEN) {
        ut_ad(buf_pool->LRU_old);
        /* Adjust the length of the
        old block list if necessary */
        buf_LRU_old_adjust_len(buf_pool);
      } else if (lru_len == BUF_LRU_OLD_MIN_LEN) {
        /* The LRU list is now long
        enough for LRU_old to become
        defined: init it */
        buf_LRU_old_init(buf_pool);
      }
#ifdef UNIV_LRU_DEBUG
      /* Check that the "old" flag is consistent
      in the block and its neighbours. */
      buf_page_set_old(b, buf_page_is_old(b));
#endif /* UNIV_LRU_DEBUG */
    } else {
      ut_d(b->in_LRU_list = false);
      buf_LRU_add_block_low(b, buf_page_is_old(b));
    }

    mutex_enter(&buf_pool->zip_mutex);
    rw_lock_x_unlock(hash_lock);
    if (b->state == BUF_BLOCK_ZIP_PAGE) {
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
      buf_LRU_insert_zip_clean(b);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
    } else {
      /* Relocate on buf_pool->flush_list. */
      buf_flush_relocate_on_flush_list(bpage, b);
    }

    bpage->zip.data = nullptr;

    page_zip_set_size(&bpage->zip, 0);

    bpage->size.copy_from(
        page_size_t(bpage->size.logical(), bpage->size.logical(), false));

    /* Prevent buf_page_get_gen() from
    decompressing the block while we release block_mutex. */

    buf_page_set_sticky(b);

    mutex_exit(&buf_pool->zip_mutex);

    mutex_exit(block_mutex);
  }

  mutex_exit(&buf_pool->LRU_list_mutex);

  /* Remove possible adaptive hash index on the page.
  The page was declared uninitialized by
  buf_LRU_block_remove_hashed().  We need to flag
  the contents of the page valid (which it still is) in
  order to avoid bogus Valgrind warnings.*/
  UNIV_MEM_VALID(((buf_block_t *)bpage)->frame, UNIV_PAGE_SIZE);
  btr_search_drop_page_hash_index((buf_block_t *)bpage, true);
  UNIV_MEM_INVALID(((buf_block_t *)bpage)->frame, UNIV_PAGE_SIZE);

  if (b != nullptr) {
    /* Compute and stamp the compressed page
    checksum while not holding any mutex.  The
    block is already half-freed
    (BUF_BLOCK_REMOVE_HASH) and removed from
    buf_pool->page_hash, thus inaccessible by any
    other thread. */

    ut_ad(b->size.is_compressed());

    BlockReporter reporter = BlockReporter(false, b->zip.data, b->size, false);

    const uint32_t checksum = reporter.calc_zip_checksum(
        static_cast<srv_checksum_algorithm_t>(srv_checksum_algorithm));

    mach_write_to_4(b->zip.data + FIL_PAGE_SPACE_OR_CHKSUM, checksum);
  }

  if (b != nullptr) {
    mutex_enter(&buf_pool->zip_mutex);

    buf_page_unset_sticky(b);

    mutex_exit(&buf_pool->zip_mutex);
  }

  buf_LRU_block_free_hashed_page((buf_block_t *)bpage);

  return (true);
}

/** Puts a block back to the free list.
@param[in]  block  block must not contain a file page */
void buf_LRU_block_free_non_file_page(buf_block_t *block) {
  void *data;
  buf_pool_t *buf_pool = buf_pool_from_block(block);

  switch (buf_block_get_state(block)) {
    case BUF_BLOCK_MEMORY:
    case BUF_BLOCK_READY_FOR_USE:
      break;
    default:
      ut_error;
  }

  block->ahi.assert_empty();
  ut_ad(!block->page.in_free_list);
  ut_ad(!block->page.in_flush_list);
  ut_ad(!block->page.in_LRU_list);

#ifdef UNIV_DEBUG
  /* Wipe contents of page to reveal possible stale pointers to it */
  memset(block->frame, '\0', UNIV_PAGE_SIZE);
#else
  /* Wipe page_no and space_id */
  memset(block->frame + FIL_PAGE_OFFSET, 0xfe, 4);
  memset(block->frame + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, 0xfe, 4);
#endif /* UNIV_DEBUG */
  UNIV_MEM_ASSERT_AND_FREE(block->frame, UNIV_PAGE_SIZE);
  data = block->page.zip.data;

  if (data != nullptr) {
    block->page.zip.data = nullptr;

    ut_ad(block->page.size.is_compressed());

    buf_buddy_free(buf_pool, data, block->page.size.physical());

    page_zip_set_size(&block->page.zip, 0);

    block->page.size.copy_from(page_size_t(block->page.size.logical(),
                                           block->page.size.logical(), false));
  }

#ifndef UNIV_HOTBACKUP
  buf_page_prepare_for_free(&block->page);
  ut_ad(block->page.get_space() == nullptr);
#endif /* !UNIV_HOTBACKUP */

  if (buf_get_withdraw_depth(buf_pool) &&
      buf_block_will_withdrawn(buf_pool, block)) {
    /* This should be withdrawn */
    buf_block_set_state(block, BUF_BLOCK_NOT_USED);
    mutex_enter(&buf_pool->free_list_mutex);
    UT_LIST_ADD_LAST(buf_pool->withdraw, &block->page);
    ut_d(block->in_withdraw_list = true);
    mutex_exit(&buf_pool->free_list_mutex);
  } else {
    buf_block_set_state(block, BUF_BLOCK_NOT_USED);
    mutex_enter(&buf_pool->free_list_mutex);
    UT_LIST_ADD_FIRST(buf_pool->free, &block->page);
    ut_d(block->page.in_free_list = true);
    ut_ad(!block->page.someone_has_io_responsibility());
    mutex_exit(&buf_pool->free_list_mutex);
  }
}

/** Takes a block out of the LRU list and page hash table.
If the block is compressed-only (BUF_BLOCK_ZIP_PAGE),
the object will be freed.

The caller must hold buf_pool->LRU_list_mutex, the buf_page_get_mutex() mutex
and the appropriate hash_lock. This function will release the
buf_page_get_mutex() and the hash_lock.

If a compressed page is freed other compressed pages may be relocated.

@param[in]      bpage           block, must contain a file page and
                                be in a state where it can be freed; there
                                may or may not be a hash index to the page
@param[in]      zip             true if should remove also the
                                compressed page of an uncompressed page
@param[in]      ignore_content  true if should ignore page content, since it
                                could be not initialized
@retval true if BUF_BLOCK_FILE_PAGE was removed from page_hash. The
caller needs to free the page to the free list
@retval false if BUF_BLOCK_ZIP_PAGE was removed from page_hash. In
this case the block is already returned to the buddy allocator. */
static bool buf_LRU_block_remove_hashed(buf_page_t *bpage, bool zip,
                                        bool ignore_content) {
  const buf_page_t *hashed_bpage;
  buf_pool_t *buf_pool = buf_pool_from_bpage(bpage);
  rw_lock_t *hash_lock;

  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
  ut_ad(mutex_own(buf_page_get_mutex(bpage)));

  hash_lock = buf_page_hash_lock_get(buf_pool, bpage->id);

  ut_ad(rw_lock_own(hash_lock, RW_LOCK_X));

  ut_a(buf_page_get_io_fix(bpage) == BUF_IO_NONE);
  ut_a(bpage->buf_fix_count == 0);

  buf_LRU_remove_block(bpage);

  buf_pool->freed_page_clock += 1;

  switch (buf_page_get_state(bpage)) {
    case BUF_BLOCK_FILE_PAGE: {
      UNIV_MEM_ASSERT_W(bpage, sizeof(buf_block_t));
      UNIV_MEM_ASSERT_W(((buf_block_t *)bpage)->frame, UNIV_PAGE_SIZE);

      buf_block_modify_clock_inc((buf_block_t *)bpage);

      if (bpage->zip.data != nullptr) {
        const page_t *page = ((buf_block_t *)bpage)->frame;

        ut_a(!zip || !bpage->is_dirty());
        ut_ad(bpage->size.is_compressed());

        switch (fil_page_get_type(page)) {
          case FIL_PAGE_TYPE_ALLOCATED:
          case FIL_PAGE_INODE:
          case FIL_PAGE_IBUF_BITMAP:
          case FIL_PAGE_TYPE_FSP_HDR:
          case FIL_PAGE_TYPE_XDES:
          case FIL_PAGE_TYPE_ZLOB_FIRST:
          case FIL_PAGE_TYPE_ZLOB_DATA:
          case FIL_PAGE_TYPE_ZLOB_INDEX:
          case FIL_PAGE_TYPE_ZLOB_FRAG:
          case FIL_PAGE_TYPE_ZLOB_FRAG_ENTRY:
            /* These are essentially uncompressed pages. */
            if (!zip) {
              /* InnoDB writes the data to the
              uncompressed page frame.  Copy it
              to the compressed page, which will
              be preserved. */
              memcpy(bpage->zip.data, page, bpage->size.physical());
            }
            break;
          case FIL_PAGE_TYPE_ZBLOB:
          case FIL_PAGE_TYPE_ZBLOB2:
          case FIL_PAGE_SDI_ZBLOB:
            break;
          case FIL_PAGE_INDEX:
          case FIL_PAGE_SDI:
          case FIL_PAGE_RTREE:
#ifdef UNIV_ZIP_DEBUG
            ut_a(page_zip_validate(&bpage->zip, page,
                                   ((buf_block_t *)bpage)->index));
#endif /* UNIV_ZIP_DEBUG */
            break;
          default:
            ib::error(ER_IB_MSG_135) << "The compressed page to be"
                                        " evicted seems corrupt:";
            ut_print_buf(stderr, page, bpage->size.logical());

            ib::error(ER_IB_MSG_136) << "Possibly older version of"
                                        " the page:";

            ut_print_buf(stderr, bpage->zip.data, bpage->size.physical());
            putc('\n', stderr);
            ut_error;
        }

        break;
      }

      if (!ignore_content) {
        /* Account the eviction of index leaf pages from
        the buffer pool(s). */

        const byte *frame = bpage->zip.data != nullptr
                                ? bpage->zip.data
                                : reinterpret_cast<buf_block_t *>(bpage)->frame;

        const ulint type = fil_page_get_type(frame);

        if ((type == FIL_PAGE_INDEX || type == FIL_PAGE_RTREE) &&
            page_is_leaf(frame)) {
          uint32_t space_id = bpage->id.space();

          space_index_t idx_id = btr_page_get_index_id(frame);

          buf_stat_per_index->dec(index_id_t(space_id, idx_id));
        }
      }
    }
      [[fallthrough]];
    case BUF_BLOCK_ZIP_PAGE:
      ut_a(!bpage->is_dirty());
      if (bpage->size.is_compressed()) {
        UNIV_MEM_ASSERT_W(bpage->zip.data, bpage->size.physical());
      }
      break;
    case BUF_BLOCK_POOL_WATCH:
    case BUF_BLOCK_ZIP_DIRTY:
    case BUF_BLOCK_NOT_USED:
    case BUF_BLOCK_READY_FOR_USE:
    case BUF_BLOCK_MEMORY:
    case BUF_BLOCK_REMOVE_HASH:
      ut_error;
      break;
  }

  hashed_bpage = buf_page_hash_get_low(buf_pool, bpage->id);

  if (bpage != hashed_bpage) {
    ib::error(ER_IB_MSG_137)
        << "Page " << bpage->id << " not found in the hash table";

    if (hashed_bpage) {
      ib::error(ER_IB_MSG_138)
          << "In hash table we find block " << hashed_bpage << " of "
          << hashed_bpage->id << " which is not " << bpage;
    }

    ut_d(mutex_exit(buf_page_get_mutex(bpage)));
    ut_d(rw_lock_x_unlock(hash_lock));
    ut_d(mutex_exit(&buf_pool->LRU_list_mutex));
    ut_d(buf_print());
    ut_d(buf_LRU_print());
    ut_d(buf_validate());
    ut_d(buf_LRU_validate());
    ut_d(ut_error);
  }

  ut_ad(!bpage->in_zip_hash);
  ut_ad(bpage->in_page_hash);
  ut_d(bpage->in_page_hash = false);

  HASH_DELETE(buf_page_t, hash, buf_pool->page_hash, bpage->id.hash(), bpage);

  switch (buf_page_get_state(bpage)) {
    case BUF_BLOCK_ZIP_PAGE:
      ut_ad(!bpage->in_free_list);
      ut_ad(!bpage->in_flush_list);
      ut_ad(!bpage->in_LRU_list);
      ut_a(bpage->zip.data);
      ut_a(bpage->size.is_compressed());

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
      UT_LIST_REMOVE(buf_pool->zip_clean, bpage);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

      mutex_exit(&buf_pool->zip_mutex);
      rw_lock_x_unlock(hash_lock);

      buf_buddy_free(buf_pool, bpage->zip.data, bpage->size.physical());

      buf_page_free_descriptor(bpage);
      return (false);

    case BUF_BLOCK_FILE_PAGE:
      memset(((buf_block_t *)bpage)->frame + FIL_PAGE_OFFSET, 0xff, 4);
      memset(((buf_block_t *)bpage)->frame + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID,
             0xff, 4);
      UNIV_MEM_INVALID(((buf_block_t *)bpage)->frame, UNIV_PAGE_SIZE);
      buf_page_set_state(bpage, BUF_BLOCK_REMOVE_HASH);

      /* Question: If we release bpage and hash mutex here
      then what protects us against:
      1) Some other thread buffer fixing this page
      2) Some other thread trying to read this page and
      not finding it in buffer pool attempting to read it
      from the disk.
      Answer:
      1) Cannot happen because the page is no longer in the
      page_hash. Only possibility is when while invalidating
      a tablespace we buffer fix the prev_page in LRU to
      avoid relocation during the scan. But that is not
      possible because we are holding LRU list mutex.

      2) Not possible because in buf_page_init_for_read()
      we do a look up of page_hash while holding LRU list
      mutex and since we are holding LRU list mutex here
      and by the time we'll release it in the caller we'd
      have inserted the compressed only descriptor in the
      page_hash. */
      ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
      rw_lock_x_unlock(hash_lock);
      mutex_exit(&((buf_block_t *)bpage)->mutex);

      if (zip && bpage->zip.data) {
        /* Free the compressed page. */
        void *data = bpage->zip.data;
        bpage->zip.data = nullptr;

        ut_ad(!bpage->in_free_list);
        ut_ad(!bpage->in_flush_list);
        ut_ad(!bpage->in_LRU_list);

        buf_buddy_free(buf_pool, data, bpage->size.physical());

        page_zip_set_size(&bpage->zip, 0);

        bpage->size.copy_from(
            page_size_t(bpage->size.logical(), bpage->size.logical(), false));
      }

      return (true);

    case BUF_BLOCK_POOL_WATCH:
    case BUF_BLOCK_ZIP_DIRTY:
    case BUF_BLOCK_NOT_USED:
    case BUF_BLOCK_READY_FOR_USE:
    case BUF_BLOCK_MEMORY:
    case BUF_BLOCK_REMOVE_HASH:
      break;
  }

  ut_error;
}

static void buf_LRU_block_free_hashed_page(buf_block_t *block) noexcept {
  buf_block_set_state(block, BUF_BLOCK_MEMORY);

  buf_LRU_block_free_non_file_page(block);
}

void buf_LRU_free_one_page(buf_page_t *bpage, bool ignore_content) {
#ifdef UNIV_DEBUG
  buf_pool_t *buf_pool = buf_pool_from_bpage(bpage);
  BPageMutex *block_mutex = buf_page_get_mutex(bpage);
  rw_lock_t *hash_lock = buf_page_hash_lock_get(buf_pool, bpage->id);

  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
  ut_ad(mutex_own(block_mutex));
  ut_ad(rw_lock_own(hash_lock, RW_LOCK_X));
#endif /* UNIV_DEBUG */

  if (buf_LRU_block_remove_hashed(bpage, true, ignore_content)) {
    buf_LRU_block_free_hashed_page((buf_block_t *)bpage);
  }

  /* buf_LRU_block_remove_hashed() releases hash_lock and block_mutex */
  ut_ad(!rw_lock_own(hash_lock, RW_LOCK_X) &&
        !rw_lock_own(hash_lock, RW_LOCK_S));

  ut_ad(!mutex_own(block_mutex));
}

/** Updates buf_pool->LRU_old_ratio for one buffer pool instance.
@param[in]      buf_pool        buffer pool instance
@param[in]      old_pct         Reserve this percentage of
                                the buffer pool for "old" blocks
@param[in]      adjust          true=adjust the LRU list;
                                false=just assign buf_pool->LRU_old_ratio
                                during the initialization of InnoDB
@return updated old_pct */
static uint buf_LRU_old_ratio_update_instance(buf_pool_t *buf_pool,
                                              uint old_pct, bool adjust) {
  uint ratio;

  ratio = old_pct * BUF_LRU_OLD_RATIO_DIV / 100;
  if (ratio < BUF_LRU_OLD_RATIO_MIN) {
    ratio = BUF_LRU_OLD_RATIO_MIN;
  } else if (ratio > BUF_LRU_OLD_RATIO_MAX) {
    ratio = BUF_LRU_OLD_RATIO_MAX;
  }

  if (adjust) {
    mutex_enter(&buf_pool->LRU_list_mutex);

    if (ratio != buf_pool->LRU_old_ratio) {
      buf_pool->LRU_old_ratio = ratio;

      if (UT_LIST_GET_LEN(buf_pool->LRU) >= BUF_LRU_OLD_MIN_LEN) {
        buf_LRU_old_adjust_len(buf_pool);
      }
    }

    mutex_exit(&buf_pool->LRU_list_mutex);
  } else {
    buf_pool->LRU_old_ratio = ratio;
  }
  /* the reverse of
  ratio = old_pct * BUF_LRU_OLD_RATIO_DIV / 100 */
  return ((uint)(ratio * 100 / (double)BUF_LRU_OLD_RATIO_DIV + 0.5));
}

/** Updates buf_pool->LRU_old_ratio.
 @return updated old_pct */
uint buf_LRU_old_ratio_update(
    uint old_pct, /*!< in: Reserve this percentage of
                  the buffer pool for "old" blocks. */
    bool adjust)  /*!< in: true=adjust the LRU list;
                   false=just assign buf_pool->LRU_old_ratio
                   during the initialization of InnoDB */
{
  uint new_ratio = 0;

  for (ulint i = 0; i < srv_buf_pool_instances; i++) {
    buf_pool_t *buf_pool;

    buf_pool = buf_pool_from_array(i);

    new_ratio = buf_LRU_old_ratio_update_instance(buf_pool, old_pct, adjust);
  }

  return (new_ratio);
}

/** Update the historical stats that we are collecting for LRU eviction
 policy at the end of each interval. */
void buf_LRU_stat_update(void) {
  buf_LRU_stat_t *item;
  buf_pool_t *buf_pool;
  bool evict_started = false;
  buf_LRU_stat_t cur_stat;

  /* If we haven't started eviction yet then don't update stats. */
  os_rmb;
  for (ulint i = 0; i < srv_buf_pool_instances; i++) {
    buf_pool = buf_pool_from_array(i);

    if (buf_pool->freed_page_clock != 0) {
      evict_started = true;
      break;
    }
  }

  if (!evict_started) {
    goto func_exit;
  }

  /* Update the index. */
  item = &buf_LRU_stat_arr[buf_LRU_stat_arr_ind];
  buf_LRU_stat_arr_ind++;
  buf_LRU_stat_arr_ind %= BUF_LRU_STAT_N_INTERVAL;

  /* Add the current value and subtract the obsolete entry.
  Since buf_LRU_stat_cur is not protected by any mutex,
  it can be changing between adding to buf_LRU_stat_sum
  and copying to item. Assign it to local variables to make
  sure the same value assign to the buf_LRU_stat_sum
  and item */
  cur_stat = buf_LRU_stat_cur;

  buf_LRU_stat_sum.io += cur_stat.io - item->io;
  buf_LRU_stat_sum.unzip += cur_stat.unzip - item->unzip;

  /* Put current entry in the array. */
  memcpy(item, &cur_stat, sizeof *item);

func_exit:
  /* Clear the current entry. */
  memset(&buf_LRU_stat_cur, 0, sizeof buf_LRU_stat_cur);
  os_wmb;
}

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/** Validates the LRU list for one buffer pool instance.
@param[in]      buf_pool        buffer pool instance */
void buf_LRU_validate_instance(buf_pool_t *buf_pool) {
  mutex_enter(&buf_pool->LRU_list_mutex);

  if (UT_LIST_GET_LEN(buf_pool->LRU) >= BUF_LRU_OLD_MIN_LEN) {
    ut_a(buf_pool->LRU_old);

    const size_t new_len = calculate_desired_LRU_old_size(buf_pool);

    ut_a(buf_pool->LRU_old_len >= new_len - BUF_LRU_OLD_TOLERANCE);
    ut_a(buf_pool->LRU_old_len <= new_len + BUF_LRU_OLD_TOLERANCE);
  }

  CheckInLRUList::validate(buf_pool);

  ulint old_len = 0;

  for (auto bpage : buf_pool->LRU) {
    switch (buf_page_get_state(bpage)) {
      case BUF_BLOCK_POOL_WATCH:
      case BUF_BLOCK_NOT_USED:
      case BUF_BLOCK_READY_FOR_USE:
      case BUF_BLOCK_MEMORY:
      case BUF_BLOCK_REMOVE_HASH:
        ut_error;
        break;
      case BUF_BLOCK_FILE_PAGE:
        ut_ad(((buf_block_t *)bpage)->in_unzip_LRU_list ==
              buf_page_belongs_to_unzip_LRU(bpage));
      case BUF_BLOCK_ZIP_PAGE:
      case BUF_BLOCK_ZIP_DIRTY:
        break;
    }

    if (buf_page_is_old(bpage)) {
      const buf_page_t *prev = UT_LIST_GET_PREV(LRU, bpage);
      const buf_page_t *next = UT_LIST_GET_NEXT(LRU, bpage);

      if (!old_len++) {
        ut_a(buf_pool->LRU_old == bpage);
      } else {
        ut_a(!prev || buf_page_is_old(prev));
      }

      ut_a(!next || buf_page_is_old(next));
    }
  }

  ut_a(buf_pool->LRU_old_len == old_len);

  mutex_exit(&buf_pool->LRU_list_mutex);

  mutex_enter(&buf_pool->free_list_mutex);

  CheckInFreeList::validate(buf_pool);

  for (auto bpage : buf_pool->free) {
    ut_a(buf_page_get_state(bpage) == BUF_BLOCK_NOT_USED);
  }

  mutex_exit(&buf_pool->free_list_mutex);

  mutex_enter(&buf_pool->LRU_list_mutex);

  CheckUnzipLRUAndLRUList::validate(buf_pool);

  for (auto block : buf_pool->unzip_LRU) {
    ut_ad(block->in_unzip_LRU_list);
    ut_ad(block->page.in_LRU_list);
    ut_a(buf_page_belongs_to_unzip_LRU(&block->page));
  }

  mutex_exit(&buf_pool->LRU_list_mutex);
}

/** Validates the LRU list. */
void buf_LRU_validate(void) {
  for (size_t i = 0; i < srv_buf_pool_instances; i++) {
    buf_pool_t *buf_pool = buf_pool_from_array(i);
    buf_LRU_validate_instance(buf_pool);
  }
}

Space_References buf_LRU_count_space_references() {
  Space_References result;
  for (size_t i = 0; i < srv_buf_pool_instances; i++) {
    buf_pool_t *buf_pool = buf_pool_from_array(i);

    mutex_enter(&buf_pool->LRU_list_mutex);

    for (auto bpage : buf_pool->LRU) {
      /* We have the LRU mutex, it is safe to assume the space ID will not be
      changed, as it would require removal from the LRU first. */
      result[bpage->get_space()]++;
    }

    for (size_t j = 0; j < BUF_POOL_WATCH_SIZE; j++) {
      const auto &bpage = &buf_pool->watch[j];

      switch (bpage->state) {
        case BUF_BLOCK_ZIP_PAGE:
          result[bpage->get_space()]++;
          break;
        default:
          break;
      }
    }

    mutex_exit(&buf_pool->LRU_list_mutex);
  }

  return result;
}
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

#if defined UNIV_DEBUG_PRINT || defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/** Prints the LRU list for one buffer pool instance.
@param[in]      buf_pool        buffer pool instance */
static void buf_LRU_print_instance(buf_pool_t *buf_pool) {
  mutex_enter(&buf_pool->LRU_list_mutex);

  for (auto bpage : buf_pool->LRU) {
    mutex_enter(buf_page_get_mutex(bpage));

    fprintf(stderr, "BLOCK space " UINT32PF " page " UINT32PF " ",
            bpage->id.space(), bpage->id.page_no());

    if (buf_page_is_old(bpage)) {
      fputs("old ", stderr);
    }

    if (bpage->buf_fix_count) {
      fprintf(stderr, "buffix count %lu ", (ulong)bpage->buf_fix_count);
    }

    if (buf_page_get_io_fix(bpage)) {
      fprintf(stderr, "io_fix %lu ", (ulong)buf_page_get_io_fix(bpage));
    }

    if (bpage->is_dirty()) {
      fputs("modif. ", stderr);
    }

    switch (buf_page_get_state(bpage)) {
      const byte *frame;
      case BUF_BLOCK_FILE_PAGE:
        frame = buf_block_get_frame((buf_block_t *)bpage);
        fprintf(stderr,
                "\ntype %lu"
                " index id " IB_ID_FMT "\n",
                (ulong)fil_page_get_type(frame), btr_page_get_index_id(frame));
        break;
      case BUF_BLOCK_ZIP_PAGE:
        frame = bpage->zip.data;
        fprintf(stderr,
                "\ntype %lu size %lu"
                " index id " IB_ID_FMT "\n",
                (ulong)fil_page_get_type(frame), (ulong)bpage->size.physical(),
                btr_page_get_index_id(frame));
        break;

      default:
        fprintf(stderr, "\n!state %lu!\n", (ulong)buf_page_get_state(bpage));
        break;
    }

    mutex_exit(buf_page_get_mutex(bpage));
  }

  mutex_exit(&buf_pool->LRU_list_mutex);
}

/** Prints the LRU list. */
void buf_LRU_print(void) {
  for (size_t i = 0; i < srv_buf_pool_instances; i++) {
    buf_pool_t *buf_pool;

    buf_pool = buf_pool_from_array(i);
    buf_LRU_print_instance(buf_pool);
  }
}
#endif /* UNIV_DEBUG_PRINT || UNIV_DEBUG || UNIV_BUF_DEBUG */
