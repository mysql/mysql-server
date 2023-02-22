/*****************************************************************************

Copyright (c) 1995, 2023, Oracle and/or its affiliates.

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

/** @file buf/buf0flu.cc
 The database buffer buf_pool flush algorithm

 Created 11/11/1995 Heikki Tuuri
 *******************************************************/

#include <math.h>
#include <my_dbug.h>
#include <mysql/service_thd_wait.h>
#include <sys/types.h>
#include <time.h>

#ifndef UNIV_HOTBACKUP
#include "buf0buf.h"
#include "buf0checksum.h"
#include "buf0flu.h"
#include "ha_prototypes.h"
#include "my_inttypes.h"
#include "sql_thd_internal_api.h"
#endif /* !UNIV_HOTBACKUP */
#include "page0zip.h"
#ifndef UNIV_HOTBACKUP
#include "arch0arch.h"
#include "buf0lru.h"
#include "buf0rea.h"
#include "fil0fil.h"
#include "fsp0sysspace.h"
#include "ibuf0ibuf.h"
#include "log0buf.h"
#include "log0chkp.h"
#include "log0write.h"
#include "my_compiler.h"
#include "os0file.h"
#include "os0thread-create.h"
#include "page0page.h"
#include "srv0mon.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "trx0sys.h"
#include "ut0byte.h"
#include "ut0stage.h"

#ifdef UNIV_LINUX
/* include defs for CPU time priority settings */
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>

static const int buf_flush_page_cleaner_priority = -20;
#endif /* UNIV_LINUX */

/** Number of pages flushed through non flush_list flushes. */
static ulint buf_lru_flush_page_count = 0;

/** Factor for scan length to determine n_pages for intended oldest LSN
progress */
static uint buf_flush_lsn_scan_factor = 3;

/** Target oldest LSN for the requested flush_sync */
static lsn_t buf_flush_sync_lsn = 0;

#ifdef UNIV_DEBUG
/** Get the lsn up to which data pages are to be synchronously flushed.
@return target lsn for the requested flush_sync */
lsn_t get_flush_sync_lsn() noexcept { return buf_flush_sync_lsn; }
#endif /* UNIV_DEBUG */

#ifdef UNIV_PFS_THREAD
mysql_pfs_key_t page_flush_thread_key;
mysql_pfs_key_t page_flush_coordinator_thread_key;
#endif /* UNIV_PFS_THREAD */

/** Event to synchronise with the flushing. */
os_event_t buf_flush_event;

/** Event to wait for one flushing step */
os_event_t buf_flush_tick_event;

/** State for page cleaner array slot */
enum page_cleaner_state_t {
  /** Not requested any yet.
  Moved from FINISHED by the coordinator. */
  PAGE_CLEANER_STATE_NONE = 0,
  /** Requested but not started flushing.
  Moved from NONE by the coordinator. */
  PAGE_CLEANER_STATE_REQUESTED,
  /** Flushing is on going.
  Moved from REQUESTED by the worker. */
  PAGE_CLEANER_STATE_FLUSHING,
  /** Flushing was finished.
  Moved from FLUSHING by the worker. */
  PAGE_CLEANER_STATE_FINISHED
};

/** Page cleaner request state for each buffer pool instance */
struct page_cleaner_slot_t {
  page_cleaner_state_t state; /*!< state of the request.
                              protected by page_cleaner_t::mutex
                              if the worker thread got the slot and
                              set to PAGE_CLEANER_STATE_FLUSHING,
                              n_flushed_lru and n_flushed_list can be
                              updated only by the worker thread */
  /* This value is set during state==PAGE_CLEANER_STATE_NONE */
  ulint n_pages_requested;
  /*!< number of requested pages
  for the slot */
  /* These values are updated during state==PAGE_CLEANER_STATE_FLUSHING,
  and committed with state==PAGE_CLEANER_STATE_FINISHED.
  The consistency is protected by the 'state' */
  ulint n_flushed_lru;
  /*!< number of flushed pages
  by LRU scan flushing */
  ulint n_flushed_list;
  /*!< number of flushed pages
  by flush_list flushing */
  bool succeeded_list;
  /*!< true if flush_list flushing
  succeeded. */
  std::chrono::milliseconds flush_lru_time;
  /*!< elapsed time for LRU flushing */
  std::chrono::milliseconds flush_list_time;
  /*!< elapsed time for flush_list
  flushing */
  ulint flush_lru_pass;
  /*!< count to attempt LRU flushing */
  ulint flush_list_pass;
  /*!< count to attempt flush_list
  flushing */
};

/** Page cleaner structure common for all threads */
struct page_cleaner_t {
  ib_mutex_t mutex;        /*!< mutex to protect whole of
                           page_cleaner_t struct and
                           page_cleaner_slot_t slots. */
  os_event_t is_requested; /*!< event to activate worker
                           threads. */
  os_event_t is_finished;  /*!< event to signal that all
                           slots were finished. */
  bool requested;          /*!< true if requested pages
                           to flush */
  lsn_t lsn_limit;         /*!< upper limit of LSN to be
                           flushed */
  ulint n_slots;           /*!< total number of slots */
  ulint n_slots_requested;
  /*!< number of slots
  in the state
  PAGE_CLEANER_STATE_REQUESTED */
  ulint n_slots_flushing;
  /*!< number of slots
  in the state
  PAGE_CLEANER_STATE_FLUSHING */
  ulint n_slots_finished;
  /*!< number of slots
  in the state
  PAGE_CLEANER_STATE_FINISHED */
  std::chrono::milliseconds flush_time;        /*!< elapsed time to flush
                              requests for all slots */
  ulint flush_pass;                            /*!< count to finish to flush
                                               requests for all slots */
  ut::unique_ptr<page_cleaner_slot_t[]> slots; /*!< pointer to the slots */
  bool is_running;                             /*!< false if attempt
                                               to shutdown */

#ifdef UNIV_DEBUG
  ulint n_disabled_debug;
  /*!< how many of pc threads
  have been disabled */
#endif /* UNIV_DEBUG */
};

static ut::unique_ptr<page_cleaner_t> page_cleaner;

#ifdef UNIV_DEBUG
bool innodb_page_cleaner_disabled_debug;
#endif /* UNIV_DEBUG */

/** If LRU list of a buf_pool is less than this size then LRU eviction
should not happen. This is because when we do LRU flushing we also put
the blocks on free list. If LRU list is very small then we can end up
in thrashing. */
constexpr uint32_t BUF_LRU_MIN_LEN = 256;

/** Flush a batch of writes to the datafiles that have already been
written to the dblwr buffer on disk. */
static void buf_flush_sync_datafiles() {
  /* Wake possible simulated AIO thread to actually post the
  writes to the operating system */
  os_aio_simulated_wake_handler_threads();

  /* Wait that all async writes to tablespaces have been posted to
  the OS */
  os_aio_wait_until_no_pending_writes();

  /* Now we flush the data to disk (for example, with fsync) */
  fil_flush_file_spaces();
}

/** Thread tasked with flushing dirty pages from the buffer pools.
As of now we'll have only one coordinator. */
static void buf_flush_page_coordinator_thread();

/** Worker thread of page_cleaner. */
static void buf_flush_page_cleaner_thread();

/** Increases flush_list size in bytes with the page size in inline function */
static inline void incr_flush_list_size_in_bytes(
    buf_block_t *block,   /*!< in: control block */
    buf_pool_t *buf_pool) /*!< in: buffer pool instance */
{
  ut_ad(buf_flush_list_mutex_own(buf_pool));

  buf_pool->stat.flush_list_bytes += block->page.size.physical();

  ut_ad(buf_pool->stat.flush_list_bytes <= buf_pool->curr_pool_size);
}

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/** Validate a buffer pool instance flush list.
@param[in] buf_pool Instance to validate
@return true on success. */
static bool buf_flush_validate_low(const buf_pool_t *buf_pool);

/** Validates the flush list some of the time.
 @return true if ok or the check was skipped */
static bool buf_flush_validate_skip(
    buf_pool_t *buf_pool) /*!< in: Buffer pool instance */
{
  /** Try buf_flush_validate_low() every this many times */
  constexpr uint32_t BUF_FLUSH_VALIDATE_SKIP = 23;

  /** The buf_flush_validate_low() call skip counter.
  Use a signed type because of the race condition below. */
  static int buf_flush_validate_count = BUF_FLUSH_VALIDATE_SKIP;

  DBUG_EXECUTE_IF("buf_flush_list_validate", buf_flush_validate_count = 1;);

  /* There is a race condition below, but it does not matter,
  because this call is only for heuristic purposes. We want to
  reduce the call frequency of the costly buf_flush_validate_low()
  check in debug builds. */
  if (--buf_flush_validate_count > 0) {
    return true;
  }

  buf_flush_validate_count = BUF_FLUSH_VALIDATE_SKIP;
  return (buf_flush_validate_low(buf_pool));
}
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

/** Insert a block in the flush_rbt and returns a pointer to its
 predecessor or NULL if no predecessor. The ordering is maintained
 on the basis of the <oldest_modification, space, offset> key.
 @return pointer to the predecessor or NULL if no predecessor. */
static buf_page_t *buf_flush_insert_in_flush_rbt(
    buf_page_t *bpage) /*!< in: bpage to be inserted. */
{
  const ib_rbt_node_t *c_node;
  const ib_rbt_node_t *p_node;
  buf_page_t *prev = nullptr;
  buf_pool_t *buf_pool = buf_pool_from_bpage(bpage);

  ut_ad(buf_flush_list_mutex_own(buf_pool));

  /* Insert this buffer into the rbt. */
  c_node = rbt_insert(buf_pool->flush_rbt, &bpage, &bpage);
  ut_a(c_node != nullptr);

  /* Get the predecessor. */
  p_node = rbt_prev(buf_pool->flush_rbt, c_node);

  if (p_node != nullptr) {
    buf_page_t **value;
    value = rbt_value(buf_page_t *, p_node);
    prev = *value;
    ut_a(prev != nullptr);
  }

  return (prev);
}

/** Delete a bpage from the flush_rbt. */
static void buf_flush_delete_from_flush_rbt(
    buf_page_t *bpage) /*!< in: bpage to be removed. */
{
  buf_pool_t *buf_pool = buf_pool_from_bpage(bpage);

  ut_ad(buf_flush_list_mutex_own(buf_pool));

#ifdef UNIV_DEBUG
  bool ret =
#endif /* UNIV_DEBUG */
      rbt_delete(buf_pool->flush_rbt, &bpage);

  ut_ad(ret);
}

/** Compare two modified blocks in the buffer pool. The key for comparison
 is:
 key = <oldest_modification, space, offset>
 This comparison is used to maintian ordering of blocks in the
 buf_pool->flush_rbt.
 Note that for the purpose of flush_rbt, we only need to order blocks
 on the oldest_modification. The other two fields are used to uniquely
 identify the blocks.
 @return < 0 if b2 < b1, 0 if b2 == b1, > 0 if b2 > b1 */
static int buf_flush_block_cmp(const void *p1, /*!< in: block1 */
                               const void *p2) /*!< in: block2 */
{
  int ret;
  const buf_page_t *b1 = *(const buf_page_t **)p1;
  const buf_page_t *b2 = *(const buf_page_t **)p2;

  ut_ad(b1 != nullptr);
  ut_ad(b2 != nullptr);

#ifdef UNIV_DEBUG
  buf_pool_t *buf_pool = buf_pool_from_bpage(b1);
#endif /* UNIV_DEBUG */

  ut_ad(buf_flush_list_mutex_own(buf_pool));

  ut_ad(b1->in_flush_list);
  ut_ad(b2->in_flush_list);

  if (b2->get_oldest_lsn() > b1->get_oldest_lsn()) {
    return (1);
  } else if (b2->get_oldest_lsn() < b1->get_oldest_lsn()) {
    return (-1);
  }

  /* If oldest_modification is same then decide on the space. */
  ret = (int)(b2->id.space() - b1->id.space());

  /* Or else decide ordering on the page number. */
  return (ret ? ret : (int)(b2->id.page_no() - b1->id.page_no()));
}

/** Initialize the red-black tree to speed up insertions into the flush_list
 during recovery process. Should be called at the start of recovery
 process before any page has been read/written. */
void buf_flush_init_flush_rbt(void) {
  ulint i;

  for (i = 0; i < srv_buf_pool_instances; i++) {
    buf_pool_t *buf_pool;

    buf_pool = buf_pool_from_array(i);

    buf_flush_list_mutex_enter(buf_pool);

    ut_ad(buf_pool->flush_rbt == nullptr);

    /* Create red black tree for speedy insertions in flush list. */
    buf_pool->flush_rbt = rbt_create(sizeof(buf_page_t *), buf_flush_block_cmp);

    buf_flush_list_mutex_exit(buf_pool);
  }
}

/** Frees up the red-black tree. */
void buf_flush_free_flush_rbt(void) {
  ulint i;

  for (i = 0; i < srv_buf_pool_instances; i++) {
    buf_pool_t *buf_pool;

    buf_pool = buf_pool_from_array(i);

    buf_flush_list_mutex_enter(buf_pool);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
    ut_a(buf_flush_validate_low(buf_pool));
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

    rbt_free(buf_pool->flush_rbt);
    buf_pool->flush_rbt = nullptr;

    buf_flush_list_mutex_exit(buf_pool);
  }
}

bool buf_are_flush_lists_empty_validate(void) {
  /* No mutex is acquired. It is used by single-thread
  in assertions during startup. */

  for (size_t i = 0; i < srv_buf_pool_instances; i++) {
    auto buf_pool = buf_pool_from_array(i);

    if (UT_LIST_GET_FIRST(buf_pool->flush_list) != nullptr) {
      return false;
    }
  }

  return true;
}

/** Checks that order of two consecutive pages in flush list would be valid,
according to their oldest_modification values.

@remarks
We have a relaxed order in flush list, but still we have guarantee,
that the earliest added page has oldest_modification not greater than
minimum oldest_midification of all dirty pages by more than number of
slots in the log recent closed buffer.

This is used by assertions only.

@param[in]      earlier_added_lsn       oldest_modification of page which was
                                        added to flush list earlier
@param[in]      new_added_lsn           oldest_modification of page which is
                                        being added to flush list
@retval true if the order is valid
*/
MY_COMPILER_DIAGNOSTIC_PUSH()
MY_COMPILER_CLANG_WORKAROUND_REF_DOCBUG()
/**
@see @ref sect_redo_log_reclaim_space
@see @ref sect_redo_log_add_dirty_pages */
MY_COMPILER_DIAGNOSTIC_POP()

[[maybe_unused]] static inline bool buf_flush_list_order_validate(
    lsn_t earlier_added_lsn, lsn_t new_added_lsn) {
  return (earlier_added_lsn <=
          new_added_lsn + log_buffer_flush_order_lag(*log_sys));
}

/** Borrows LSN from the recent added dirty page to the flush list.

This should be the lsn which we may use to mark pages dirtied without
underlying redo records, when we add them to the flush list.

The lsn should be chosen in a way which will guarantee that we will
not destroy checkpoint calculations if we inserted a new dirty page
with such lsn to the flush list. This is strictly related to the
limitations we put on the relaxed order in flush lists, which have
direct impact on computation of lsn available for next checkpoint.

Therefore when the flush list is empty, the lsn is chosen as the
maximum lsn up to which we know, that all dirty pages with smaller
oldest_modification were added to the flush list.

This guarantees that the limitations put on the relaxed order are
hold and lsn available for next checkpoint is not miscalculated.

@param[in]  buf_pool  buffer pool instance
@return     the borrowed newest_modification of the page or lsn up
            to which all dirty pages were added to the flush list
            if the flush list is empty */
static inline lsn_t buf_flush_borrow_lsn(const buf_pool_t *buf_pool) {
  ut_ad(buf_flush_list_mutex_own(buf_pool));

  const auto page = UT_LIST_GET_FIRST(buf_pool->flush_list);

  if (page == nullptr) {
    /* Flush list is empty - use lsn up to which we know that all
    dirty pages with smaller oldest_modification were added to
    the flush list (they were flushed as the flush list is empty). */
    const lsn_t lsn = log_buffer_dirty_pages_added_up_to_lsn(*log_sys);

    if (lsn < LOG_START_LSN) {
      ut_ad(srv_read_only_mode);
      return LOG_START_LSN + LOG_BLOCK_HDR_SIZE;
    }
    return lsn;
  }

  ut_ad(page->is_dirty());
  ut_ad(page->get_newest_lsn() >= page->get_oldest_lsn());

  return page->get_oldest_lsn();
}

/** Inserts a modified block into the flush list. */
void buf_flush_insert_into_flush_list(
    buf_pool_t *buf_pool, /*!< buffer pool instance */
    buf_block_t *block,   /*!< in/out: block which is modified */
    lsn_t lsn)            /*!< in: oldest modification */
{
  ut_ad(mutex_own(buf_page_get_mutex(&block->page)));
  ut_ad(log_sys != nullptr);

  buf_flush_list_mutex_enter(buf_pool);

  /* If we are in the recovery then we need to update the flush
  red-black tree as well. */
  if (buf_pool->flush_rbt != nullptr) {
    ut_ad(lsn != 0);
    ut_ad(block->page.get_newest_lsn() != 0);
    buf_flush_list_mutex_exit(buf_pool);
    buf_flush_insert_sorted_into_flush_list(buf_pool, block, lsn);
    return;
  }

  ut_ad(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);
  ut_ad(!block->page.in_flush_list);

  ut_d(block->page.in_flush_list = true);

  if (lsn == 0) {
    /* This is no-redo dirtied page. Borrow the lsn. */
    lsn = buf_flush_borrow_lsn(buf_pool);

    ut_ad(log_is_data_lsn(lsn));

    /* This page could already be no-redo dirtied before,
    and flushed since then. Also the page from which we
    borrowed lsn last time could be flushed by LRU and
    we would end up borrowing smaller LSN.

    Another risk is that this page was flushed earlier
    and freed. We should not re-flush it to disk with
    smaller FIL_PAGE_LSN.

    The best way to go is to use flushed_to_disk_lsn,
    unless we borrowed even higher value.

    This way we are sure that no page has ever been
    flushed with higher newest_modification - it would
    first need to wait until redo is flushed up to
    such point and it would ensure that by checking
    the log_sys->flushed_to_disk_lsn's value too.

    Because we keep the page latched, after we read
    flushed_to_disk_lsn this page cannot be flushed
    in background with higher lsn (hence we are safe
    even if the flushed_to_disk_lsn advanced after
    we read it). */

    block->page.set_newest_lsn(
        std::max(lsn, log_sys->flushed_to_disk_lsn.load()));
  }

  ut_ad(log_is_data_lsn(lsn));
  ut_ad(!block->page.is_dirty());
  ut_ad(block->page.get_newest_lsn() >= lsn);

  ut_ad(UT_LIST_GET_FIRST(buf_pool->flush_list) == nullptr ||
        buf_flush_list_order_validate(
            UT_LIST_GET_FIRST(buf_pool->flush_list)->get_oldest_lsn(), lsn));

  block->page.set_oldest_lsn(lsn);

  UT_LIST_ADD_FIRST(buf_pool->flush_list, &block->page);

  incr_flush_list_size_in_bytes(block, buf_pool);

#ifdef UNIV_DEBUG_VALGRIND
  void *p;

  if (block->page.size.is_compressed()) {
    p = block->page.zip.data;
  } else {
    p = block->frame;
  }

  UNIV_MEM_ASSERT_RW(p, block->page.size.physical());
#endif /* UNIV_DEBUG_VALGRIND */

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
  ut_a(buf_flush_validate_skip(buf_pool));
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

  buf_flush_list_mutex_exit(buf_pool);
}

/** Inserts a modified block into the flush list in the right sorted position.
 This function is used by recovery, because there the modifications do not
 necessarily come in the order of lsn's. */
void buf_flush_insert_sorted_into_flush_list(
    buf_pool_t *buf_pool, /*!< in: buffer pool instance */
    buf_block_t *block,   /*!< in/out: block which is modified */
    lsn_t lsn)            /*!< in: oldest modification */
{
  buf_page_t *prev_b;
  buf_page_t *b;

  ut_ad(mutex_own(buf_page_get_mutex(&block->page)));
  ut_ad(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);

  buf_flush_list_mutex_enter(buf_pool);

  /* The field in_LRU_list is protected by buf_pool->LRU_list_mutex,
  which we are not holding.  However, while a block is in the flush
  list, it is dirty and cannot be discarded, not from the
  page_hash or from the LRU list.  At most, the uncompressed
  page frame of a compressed block may be discarded or created
  (copying the block->page to or from a buf_page_t that is
  dynamically allocated from buf_buddy_alloc()).  Because those
  transitions hold block->mutex and the flush list mutex (via
  buf_flush_relocate_on_flush_list()), there is no possibility
  of a race condition in the assertions below. */
  ut_ad(block->page.in_LRU_list);
  ut_ad(block->page.in_page_hash);
  /* buf_buddy_block_register() will take a block in the
  BUF_BLOCK_MEMORY state, not a file page. */
  ut_ad(!block->page.in_zip_hash);

  ut_ad(!block->page.in_flush_list);
  ut_d(block->page.in_flush_list = true);
  block->page.set_oldest_lsn(lsn);

#ifdef UNIV_DEBUG_VALGRIND
  void *p;

  if (block->page.size.is_compressed()) {
    p = block->page.zip.data;
  } else {
    p = block->frame;
  }

  UNIV_MEM_ASSERT_RW(p, block->page.size.physical());
#endif /* UNIV_DEBUG_VALGRIND */

  prev_b = nullptr;

  /* For the most part when this function is called the flush_rbt
  should not be NULL. In a very rare boundary case it is possible
  that the flush_rbt has already been freed by the recovery thread
  before the last page was hooked up in the flush_list by the
  io-handler thread. In that case we'll just do a simple
  linear search in the else block. */
  if (buf_pool->flush_rbt != nullptr) {
    prev_b = buf_flush_insert_in_flush_rbt(&block->page);

  } else {
    b = UT_LIST_GET_FIRST(buf_pool->flush_list);

    while (b != nullptr && b->get_oldest_lsn() > block->page.get_oldest_lsn()) {
      ut_ad(b->in_flush_list);
      prev_b = b;
      b = UT_LIST_GET_NEXT(list, b);
    }
  }

  if (prev_b == nullptr) {
    UT_LIST_ADD_FIRST(buf_pool->flush_list, &block->page);
  } else {
    UT_LIST_INSERT_AFTER(buf_pool->flush_list, prev_b, &block->page);
  }

  if (buf_pool->oldest_hp.get() != nullptr) {
    /* clear oldest_hp */
    buf_pool->oldest_hp.set(nullptr);
  }

  incr_flush_list_size_in_bytes(block, buf_pool);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
  ut_a(buf_flush_validate_low(buf_pool));
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

  buf_flush_list_mutex_exit(buf_pool);
}

bool buf_flush_ready_for_replace(buf_page_t *bpage) {
  ut_d(auto buf_pool = buf_pool_from_bpage(bpage));
  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
  ut_ad(mutex_own(buf_page_get_mutex(bpage)));
  ut_ad(bpage->in_LRU_list);

  if (!buf_page_in_file(bpage)) {
    ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_123)
        << "Buffer block " << bpage << " state "
        << static_cast<unsigned>(bpage->state) << " in the LRU list!";
  }

  /* We can't replace a page that is fixed in any way.*/
  if (!buf_page_can_relocate(bpage)) {
    return false;
  }
  /* We can replace it if it is stale, but only if the page is not fixed. */
  if (bpage->was_stale()) {
    return true;
  }
  return !bpage->is_dirty();
}

/** Check if the block was modified and was ready for flushing.
This is a common part of the logic of buf_flush_was_ready_for_flush() and
buf_flush_ready_for_flush() which differ by the tolerance for stale result.
@param[in]      bpage           buffer control block, must be buf_page_in_file()
@param[in]      flush_type      type of flush
@param[in]      atomic          false if the caller can tolerate stale data,
                                true if the caller needs accurate answer, which
                                requires the caller to hold buf_page_get_mutex.
@return true if page seems ready for flush */
static bool buf_flush_ready_for_flush_gen(buf_page_t *bpage,
                                          buf_flush_t flush_type, bool atomic) {
#ifdef UNIV_DEBUG
  auto buf_pool = buf_pool_from_bpage(bpage);

  ut_a(buf_page_in_file(bpage) ||
       (buf_page_get_state(bpage) == BUF_BLOCK_REMOVE_HASH &&
        !mutex_own(&buf_pool->LRU_list_mutex)));
#else
  ut_a(buf_page_in_file(bpage) ||
       buf_page_get_state(bpage) == BUF_BLOCK_REMOVE_HASH);
#endif /* UNIV_DEBUG */

  /*As buf_flush_insert_into_flush_list() acquires SYNC_BUF_BLOCK after
  SYNC_BUF_FLUSH_LIST, the latch ordering prevents buf_do_flush_list_batch()
  from acquiring SYNC_BUF_BLOCK after SYNC_BUF_FLUSH_LIST taken for iterating
  over the flush_list. Thus for BUF_FLUSH_LIST we first perform a heuristic
  check with atomic==false, and if it looks ready to flush, we relatch, and
  recheck under proper mutex protection. In all other cases we already have
  the block_mutex.
  However, for BUF_LRU_LIST if we are called with atomic==false, then we will
  have the block mutex which will allow us to provide exact answer, even if we
  don't require it to be so (because atomic==false), and this is not required to
  have flush_list mutex in such situation.*/
  ut_ad(mutex_own(buf_page_get_mutex(bpage)) ||
        (!atomic && flush_type == BUF_FLUSH_LIST &&
         buf_flush_list_mutex_own(buf_pool)));

  ut_ad(flush_type < BUF_FLUSH_N_TYPES);

  if (!bpage->is_dirty() ||
      (atomic ? bpage->get_io_fix() != BUF_IO_NONE : bpage->was_io_fixed())) {
    return false;
  }

  ut_ad(bpage->in_flush_list);

  switch (flush_type) {
    case BUF_FLUSH_LIST:
      return (buf_page_get_state(bpage) != BUF_BLOCK_REMOVE_HASH);
    case BUF_FLUSH_LRU:
    case BUF_FLUSH_SINGLE_PAGE:
      return true;

    case BUF_FLUSH_N_TYPES:
      break;
  }

  ut_error;
}

/** Check if the block was modified and was ready for flushing at some point in
time during the call. Result might be obsolete.
@param[in]      bpage           buffer control block, must be buf_page_in_file()
@param[in]      flush_type      type of flush
@return true if can flush immediately */
static bool buf_flush_was_ready_for_flush(buf_page_t *bpage,
                                          buf_flush_t flush_type) {
  return buf_flush_ready_for_flush_gen(bpage, flush_type, false);
}

bool buf_flush_ready_for_flush(buf_page_t *bpage, buf_flush_t flush_type) {
  return buf_flush_ready_for_flush_gen(bpage, flush_type, true);
}

/** Remove a block from the flush list of modified blocks.
@param[in]      bpage   pointer to the block in question */
void buf_flush_remove(buf_page_t *bpage) {
  buf_pool_t *buf_pool = buf_pool_from_bpage(bpage);

  ut_ad(mutex_own(buf_page_get_mutex(bpage)));
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
  ut_ad(buf_page_get_state(bpage) != BUF_BLOCK_ZIP_DIRTY ||
        mutex_own(&buf_pool->LRU_list_mutex));
#endif
  ut_ad(bpage->in_flush_list);

  buf_flush_list_mutex_enter(buf_pool);

  /* Important that we adjust the hazard pointer before removing
  the bpage from flush list. */
  buf_pool->flush_hp.adjust(bpage);
  buf_pool->oldest_hp.adjust(bpage);

  switch (buf_page_get_state(bpage)) {
    case BUF_BLOCK_POOL_WATCH:
    case BUF_BLOCK_ZIP_PAGE:
      /* Clean compressed pages should not be on the flush list */
    case BUF_BLOCK_NOT_USED:
    case BUF_BLOCK_READY_FOR_USE:
    case BUF_BLOCK_MEMORY:
    case BUF_BLOCK_REMOVE_HASH:
      ut_error;
      return;
    case BUF_BLOCK_ZIP_DIRTY:
      buf_page_set_state(bpage, BUF_BLOCK_ZIP_PAGE);
      UT_LIST_REMOVE(buf_pool->flush_list, bpage);
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
      buf_LRU_insert_zip_clean(bpage);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
      break;
    case BUF_BLOCK_FILE_PAGE:
      UT_LIST_REMOVE(buf_pool->flush_list, bpage);
      break;
  }

  /* If the flush_rbt is active then delete from there as well. */
  if (buf_pool->flush_rbt != nullptr) {
    buf_flush_delete_from_flush_rbt(bpage);
  }

  /* Must be done after we have removed it from the flush_rbt
  because we assert on in_flush_list in comparison function. */
  ut_d(bpage->in_flush_list = false);

  buf_pool->stat.flush_list_bytes -= bpage->size.physical();

  bpage->set_clean();

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
  ut_a(buf_flush_validate_skip(buf_pool));
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

  /* If there is an observer that want to know if the asynchronous
  flushing was done then notify it. */
  if (bpage->get_flush_observer() != nullptr) {
    bpage->get_flush_observer()->notify_remove(buf_pool, bpage);

    bpage->reset_flush_observer();
  }

  buf_flush_list_mutex_exit(buf_pool);
}

/** Relocates a buffer control block on the flush_list.
 Note that it is assumed that the contents of bpage have already been
 copied to dpage.
 IMPORTANT: When this function is called bpage and dpage are not
 exact copies of each other. For example, they both will have different
 "::state". Also the "::list" pointers in dpage may be stale. We need to
 use the current list node (bpage) to do the list manipulation because
 the list pointers could have changed between the time that we copied
 the contents of bpage to the dpage and the flush list manipulation
 below. */
void buf_flush_relocate_on_flush_list(
    buf_page_t *bpage, /*!< in/out: control block being moved */
    buf_page_t *dpage) /*!< in/out: destination block */
{
  buf_page_t *prev;
  buf_page_t *prev_b = nullptr;
  buf_pool_t *buf_pool = buf_pool_from_bpage(bpage);

  /* Must reside in the same buffer pool. */
  ut_ad(buf_pool == buf_pool_from_bpage(dpage));

  ut_ad(mutex_own(buf_page_get_mutex(bpage)));

  buf_flush_list_mutex_enter(buf_pool);

  ut_ad(bpage->in_flush_list);
  ut_ad(dpage->in_flush_list);

  /* If recovery is active we must swap the control blocks in
  the flush_rbt as well. */
  if (buf_pool->flush_rbt != nullptr) {
    buf_flush_delete_from_flush_rbt(bpage);
    prev_b = buf_flush_insert_in_flush_rbt(dpage);
  }

  /* Important that we adjust the hazard pointer before removing
  the bpage from the flush list. */
  buf_pool->flush_hp.move(bpage, dpage);
  buf_pool->oldest_hp.move(bpage, dpage);

  /* Must be done after we have removed it from the flush_rbt
  because we assert on in_flush_list in comparison function. */
  ut_d(bpage->in_flush_list = false);

  prev = UT_LIST_GET_PREV(list, bpage);
  UT_LIST_REMOVE(buf_pool->flush_list, bpage);

  if (prev) {
    ut_ad(prev->in_flush_list);
    UT_LIST_INSERT_AFTER(buf_pool->flush_list, prev, dpage);
  } else {
    UT_LIST_ADD_FIRST(buf_pool->flush_list, dpage);
  }

  /* Just an extra check. Previous in flush_list
  should be the same control block as in flush_rbt. */
  ut_a(buf_pool->flush_rbt == nullptr || prev_b == prev);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
  ut_a(buf_flush_validate_low(buf_pool));
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

  buf_flush_list_mutex_exit(buf_pool);
}

/** Updates the flush system data structures when a write is completed.
@param[in]      bpage   pointer to the block in question */
void buf_flush_write_complete(buf_page_t *bpage) {
  auto buf_pool = buf_pool_from_bpage(bpage);

  ut_ad(mutex_own(buf_page_get_mutex(bpage)));

  ut_ad(buf_page_get_state(bpage) != BUF_BLOCK_ZIP_DIRTY ||
        mutex_own(&buf_pool->LRU_list_mutex));

  const buf_flush_t flush_type = buf_page_get_flush_type(bpage);

  mutex_enter(&buf_pool->flush_state_mutex);

  buf_flush_remove(bpage);

  buf_page_set_io_fix(bpage, BUF_IO_NONE);

  buf_pool->n_flush[flush_type]--;

  if (buf_pool->n_flush[flush_type] == 0 &&
      buf_pool->init_flush[flush_type] == false) {
    /* The running flush batch has ended */

    os_event_set(buf_pool->no_flush[flush_type]);
  }

  mutex_exit(&buf_pool->flush_state_mutex);

  if (!fsp_is_system_temporary(bpage->id.space()) && dblwr::is_enabled()) {
    dblwr::write_complete(bpage, flush_type);
  }
}
#endif /* !UNIV_HOTBACKUP */

/** Calculate the checksum of a page from compressed table and update
the page.
@param[in,out]  page            page to update
@param[in]      size            compressed page size
@param[in]      lsn             LSN to stamp on the page
@param[in]      skip_lsn_check  true to skip check for lsn (in DEBUG) */
void buf_flush_update_zip_checksum(buf_frame_t *page, ulint size, lsn_t lsn,
                                   bool skip_lsn_check) {
  ut_a(size > 0);

  BlockReporter reporter = BlockReporter(false, nullptr, univ_page_size, false);

  const uint32_t checksum = reporter.calc_zip_checksum(
      page, size,
      static_cast<srv_checksum_algorithm_t>(srv_checksum_algorithm));

  ut_ad(skip_lsn_check || mach_read_from_8(page + FIL_PAGE_LSN) <= lsn);

  mach_write_to_8(page + FIL_PAGE_LSN, lsn);
  mach_write_to_4(page + FIL_PAGE_SPACE_OR_CHKSUM, checksum);
}

bool page_is_uncompressed_type(const byte *page) {
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
      return (true);
  }
  return (false);
}

/** Initialize a page for writing to the tablespace.
@param[in]      block           buffer block; NULL if bypassing the buffer pool
@param[in,out]  page            page frame
@param[in,out]  page_zip_       compressed page, or NULL if uncompressed
@param[in]      newest_lsn      newest modification LSN to the page
@param[in]      skip_checksum   whether to disable the page checksum
@param[in]      skip_lsn_check  true to skip check for LSN (in DEBUG) */
void buf_flush_init_for_writing(const buf_block_t *block, byte *page,
                                void *page_zip_, lsn_t newest_lsn,
                                bool skip_checksum, bool skip_lsn_check) {
  uint32_t checksum = BUF_NO_CHECKSUM_MAGIC;

  ut_ad(block == nullptr || block->frame == page);
  ut_ad(block == nullptr || page_zip_ == nullptr ||
        &block->page.zip == page_zip_);
  ut_ad(page);

  if (page_zip_) {
    page_zip_des_t *page_zip;
    ulint size;

    page_zip = static_cast<page_zip_des_t *>(page_zip_);
    size = page_zip_get_size(page_zip);

    ut_ad(size);
    ut_ad(ut_is_2pow(size));
    ut_ad(size <= UNIV_ZIP_SIZE_MAX);

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
        ut_ad(page_is_uncompressed_type(page));
        /* Skip copy if they points to same memory: clone page copy. */
        if (page_zip->data != page) {
          memcpy(page_zip->data, page, size);
        }
        [[fallthrough]];
      case FIL_PAGE_TYPE_ZBLOB:
      case FIL_PAGE_TYPE_ZBLOB2:
      case FIL_PAGE_SDI_ZBLOB:
      case FIL_PAGE_INDEX:
      case FIL_PAGE_SDI:
      case FIL_PAGE_RTREE:

        buf_flush_update_zip_checksum(page_zip->data, size, newest_lsn,
                                      skip_lsn_check);

        return;
    }

    ib::error(ER_IB_MSG_124) << "The compressed page to be written"
                                " seems corrupt:";
    ut_print_buf(stderr, page, size);
    fputs("\nInnoDB: Possibly older version of the page:", stderr);
    ut_print_buf(stderr, page_zip->data, size);
    putc('\n', stderr);
    ut_error;
  }

  /* Write the newest modification lsn to the page header and trailer */
  ut_ad(skip_lsn_check || mach_read_from_8(page + FIL_PAGE_LSN) <= newest_lsn);

  mach_write_to_8(page + FIL_PAGE_LSN, newest_lsn);

  mach_write_to_8(page + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM,
                  newest_lsn);

  if (skip_checksum) {
    mach_write_to_4(page + FIL_PAGE_SPACE_OR_CHKSUM, checksum);
  } else {
    if (block != nullptr && UNIV_PAGE_SIZE == 16384) {
      /* The page type could be garbage in old files
      created before MySQL 5.5. Such files always
      had a page size of 16 kilobytes. */
      ulint page_type = fil_page_get_type(page);
      ulint reset_type = page_type;

      switch (block->page.id.page_no() % 16384) {
        case 0:
          reset_type = block->page.id.page_no() == 0 ? FIL_PAGE_TYPE_FSP_HDR
                                                     : FIL_PAGE_TYPE_XDES;
          break;
        case 1:
          reset_type = FIL_PAGE_IBUF_BITMAP;
          break;
        default:
          switch (page_type) {
            case FIL_PAGE_INDEX:
            case FIL_PAGE_RTREE:
            case FIL_PAGE_SDI:
            case FIL_PAGE_UNDO_LOG:
            case FIL_PAGE_INODE:
            case FIL_PAGE_IBUF_FREE_LIST:
            case FIL_PAGE_TYPE_ALLOCATED:
            case FIL_PAGE_TYPE_SYS:
            case FIL_PAGE_TYPE_TRX_SYS:
            case FIL_PAGE_TYPE_BLOB:
            case FIL_PAGE_TYPE_ZBLOB:
            case FIL_PAGE_TYPE_ZBLOB2:
            case FIL_PAGE_SDI_BLOB:
            case FIL_PAGE_SDI_ZBLOB:
            case FIL_PAGE_TYPE_LOB_INDEX:
            case FIL_PAGE_TYPE_LOB_DATA:
            case FIL_PAGE_TYPE_LOB_FIRST:
            case FIL_PAGE_TYPE_ZLOB_FIRST:
            case FIL_PAGE_TYPE_ZLOB_DATA:
            case FIL_PAGE_TYPE_ZLOB_INDEX:
            case FIL_PAGE_TYPE_ZLOB_FRAG:
            case FIL_PAGE_TYPE_ZLOB_FRAG_ENTRY:
            case FIL_PAGE_TYPE_RSEG_ARRAY:
              break;
            case FIL_PAGE_TYPE_FSP_HDR:
            case FIL_PAGE_TYPE_XDES:
            case FIL_PAGE_IBUF_BITMAP:
              /* These pages should have
              predetermined page numbers
              (see above). */
            default:
              reset_type = FIL_PAGE_TYPE_UNKNOWN;
              break;
          }
      }

      if (UNIV_UNLIKELY(page_type != reset_type)) {
        ib::info(ER_IB_MSG_125)
            << "Resetting invalid page " << block->page.id << " type "
            << page_type << " to " << reset_type << " when flushing.";
        fil_page_set_type(page, reset_type);
      }
    }

    switch ((srv_checksum_algorithm_t)srv_checksum_algorithm) {
      case SRV_CHECKSUM_ALGORITHM_CRC32:
      case SRV_CHECKSUM_ALGORITHM_STRICT_CRC32:
        checksum = buf_calc_page_crc32(page);
        mach_write_to_4(page + FIL_PAGE_SPACE_OR_CHKSUM, checksum);
        break;
      case SRV_CHECKSUM_ALGORITHM_INNODB:
      case SRV_CHECKSUM_ALGORITHM_STRICT_INNODB:
        checksum = (uint32_t)buf_calc_page_new_checksum(page);
        mach_write_to_4(page + FIL_PAGE_SPACE_OR_CHKSUM, checksum);
        checksum = (uint32_t)buf_calc_page_old_checksum(page);
        break;
      case SRV_CHECKSUM_ALGORITHM_NONE:
      case SRV_CHECKSUM_ALGORITHM_STRICT_NONE:
        mach_write_to_4(page + FIL_PAGE_SPACE_OR_CHKSUM, checksum);
        break;
        /* no default so the compiler will emit a warning if
        new enum is added and not handled here */
    }
  }

  /* With the InnoDB checksum, we overwrite the first 4 bytes of
  the end lsn field to store the old formula checksum. Since it
  depends also on the field FIL_PAGE_SPACE_OR_CHKSUM, it has to
  be calculated after storing the new formula checksum.

  In other cases we write the same value to both fields.
  If CRC32 is used then it is faster to use that checksum
  (calculated above) instead of calculating another one.
  We can afford to store something other than
  buf_calc_page_old_checksum() or BUF_NO_CHECKSUM_MAGIC in
  this field because the file will not be readable by old
  versions of MySQL/InnoDB anyway (older than MySQL 5.6.3) */

  mach_write_to_4(page + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM,
                  checksum);
}

#ifndef UNIV_HOTBACKUP
/** Does an asynchronous write of a buffer page.
@param[in]      bpage           buffer block to write
@param[in]      flush_type      type of flush
@param[in]      sync            true if sync IO request */
static void buf_flush_write_block_low(buf_page_t *bpage, buf_flush_t flush_type,
                                      bool sync) {
  page_t *frame = nullptr;

#ifdef UNIV_DEBUG
  buf_pool_t *buf_pool = buf_pool_from_bpage(bpage);
  ut_ad(!mutex_own(&buf_pool->LRU_list_mutex));
#endif /* UNIV_DEBUG */

  DBUG_PRINT("ib_buf", ("flush %s %u page " UINT32PF ":" UINT32PF,
                        sync ? "sync" : "async", (unsigned)flush_type,
                        bpage->id.space(), bpage->id.page_no()));

  ut_ad(buf_page_in_file(bpage));

  /* We are not holding block_mutex here. Nevertheless, it is safe to
  access bpage, because it is io_fixed and oldest_modification != 0.
  Thus, it cannot be relocated in the buffer pool or removed from
  flush_list or LRU_list. */
  ut_ad(!buf_flush_list_mutex_own(buf_pool));
  ut_ad(!buf_page_get_mutex(bpage)->is_owned());
  ut_ad(bpage->is_io_fix_write());
  ut_ad(bpage->is_dirty());

#ifdef UNIV_IBUF_COUNT_DEBUG
  ut_a(ibuf_count_get(bpage->id) == 0);
#endif /* UNIV_IBUF_COUNT_DEBUG */

  ut_ad(recv_recovery_is_on() || bpage->get_newest_lsn() != 0);

  /* Force the log to the disk before writing the modified block */
  if (!srv_read_only_mode) {
    const lsn_t flush_to_lsn = bpage->get_newest_lsn();

    /* Do the check before calling log_write_up_to() because in most
    cases it would allow to avoid call, and because of that we don't
    want those calls because they would have bad impact on the counter
    of calls, which is monitored to save CPU on spinning in log threads. */

    if (log_sys->flushed_to_disk_lsn.load() < flush_to_lsn) {
      Wait_stats wait_stats;

      wait_stats = log_write_up_to(*log_sys, flush_to_lsn, true);

      MONITOR_INC_WAIT_STATS_EX(MONITOR_ON_LOG_, _PAGE_WRITTEN, wait_stats);
    }
  }

  switch (buf_page_get_state(bpage)) {
    case BUF_BLOCK_POOL_WATCH:
    case BUF_BLOCK_ZIP_PAGE: /* The page should be dirty. */
    case BUF_BLOCK_NOT_USED:
    case BUF_BLOCK_READY_FOR_USE:
    case BUF_BLOCK_MEMORY:
    case BUF_BLOCK_REMOVE_HASH:
      ut_error;
      break;
    case BUF_BLOCK_ZIP_DIRTY: {
      frame = bpage->zip.data;
      BlockReporter reporter =
          BlockReporter(false, frame, bpage->size,
                        fsp_is_checksum_disabled(bpage->id.space()));

      mach_write_to_8(frame + FIL_PAGE_LSN, bpage->get_newest_lsn());

      ut_a(reporter.verify_zip_checksum());
      break;
    }
    case BUF_BLOCK_FILE_PAGE:
      frame = bpage->zip.data;
      if (!frame) {
        frame = ((buf_block_t *)bpage)->frame;
      }

      buf_flush_init_for_writing(
          reinterpret_cast<const buf_block_t *>(bpage),
          reinterpret_cast<const buf_block_t *>(bpage)->frame,
          bpage->zip.data ? &bpage->zip : nullptr, bpage->get_newest_lsn(),
          fsp_is_checksum_disabled(bpage->id.space()),
          false /* do not skip lsn check */);
      break;
  }

  dberr_t err = dblwr::write(flush_type, bpage, sync);

  ut_a(err == DB_SUCCESS || err == DB_TABLESPACE_DELETED);

  /* Increment the counter of I/O operations used
  for selecting LRU policy. */
  buf_LRU_stat_inc_io();
}

/** Writes a flushable page asynchronously from the buffer pool to a file.
NOTE: 1. in simulated aio we must call os_aio_simulated_wake_handler_threads
after we have posted a batch of writes! 2. buf_page_get_mutex(bpage) must be
held upon entering this function. The LRU list mutex must be held if flush_type
== BUF_FLUSH_SINGLE_PAGE. Both mutexes will be released by this function if it
returns true.
@param[in]      buf_pool        buffer pool instance
@param[in]      bpage           buffer control block
@param[in]      flush_type      type of flush
@param[in]      sync            true if sync IO request
@return true if page was flushed */
bool buf_flush_page(buf_pool_t *buf_pool, buf_page_t *bpage,
                    buf_flush_t flush_type, bool sync) {
  BPageMutex *block_mutex;

  ut_ad(flush_type < BUF_FLUSH_N_TYPES);
  /* Hold the LRU list mutex iff called for a single page LRU
  flush. A single page LRU flush is already non-performant, and holding
  the LRU list mutex allows us to avoid having to store the previous LRU
  list page or to restart the LRU scan in
  buf_flush_single_page_from_LRU(). */
  ut_ad(flush_type == BUF_FLUSH_SINGLE_PAGE ||
        !mutex_own(&buf_pool->LRU_list_mutex));
  ut_ad(flush_type != BUF_FLUSH_SINGLE_PAGE ||
        mutex_own(&buf_pool->LRU_list_mutex));
  ut_ad(buf_page_in_file(bpage));
  ut_ad(!sync || flush_type == BUF_FLUSH_SINGLE_PAGE);

  block_mutex = buf_page_get_mutex(bpage);
  ut_ad(mutex_own(block_mutex));

  ut_ad(buf_flush_ready_for_flush(bpage, flush_type));

  bool is_uncompressed;

  is_uncompressed = (buf_page_get_state(bpage) == BUF_BLOCK_FILE_PAGE);
  ut_ad(is_uncompressed == (block_mutex != &buf_pool->zip_mutex));

  bool flush;
  rw_lock_t *rw_lock = nullptr;
  bool no_fix_count = bpage->buf_fix_count == 0;

  if (!is_uncompressed) {
    flush = true;
    rw_lock = nullptr;
  } else if (!(no_fix_count || flush_type == BUF_FLUSH_LIST) ||
             (!no_fix_count &&
              srv_shutdown_state.load() < SRV_SHUTDOWN_FLUSH_PHASE &&
              fsp_is_system_temporary(bpage->id.space()))) {
    /* This is a heuristic, to avoid expensive SX attempts. */
    /* For table residing in temporary tablespace sync is done
    using IO_FIX and so before scheduling for flush ensure that
    page is not fixed. */
    flush = false;
  } else {
    rw_lock = &reinterpret_cast<buf_block_t *>(bpage)->lock;
    if (flush_type != BUF_FLUSH_LIST) {
      flush = rw_lock_sx_lock_nowait(rw_lock, BUF_IO_WRITE, UT_LOCATION_HERE);
    } else {
      /* Will SX lock later */
      flush = true;
    }
  }

  if (flush) {
    /* We are committed to flushing by the time we get here */

    mutex_enter(&buf_pool->flush_state_mutex);

    buf_page_set_io_fix(bpage, BUF_IO_WRITE);

    buf_page_set_flush_type(bpage, flush_type);

    if (buf_pool->n_flush[flush_type] == 0) {
      os_event_reset(buf_pool->no_flush[flush_type]);
    }

    ++buf_pool->n_flush[flush_type];

    if (bpage->get_oldest_lsn() > buf_pool->max_lsn_io) {
      buf_pool->max_lsn_io = bpage->get_oldest_lsn();
    }

    if (!fsp_is_system_temporary(bpage->id.space()) &&
        buf_pool->track_page_lsn != LSN_MAX) {
      auto frame = bpage->zip.data;

      if (frame == nullptr) {
        frame = ((buf_block_t *)bpage)->frame;
      }
      lsn_t frame_lsn = mach_read_from_8(frame + FIL_PAGE_LSN);

      arch_page_sys->track_page(bpage, buf_pool->track_page_lsn, frame_lsn,
                                false);
    }

    mutex_exit(&buf_pool->flush_state_mutex);

    mutex_exit(block_mutex);

    if (flush_type == BUF_FLUSH_SINGLE_PAGE) {
      mutex_exit(&buf_pool->LRU_list_mutex);
    }

    if (flush_type == BUF_FLUSH_LIST && is_uncompressed &&
        !rw_lock_sx_lock_nowait(rw_lock, BUF_IO_WRITE, UT_LOCATION_HERE)) {
      if (!fsp_is_system_temporary(bpage->id.space()) && dblwr::is_enabled()) {
        dblwr::force_flush(flush_type, buf_pool_index(buf_pool));
      } else {
        buf_flush_sync_datafiles();
      }

      rw_lock_sx_lock_gen(rw_lock, BUF_IO_WRITE, UT_LOCATION_HERE);
    }

    /* If there is an observer that wants to know if the
    asynchronous flushing was sent then notify it.
    Note: we set flush observer to a page with x-latch, so we can
    guarantee that notify_flush and notify_remove are called in pair
    with s-latch on a uncompressed page. */
    if (bpage->get_flush_observer() != nullptr) {
      bpage->get_flush_observer()->notify_flush(buf_pool, bpage);
    }

    /* Even though bpage is not protected by any mutex at this
    point, it is safe to access bpage, because it is io_fixed and
    oldest_modification != 0.  Thus, it cannot be relocated in the
    buffer pool or removed from flush_list or LRU_list. */

    buf_flush_write_block_low(bpage, flush_type, sync);
  }

  return flush;
}

#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
/** Writes a flushable page asynchronously from the buffer pool to a file.
NOTE: block and LRU list mutexes must be held upon entering this function, and
they will be released by this function after flushing. This is loosely based on
buf_flush_batch() and buf_flush_page().
@param[in,out]  buf_pool        buffer pool instance
@param[in,out]  block           buffer control block
@return true if the page was flushed and the mutex released */
bool buf_flush_page_try(buf_pool_t *buf_pool, buf_block_t *block) {
  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
  ut_ad(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);
  ut_ad(mutex_own(buf_page_get_mutex(&block->page)));

  if (!buf_flush_ready_for_flush(&block->page, BUF_FLUSH_SINGLE_PAGE)) {
    return false;
  }

  /* The following call will release the LRU list and block mutexes. */
  return (buf_flush_page(buf_pool, &block->page, BUF_FLUSH_SINGLE_PAGE, true));
}
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

/** Check if the page is in buffer pool and can be flushed.
@param[in]      page_id         page id
@param[in]      flush_type      BUF_FLUSH_LRU or BUF_FLUSH_LIST
@return true if the page can be flushed. */
static bool buf_flush_check_neighbor(const page_id_t &page_id,
                                     buf_flush_t flush_type) {
  buf_page_t *bpage;
  buf_pool_t *buf_pool = buf_pool_get(page_id);
  bool ret;
  rw_lock_t *hash_lock;
  BPageMutex *block_mutex;

  ut_ad(flush_type == BUF_FLUSH_LRU || flush_type == BUF_FLUSH_LIST);

  /* We only want to flush pages from this buffer pool. */
  bpage = buf_page_hash_get_s_locked(buf_pool, page_id, &hash_lock);

  if (!bpage) {
    return (false);
  }

  block_mutex = buf_page_get_mutex(bpage);

  mutex_enter(block_mutex);

  rw_lock_s_unlock(hash_lock);

  ut_a(buf_page_in_file(bpage));

  /* We avoid flushing 'non-old' blocks in an LRU flush,
  because the flushed blocks are soon freed */

  ret = false;
  if (flush_type != BUF_FLUSH_LRU || buf_page_is_old(bpage)) {
    if (buf_flush_ready_for_flush(bpage, flush_type)) {
      ret = true;
    }
  }

  mutex_exit(block_mutex);

  return (ret);
}

/** Flushes to disk all flushable pages within the flush area.
@param[in]      page_id         page id
@param[in]      flush_type      BUF_FLUSH_LRU or BUF_FLUSH_LIST
@param[in]      n_flushed       number of pages flushed so far in this batch
@param[in]      n_to_flush      maximum number of pages we are allowed to flush
@return number of pages flushed */
static ulint buf_flush_try_neighbors(const page_id_t &page_id,
                                     buf_flush_t flush_type, ulint n_flushed,
                                     ulint n_to_flush) {
  page_no_t i;
  page_no_t low;
  page_no_t high;
  ulint count = 0;
  buf_pool_t *buf_pool = buf_pool_get(page_id);

  ut_ad(flush_type == BUF_FLUSH_LRU || flush_type == BUF_FLUSH_LIST);
  ut_ad(!mutex_own(&buf_pool->LRU_list_mutex));
  ut_ad(!buf_flush_list_mutex_own(buf_pool));

  if (UT_LIST_GET_LEN(buf_pool->LRU) < BUF_LRU_OLD_MIN_LEN ||
      srv_flush_neighbors == 0) {
    /* If there is little space or neighbor flushing is
    not enabled then just flush the victim. */
    low = page_id.page_no();
    high = page_id.page_no() + 1;
  } else {
    /* When flushed, dirty blocks are searched in
    neighborhoods of this size, and flushed along with the
    original page. */

    page_no_t buf_flush_area;

    buf_flush_area = std::min(buf_pool->read_ahead_area,
                              static_cast<page_no_t>(buf_pool->curr_size / 16));

    low = (page_id.page_no() / buf_flush_area) * buf_flush_area;
    high = (page_id.page_no() / buf_flush_area + 1) * buf_flush_area;

    if (srv_flush_neighbors == 1) {
      /* adjust 'low' and 'high' to limit
         for contiguous dirty area */
      if (page_id.page_no() > low) {
        for (i = page_id.page_no() - 1; i >= low; i--) {
          if (!buf_flush_check_neighbor(page_id_t(page_id.space(), i),
                                        flush_type)) {
            break;
          }

          if (i == low) {
            /* Avoid overwrap when low == 0
            and calling
            buf_flush_check_neighbor() with
            i == (ulint) -1 */
            i--;
            break;
          }
        }
        low = i + 1;
      }

      for (i = page_id.page_no() + 1;
           i < high &&
           buf_flush_check_neighbor(page_id_t(page_id.space(), i), flush_type);
           i++) {
        /* do nothing */
      }
      high = i;
    }
  }

  DBUG_PRINT("ib_buf", ("flush " UINT32PF ":%u..%u", page_id.space(),
                        (unsigned)low, (unsigned)high));

  for (i = low; i < high; i++) {
    if ((count + n_flushed) >= n_to_flush) {
      /* We have already flushed enough pages and
      should call it a day. There is, however, one
      exception. If the page whose neighbors we
      are flushing has not been flushed yet then
      we'll try to flush the victim that we
      selected originally. */
      if (i <= page_id.page_no()) {
        i = page_id.page_no();
      } else {
        break;
      }
    }

    const page_id_t cur_page_id(page_id.space(), i);

    auto buf_pool = buf_pool_get(cur_page_id);

    rw_lock_t *hash_lock;

    /* We only want to flush pages from this buffer pool. */
    auto bpage = buf_page_hash_get_s_locked(buf_pool, cur_page_id, &hash_lock);

    if (bpage == nullptr) {
      continue;
    }

    auto block_mutex = buf_page_get_mutex(bpage);

    mutex_enter(block_mutex);

    if (flush_type == BUF_FLUSH_LIST &&
        buf_flush_ready_for_flush(bpage, flush_type) &&
        bpage->buf_fix_count == 0 && bpage->was_stale()) {
      mutex_exit(block_mutex);
      buf_page_free_stale(buf_pool, bpage, hash_lock);
      continue;
    }

    rw_lock_s_unlock(hash_lock);

    ut_a(buf_page_in_file(bpage));

    /* We avoid flushing 'non-old' blocks in an LRU flush,
    because the flushed blocks are soon freed */

    if (flush_type != BUF_FLUSH_LRU || i == page_id.page_no() ||
        buf_page_is_old(bpage)) {
      if (buf_flush_ready_for_flush(bpage, flush_type) &&
          (i == page_id.page_no() || bpage->buf_fix_count == 0)) {
        /* We also try to flush those
        neighbors != offset */

        if (buf_flush_page(buf_pool, bpage, flush_type, false)) {
          ++count;
        } else {
          mutex_exit(block_mutex);
        }

        continue;
      }
    }

    mutex_exit(block_mutex);
  }

  if (count > 1) {
    MONITOR_INC_VALUE_CUMULATIVE(MONITOR_FLUSH_NEIGHBOR_TOTAL_PAGE,
                                 MONITOR_FLUSH_NEIGHBOR_COUNT,
                                 MONITOR_FLUSH_NEIGHBOR_PAGES, (count - 1));
  }

  return (count);
}

/** Check if the block is modified and ready for flushing.
If ready to flush then flush the page and try o flush its neighbors. The caller
must hold the buffer pool list mutex corresponding to the type of flush.
@param[in]  bpage       buffer control block,
                        must be buf_page_in_file(bpage)
@param[in]  flush_type  BUF_FLUSH_LRU or BUF_FLUSH_LIST
@param[in]  n_to_flush  number of pages to flush
@param[in,out]  count   number of pages flushed
@return true if the list mutex was released during this function.  This does
not guarantee that some pages were written as well. */
static bool buf_flush_page_and_try_neighbors(buf_page_t *bpage,
                                             buf_flush_t flush_type,
                                             ulint n_to_flush, ulint *count) {
#ifdef UNIV_DEBUG
  buf_pool_t *buf_pool = buf_pool_from_bpage(bpage);
#endif /* UNIV_DEBUG */

  bool flushed;
  BPageMutex *block_mutex = nullptr;

  ut_ad(flush_type != BUF_FLUSH_SINGLE_PAGE);

  ut_ad((flush_type == BUF_FLUSH_LRU && mutex_own(&buf_pool->LRU_list_mutex)) ||
        (flush_type == BUF_FLUSH_LIST && buf_flush_list_mutex_own(buf_pool)));

  if (flush_type == BUF_FLUSH_LRU) {
    block_mutex = buf_page_get_mutex(bpage);
    mutex_enter(block_mutex);
  }

#ifdef UNIV_DEBUG
  if (!buf_page_in_file(bpage)) {
    ut_ad(buf_page_get_state(bpage) == BUF_BLOCK_REMOVE_HASH);
    ut_ad(!mutex_own(&buf_pool->LRU_list_mutex));
  }
#else
  ut_a(buf_page_in_file(bpage) ||
       buf_page_get_state(bpage) == BUF_BLOCK_REMOVE_HASH);
#endif /* UNIV_DEBUG */

  /* This is just a heuristic check, perhaps without block mutex latch, so we
  will repeat the check with block mutexes in buf_flush_try_neighbors. */
  if (buf_flush_was_ready_for_flush(bpage, flush_type)) {
    buf_pool_t *buf_pool;

    buf_pool = buf_pool_from_bpage(bpage);

    if (flush_type == BUF_FLUSH_LRU) {
      mutex_exit(&buf_pool->LRU_list_mutex);
    }

    const page_id_t page_id = bpage->id;

    if (flush_type == BUF_FLUSH_LRU) {
      mutex_exit(block_mutex);
    } else {
      buf_flush_list_mutex_exit(buf_pool);
    }

    /* Try to flush also all the neighbors */
    *count += buf_flush_try_neighbors(page_id, flush_type, *count, n_to_flush);

    if (flush_type == BUF_FLUSH_LRU) {
      mutex_enter(&buf_pool->LRU_list_mutex);
    } else {
      buf_flush_list_mutex_enter(buf_pool);
    }
    flushed = true;

  } else if (flush_type == BUF_FLUSH_LRU) {
    mutex_exit(block_mutex);

    flushed = false;
  } else {
    flushed = false;
  }

  ut_ad((flush_type == BUF_FLUSH_LRU && mutex_own(&buf_pool->LRU_list_mutex)) ||
        (flush_type == BUF_FLUSH_LIST && buf_flush_list_mutex_own(buf_pool)));

  return (flushed);
}

/** This utility moves the uncompressed frames of pages to the free list.
Note that this function does not actually flush any data to disk. It
just detaches the uncompressed frames from the compressed pages at the
tail of the unzip_LRU and puts those freed frames in the free list.
Note that it is a best effort attempt and it is not guaranteed that
after a call to this function there will be 'max' blocks in the free
list. The caller must hold the LRU list mutex.
@param[in]      buf_pool        buffer pool instance
@param[in]      max             desired number of blocks in the free_list
@return number of blocks moved to the free list. */
static ulint buf_free_from_unzip_LRU_list_batch(buf_pool_t *buf_pool,
                                                ulint max) {
  ulint scanned = 0;
  ulint count = 0;
  ulint free_len = UT_LIST_GET_LEN(buf_pool->free);
  ulint lru_len = UT_LIST_GET_LEN(buf_pool->unzip_LRU);

  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

  buf_block_t *block = UT_LIST_GET_LAST(buf_pool->unzip_LRU);

  while (block != nullptr && count < max && free_len < srv_LRU_scan_depth &&
         lru_len > UT_LIST_GET_LEN(buf_pool->LRU) / 10) {
    BPageMutex *block_mutex = buf_page_get_mutex(&block->page);

    ++scanned;

    mutex_enter(block_mutex);

    if (buf_LRU_free_page(&block->page, false)) {
      /* Block was freed, all mutexes released */
      ++count;
      mutex_enter(&buf_pool->LRU_list_mutex);
      block = UT_LIST_GET_LAST(buf_pool->unzip_LRU);

    } else {
      mutex_exit(block_mutex);
      block = UT_LIST_GET_PREV(unzip_LRU, block);
    }

    free_len = UT_LIST_GET_LEN(buf_pool->free);
    lru_len = UT_LIST_GET_LEN(buf_pool->unzip_LRU);
  }

  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

  if (scanned) {
    MONITOR_INC_VALUE_CUMULATIVE(MONITOR_LRU_BATCH_SCANNED,
                                 MONITOR_LRU_BATCH_SCANNED_NUM_CALL,
                                 MONITOR_LRU_BATCH_SCANNED_PER_CALL, scanned);
  }

  return (count);
}

/** This utility flushes dirty blocks from the end of the LRU list.
The calling thread is not allowed to own any latches on pages!
It attempts to make 'max' blocks available in the free list. Note that
it is a best effort attempt and it is not guaranteed that after a call
to this function there will be 'max' blocks in the free list.
@param[in]      buf_pool        buffer pool instance
@param[in]      max             desired number for blocks in the free_list
@return number of blocks for which the write request was queued. */
static ulint buf_flush_LRU_list_batch(buf_pool_t *buf_pool, ulint max) {
  buf_page_t *bpage;
  ulint scanned = 0;
  ulint evict_count = 0;
  ulint count = 0;
  ulint free_len = UT_LIST_GET_LEN(buf_pool->free);
  ulint lru_len = UT_LIST_GET_LEN(buf_pool->LRU);
  ulint withdraw_depth;

  withdraw_depth = buf_get_withdraw_depth(buf_pool);

  for (bpage = UT_LIST_GET_LAST(buf_pool->LRU);
       bpage != nullptr && count + evict_count < max &&
       free_len < srv_LRU_scan_depth + withdraw_depth &&
       lru_len > BUF_LRU_MIN_LEN;
       ++scanned, bpage = buf_pool->lru_hp.get()) {
    ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

    auto prev = UT_LIST_GET_PREV(LRU, bpage);
    buf_pool->lru_hp.set(prev);

    auto block_mutex = buf_page_get_mutex(bpage);

    if (bpage->was_stale()) {
      if (buf_page_free_stale(buf_pool, bpage)) {
        ++evict_count;
        mutex_enter(&buf_pool->LRU_list_mutex);
      }
    } else {
      auto acquired = mutex_enter_nowait(block_mutex) == 0;

      if (acquired && buf_flush_ready_for_replace(bpage)) {
        /* block is ready for eviction i.e., it is
        clean and is not IO-fixed or buffer fixed. */
        if (buf_LRU_free_page(bpage, true)) {
          ++evict_count;
          mutex_enter(&buf_pool->LRU_list_mutex);
        } else {
          mutex_exit(block_mutex);
        }
      } else if (acquired && buf_flush_ready_for_flush(bpage, BUF_FLUSH_LRU)) {
        /* Block is ready for flush. Dispatch an IO request. The IO helper
        thread will put it on the free list in the IO completion routine. */
        mutex_exit(block_mutex);
        buf_flush_page_and_try_neighbors(bpage, BUF_FLUSH_LRU, max, &count);
      } else if (!acquired) {
        ut_ad(buf_pool->lru_hp.is_hp(prev));
      } else {
        /* Can't evict or dispatch this block. Go to previous. */
        mutex_exit(block_mutex);
        ut_ad(buf_pool->lru_hp.is_hp(prev));
      }
    }

    ut_ad(!mutex_own(block_mutex));
    ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

    free_len = UT_LIST_GET_LEN(buf_pool->free);
    lru_len = UT_LIST_GET_LEN(buf_pool->LRU);
    withdraw_depth = buf_get_withdraw_depth(buf_pool);
  }

  buf_pool->lru_hp.set(nullptr);

  /* We keep track of all flushes happening as part of LRU
  flush. When estimating the desired rate at which flush_list
  should be flushed, we factor in this value. */
  buf_lru_flush_page_count += count;

  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

  if (evict_count) {
    MONITOR_INC_VALUE_CUMULATIVE(MONITOR_LRU_BATCH_EVICT_TOTAL_PAGE,
                                 MONITOR_LRU_BATCH_EVICT_COUNT,
                                 MONITOR_LRU_BATCH_EVICT_PAGES, evict_count);
  }

  if (scanned) {
    MONITOR_INC_VALUE_CUMULATIVE(MONITOR_LRU_BATCH_SCANNED,
                                 MONITOR_LRU_BATCH_SCANNED_NUM_CALL,
                                 MONITOR_LRU_BATCH_SCANNED_PER_CALL, scanned);
  }

  return (count);
}

/** Flush and move pages from LRU or unzip_LRU list to the free list.
Whether LRU or unzip_LRU is used depends on the state of the system.
@param[in]      buf_pool        buffer pool instance
@param[in]      max             desired number of blocks in the free_list
@return number of blocks for which either the write request was queued
or in case of unzip_LRU the number of blocks actually moved to the
free list */
static ulint buf_do_LRU_batch(buf_pool_t *buf_pool, ulint max) {
  ulint count = 0;

  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

  if (buf_LRU_evict_from_unzip_LRU(buf_pool)) {
    count += buf_free_from_unzip_LRU_list_batch(buf_pool, max);
  }

  if (max > count) {
    count += buf_flush_LRU_list_batch(buf_pool, max - count);
  }

  return (count);
}

/** This utility flushes dirty blocks from the end of the flush_list.
The calling thread is not allowed to own any latches on pages!
@param[in]      buf_pool        buffer pool instance
@param[in]      min_n           wished minimum number of blocks flushed (it is
not guaranteed that the actual number is that big, though)
@param[in]      lsn_limit       all blocks whose oldest_modification is smaller
than this should be flushed (if their number does not exceed min_n)
@return number of blocks for which the write request was queued;
ULINT_UNDEFINED if there was a flush of the same type already
running */
static ulint buf_do_flush_list_batch(buf_pool_t *buf_pool, ulint min_n,
                                     lsn_t lsn_limit) {
  ulint count = 0;
  ulint scanned = 0;

  /* Start from the end of the list looking for a suitable
  block to be flushed. */
  buf_flush_list_mutex_enter(buf_pool);
  ulint len = UT_LIST_GET_LEN(buf_pool->flush_list);

  /* In order not to degenerate this scan to O(n*n) we attempt
  to preserve pointer of previous block in the flush list. To do
  so we declare it a hazard pointer. Any thread working on the
  flush list must check the hazard pointer and if it is removing
  the same block then it must reset it. */
  for (buf_page_t *bpage = UT_LIST_GET_LAST(buf_pool->flush_list);
       count < min_n && bpage != nullptr && len > 0 &&
       bpage->get_oldest_lsn() < lsn_limit;
       bpage = buf_pool->flush_hp.get(), ++scanned) {
    buf_page_t *prev;

    ut_a(bpage->is_dirty());
    ut_ad(bpage->in_flush_list);

    prev = UT_LIST_GET_PREV(list, bpage);
    buf_pool->flush_hp.set(prev);

#ifdef UNIV_DEBUG
    bool flushed =
#endif /* UNIV_DEBUG */
        buf_flush_page_and_try_neighbors(bpage, BUF_FLUSH_LIST, min_n, &count);

    ut_ad(flushed || buf_pool->flush_hp.is_hp(prev));

    --len;
  }

  buf_pool->flush_hp.set(nullptr);
  buf_flush_list_mutex_exit(buf_pool);

  if (scanned) {
    MONITOR_INC_VALUE_CUMULATIVE(MONITOR_FLUSH_BATCH_SCANNED,
                                 MONITOR_FLUSH_BATCH_SCANNED_NUM_CALL,
                                 MONITOR_FLUSH_BATCH_SCANNED_PER_CALL, scanned);
  }

  if (count) {
    MONITOR_INC_VALUE_CUMULATIVE(MONITOR_FLUSH_BATCH_TOTAL_PAGE,
                                 MONITOR_FLUSH_BATCH_COUNT,
                                 MONITOR_FLUSH_BATCH_PAGES, count);
  }

  return (count);
}

/** This utility flushes dirty blocks from the end of the LRU list or
flush_list.
NOTE 1: in the case of an LRU flush the calling thread may own latches to
pages: to avoid deadlocks, this function must be written so that it cannot
end up waiting for these latches! NOTE 2: in the case of a flush list flush,
the calling thread is not allowed to own any latches on pages!
@param[in]      buf_pool        buffer pool instance
@param[in]      flush_type      BUF_FLUSH_LRU or BUF_FLUSH_LIST; if
BUF_FLUSH_LIST, then the caller must not own any latches on pages
@param[in]      min_n           wished minimum number of blocks flushed (it is
not guaranteed that the actual number is that big, though)
@param[in]      lsn_limit       in the case of BUF_FLUSH_LIST all blocks whose
oldest_modification is smaller than this should be flushed (if their number
does not exceed min_n), otherwise ignored
@return number of blocks for which the write request was queued */
static ulint buf_flush_batch(buf_pool_t *buf_pool, buf_flush_t flush_type,
                             ulint min_n, lsn_t lsn_limit) {
  ut_ad(flush_type == BUF_FLUSH_LRU || flush_type == BUF_FLUSH_LIST);

#ifdef UNIV_DEBUG
  {
    dict_sync_check check(true);

    ut_ad(flush_type != BUF_FLUSH_LIST || !sync_check_iterate(check));
  }
#endif /* UNIV_DEBUG */

  ulint count = 0;

  /* Note: The buffer pool mutexes is released and reacquired within
  the flush functions. */
  switch (flush_type) {
    case BUF_FLUSH_LRU:
      mutex_enter(&buf_pool->LRU_list_mutex);
      count = buf_do_LRU_batch(buf_pool, min_n);
      mutex_exit(&buf_pool->LRU_list_mutex);
      break;
    case BUF_FLUSH_LIST:
      count = buf_do_flush_list_batch(buf_pool, min_n, lsn_limit);
      break;
    default:
      ut_error;
  }

  DBUG_PRINT("ib_buf", ("flush %u completed, %u pages", unsigned(flush_type),
                        unsigned(count)));

  return (count);
}

/** Gather the aggregated stats for both flush list and LRU list flushing.
 @param page_count_flush        number of pages flushed from the end of the
 flush_list
 @param page_count_LRU  number of pages flushed from the end of the LRU list
 */
static void buf_flush_stats(ulint page_count_flush, ulint page_count_LRU) {
  DBUG_PRINT("ib_buf", ("flush completed, from flush_list %u pages, "
                        "from LRU_list %u pages",
                        unsigned(page_count_flush), unsigned(page_count_LRU)));

  srv_stats.buf_pool_flushed.add(page_count_flush + page_count_LRU);
}

/** Start a buffer flush batch for LRU or flush list
@param[in]      buf_pool        buffer pool instance
@param[in]      flush_type      BUF_FLUSH_LRU or BUF_FLUSH_LIST */
static bool buf_flush_start(buf_pool_t *buf_pool, buf_flush_t flush_type) {
  ut_ad(flush_type == BUF_FLUSH_LRU || flush_type == BUF_FLUSH_LIST);

  mutex_enter(&buf_pool->flush_state_mutex);

  if (buf_pool->n_flush[flush_type] > 0 ||
      buf_pool->init_flush[flush_type] == true) {
    /* There is already a flush batch of the same type running */

    mutex_exit(&buf_pool->flush_state_mutex);

    return false;
  }

  buf_pool->init_flush[flush_type] = true;

  os_event_reset(buf_pool->no_flush[flush_type]);

  mutex_exit(&buf_pool->flush_state_mutex);

  return true;
}

/** End a buffer flush batch for LRU or flush list
@param[in]      buf_pool        buffer pool instance
@param[in]      flush_type      BUF_FLUSH_LRU or BUF_FLUSH_LIST */
static void buf_flush_end(buf_pool_t *buf_pool, buf_flush_t flush_type) {
  mutex_enter(&buf_pool->flush_state_mutex);

  buf_pool->init_flush[flush_type] = false;

  buf_pool->try_LRU_scan = true;

  if (buf_pool->n_flush[flush_type] == 0) {
    /* The running flush batch has ended */

    os_event_set(buf_pool->no_flush[flush_type]);
  }

  mutex_exit(&buf_pool->flush_state_mutex);

  if (!srv_read_only_mode) {
    if (dblwr::is_enabled()) {
      dblwr::force_flush(flush_type, buf_pool_index(buf_pool));
    } else {
      buf_flush_sync_datafiles();
    }
  } else {
    os_aio_simulated_wake_handler_threads();
  }
}

void buf_flush_wait_batch_end(buf_pool_t *buf_pool, buf_flush_t flush_type) {
  ut_ad(flush_type == BUF_FLUSH_LRU || flush_type == BUF_FLUSH_LIST);

  if (buf_pool == nullptr) {
    ulint i;

    for (i = 0; i < srv_buf_pool_instances; ++i) {
      auto buf_pool = buf_pool_from_array(i);

      thd_wait_begin(nullptr, THD_WAIT_DISKIO);
      os_event_wait(buf_pool->no_flush[flush_type]);
      thd_wait_end(nullptr);
    }
  } else {
    thd_wait_begin(nullptr, THD_WAIT_DISKIO);
    os_event_wait(buf_pool->no_flush[flush_type]);
    thd_wait_end(nullptr);
  }
}

bool buf_flush_do_batch(buf_pool_t *buf_pool, buf_flush_t type, ulint min_n,
                        lsn_t lsn_limit, ulint *n_processed) {
  ut_ad(type == BUF_FLUSH_LRU || type == BUF_FLUSH_LIST);

  if (n_processed != nullptr) {
    *n_processed = 0;
  }

  if (!buf_flush_start(buf_pool, type)) {
    return (false);
  }

  ulint page_count = buf_flush_batch(buf_pool, type, min_n, lsn_limit);

  buf_flush_end(buf_pool, type);

  if (n_processed != nullptr) {
    *n_processed = page_count;
  }

  return (true);
}

bool buf_flush_lists(ulint min_n, lsn_t lsn_limit, ulint *n_processed) {
  ulint n_flushed = 0;
  bool success = true;

  if (n_processed) {
    *n_processed = 0;
  }

  if (min_n != ULINT_MAX) {
    /* Ensure that flushing is spread evenly amongst the
    buffer pool instances. When min_n is ULINT_MAX
    we need to flush everything up to the lsn limit
    so no limit here. */
    min_n = (min_n + srv_buf_pool_instances - 1) / srv_buf_pool_instances;
  }

  /* Flush to lsn_limit in all buffer pool instances */
  for (ulint i = 0; i < srv_buf_pool_instances; i++) {
    buf_pool_t *buf_pool;
    ulint page_count = 0;

    buf_pool = buf_pool_from_array(i);

    if (!buf_flush_do_batch(buf_pool, BUF_FLUSH_LIST, min_n, lsn_limit,
                            &page_count)) {
      /* We have two choices here. If lsn_limit was
      specified then skipping an instance of buffer
      pool means we cannot guarantee that all pages
      up to lsn_limit has been flushed. We can
      return right now with failure or we can try
      to flush remaining buffer pools up to the
      lsn_limit. We attempt to flush other buffer
      pools based on the assumption that it will
      help in the retry which will follow the
      failure. */
      success = false;

      continue;
    }

    n_flushed += page_count;
  }

  if (n_flushed) {
    buf_flush_stats(n_flushed, 0);
  }

  if (n_processed) {
    *n_processed = n_flushed;
  }

  return (success);
}

/** This function picks up a single page from the tail of the LRU
list, flushes it (if it is dirty), removes it from page_hash and LRU
list and puts it on the free list. It is called from user threads when
they are unable to find a replaceable page at the tail of the LRU
list i.e.: when the background LRU flushing in the page_cleaner thread
is not fast enough to keep pace with the workload.
@param[in,out]  buf_pool        buffer pool instance
@return true if success. */
bool buf_flush_single_page_from_LRU(buf_pool_t *buf_pool) {
  bool freed;
  ulint scanned;
  buf_page_t *bpage;

  mutex_enter(&buf_pool->LRU_list_mutex);

  for (bpage = buf_pool->single_scan_itr.start(), scanned = 0, freed = false;
       bpage != nullptr; ++scanned, bpage = buf_pool->single_scan_itr.get()) {
    ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

    auto prev = UT_LIST_GET_PREV(LRU, bpage);

    buf_pool->single_scan_itr.set(prev);

    if (bpage->was_stale()) {
      freed = buf_page_free_stale(buf_pool, bpage);
      if (freed) {
        break;
      }
    } else {
      auto block_mutex = buf_page_get_mutex(bpage);

      mutex_enter(block_mutex);

      if (buf_flush_ready_for_replace(bpage)) {
        /* block is ready for eviction i.e., it is
        clean and is not IO-fixed or buffer fixed. */

        if (buf_LRU_free_page(bpage, true)) {
          freed = true;
          break;
        }

        mutex_exit(block_mutex);

      } else if (buf_flush_ready_for_flush(bpage, BUF_FLUSH_SINGLE_PAGE)) {
        /* Block is ready for flush. Try and dispatch an IO
        request. We'll put it on free list in IO completion
        routine if it is not buffer fixed. The following call
        will release the buffer pool and block mutex.

        Note: There is no guarantee that this page has actually
        been freed, only that it has been flushed to disk */

        freed = buf_flush_page(buf_pool, bpage, BUF_FLUSH_SINGLE_PAGE, true);

        if (freed) {
          break;
        }

        mutex_exit(block_mutex);
      } else {
        mutex_exit(block_mutex);
      }
      ut_ad(!mutex_own(block_mutex));
    }
  }

  if (!freed) {
    /* Can't find a single flushable page. */
    ut_ad(bpage == nullptr);
    mutex_exit(&buf_pool->LRU_list_mutex);
  }

  if (scanned) {
    MONITOR_INC_VALUE_CUMULATIVE(MONITOR_LRU_SINGLE_FLUSH_SCANNED,
                                 MONITOR_LRU_SINGLE_FLUSH_SCANNED_NUM_CALL,
                                 MONITOR_LRU_SINGLE_FLUSH_SCANNED_PER_CALL,
                                 scanned);
  }

  ut_ad(!mutex_own(&buf_pool->LRU_list_mutex));

  return freed;
}

/**
Clears up tail of the LRU list of a given buffer pool instance:
* Put replaceable pages at the tail of LRU to the free list
* Flush dirty pages at the tail of LRU to the disk
The depth to which we scan each buffer pool is controlled by dynamic
config parameter innodb_LRU_scan_depth.
@param buf_pool buffer pool instance
@return total pages flushed */
static ulint buf_flush_LRU_list(buf_pool_t *buf_pool) {
  ulint scan_depth, withdraw_depth;
  ulint n_flushed = 0;

  ut_ad(buf_pool);

  /* srv_LRU_scan_depth can be arbitrarily large value.
  We cap it with current LRU size. */
  scan_depth = UT_LIST_GET_LEN(buf_pool->LRU);
  withdraw_depth = buf_get_withdraw_depth(buf_pool);

  if (withdraw_depth > srv_LRU_scan_depth) {
    scan_depth = std::min(withdraw_depth, scan_depth);
  } else {
    scan_depth = std::min(static_cast<ulint>(srv_LRU_scan_depth), scan_depth);
  }

  /* Currently one of page_cleaners is the only thread
  that can trigger an LRU flush at the same time.
  So, it is not possible that a batch triggered during
  last iteration is still running, */
  buf_flush_do_batch(buf_pool, BUF_FLUSH_LRU, scan_depth, 0, &n_flushed);

  return (n_flushed);
}

/** Wait for any possible LRU flushes that are in progress to end. */
void buf_flush_wait_LRU_batch_end(void) {
  for (ulint i = 0; i < srv_buf_pool_instances; i++) {
    buf_pool_t *buf_pool;

    buf_pool = buf_pool_from_array(i);

    mutex_enter(&buf_pool->flush_state_mutex);

    if (buf_pool->n_flush[BUF_FLUSH_LRU] > 0 ||
        buf_pool->init_flush[BUF_FLUSH_LRU]) {
      mutex_exit(&buf_pool->flush_state_mutex);
      buf_flush_wait_batch_end(buf_pool, BUF_FLUSH_LRU);
    } else {
      mutex_exit(&buf_pool->flush_state_mutex);
    }
  }
}

namespace Adaptive_flush {

/** Time stamp of current iteration. */
std::chrono::steady_clock::time_point cur_iter_time;

/** LSN at current iteration. */
lsn_t cur_iter_lsn = 0;

/** Number of dirty pages in flush list in current iteration. */
ulint cur_iter_pages_dirty = 0;

/** Dirty page percentage in buffer pool. */
ulint cur_iter_dirty_pct = 0;

/** Time stamp of previous iteration. */
std::chrono::steady_clock::time_point prev_iter_time;

/** Number of dirty pages in flush list at previous iteration. */
ulint prev_iter_pages_dirty = 0;

/** Actual number of pages flushed by last iteration. */
ulint prev_iter_pages_flushed = 0;

/** Average redo generation rate */
lsn_t lsn_avg_rate = 0;

/** Average page flush rate */
ulint page_avg_rate = 0;

/** LSN  when last average rates are computed. */
lsn_t prev_lsn = 0;

/** Time stamp when last average rates are computed. */
std::chrono::steady_clock::time_point prev_time;

/** Number of iteration till average rates are computed. */
ulint n_iterations = 0;

/** Pages flushed till last average rates are computed.*/
ulint sum_pages = 0;

/** Initialize flush parameters for current iteration.
@param[in]      n_pages_last    number of pages flushed in last iteration
@return true if current iteration should be skipped. */
bool initialize(ulint n_pages_last) {
  lsn_t curr_lsn = log_buffer_dirty_pages_added_up_to_lsn(*log_sys);
  const auto curr_time = std::chrono::steady_clock::now();

  if (prev_lsn == 0) {
    /* First time initialization for next average computation. */
    prev_lsn = curr_lsn;
    prev_time = curr_time;
    prev_iter_time = curr_time;

    return (true);
  }

  prev_iter_pages_flushed = n_pages_last;

  cur_iter_lsn = curr_lsn;
  cur_iter_time = curr_time;
  cur_iter_pages_dirty = 0;

  return (false);
}

/** Set average LSN and page flush speed across multiple iterations. */
void set_average() {
  ++n_iterations;
  sum_pages += prev_iter_pages_flushed;
  auto time_elapsed = cur_iter_time - prev_time;

  if (time_elapsed < std::chrono::seconds{1}) {
    time_elapsed = std::chrono::seconds{1};
  }

  auto avg_loops = srv_flushing_avg_loops;

  /* Adjust flushing loop when redo log flush is disabled. */
  if (mtr_t::s_logging.is_disabled()) {
    auto nolog_loop = mtr_t::s_logging.get_nolog_flush_loop();
    if (nolog_loop < avg_loops) {
      avg_loops = nolog_loop;
    }
  }

  /* We update our variables every srv_flushing_avg_loops iterations to smooth
  out transition in workload. */
  if (n_iterations < avg_loops &&
      time_elapsed < std::chrono::seconds{avg_loops}) {
    return;
  }

  const auto time_elapsed_sec =
      std::chrono::duration_cast<std::chrono::duration<double>>(time_elapsed)
          .count();

  page_avg_rate =
      static_cast<ulint>(((sum_pages / time_elapsed_sec) + page_avg_rate) / 2);

  /* How much LSN we have generated since last call. */
  auto lsn_rate =
      static_cast<lsn_t>((cur_iter_lsn - prev_lsn) / time_elapsed_sec);

  lsn_avg_rate = (lsn_avg_rate + lsn_rate) / 2;

  MONITOR_SET(MONITOR_FLUSH_AVG_PAGE_RATE, page_avg_rate);
  MONITOR_SET(MONITOR_FLUSH_LSN_AVG_RATE, lsn_avg_rate);

  /* aggregate stats of all slots */
  mutex_enter(&page_cleaner->mutex);

  auto flush_tm = page_cleaner->flush_time.count();
  ulint flush_pass = page_cleaner->flush_pass;

  page_cleaner->flush_time = std::chrono::seconds::zero();
  page_cleaner->flush_pass = 0;

  uint64_t lru_tm = 0;
  uint64_t list_tm = 0;
  ulint lru_pass = 0;
  ulint list_pass = 0;

  for (ulint i = 0; i < page_cleaner->n_slots; i++) {
    page_cleaner_slot_t *slot;

    slot = &page_cleaner->slots[i];

    lru_tm += slot->flush_lru_time.count();
    lru_pass += slot->flush_lru_pass;
    list_tm += slot->flush_list_time.count();
    list_pass += slot->flush_list_pass;

    slot->flush_lru_time = std::chrono::seconds::zero();
    slot->flush_lru_pass = 0;
    slot->flush_list_time = std::chrono::seconds::zero();
    slot->flush_list_pass = 0;
  }

  mutex_exit(&page_cleaner->mutex);

  /* minimum values are 1, to avoid dividing by zero. */
  if (lru_tm < 1) {
    lru_tm = 1;
  }
  if (list_tm < 1) {
    list_tm = 1;
  }
  if (flush_tm < 1) {
    flush_tm = 1;
  }

  if (lru_pass < 1) {
    lru_pass = 1;
  }
  if (list_pass < 1) {
    list_pass = 1;
  }
  if (flush_pass < 1) {
    flush_pass = 1;
  }

  MONITOR_SET(MONITOR_FLUSH_ADAPTIVE_AVG_TIME_SLOT, list_tm / list_pass);
  MONITOR_SET(MONITOR_LRU_BATCH_FLUSH_AVG_TIME_SLOT, lru_tm / lru_pass);

  MONITOR_SET(MONITOR_FLUSH_ADAPTIVE_AVG_TIME_THREAD,
              list_tm / (srv_n_page_cleaners * flush_pass));
  MONITOR_SET(MONITOR_LRU_BATCH_FLUSH_AVG_TIME_THREAD,
              lru_tm / (srv_n_page_cleaners * flush_pass));
  MONITOR_SET(MONITOR_FLUSH_ADAPTIVE_AVG_TIME_EST,
              flush_tm * list_tm / flush_pass / (list_tm + lru_tm));
  MONITOR_SET(MONITOR_LRU_BATCH_FLUSH_AVG_TIME_EST,
              flush_tm * lru_tm / flush_pass / (list_tm + lru_tm));
  MONITOR_SET(MONITOR_FLUSH_AVG_TIME, flush_tm / flush_pass);

  MONITOR_SET(MONITOR_FLUSH_ADAPTIVE_AVG_PASS,
              list_pass / page_cleaner->n_slots);
  MONITOR_SET(MONITOR_LRU_BATCH_FLUSH_AVG_PASS,
              lru_pass / page_cleaner->n_slots);
  MONITOR_SET(MONITOR_FLUSH_AVG_PASS, flush_pass);

  prev_lsn = cur_iter_lsn;
  prev_time = cur_iter_time;

  n_iterations = 0;
  sum_pages = 0;
}

/** Calculates if flushing is required based on number of dirty pages in
 the buffer pool.
 @return percent of io_capacity to flush to manage dirty page ratio */
ulint get_pct_for_dirty() {
  double dirty_pct = buf_get_modified_ratio_pct();

  if (dirty_pct == 0.0) {
    /* No pages modified */
    return (0);
  }

  ut_a(srv_max_dirty_pages_pct_lwm <= srv_max_buf_pool_modified_pct);

  if (srv_max_dirty_pages_pct_lwm == 0) {
    /* The user has not set the option to preflush dirty
    pages as we approach the high water mark. */
    if (dirty_pct >= srv_max_buf_pool_modified_pct) {
      /* We have crossed the high water mark of dirty
      pages In this case we start flushing at 100% of
      innodb_io_capacity. */
      return (100);
    }
  } else if (dirty_pct >= srv_max_dirty_pages_pct_lwm) {
    /* We should start flushing pages gradually. */
    return (static_cast<ulint>((dirty_pct * 100) /
                               (srv_max_buf_pool_modified_pct + 1)));
  }

  return (0);
}

/** Calculates if flushing is required based on redo generation rate.
 @return percent of io_capacity to flush to manage redo space */
ulint get_pct_for_lsn(lsn_t age) /*!< in: current age of LSN. */
{
  ut_a(log_sys != nullptr);
  log_t &log = *log_sys;

  lsn_t limit_for_free_check;
  lsn_t limit_for_dirty_page_age;

  log_files_capacity_get_limits(log, limit_for_free_check,
                                limit_for_dirty_page_age);

  double lsn_age_factor;
  lsn_t af_lwm = (srv_adaptive_flushing_lwm * limit_for_free_check) / 100;

  if (age < af_lwm) {
    /* No adaptive flushing. */
    return (0);
  }

  if (age < limit_for_dirty_page_age && !srv_adaptive_flushing) {
    /* We have still not reached the max_async point and
    the user has disabled adaptive flushing. */
    return (0);
  }

  /* If we are here then we know that either:
  1) User has enabled adaptive flushing
  2) User may have disabled adaptive flushing but we have reached
  limit_for_dirty_page_age. */
  lsn_age_factor = (age * 100.0) / limit_for_dirty_page_age;

  ut_ad(srv_max_io_capacity >= srv_io_capacity);

  return (static_cast<ulint>(((srv_max_io_capacity / srv_io_capacity) *
                              (lsn_age_factor * sqrt(lsn_age_factor))) /
                             7.5));
}

/** Set page flush target based on LSN change and checkpoint age.
@param[in]  sync_flush            true iff this is sync flush mode
@param[in]  sync_flush_limit_lsn  low limit for oldest_modification
                                  if sync_flush is true
@return number of pages requested to flush */
ulint set_flush_target_by_lsn(bool sync_flush, lsn_t sync_flush_limit_lsn) {
  lsn_t oldest_lsn = buf_pool_get_oldest_modification_approx();
  ut_ad(oldest_lsn <= log_get_lsn(*log_sys));

  lsn_t age = cur_iter_lsn > oldest_lsn ? cur_iter_lsn - oldest_lsn : 0;

  ulint pct_for_dirty = get_pct_for_dirty();
  ulint pct_for_lsn = get_pct_for_lsn(age);
  ulint pct_total = std::max(pct_for_dirty, pct_for_lsn);

  /* Estimate pages to be flushed for the lsn progress */
  ulint sum_pages_for_lsn = 0;

  lsn_t target_lsn;
  uint scan_factor;

  if (sync_flush) {
    target_lsn = sync_flush_limit_lsn;
    ut_a(target_lsn < LSN_MAX);
    scan_factor = 1;
    buf_flush_sync_lsn = target_lsn;
  } else {
    target_lsn = oldest_lsn + lsn_avg_rate * buf_flush_lsn_scan_factor;
    scan_factor = buf_flush_lsn_scan_factor;
    buf_flush_sync_lsn = 0;
  }

  /* Cap the maximum IO capacity that we are going to use by
  max_io_capacity. Limit the value to avoid too quick increase */
  const ulint sum_pages_max = srv_max_io_capacity * 2;

  /* Limit individual BP scan based on overall capacity. */
  const ulint pages_for_lsn_max =
      (sum_pages_max / srv_buf_pool_instances) * scan_factor * 2;

  for (ulint i = 0; i < srv_buf_pool_instances; i++) {
    buf_pool_t *buf_pool = buf_pool_from_array(i);
    ulint pages_for_lsn = 0;

    buf_flush_list_mutex_enter(buf_pool);
    for (buf_page_t *b = UT_LIST_GET_LAST(buf_pool->flush_list); b != nullptr;
         b = UT_LIST_GET_PREV(list, b)) {
      if (b->get_oldest_lsn() > target_lsn) {
        break;
      }
      ++pages_for_lsn;
      if (pages_for_lsn >= pages_for_lsn_max) {
        break;
      }
    }
    buf_flush_list_mutex_exit(buf_pool);

    sum_pages_for_lsn += pages_for_lsn;

    mutex_enter(&page_cleaner->mutex);
    ut_ad(page_cleaner->slots[i].state == PAGE_CLEANER_STATE_NONE);
    page_cleaner->slots[i].n_pages_requested = pages_for_lsn / scan_factor + 1;
    mutex_exit(&page_cleaner->mutex);
  }

  sum_pages_for_lsn /= scan_factor;
  if (sum_pages_for_lsn < 1) {
    sum_pages_for_lsn = 1;
  }

  /* Cap the maximum IO capacity that we are going to use by
  max_io_capacity. Limit the value to avoid too quick increase */
  ulint pages_for_lsn = std::min<ulint>(sum_pages_for_lsn, sum_pages_max);

  /* Estimate based on LSN and dirty pages. */
  ulint n_pages;
  if (sync_flush) {
    n_pages = pages_for_lsn;
    /* For sync flush, make sure we flush at least at io capacity rate. This
    lower bound works as a safeguard against any miscalculation leading to
    too less flushing while we are in urgent flushing mode. Specifically, for
    small target, if the target is evaluated to zero the flush could be stuck
    in sync flush mode indefinitely, flushing nothing. */
    if (n_pages < srv_io_capacity) {
      n_pages = srv_io_capacity;
    }
  } else {
    n_pages = (PCT_IO(pct_total) + page_avg_rate + pages_for_lsn) / 3;
    if (n_pages > srv_max_io_capacity) {
      n_pages = srv_max_io_capacity;
    }
  }

  /* Normalize request for each instance */
  mutex_enter(&page_cleaner->mutex);
  ut_ad(page_cleaner->n_slots_requested == 0);
  ut_ad(page_cleaner->n_slots_flushing == 0);
  ut_ad(page_cleaner->n_slots_finished == 0);

  for (ulint i = 0; i < srv_buf_pool_instances; i++) {
    /* if REDO has enough of free space,
    don't care about age distribution of pages */
    page_cleaner->slots[i].n_pages_requested =
        pct_for_lsn > 30 ? page_cleaner->slots[i].n_pages_requested * n_pages /
                                   sum_pages_for_lsn +
                               1
                         : n_pages / srv_buf_pool_instances + 1;
  }
  mutex_exit(&page_cleaner->mutex);

  MONITOR_SET(MONITOR_FLUSH_N_TO_FLUSH_BY_AGE, sum_pages_for_lsn);
  MONITOR_SET(MONITOR_FLUSH_PCT_FOR_DIRTY, pct_for_dirty);
  MONITOR_SET(MONITOR_FLUSH_PCT_FOR_LSN, pct_for_lsn);

  return (n_pages);
}

/** Set page flush target based on dirty pages in buffer pool. Set only if
the target are is found to be higher than the target evaluated based on LSN.
@param[in]      n_pages_lsn     number of pages estimated and set based on LSN
@return page flush target. */
ulint set_flush_target_by_page(ulint n_pages_lsn) {
  ulint lru_len = 0;
  ulint free_len = 0;
  ulint flush_list_len = 0;

  buf_get_total_list_len(&lru_len, &free_len, &flush_list_len);

  cur_iter_pages_dirty = flush_list_len;

  cur_iter_dirty_pct = get_pct_for_dirty();
  MONITOR_SET(MONITOR_FLUSH_PCT_FOR_DIRTY, cur_iter_dirty_pct);

  /* Enable page based target only when redo logging is disabled. */
  if (mtr_t::s_logging.is_enabled()) {
    MONITOR_SET(MONITOR_FLUSH_N_TO_FLUSH_BY_DIRTY_PAGE, 0);
    return (n_pages_lsn);
  }

  /* No dirty pages to flush. */
  if (cur_iter_dirty_pct == 0) {
    MONITOR_SET(MONITOR_FLUSH_N_TO_FLUSH_BY_DIRTY_PAGE, 0);
    return (n_pages_lsn);
  }

  ut_ad(cur_iter_time >= prev_iter_time);

  auto delta_time_s = 1.0;
  if (cur_iter_time > prev_iter_time) {
    delta_time_s = std::chrono::duration_cast<std::chrono::duration<double>>(
                       cur_iter_time - prev_iter_time)
                       .count();
  }

  /* Number of pages flushed per second in last iteration. */
  double prev_page_rate_sec = prev_iter_pages_flushed / delta_time_s;

  auto delta_dirty_pages = static_cast<double>(cur_iter_pages_dirty) -
                           static_cast<double>(prev_iter_pages_dirty);

  /* Change in number of dirty pages per second. It could be negative. */
  double dirty_page_change_sec = delta_dirty_pages / delta_time_s;

  /* Next iteration we would like to adapt the flush rate based on changes in
  dirty page rate. */
  auto estimate = prev_page_rate_sec + dirty_page_change_sec;

  ulint n_pages = 0;

  if (estimate <= 0) {
    n_pages = 0;
  } else {
    n_pages = static_cast<ulint>(estimate);
  }

  /* We use radical function of current dirty page percentage to boost
  the flush rate when dirty page percentage goes higher. The boost factor
  monotonically increases from 0.10(1%) - 1.05 (100%) with a value 1 at 90%. */
  double boost_factor = sqrt(static_cast<double>(cur_iter_dirty_pct) / 90.0);

  n_pages = static_cast<ulint>(boost_factor * n_pages);

  /* We moderate the effect of spikes by including average page rate across
  multiple iterations. */
  n_pages = (page_avg_rate + n_pages) / 2;

  if (n_pages > srv_max_io_capacity) {
    n_pages = srv_max_io_capacity;
  }

  if (n_pages <= n_pages_lsn) {
    MONITOR_SET(MONITOR_FLUSH_N_TO_FLUSH_BY_DIRTY_PAGE, n_pages);
    return (n_pages_lsn);
  }

  /* Set new targets for each instance */
  mutex_enter(&page_cleaner->mutex);
  for (ulint i = 0; i < srv_buf_pool_instances; i++) {
    page_cleaner->slots[i].n_pages_requested = n_pages / srv_buf_pool_instances;
  }
  mutex_exit(&page_cleaner->mutex);

  MONITOR_SET(MONITOR_FLUSH_N_TO_FLUSH_BY_DIRTY_PAGE, n_pages);
  return (n_pages);
}

/** This function is called approximately once every second by the
page_cleaner thread, unless it is sync flushing mode, in which case
it is called every small round. Based on various factors it decides
if there is a need to do flushing.
@param  last_pages_in         the number of pages flushed by the last
                              flush_list flushing
@param  is_sync_flush         true iff this is sync flush mode
@param  sync_flush_limit_lsn  low limit for oldest_modification
                              if is_sync_flush is true
@return number of pages recommended to be flushed */
ulint page_recommendation(ulint last_pages_in, bool is_sync_flush,
                          lsn_t sync_flush_limit_lsn) {
  if (initialize(last_pages_in)) {
    /* First time around. */
    return (0);
  }

  /* No LSN based estimate if system LSN has not moved and not sync flush. */
  bool skip_lsn = (prev_lsn == cur_iter_lsn && !is_sync_flush);

  /* Compute and set average rates across multiple iterations,
  if limit is reached. */
  set_average();

  /* Set page flush target based on LSN. */
  auto n_pages =
      skip_lsn ? 0
               : set_flush_target_by_lsn(is_sync_flush, sync_flush_limit_lsn);

  /* Estimate based on only dirty pages. We don't want to flush at lesser rate
  as LSN based estimate may not represent the right picture for modifications
  without redo logging - temp tables, bulk load and global redo off. */
  n_pages = set_flush_target_by_page(n_pages);

  prev_iter_time = cur_iter_time;
  prev_iter_pages_dirty = cur_iter_pages_dirty;

  MONITOR_SET(MONITOR_FLUSH_N_TO_FLUSH_REQUESTED, n_pages);
  return (n_pages);
}
}  // namespace Adaptive_flush

/** Puts the page_cleaner thread to sleep if it has finished work in less
 than a second
 @retval 0 wake up by event set,
 @retval OS_SYNC_TIME_EXCEEDED if timeout was exceeded
 @param next_loop_time  time when next loop iteration should start
 @param sig_count       zero or the value returned by previous call of
                         os_event_reset() */
static ulint pc_sleep_if_needed(
    std::chrono::steady_clock::time_point next_loop_time, int64_t sig_count) {
  const auto cur_time = std::chrono::steady_clock::now();

  if (next_loop_time > cur_time) {
    auto sleep_time = next_loop_time - cur_time;
    if (sleep_time > std::chrono::seconds{1}) {
      sleep_time = std::chrono::seconds{1};
    }

    ut_a(sleep_time.count() > 0);

    return (os_event_wait_time_low(
        buf_flush_event,
        std::chrono::duration_cast<std::chrono::microseconds>(sleep_time),
        sig_count));
  }

  return (OS_SYNC_TIME_EXCEEDED);
}

/** Checks if page cleaners are active. */
bool buf_flush_page_cleaner_is_active() {
  return (srv_thread_is_active(srv_threads.m_page_cleaner_coordinator));
}

void buf_flush_page_cleaner_init() {
  ut_ad(page_cleaner == nullptr);

  page_cleaner = ut::make_unique<page_cleaner_t>(UT_NEW_THIS_FILE_PSI_KEY);

  mutex_create(LATCH_ID_PAGE_CLEANER, &page_cleaner->mutex);

  page_cleaner->is_requested = os_event_create();
  page_cleaner->is_finished = os_event_create();

  page_cleaner->n_slots = static_cast<ulint>(srv_buf_pool_instances);

  page_cleaner->slots = ut::make_unique<page_cleaner_slot_t[]>(
      UT_NEW_THIS_FILE_PSI_KEY, page_cleaner->n_slots);

  ut_d(page_cleaner->n_disabled_debug = 0);

  page_cleaner->is_running = true;

  srv_threads.m_page_cleaner_coordinator = os_thread_create(
      page_flush_coordinator_thread_key, 0, buf_flush_page_coordinator_thread);

  srv_threads.m_page_cleaner_workers[0] =
      srv_threads.m_page_cleaner_coordinator;

  srv_threads.m_page_cleaner_coordinator.start();

  /* Make sure page cleaner is active. */
  ut_a(buf_flush_page_cleaner_is_active());
}

/**
Close page_cleaner. */
static void buf_flush_page_cleaner_close(void) {
  /* Waiting for all worker threads to exit, note that worker 0 is actually
  the page cleaner coordinator itself which is calling the function which
  we are inside. */
  for (size_t i = 1; i < srv_threads.m_page_cleaner_workers_n; ++i) {
    srv_threads.m_page_cleaner_workers[i].wait();
  }

  mutex_destroy(&page_cleaner->mutex);

  os_event_destroy(page_cleaner->is_finished);
  os_event_destroy(page_cleaner->is_requested);

  page_cleaner.reset();
}

/**
Requests for all slots to flush all buffer pool instances.
@param min_n    wished minimum number of blocks flushed
                (it is not guaranteed that the actual number is that big)
@param lsn_limit in the case BUF_FLUSH_LIST all blocks whose
                oldest_modification is smaller than this should be flushed
                (if their number does not exceed min_n), otherwise ignored
*/
static void pc_request(ulint min_n, lsn_t lsn_limit) {
  if (min_n != ULINT_MAX) {
    /* Ensure that flushing is spread evenly amongst the
    buffer pool instances. When min_n is ULINT_MAX
    we need to flush everything up to the lsn limit
    so no limit here. */
    min_n = (min_n + srv_buf_pool_instances - 1) / srv_buf_pool_instances;
  }

  mutex_enter(&page_cleaner->mutex);

  ut_ad(page_cleaner->n_slots_requested == 0);
  ut_ad(page_cleaner->n_slots_flushing == 0);
  ut_ad(page_cleaner->n_slots_finished == 0);

  page_cleaner->requested = (min_n > 0);
  page_cleaner->lsn_limit = lsn_limit;

  for (ulint i = 0; i < page_cleaner->n_slots; i++) {
    page_cleaner_slot_t *slot = &page_cleaner->slots[i];

    ut_ad(slot->state == PAGE_CLEANER_STATE_NONE);

    if (min_n == ULINT_MAX) {
      slot->n_pages_requested = ULINT_MAX;
    } else if (min_n == 0) {
      slot->n_pages_requested = 0;
    }

    /* slot->n_pages_requested was already set by
    Adaptive_flush::page_recommendation() */

    slot->state = PAGE_CLEANER_STATE_REQUESTED;
  }

  page_cleaner->n_slots_requested = page_cleaner->n_slots;
  page_cleaner->n_slots_flushing = 0;
  page_cleaner->n_slots_finished = 0;

  os_event_set(page_cleaner->is_requested);

  mutex_exit(&page_cleaner->mutex);
}

/**
Do flush for one slot.
@return the number of the slots which has not been treated yet. */
static ulint pc_flush_slot(void) {
  std::chrono::steady_clock::duration lru_time;
  std::chrono::steady_clock::duration flush_list_time{};
  int lru_pass = 0;
  int list_pass = 0;

  mutex_enter(&page_cleaner->mutex);

  if (page_cleaner->n_slots_requested > 0) {
    page_cleaner_slot_t *slot = nullptr;
    ulint i;

    for (i = 0; i < page_cleaner->n_slots; i++) {
      slot = &page_cleaner->slots[i];

      if (slot->state == PAGE_CLEANER_STATE_REQUESTED) {
        break;
      }
    }

    /* slot should be found because
    page_cleaner->n_slots_requested > 0 */
    ut_a(i < page_cleaner->n_slots);

    buf_pool_t *buf_pool = buf_pool_from_array(i);

    page_cleaner->n_slots_requested--;
    page_cleaner->n_slots_flushing++;
    slot->state = PAGE_CLEANER_STATE_FLUSHING;

    if (page_cleaner->n_slots_requested == 0) {
      os_event_reset(page_cleaner->is_requested);
    }

    if (!page_cleaner->is_running) {
      slot->n_flushed_lru = 0;
      slot->n_flushed_list = 0;
    } else {
      mutex_exit(&page_cleaner->mutex);

      const auto lru_start = std::chrono::steady_clock::now();

      /* Flush pages from end of LRU if required */
      slot->n_flushed_lru = buf_flush_LRU_list(buf_pool);

      lru_time = std::chrono::steady_clock::now() - lru_start;
      lru_pass = 1;

      if (!page_cleaner->is_running) {
        slot->n_flushed_list = 0;
      } else {
        /* Flush pages from flush_list if required */
        if (page_cleaner->requested) {
          const auto flush_list_start = std::chrono::steady_clock::now();

          slot->succeeded_list = buf_flush_do_batch(
              buf_pool, BUF_FLUSH_LIST, slot->n_pages_requested,
              page_cleaner->lsn_limit, &slot->n_flushed_list);

          flush_list_time = std::chrono::steady_clock::now() - flush_list_start;
          list_pass = 1;
        } else {
          slot->n_flushed_list = 0;
          slot->succeeded_list = true;
        }
      }
      mutex_enter(&page_cleaner->mutex);
    }
    page_cleaner->n_slots_flushing--;
    page_cleaner->n_slots_finished++;
    slot->state = PAGE_CLEANER_STATE_FINISHED;

    slot->flush_lru_time +=
        std::chrono::duration_cast<std::chrono::milliseconds>(lru_time);
    slot->flush_list_time +=
        std::chrono::duration_cast<std::chrono::milliseconds>(flush_list_time);
    slot->flush_lru_pass += lru_pass;
    slot->flush_list_pass += list_pass;

    if (page_cleaner->n_slots_requested == 0 &&
        page_cleaner->n_slots_flushing == 0) {
      os_event_set(page_cleaner->is_finished);
    }
  }

  ulint ret = page_cleaner->n_slots_requested;

  mutex_exit(&page_cleaner->mutex);

  return (ret);
}

/**
Wait until all flush requests are finished.
@param n_flushed_lru    number of pages flushed from the end of the LRU list.
@param n_flushed_list   number of pages flushed from the end of the
                        flush_list.
@return                 true if all flush_list flushing batch were success. */
static bool pc_wait_finished(ulint *n_flushed_lru, ulint *n_flushed_list) {
  bool all_succeeded = true;

  *n_flushed_lru = 0;
  *n_flushed_list = 0;

  os_event_wait(page_cleaner->is_finished);

  mutex_enter(&page_cleaner->mutex);

  ut_ad(page_cleaner->n_slots_requested == 0);
  ut_ad(page_cleaner->n_slots_flushing == 0);
  ut_ad(page_cleaner->n_slots_finished == page_cleaner->n_slots);

  for (ulint i = 0; i < page_cleaner->n_slots; i++) {
    page_cleaner_slot_t *slot = &page_cleaner->slots[i];

    ut_ad(slot->state == PAGE_CLEANER_STATE_FINISHED);

    *n_flushed_lru += slot->n_flushed_lru;
    *n_flushed_list += slot->n_flushed_list;
    all_succeeded &= slot->succeeded_list;

    slot->state = PAGE_CLEANER_STATE_NONE;

    slot->n_pages_requested = 0;
  }

  page_cleaner->n_slots_finished = 0;

  os_event_reset(page_cleaner->is_finished);

  mutex_exit(&page_cleaner->mutex);

  os_event_set(buf_flush_tick_event);

  return (all_succeeded);
}

#ifdef UNIV_LINUX
/**
Set priority for page_cleaner threads.
@param[in]      priority        priority intended to set
@return true if set as intended */
static bool buf_flush_page_cleaner_set_priority(int priority) {
  setpriority(PRIO_PROCESS, (pid_t)syscall(SYS_gettid), priority);
  return (getpriority(PRIO_PROCESS, (pid_t)syscall(SYS_gettid)) == priority);
}
#endif /* UNIV_LINUX */

#ifdef UNIV_DEBUG
/** Loop used to disable page cleaner threads. */
static void buf_flush_page_cleaner_disabled_loop(void) {
  ut_ad(page_cleaner != nullptr);

  if (!innodb_page_cleaner_disabled_debug) {
    /* We return to avoid entering and exiting mutex. */
    return;
  }

  mutex_enter(&page_cleaner->mutex);
  page_cleaner->n_disabled_debug++;
  mutex_exit(&page_cleaner->mutex);

  while (innodb_page_cleaner_disabled_debug &&
         srv_shutdown_state.load() < SRV_SHUTDOWN_CLEANUP &&
         page_cleaner->is_running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); /* [A] */
  }

  /* We need to wait for threads exiting here, otherwise we would
  encounter problem when we quickly perform following steps:
          1) SET GLOBAL innodb_page_cleaner_disabled_debug = 1;
          2) SET GLOBAL innodb_page_cleaner_disabled_debug = 0;
          3) SET GLOBAL innodb_page_cleaner_disabled_debug = 1;
  That's because after step 1 this thread could still be sleeping
  inside the loop above at [A] and steps 2, 3 could happen before
  this thread wakes up from [A]. In such case this thread would
  not re-increment n_disabled_debug and we would be waiting for
  him forever in buf_flush_page_cleaner_disabled_debug_update(...).

  Therefore we are waiting in step 2 for this thread exiting here. */

  mutex_enter(&page_cleaner->mutex);
  page_cleaner->n_disabled_debug--;
  mutex_exit(&page_cleaner->mutex);
}

void buf_flush_page_cleaner_disabled_debug_update(THD *, SYS_VAR *, void *,
                                                  const void *save) {
  if (page_cleaner == nullptr) {
    return;
  }

  if (!*static_cast<const bool *>(save)) {
    if (!innodb_page_cleaner_disabled_debug) {
      return;
    }

    innodb_page_cleaner_disabled_debug = false;

    /* Enable page cleaner threads. */
    while (srv_shutdown_state.load() < SRV_SHUTDOWN_CLEANUP) {
      mutex_enter(&page_cleaner->mutex);
      const ulint n = page_cleaner->n_disabled_debug;
      mutex_exit(&page_cleaner->mutex);
      /* Check if all threads have been enabled, to avoid
      problem when we decide to re-disable them soon. */
      if (n == 0) {
        break;
      }
    }
    return;
  }

  if (innodb_page_cleaner_disabled_debug) {
    return;
  }

  innodb_page_cleaner_disabled_debug = true;

  while (srv_shutdown_state.load() < SRV_SHUTDOWN_CLEANUP) {
    /* Workers are possibly sleeping on is_requested.

    We have to wake them, otherwise they could possibly
    have never noticed, that they should be disabled,
    and we would wait for them here forever.

    That's why we have sleep-loop instead of simply
    waiting on some disabled_debug_event. */
    os_event_set(page_cleaner->is_requested);

    mutex_enter(&page_cleaner->mutex);

    ut_ad(page_cleaner->n_disabled_debug <= srv_n_page_cleaners);

    if (page_cleaner->n_disabled_debug == srv_n_page_cleaners) {
      mutex_exit(&page_cleaner->mutex);
      break;
    }

    mutex_exit(&page_cleaner->mutex);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}
#endif /* UNIV_DEBUG */

/** Thread tasked with flushing dirty pages from the buffer pools.
As of now we'll have only one coordinator.
@param[in]      n_page_cleaners Number of page cleaner threads to create */
static void buf_flush_page_coordinator_thread() {
  auto loop_start_time = std::chrono::steady_clock::now();
  ulint n_flushed = 0;
  ulint last_activity = srv_get_activity_count();
  ulint last_pages = 0;

  THD *thd = create_internal_thd();

#ifdef UNIV_LINUX
  /* linux might be able to set different setting for each thread.
  worth to try to set high priority for page cleaner threads */
  if (buf_flush_page_cleaner_set_priority(buf_flush_page_cleaner_priority)) {
    ib::info(ER_IB_MSG_126) << "page_cleaner coordinator priority: "
                            << buf_flush_page_cleaner_priority;
  } else {
    ib::info(ER_IB_MSG_127) << "If the mysqld execution user is authorized,"
                               " page cleaner thread priority can be changed."
                               " See the man page of setpriority().";
  }
#endif /* UNIV_LINUX */

  /* We start from 1 because the coordinator thread is part of the
  same set */
  for (size_t i = 1; i < srv_threads.m_page_cleaner_workers_n; ++i) {
    srv_threads.m_page_cleaner_workers[i] = os_thread_create(
        page_flush_thread_key, i, buf_flush_page_cleaner_thread);

    srv_threads.m_page_cleaner_workers[i].start();
  }

  while (!srv_read_only_mode &&
         srv_shutdown_state.load() < SRV_SHUTDOWN_CLEANUP &&
         recv_sys->spaces != nullptr) {
    /* treat flushing requests during recovery. */
    ulint n_flushed_lru = 0;
    ulint n_flushed_list = 0;

    os_event_wait(recv_sys->flush_start);

    if (srv_shutdown_state.load() >= SRV_SHUTDOWN_CLEANUP ||
        recv_sys->spaces == nullptr) {
      break;
    }

    switch (recv_sys->flush_type) {
      case BUF_FLUSH_LRU:
        /* Flush pages from end of LRU if required */
        pc_request(0, LSN_MAX);
        while (pc_flush_slot() > 0) {
        }
        pc_wait_finished(&n_flushed_lru, &n_flushed_list);
        break;

      case BUF_FLUSH_LIST:
        /* Flush all pages */
        do {
          pc_request(ULINT_MAX, LSN_MAX);
          while (pc_flush_slot() > 0) {
          }
        } while (!pc_wait_finished(&n_flushed_lru, &n_flushed_list));
        break;

      default:
        ut_d(ut_error);
    }

    os_event_reset(recv_sys->flush_start);
    os_event_set(recv_sys->flush_end);
  }

  os_event_wait(buf_flush_event);

  ulint ret_sleep = 0;
  ulint n_evicted = 0;
  ulint n_flushed_last = 0;
  ulint warn_interval = 1;
  ulint warn_count = 0;
  bool is_sync_flush = false;
  bool was_server_active = true;
  int64_t sig_count = os_event_reset(buf_flush_event);

  while (srv_shutdown_state.load() < SRV_SHUTDOWN_CLEANUP) {
    /* We consider server active if either we have just discovered a first
    activity after a period of inactive server, or we are after the period
    of active server in which case, it could be just the beginning of the
    next period, so there is no reason to consider it idle yet.
    The withdrawing blocks process when shrinking the buffer pool always
    needs the page_cleaner activity. So, we consider server is active
    during the withdrawing blocks process also. */

    bool is_withdrawing = false;
    for (ulint i = 0; i < srv_buf_pool_instances; i++) {
      buf_pool_t *buf_pool = buf_pool_from_array(i);
      if (buf_get_withdraw_depth(buf_pool) > 0) {
        is_withdrawing = true;
        break;
      }
    }

    const bool is_server_active = is_withdrawing || was_server_active ||
                                  srv_check_activity(last_activity);

    /* The page_cleaner skips sleep if the server is
    idle and there are no pending IOs in the buffer pool
    and there is work to do. */
    if ((is_server_active || buf_get_n_pending_read_ios() || n_flushed == 0) &&
        !is_sync_flush) {
      ret_sleep = pc_sleep_if_needed(loop_start_time + std::chrono::seconds{1},
                                     sig_count);

      if (srv_shutdown_state.load() >= SRV_SHUTDOWN_CLEANUP) {
        break;
      }
    } else if (std::chrono::steady_clock::now() >
               loop_start_time + std::chrono::seconds{1}) {
      ret_sleep = OS_SYNC_TIME_EXCEEDED;
    } else {
      ret_sleep = 0;
    }

    sig_count = os_event_reset(buf_flush_event);

    if (ret_sleep == OS_SYNC_TIME_EXCEEDED) {
      const auto curr_time = std::chrono::steady_clock::now();

      if (curr_time > loop_start_time + std::chrono::seconds{4}) {
        if (warn_count == 0) {
          auto diff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
              curr_time - loop_start_time);

          ib::info(ER_IB_MSG_128)
              << "Page cleaner took " << diff_ms.count() << "ms to flush "
              << n_flushed_last << " and evict " << n_evicted << " pages";

          if (warn_interval > 300) {
            warn_interval = 600;
          } else {
            warn_interval *= 2;
          }

          warn_count = warn_interval;
        } else {
          --warn_count;
        }
      } else {
        /* reset counter */
        warn_interval = 1;
        warn_count = 0;
      }

      loop_start_time = curr_time;
      n_flushed_last = n_evicted = 0;

      was_server_active = srv_check_activity(last_activity);
      last_activity = srv_get_activity_count();
    }

    lsn_t lsn_limit;
    if (srv_flush_sync && !srv_read_only_mode) {
      /* lsn_limit!=0 means there are requests. needs to check the lsn. */
      lsn_limit = log_sync_flush_lsn(*log_sys);
      if (lsn_limit != 0) {
        /* Avoid aggressive sync flush beyond limit when redo is disabled. */
        if (mtr_t::s_logging.is_enabled()) {
          lsn_limit += Adaptive_flush::lsn_avg_rate * buf_flush_lsn_scan_factor;
        }
        is_sync_flush = true;
      } else {
        /* Stop the sync flush. */
        is_sync_flush = false;
      }
    } else {
      is_sync_flush = false;
      lsn_limit = LSN_MAX;
    }

    if (!srv_read_only_mode && mtr_t::s_logging.is_enabled() &&
        ret_sleep == OS_SYNC_TIME_EXCEEDED) {
      /* For smooth page flushing along with WAL,
      flushes log as much as possible. */
      log_sys->recent_written.advance_tail();
      auto wait_stats = log_write_up_to(
          *log_sys, log_buffer_ready_for_write_lsn(*log_sys), true);
      MONITOR_INC_WAIT_STATS_EX(MONITOR_ON_LOG_, _PAGE_WRITTEN, wait_stats);
    }

    if (is_sync_flush || is_server_active) {
      ulint n_to_flush;

      /* Estimate pages from flush_list to be flushed */
      if (is_sync_flush) {
        ut_a(lsn_limit > 0);
        ut_a(lsn_limit < LSN_MAX);
        n_to_flush =
            Adaptive_flush::page_recommendation(last_pages, true, lsn_limit);
        last_pages = 0;
        /* Flush n_to_flush pages or stop if you reach lsn_limit earlier.
        This is because in sync-flush mode we want finer granularity of
        flushes through all BP instances. */
      } else if (ret_sleep == OS_SYNC_TIME_EXCEEDED) {
        n_to_flush =
            Adaptive_flush::page_recommendation(last_pages, false, LSN_MAX);
        lsn_limit = LSN_MAX;
        last_pages = 0;
      } else {
        n_to_flush = 0;
        lsn_limit = 0;
      }

      /* Request flushing for threads */
      pc_request(n_to_flush, lsn_limit);

      const auto flush_start = std::chrono::steady_clock::now();

      /* Coordinator also treats requests */
      while (pc_flush_slot() > 0) {
        /* No op */
      }

      /* only coordinator is using these counters,
      so no need to protect by lock. */
      page_cleaner->flush_time +=
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - flush_start);
      page_cleaner->flush_pass++;

      /* Wait for all slots to be finished */
      ulint n_flushed_lru = 0;
      ulint n_flushed_list = 0;

      pc_wait_finished(&n_flushed_lru, &n_flushed_list);

      if (n_flushed_list > 0 || n_flushed_lru > 0) {
        buf_flush_stats(n_flushed_list, n_flushed_lru);
      }

      if (n_to_flush != 0) {
        last_pages = n_flushed_list;
      }

      n_evicted += n_flushed_lru;
      n_flushed_last += n_flushed_list;

      n_flushed = n_flushed_lru + n_flushed_list;

      if (is_sync_flush) {
        MONITOR_INC_VALUE_CUMULATIVE(
            MONITOR_FLUSH_SYNC_TOTAL_PAGE, MONITOR_FLUSH_SYNC_COUNT,
            MONITOR_FLUSH_SYNC_PAGES, n_flushed_lru + n_flushed_list);
      } else {
        if (n_flushed_lru) {
          MONITOR_INC_VALUE_CUMULATIVE(
              MONITOR_LRU_BATCH_FLUSH_TOTAL_PAGE, MONITOR_LRU_BATCH_FLUSH_COUNT,
              MONITOR_LRU_BATCH_FLUSH_PAGES, n_flushed_lru);
        }
        if (n_flushed_list) {
          MONITOR_INC_VALUE_CUMULATIVE(
              MONITOR_FLUSH_ADAPTIVE_TOTAL_PAGE, MONITOR_FLUSH_ADAPTIVE_COUNT,
              MONITOR_FLUSH_ADAPTIVE_PAGES, n_flushed_list);
        }
      }

    } else if (ret_sleep == OS_SYNC_TIME_EXCEEDED && srv_idle_flush_pct) {
      /* no activity, slept enough */
      buf_flush_lists(PCT_IO(srv_idle_flush_pct), LSN_MAX, &n_flushed);

      n_flushed_last += n_flushed;

      if (n_flushed) {
        MONITOR_INC_VALUE_CUMULATIVE(MONITOR_FLUSH_BACKGROUND_TOTAL_PAGE,
                                     MONITOR_FLUSH_BACKGROUND_COUNT,
                                     MONITOR_FLUSH_BACKGROUND_PAGES, n_flushed);
      }

    } else {
      /* no activity, but woken up by event */
      n_flushed = 0;
    }

    ut_d(buf_flush_page_cleaner_disabled_loop());
  }

  /* This is just for test scenarios. */
  srv_thread_delay_cleanup_if_needed(thd);

  ut_ad(srv_shutdown_state.load() >= SRV_SHUTDOWN_CLEANUP);

  if (srv_fast_shutdown == 2 ||
      srv_shutdown_state.load() == SRV_SHUTDOWN_EXIT_THREADS) {
    /* In very fast shutdown or when innodb failed to start, we
    simulate a crash of the buffer pool. We are not required to do
    any flushing. */
    goto thread_exit;
  }

  /* In case of normal and slow shutdown the page_cleaner thread
  must wait for all other activity in the server to die down.
  Note that we can start flushing the buffer pool as soon as the
  server enters shutdown phase but we must stay alive long enough
  to ensure that any work done by the master or purge threads is
  also flushed.
  During shutdown we pass through three stages. In the first stage,
  when SRV_SHUTDOWN_CLEANUP is set other threads like the master
  and the purge threads may be working as well. We start flushing
  the buffer pool but can't be sure that no new pages are being
  dirtied until we enter SRV_SHUTDOWN_FLUSH_PHASE phase which is
  the last phase (meanwhile we visit SRV_SHUTDOWN_MASTER_STOP).

  Note, that if we are handling fatal error, we set the state
  directly to EXIT_THREADS in which case we also might exit the loop
  below, but still some new dirty pages could be arriving...
  In such case we just want to stop and don't care about the new pages.
  However we need to be careful not to crash (e.g. in assertions). */

  do {
    pc_request(ULINT_MAX, LSN_MAX);

    while (pc_flush_slot() > 0) {
    }

    ulint n_flushed_lru = 0;
    ulint n_flushed_list = 0;
    pc_wait_finished(&n_flushed_lru, &n_flushed_list);

    n_flushed = n_flushed_lru + n_flushed_list;

    /* We sleep only if there are no pages to flush */
    if (n_flushed == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  } while (srv_shutdown_state.load() < SRV_SHUTDOWN_FLUSH_PHASE);

  /* At this point all threads including the master and the purge
  thread must have been closed, unless we are handling some error
  during initialization of InnoDB (srv_init_abort). In such case
  we could have SRV_SHUTDOWN_EXIT_THREADS set directly from the
  srv_shutdown_exit_threads(). */
  if (srv_shutdown_state.load() != SRV_SHUTDOWN_EXIT_THREADS) {
    /* We could have srv_shutdown_state.load() >= FLUSH_PHASE only
    when either: shutdown started or init is being aborted. In the
    first case we would have FLUSH_PHASE and keep waiting until
    this thread is alive before we switch to LAST_PHASE.

    In the second case, we would jump to EXIT_THREADS from NONE,
    so we would not enter here. */
    ut_a(!srv_is_being_started);
    ut_a(srv_shutdown_state.load() == SRV_SHUTDOWN_FLUSH_PHASE);

    ut_a(!srv_master_thread_is_active());
    if (!srv_read_only_mode) {
      ut_a(!srv_purge_threads_active());
      ut_a(!srv_thread_is_active(srv_threads.m_dict_stats));
      ut_a(!srv_thread_is_active(srv_threads.m_ts_alter_encrypt));
    }
  }

  /* We can now make a final sweep on flushing the buffer pool
  and exit after we have cleaned the whole buffer pool.
  It is important that we wait for any running batch that has
  been triggered by us to finish. Otherwise we can end up
  considering end of that batch as a finish of our final
  sweep and we'll come out of the loop leaving behind dirty pages
  in the flush_list */
  buf_flush_wait_batch_end(nullptr, BUF_FLUSH_LIST);
  buf_flush_wait_LRU_batch_end();

  bool success;
  bool are_any_read_ios_still_underway;

  do {
    /* If there are any read operations pending, they can result in the ibuf
    merges and a dirtying page after the read is completed. If there are any
    IO reads running before we run the flush loop, we risk having some dirty
    pages after flushing reports n_flushed == 0. The ibuf change merging on
    page results in dirtying the page and is followed by decreasing the
    n_pend_reads counter, thus it's safe to check it before flush loop and
    have guarantees if it was seen with value of 0. These reads could be issued
    in the previous stage(s), the srv_master thread on shutdown tasks clear the
    ibuf unless it's the fast shutdown. */
    are_any_read_ios_still_underway = buf_get_n_pending_read_ios() > 0;
    pc_request(ULINT_MAX, LSN_MAX);

    while (pc_flush_slot() > 0) {
    }

    ulint n_flushed_lru = 0;
    ulint n_flushed_list = 0;
    success = pc_wait_finished(&n_flushed_lru, &n_flushed_list);

    n_flushed = n_flushed_lru + n_flushed_list;

    buf_flush_wait_batch_end(nullptr, BUF_FLUSH_LIST);
    buf_flush_wait_LRU_batch_end();

  } while (!success || n_flushed > 0 || are_any_read_ios_still_underway);

  for (ulint i = 0; i < srv_buf_pool_instances; i++) {
    buf_pool_t *buf_pool = buf_pool_from_array(i);
    ut_a(UT_LIST_GET_LEN(buf_pool->flush_list) == 0);
  }

  /* Mark that it is safe to recover as we have already flushed all dirty
  pages in buffer pools. */
  if (mtr_t::s_logging.is_disabled() && !srv_read_only_mode) {
    log_persist_crash_safe(*log_sys);
  }
  log_crash_safe_validate(*log_sys);

  /* We have lived our life. Time to die. */

thread_exit:
  /* All worker threads are waiting for the event here,
  and no more access to page_cleaner structure by them.
  Wakes worker threads up just to make them exit. */
  page_cleaner->is_running = false;
  os_event_set(page_cleaner->is_requested);

  buf_flush_page_cleaner_close();

  destroy_internal_thd(thd);
}

/** Worker thread of page_cleaner. */
static void buf_flush_page_cleaner_thread() {
#ifdef UNIV_LINUX
  /* linux might be able to set different setting for each thread
  worth to try to set high priority for page cleaner threads */
  if (buf_flush_page_cleaner_set_priority(buf_flush_page_cleaner_priority)) {
    ib::info(ER_IB_MSG_129)
        << "page_cleaner worker priority: " << buf_flush_page_cleaner_priority;
  }
#endif /* UNIV_LINUX */

  for (;;) {
    os_event_wait(page_cleaner->is_requested);

    ut_d(buf_flush_page_cleaner_disabled_loop());

    if (!page_cleaner->is_running) {
      break;
    }

    pc_flush_slot();
  }
}

void buf_flush_fsync() {
#ifdef _WIN32
  switch (srv_win_file_flush_method) {
    case SRV_WIN_IO_UNBUFFERED:
      break;
    case SRV_WIN_IO_NORMAL:
      fil_flush_file_spaces();
      break;
  }
#else  /* !_WIN32 */
  switch (srv_unix_file_flush_method) {
    case SRV_UNIX_NOSYNC:
      break;
    case SRV_UNIX_O_DSYNC:
      /* O_SYNC is respected only for redo files and we need to
      flush data files here. For details look inside os0file.cc. */
    case SRV_UNIX_FSYNC:
    case SRV_UNIX_LITTLESYNC:
    case SRV_UNIX_O_DIRECT:
    case SRV_UNIX_O_DIRECT_NO_FSYNC:
      fil_flush_file_spaces();
  }
#endif /* _WIN32 */
}

/** Synchronously flush dirty blocks from the end of the flush list of all
 buffer pool instances. NOTE: The calling thread is not allowed to own any
 latches on pages! */
void buf_flush_sync_all_buf_pools() {
  bool success;
  ulint n_pages;
  do {
    n_pages = 0;
    success = buf_flush_lists(ULINT_MAX, LSN_MAX, &n_pages);
    buf_flush_wait_batch_end(nullptr, BUF_FLUSH_LIST);

    if (!success) {
      MONITOR_INC(MONITOR_FLUSH_SYNC_WAITS);
    }

    MONITOR_INC_VALUE_CUMULATIVE(MONITOR_FLUSH_SYNC_TOTAL_PAGE,
                                 MONITOR_FLUSH_SYNC_COUNT,
                                 MONITOR_FLUSH_SYNC_PAGES, n_pages);
  } while (!success);

  ut_a(success);

  /* All pages have been written to disk, but we need to make fsync for files
  to which the writes have been made. */
  buf_flush_fsync();
}

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG

/** Functor to validate the flush list. */
struct Check {
  void operator()(const buf_page_t *elem) { ut_a(elem->in_flush_list); }
};

static bool buf_flush_validate_low(const buf_pool_t *buf_pool) {
  buf_page_t *bpage;
  const ib_rbt_node_t *rnode = nullptr;
  Check check;

  ut_ad(buf_flush_list_mutex_own(buf_pool));

  ut_list_validate(buf_pool->flush_list, check);

  bpage = UT_LIST_GET_FIRST(buf_pool->flush_list);

  /* If we are in recovery mode i.e.: flush_rbt != NULL
  then each block in the flush_list must also be present
  in the flush_rbt. */
  if (buf_pool->flush_rbt != nullptr) {
    rnode = rbt_first(buf_pool->flush_rbt);
  }

  while (bpage != nullptr) {
    const lsn_t om = bpage->get_oldest_lsn();

    ut_ad(buf_pool_from_bpage(bpage) == buf_pool);

    ut_ad(bpage->in_flush_list);

    /* A page in buf_pool->flush_list can be in
    BUF_BLOCK_REMOVE_HASH state. This happens when a page
    is in the middle of being relocated. In that case the
    original descriptor can have this state and still be
    in the flush list waiting to acquire the
    buf_pool->flush_list_mutex to complete the relocation. */
    ut_a(buf_page_in_file(bpage) ||
         buf_page_get_state(bpage) == BUF_BLOCK_REMOVE_HASH);
    ut_a(om > 0);

    if (buf_pool->flush_rbt != nullptr) {
      buf_page_t **prpage;

      ut_a(rnode != nullptr);
      prpage = rbt_value(buf_page_t *, rnode);

      ut_a(*prpage != nullptr);
      ut_a(*prpage == bpage);
      rnode = rbt_next(buf_pool->flush_rbt, rnode);
    }

    bpage = UT_LIST_GET_NEXT(list, bpage);

    ut_a(bpage == nullptr ||
         buf_flush_list_order_validate(bpage->get_oldest_lsn(), om));
  }

  /* By this time we must have exhausted the traversal of
  flush_rbt (if active) as well. */
  ut_a(rnode == nullptr);

  return true;
}

bool buf_flush_validate(buf_pool_t *buf_pool) {
  buf_flush_list_mutex_enter(buf_pool);

  auto ret = buf_flush_validate_low(buf_pool);

  buf_flush_list_mutex_exit(buf_pool);

  return (ret);
}
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

/** Check if there are any dirty pages that belong to a space id in the flush
 list in a particular buffer pool.
 @return number of dirty pages present in a single buffer pool */
ulint buf_pool_get_dirty_pages_count(
    buf_pool_t *buf_pool,     /*!< in: buffer pool */
    space_id_t id,            /*!< in: space id to check */
    Flush_observer *observer) /*!< in: flush observer to check */

{
  ulint count = 0;

  buf_flush_list_mutex_enter(buf_pool);

  for (auto bpage : buf_pool->flush_list) {
    ut_ad(buf_page_in_file(bpage) ||
          buf_page_get_state(bpage) == BUF_BLOCK_REMOVE_HASH);
    ut_ad(bpage->in_flush_list);
    ut_ad(bpage->is_dirty());

    if ((observer != nullptr && observer == bpage->get_flush_observer()) ||
        (observer == nullptr && id == bpage->id.space())) {
      ++count;
    }
  }

  buf_flush_list_mutex_exit(buf_pool);

  return (count);
}

/** Check if there are any dirty pages that belong to a space id in the flush
 list.
 @return number of dirty pages present in all the buffer pools */
static ulint buf_flush_get_dirty_pages_count(
    space_id_t id,            /*!< in: space id to check */
    Flush_observer *observer) /*!< in: flush observer to check */
{
  ulint count = 0;

  for (ulint i = 0; i < srv_buf_pool_instances; ++i) {
    buf_pool_t *buf_pool;

    buf_pool = buf_pool_from_array(i);

    count += buf_pool_get_dirty_pages_count(buf_pool, id, observer);
  }

  return (count);
}

Flush_observer::Flush_observer(space_id_t space_id, trx_t *trx,
                               Alter_stage *stage) noexcept
    : m_space_id(space_id),
      m_trx(trx),
      m_stage(stage),
      m_flushed(srv_buf_pool_instances),
      m_removed(srv_buf_pool_instances) {
#ifdef FLUSH_LIST_OBSERVER_DEBUG
  ib::info(ER_IB_MSG_130) << "Flush_observer : ID= " << m_id
                          << ", space_id=" << space_id << ", trx_id="
                          << (m_trx == nullptr ? TRX_ID_MAX : trx->id);
#endif /* FLUSH_LIST_OBSERVER_DEBUG */
}

Flush_observer::~Flush_observer() noexcept {
  ut_a(m_n_ref_count.fetch_add(0, std::memory_order_relaxed) == 0);
  ut_ad(buf_flush_get_dirty_pages_count(m_space_id, this) == 0);

#ifdef FLUSH_LIST_OBSERVER_DEBUG
  ib::info(ER_IB_MSG_131) << "~Flush_observer : ID= " << m_id
                          << ", space_id=" << space_id << ", trx_id="
                          << (m_trx == nullptr ? TRX_ID_MAX : trx->id);
#endif /* FLUSH_LIST_OBSERVER_DEBUG */
}

bool Flush_observer::check_interrupted() {
  if (m_trx != nullptr && trx_is_interrupted(m_trx)) {
    interrupted();

    return true;
  }

  return false;
}

void Flush_observer::notify_flush(buf_pool_t *buf_pool, buf_page_t *) {
  m_flushed.at(buf_pool->instance_no).fetch_add(1, std::memory_order_relaxed);

  if (m_stage != nullptr) {
    m_stage->inc(1);
  }
}

void Flush_observer::notify_remove(buf_pool_t *buf_pool, buf_page_t *) {
  m_removed.at(buf_pool->instance_no).fetch_add(1, std::memory_order_relaxed);
}

void Flush_observer::flush() {
  buf_remove_t buf_remove;

  if (m_interrupted) {
    buf_remove = BUF_REMOVE_FLUSH_NO_WRITE;
  } else {
    buf_remove = BUF_REMOVE_FLUSH_WRITE;

    if (m_stage != nullptr) {
      auto pages_to_flush = buf_flush_get_dirty_pages_count(m_space_id, this);
      m_stage->begin_phase_flush(pages_to_flush);
    }
  }

  /* Flush or remove dirty pages. */
  buf_LRU_flush_or_remove_pages(m_space_id, buf_remove, m_trx);

  /* Wait for all dirty pages were flushed. */
  for (ulint i = 0; i < srv_buf_pool_instances; i++) {
    while (!is_complete(i)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  }
}
#else

bool buf_flush_page_cleaner_is_active() { return (false); }

#endif /* UNIV_HOTBACKUP */
