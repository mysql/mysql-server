/*****************************************************************************

Copyright (c) 1995, 2018, Oracle and/or its affiliates. All Rights Reserved.

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
#endif /* !UNIV_HOTBACKUP */
#include "page0zip.h"
#ifndef UNIV_HOTBACKUP
#include "arch0arch.h"
#include "buf0lru.h"
#include "buf0rea.h"
#include "fil0fil.h"
#include "fsp0sysspace.h"
#include "ibuf0ibuf.h"
#include "log0log.h"
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
#endif /* !UNIV_HOTBACKUP */

/** Flag indicating if the page_cleaner is in active state. This flag
is set to TRUE by the page_cleaner thread when it is spawned and is set
back to FALSE at shutdown by the page_cleaner as well. Therefore no
need to protect it by a mutex. It is only ever read by the thread
doing the shutdown */
bool buf_page_cleaner_is_active = false;

#ifndef UNIV_HOTBACKUP
/** Factor for scan length to determine n_pages for intended oldest LSN
progress */
static ulint buf_flush_lsn_scan_factor = 3;

/** Average redo generation rate */
static lsn_t lsn_avg_rate = 0;

/** Target oldest LSN for the requested flush_sync */
static lsn_t buf_flush_sync_lsn = 0;

#ifdef UNIV_PFS_THREAD
mysql_pfs_key_t page_flush_thread_key;
mysql_pfs_key_t page_flush_coordinator_thread_key;
#endif /* UNIV_PFS_THREAD */

/** Event to synchronise with the flushing. */
os_event_t buf_flush_event;

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
  and commited with state==PAGE_CLEANER_STATE_FINISHED.
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
  ulint flush_lru_time;
  /*!< elapsed time for LRU flushing */
  ulint flush_list_time;
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
  ib_mutex_t mutex;         /*!< mutex to protect whole of
                            page_cleaner_t struct and
                            page_cleaner_slot_t slots. */
  os_event_t is_requested;  /*!< event to activate worker
                            threads. */
  os_event_t is_finished;   /*!< event to signal that all
                            slots were finished. */
  volatile ulint n_workers; /*!< number of worker threads
                            in existence */
  bool requested;           /*!< true if requested pages
                            to flush */
  lsn_t lsn_limit;          /*!< upper limit of LSN to be
                            flushed */
  ulint n_slots;            /*!< total number of slots */
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
  ulint flush_time;           /*!< elapsed time to flush
                              requests for all slots */
  ulint flush_pass;           /*!< count to finish to flush
                              requests for all slots */
  page_cleaner_slot_t *slots; /*!< pointer to the slots */
  bool is_running;            /*!< false if attempt
                              to shutdown */

#ifdef UNIV_DEBUG
  ulint n_disabled_debug;
  /*!< how many of pc threads
  have been disabled */
#endif /* UNIV_DEBUG */
};

static page_cleaner_t *page_cleaner = NULL;

#ifdef UNIV_DEBUG
bool innodb_page_cleaner_disabled_debug;
#endif /* UNIV_DEBUG */

/** If LRU list of a buf_pool is less than this size then LRU eviction
should not happen. This is because when we do LRU flushing we also put
the blocks on free list. If LRU list is very small then we can end up
in thrashing. */
#define BUF_LRU_MIN_LEN 256

/* @} */

/** Thread tasked with flushing dirty pages from the buffer pools.
As of now we'll have only one coordinator.
@param[in]	n_page_cleaners	Number of page cleaner threads to create */
static void buf_flush_page_coordinator_thread(size_t n_page_cleaners);

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
/** Validates the flush list.
 @return true if ok */
static ibool buf_flush_validate_low(
    buf_pool_t *buf_pool); /*!< in: Buffer pool instance */

/** Validates the flush list some of the time.
 @return true if ok or the check was skipped */
static ibool buf_flush_validate_skip(
    buf_pool_t *buf_pool) /*!< in: Buffer pool instance */
{
/** Try buf_flush_validate_low() every this many times */
#define BUF_FLUSH_VALIDATE_SKIP 23

  /** The buf_flush_validate_low() call skip counter.
  Use a signed type because of the race condition below. */
  static int buf_flush_validate_count = BUF_FLUSH_VALIDATE_SKIP;

  DBUG_EXECUTE_IF("buf_flush_list_validate", buf_flush_validate_count = 1;);

  /* There is a race condition below, but it does not matter,
  because this call is only for heuristic purposes. We want to
  reduce the call frequency of the costly buf_flush_validate_low()
  check in debug builds. */
  if (--buf_flush_validate_count > 0) {
    return (TRUE);
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
  buf_page_t *prev = NULL;
  buf_pool_t *buf_pool = buf_pool_from_bpage(bpage);

  ut_ad(buf_flush_list_mutex_own(buf_pool));

  /* Insert this buffer into the rbt. */
  c_node = rbt_insert(buf_pool->flush_rbt, &bpage, &bpage);
  ut_a(c_node != NULL);

  /* Get the predecessor. */
  p_node = rbt_prev(buf_pool->flush_rbt, c_node);

  if (p_node != NULL) {
    buf_page_t **value;
    value = rbt_value(buf_page_t *, p_node);
    prev = *value;
    ut_a(prev != NULL);
  }

  return (prev);
}

/** Delete a bpage from the flush_rbt. */
static void buf_flush_delete_from_flush_rbt(
    buf_page_t *bpage) /*!< in: bpage to be removed. */
{
#ifdef UNIV_DEBUG
  ibool ret = FALSE;
#endif /* UNIV_DEBUG */
  buf_pool_t *buf_pool = buf_pool_from_bpage(bpage);

  ut_ad(buf_flush_list_mutex_own(buf_pool));

#ifdef UNIV_DEBUG
  ret =
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

  ut_ad(b1 != NULL);
  ut_ad(b2 != NULL);

#ifdef UNIV_DEBUG
  buf_pool_t *buf_pool = buf_pool_from_bpage(b1);
#endif /* UNIV_DEBUG */

  ut_ad(buf_flush_list_mutex_own(buf_pool));

  ut_ad(b1->in_flush_list);
  ut_ad(b2->in_flush_list);

  if (b2->oldest_modification > b1->oldest_modification) {
    return (1);
  } else if (b2->oldest_modification < b1->oldest_modification) {
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

    ut_ad(buf_pool->flush_rbt == NULL);

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
    buf_pool->flush_rbt = NULL;

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

@param[in]	earlier_added_lsn	oldest_modification of page which was
                                        added to flush list earlier
@param[in]	new_added_lsn		oldest_modification of page which is
                                        being added to flush list
@return true iff the order is valid
@see @ref sect_redo_log_reclaim_space
@see @ref sect_redo_log_add_dirty_pages */
MY_ATTRIBUTE((unused))
static inline bool buf_flush_list_order_validate(lsn_t earlier_added_lsn,
                                                 lsn_t new_added_lsn) {
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

  ut_ad(page->oldest_modification != 0);
  ut_ad(page->newest_modification >= page->oldest_modification);

  return page->oldest_modification;
}

/** Inserts a modified block into the flush list. */
void buf_flush_insert_into_flush_list(
    buf_pool_t *buf_pool, /*!< buffer pool instance */
    buf_block_t *block,   /*!< in/out: block which is modified */
    lsn_t lsn)            /*!< in: oldest modification */
{
  ut_ad(mutex_own(buf_page_get_mutex(&block->page)));

  buf_flush_list_mutex_enter(buf_pool);

  /* If we are in the recovery then we need to update the flush
  red-black tree as well. */
  if (buf_pool->flush_rbt != NULL) {
    ut_ad(lsn != 0);
    ut_ad(block->page.newest_modification != 0);
    buf_flush_list_mutex_exit(buf_pool);
    buf_flush_insert_sorted_into_flush_list(buf_pool, block, lsn);
    return;
  }

  ut_ad(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);
  ut_ad(!block->page.in_flush_list);

  ut_d(block->page.in_flush_list = TRUE);

  if (lsn == 0) {
    /* This is no-redo dirtied page. Borrow the lsn. */
    lsn = buf_flush_borrow_lsn(buf_pool);

    ut_ad(log_lsn_validate(lsn));

    /* This page could already be no-redo dirtied before,
    and flushed since then. Also the page from which we
    borrowed lsn last time could be flushed by LRU and
    we would end up borrowing smaller LSN. In such case
    it's better to stay with previously borrowed lsn,
    not to decrease page's newest_modification.

    Note that in such case, we preserve the previous
    newest_modification but set the oldest_modification
    to the borrowed lsn (smaller value).

    This is required, because possibly the previous
    newest_modification comes from redo-based mtr, and
    it could be much bigger than the corresponding
    previous oldest_modification. Therefore we might
    not use such value for oldest_modification because
    it would break flush order properties. */
    if (block->page.newest_modification < lsn) {
      block->page.newest_modification = lsn;
    }
  }

  ut_ad(log_lsn_validate(lsn));
  ut_ad(block->page.oldest_modification == 0);
  ut_ad(block->page.newest_modification >= lsn);

  ut_ad(UT_LIST_GET_FIRST(buf_pool->flush_list) == NULL ||
        buf_flush_list_order_validate(
            UT_LIST_GET_FIRST(buf_pool->flush_list)->oldest_modification, lsn));

  block->page.oldest_modification = lsn;

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
  ut_d(block->page.in_flush_list = TRUE);
  block->page.oldest_modification = lsn;

#ifdef UNIV_DEBUG_VALGRIND
  void *p;

  if (block->page.size.is_compressed()) {
    p = block->page.zip.data;
  } else {
    p = block->frame;
  }

  UNIV_MEM_ASSERT_RW(p, block->page.size.physical());
#endif /* UNIV_DEBUG_VALGRIND */

  prev_b = NULL;

  /* For the most part when this function is called the flush_rbt
  should not be NULL. In a very rare boundary case it is possible
  that the flush_rbt has already been freed by the recovery thread
  before the last page was hooked up in the flush_list by the
  io-handler thread. In that case we'll just do a simple
  linear search in the else block. */
  if (buf_pool->flush_rbt != NULL) {
    prev_b = buf_flush_insert_in_flush_rbt(&block->page);

  } else {
    b = UT_LIST_GET_FIRST(buf_pool->flush_list);

    while (b != NULL &&
           b->oldest_modification > block->page.oldest_modification) {
      ut_ad(b->in_flush_list);
      prev_b = b;
      b = UT_LIST_GET_NEXT(list, b);
    }
  }

  if (prev_b == NULL) {
    UT_LIST_ADD_FIRST(buf_pool->flush_list, &block->page);
  } else {
    UT_LIST_INSERT_AFTER(buf_pool->flush_list, prev_b, &block->page);
  }

  incr_flush_list_size_in_bytes(block, buf_pool);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
  ut_a(buf_flush_validate_low(buf_pool));
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

  buf_flush_list_mutex_exit(buf_pool);
}

/** Returns TRUE if the file page block is immediately suitable for replacement,
i.e., the transition FILE_PAGE => NOT_USED allowed. The caller must hold the
LRU list and block mutexes.
@param[in]	bpage	buffer control block, must be buf_page_in_file() and
                        in the LRU list
@return true if can replace immediately */
ibool buf_flush_ready_for_replace(buf_page_t *bpage) {
#ifdef UNIV_DEBUG
  buf_pool_t *buf_pool = buf_pool_from_bpage(bpage);
  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
#endif /* UNIV_DEBUG */
  ut_ad(mutex_own(buf_page_get_mutex(bpage)));
  ut_ad(bpage->in_LRU_list);

  if (buf_page_in_file(bpage)) {
    return (bpage->oldest_modification == 0 && bpage->buf_fix_count == 0 &&
            buf_page_get_io_fix(bpage) == BUF_IO_NONE);
  }

  ib::fatal(ER_IB_MSG_123) << "Buffer block " << bpage << " state "
                           << bpage->state << " in the LRU list!";

  return (FALSE);
}

/** Check if the block is modified and ready for flushing.
@param[in]	bpage		buffer control block, must be buf_page_in_file()
@param[in]	flush_type	type of flush
@return true if can flush immediately */
bool buf_flush_ready_for_flush(buf_page_t *bpage, buf_flush_t flush_type) {
#ifdef UNIV_DEBUG
  buf_pool_t *buf_pool = buf_pool_from_bpage(bpage);

  ut_a(buf_page_in_file(bpage) ||
       (buf_page_get_state(bpage) == BUF_BLOCK_REMOVE_HASH &&
        !mutex_own(&buf_pool->LRU_list_mutex)));
#else
  ut_a(buf_page_in_file(bpage) ||
       buf_page_get_state(bpage) == BUF_BLOCK_REMOVE_HASH);
#endif
  ut_ad(mutex_own(buf_page_get_mutex(bpage)) ||
        (flush_type == BUF_FLUSH_LIST && buf_flush_list_mutex_own(buf_pool)));
  ut_ad(flush_type < BUF_FLUSH_N_TYPES);

  if (bpage->oldest_modification == 0 ||
      buf_page_get_io_fix_unlocked(bpage) != BUF_IO_NONE) {
    return (false);
  }

  ut_ad(bpage->in_flush_list);

  switch (flush_type) {
    case BUF_FLUSH_LIST:
      return (buf_page_get_state(bpage) != BUF_BLOCK_REMOVE_HASH);
    case BUF_FLUSH_LRU:
    case BUF_FLUSH_SINGLE_PAGE:
      return (true);

    case BUF_FLUSH_N_TYPES:
      break;
  }

  ut_error;
}

/** Remove a block from the flush list of modified blocks.
@param[in]	bpage	pointer to the block in question */
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
  if (buf_pool->flush_rbt != NULL) {
    buf_flush_delete_from_flush_rbt(bpage);
  }

  /* Must be done after we have removed it from the flush_rbt
  because we assert on in_flush_list in comparison function. */
  ut_d(bpage->in_flush_list = FALSE);

  buf_pool->stat.flush_list_bytes -= bpage->size.physical();

  bpage->oldest_modification = 0;

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
  ut_a(buf_flush_validate_skip(buf_pool));
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

  /* If there is an observer that want to know if the asynchronous
  flushing was done then notify it. */
  if (bpage->flush_observer != NULL) {
    bpage->flush_observer->notify_remove(buf_pool, bpage);

    bpage->flush_observer = NULL;
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
  buf_page_t *prev_b = NULL;
  buf_pool_t *buf_pool = buf_pool_from_bpage(bpage);

  /* Must reside in the same buffer pool. */
  ut_ad(buf_pool == buf_pool_from_bpage(dpage));

  ut_ad(mutex_own(buf_page_get_mutex(bpage)));

  buf_flush_list_mutex_enter(buf_pool);

  ut_ad(bpage->in_flush_list);
  ut_ad(dpage->in_flush_list);

  /* If recovery is active we must swap the control blocks in
  the flush_rbt as well. */
  if (buf_pool->flush_rbt != NULL) {
    buf_flush_delete_from_flush_rbt(bpage);
    prev_b = buf_flush_insert_in_flush_rbt(dpage);
  }

  /* Important that we adjust the hazard pointer before removing
  the bpage from the flush list. */
  buf_pool->flush_hp.adjust(bpage);

  /* Must be done after we have removed it from the flush_rbt
  because we assert on in_flush_list in comparison function. */
  ut_d(bpage->in_flush_list = FALSE);

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
  ut_a(buf_pool->flush_rbt == NULL || prev_b == prev);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
  ut_a(buf_flush_validate_low(buf_pool));
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

  buf_flush_list_mutex_exit(buf_pool);
}

/** Updates the flush system data structures when a write is completed.
@param[in]	bpage	pointer to the block in question */
void buf_flush_write_complete(buf_page_t *bpage) {
  ut_ad(bpage != NULL);
  ut_ad(mutex_own(buf_page_get_mutex(bpage)));

  const buf_flush_t flush_type = buf_page_get_flush_type(bpage);
  buf_pool_t *buf_pool = buf_pool_from_bpage(bpage);

  mutex_enter(&buf_pool->flush_state_mutex);

  buf_flush_remove(bpage);

  buf_page_set_io_fix(bpage, BUF_IO_NONE);

  buf_pool->n_flush[flush_type]--;

  if (buf_pool->n_flush[flush_type] == 0 &&
      buf_pool->init_flush[flush_type] == FALSE) {
    /* The running flush batch has ended */

    os_event_set(buf_pool->no_flush[flush_type]);
  }

  mutex_exit(&buf_pool->flush_state_mutex);

  buf_dblwr_update(bpage, flush_type);
}
#endif /* !UNIV_HOTBACKUP */

/** Calculate the checksum of a page from compressed table and update
the page.
@param[in,out]	page	page to update
@param[in]	size	compressed page size
@param[in]	lsn	LSN to stamp on the page */
void buf_flush_update_zip_checksum(buf_frame_t *page, ulint size, lsn_t lsn) {
  ut_a(size > 0);

  BlockReporter reporter = BlockReporter(false, NULL, univ_page_size, false);

  const uint32_t checksum = reporter.calc_zip_checksum(
      page, size,
      static_cast<srv_checksum_algorithm_t>(srv_checksum_algorithm));

  mach_write_to_8(page + FIL_PAGE_LSN, lsn);
  mach_write_to_4(page + FIL_PAGE_SPACE_OR_CHKSUM, checksum);
}

/** Initialize a page for writing to the tablespace.
@param[in]	block		buffer block; NULL if bypassing the buffer pool
@param[in,out]	page		page frame
@param[in,out]	page_zip_	compressed page, or NULL if uncompressed
@param[in]	newest_lsn	newest modification LSN to the page
@param[in]	skip_checksum	whether to disable the page checksum */
void buf_flush_init_for_writing(const buf_block_t *block, byte *page,
                                void *page_zip_, lsn_t newest_lsn,
                                bool skip_checksum) {
  ib_uint32_t checksum = BUF_NO_CHECKSUM_MAGIC;

  ut_ad(block == NULL || block->frame == page);
  ut_ad(block == NULL || page_zip_ == NULL || &block->page.zip == page_zip_);
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
        memcpy(page_zip->data, page, size);
        /* fall through */
      case FIL_PAGE_TYPE_ZBLOB:
      case FIL_PAGE_TYPE_ZBLOB2:
      case FIL_PAGE_SDI_ZBLOB:
      case FIL_PAGE_INDEX:
      case FIL_PAGE_SDI:
      case FIL_PAGE_RTREE:

        buf_flush_update_zip_checksum(page_zip->data, size, newest_lsn);

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
  mach_write_to_8(page + FIL_PAGE_LSN, newest_lsn);

  mach_write_to_8(page + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM,
                  newest_lsn);

  if (skip_checksum) {
    mach_write_to_4(page + FIL_PAGE_SPACE_OR_CHKSUM, checksum);
  } else {
    if (block != NULL && UNIV_PAGE_SIZE == 16384) {
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
        checksum = (ib_uint32_t)buf_calc_page_new_checksum(page);
        mach_write_to_4(page + FIL_PAGE_SPACE_OR_CHKSUM, checksum);
        checksum = (ib_uint32_t)buf_calc_page_old_checksum(page);
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
/** Does an asynchronous write of a buffer page. NOTE: in simulated aio and
also when the doublewrite buffer is used, we must call
buf_dblwr_flush_buffered_writes after we have posted a batch of writes!
@param[in]	bpage		buffer block to write
@param[in]	flush_type	type of flush
@param[in]	sync		true if sync IO request */
static void buf_flush_write_block_low(buf_page_t *bpage, buf_flush_t flush_type,
                                      bool sync) {
  page_t *frame = NULL;

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
  ut_ad(buf_page_get_io_fix_unlocked(bpage) == BUF_IO_WRITE);
  ut_ad(bpage->oldest_modification != 0);

#ifdef UNIV_IBUF_COUNT_DEBUG
  ut_a(ibuf_count_get(bpage->id) == 0);
#endif /* UNIV_IBUF_COUNT_DEBUG */

  ut_ad(bpage->newest_modification != 0);

  /* Force the log to the disk before writing the modified block */
  if (!srv_read_only_mode) {
    Wait_stats wait_stats;

    wait_stats = log_write_up_to(*log_sys, bpage->newest_modification, true);

    MONITOR_INC_WAIT_STATS_EX(MONITOR_ON_LOG_, _PAGE_WRITTEN, wait_stats);
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

      mach_write_to_8(frame + FIL_PAGE_LSN, bpage->newest_modification);

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
          bpage->zip.data ? &bpage->zip : NULL, bpage->newest_modification,
          fsp_is_checksum_disabled(bpage->id.space()));
      break;
  }

  /* Disable use of double-write buffer for temporary tablespace.
  Given the nature and load of temporary tablespace doublewrite buffer
  adds an overhead during flushing. */

  if (!srv_use_doublewrite_buf || buf_dblwr == NULL || srv_read_only_mode ||
      fsp_is_system_temporary(bpage->id.space())) {
    ut_ad(!srv_read_only_mode || fsp_is_system_temporary(bpage->id.space()));

    ulint type = IORequest::WRITE | IORequest::DO_NOT_WAKE;

    dberr_t err;
    IORequest request(type);

    err = fil_io(request, sync, bpage->id, bpage->size, 0,
                 bpage->size.physical(), frame, bpage);

    ut_a(err == DB_SUCCESS);

  } else if (flush_type == BUF_FLUSH_SINGLE_PAGE) {
    buf_dblwr_write_single_page(bpage, sync);
  } else {
    ut_ad(!sync);
    buf_dblwr_add_to_batch(bpage);
  }

  /* When doing single page flushing the IO is done synchronously
  and we flush the changes to disk only for the tablespace we
  are working on. */
  if (sync) {
    ut_ad(flush_type == BUF_FLUSH_SINGLE_PAGE);
    fil_flush(bpage->id.space());

    /* true means we want to evict this page from the
    LRU list as well. */
    buf_page_io_complete(bpage, true);
  }

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
@param[in]	buf_pool	buffer pool instance
@param[in]	bpage		buffer control block
@param[in]	flush_type	type of flush
@param[in]	sync		true if sync IO request
@return true if page was flushed */
ibool buf_flush_page(buf_pool_t *buf_pool, buf_page_t *bpage,
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

  ibool flush;
  rw_lock_t *rw_lock = NULL;
  bool no_fix_count = bpage->buf_fix_count == 0;

  if (!is_uncompressed) {
    flush = TRUE;
    rw_lock = NULL;
  } else if (!(no_fix_count || flush_type == BUF_FLUSH_LIST) ||
             (!no_fix_count && srv_shutdown_state <= SRV_SHUTDOWN_CLEANUP &&
              fsp_is_system_temporary(bpage->id.space()))) {
    /* This is a heuristic, to avoid expensive SX attempts. */
    /* For table residing in temporary tablespace sync is done
    using IO_FIX and so before scheduling for flush ensure that
    page is not fixed. */
    flush = FALSE;
  } else {
    rw_lock = &reinterpret_cast<buf_block_t *>(bpage)->lock;
    if (flush_type != BUF_FLUSH_LIST) {
      flush = rw_lock_sx_lock_nowait(rw_lock, BUF_IO_WRITE);
    } else {
      /* Will SX lock later */
      flush = TRUE;
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

    if (bpage->oldest_modification > buf_pool->max_lsn_io) {
      buf_pool->max_lsn_io = bpage->oldest_modification;
    }

    if (!fsp_is_system_temporary(bpage->id.space()) &&
        buf_pool->track_page_lsn != LSN_MAX) {
      page_t *frame;
      lsn_t frame_lsn;

      frame = bpage->zip.data;

      if (!frame) {
        frame = ((buf_block_t *)bpage)->frame;
      }
      frame_lsn = mach_read_from_8(frame + FIL_PAGE_LSN);

      arch_page_sys->track_page(bpage, buf_pool->track_page_lsn, frame_lsn,
                                false);
    }

    mutex_exit(&buf_pool->flush_state_mutex);

    mutex_exit(block_mutex);

    if (flush_type == BUF_FLUSH_SINGLE_PAGE) {
      mutex_exit(&buf_pool->LRU_list_mutex);
    }

    if (flush_type == BUF_FLUSH_LIST && is_uncompressed &&
        !rw_lock_sx_lock_nowait(rw_lock, BUF_IO_WRITE)) {
      if (!fsp_is_system_temporary(bpage->id.space())) {
        /* avoiding deadlock possibility involves
        doublewrite buffer, should flush it, because
        it might hold the another block->lock. */
        buf_dblwr_flush_buffered_writes();
      } else {
        buf_dblwr_sync_datafiles();
      }

      rw_lock_sx_lock_gen(rw_lock, BUF_IO_WRITE);
    }

    /* If there is an observer that want to know if the asynchronous
    flushing was sent then notify it.
    Note: we set flush observer to a page with x-latch, so we can
    guarantee that notify_flush and notify_remove are called in pair
    with s-latch on a uncompressed page. */
    if (bpage->flush_observer != NULL) {
      bpage->flush_observer->notify_flush(buf_pool, bpage);
    }

    /* Even though bpage is not protected by any mutex at this
    point, it is safe to access bpage, because it is io_fixed and
    oldest_modification != 0.  Thus, it cannot be relocated in the
    buffer pool or removed from flush_list or LRU_list. */

    buf_flush_write_block_low(bpage, flush_type, sync);
  }

  return (flush);
}

#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
/** Writes a flushable page asynchronously from the buffer pool to a file.
NOTE: block and LRU list mutexes must be held upon entering this function, and
they will be released by this function after flushing. This is loosely based on
buf_flush_batch() and buf_flush_page().
@param[in,out]	buf_pool	buffer pool instance
@param[in,out]	block		buffer control block
@return true if the page was flushed and the mutex released */
ibool buf_flush_page_try(buf_pool_t *buf_pool, buf_block_t *block) {
  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
  ut_ad(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);
  ut_ad(mutex_own(buf_page_get_mutex(&block->page)));

  if (!buf_flush_ready_for_flush(&block->page, BUF_FLUSH_SINGLE_PAGE)) {
    return (FALSE);
  }

  /* The following call will release the LRU list and block mutexes. */
  return (buf_flush_page(buf_pool, &block->page, BUF_FLUSH_SINGLE_PAGE, true));
}
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

/** Check the page is in buffer pool and can be flushed.
@param[in]	page_id		page id
@param[in]	flush_type	BUF_FLUSH_LRU or BUF_FLUSH_LIST
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
@param[in]	page_id		page id
@param[in]	flush_type	BUF_FLUSH_LRU or BUF_FLUSH_LIST
@param[in]	n_flushed	number of pages flushed so far in this batch
@param[in]	n_to_flush	maximum number of pages we are allowed to flush
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

    buf_flush_area = std::min(BUF_READ_AHEAD_AREA(buf_pool),
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

  const page_no_t space_size = fil_space_get_size(page_id.space());
  if (high > space_size) {
    high = space_size;
  }

  DBUG_PRINT("ib_buf", ("flush " UINT32PF ":%u..%u", page_id.space(),
                        (unsigned)low, (unsigned)high));

  for (i = low; i < high; i++) {
    buf_page_t *bpage;
    rw_lock_t *hash_lock;
    BPageMutex *block_mutex;

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

    buf_pool = buf_pool_get(cur_page_id);

    /* We only want to flush pages from this buffer pool. */
    bpage = buf_page_hash_get_s_locked(buf_pool, cur_page_id, &hash_lock);

    if (bpage == NULL) {
      continue;
    }

    block_mutex = buf_page_get_mutex(bpage);

    mutex_enter(block_mutex);

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
is ready to flush then flush the page and try o flush its neighbors. The caller
must hold the buffer pool list mutex corresponding to the type of flush.
@param[in]	bpage		buffer control block,
                                must be buf_page_in_file(bpage)
@param[in]	flush_type	BUF_FLUSH_LRU or BUF_FLUSH_LIST
@param[in]	n_to_flush	number of pages to flush
@param[in,out]	count		number of pages flushed
@return	true if the list mutex was released during this function.  This does
not guarantee that some pages were written as well. */
static bool buf_flush_page_and_try_neighbors(buf_page_t *bpage,
                                             buf_flush_t flush_type,
                                             ulint n_to_flush, ulint *count) {
#ifdef UNIV_DEBUG
  buf_pool_t *buf_pool = buf_pool_from_bpage(bpage);
#endif /* UNIV_DEBUG */

  bool flushed;
  BPageMutex *block_mutex = NULL;

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

  if (buf_flush_ready_for_flush(bpage, flush_type)) {
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
@param[in]	buf_pool	buffer pool instance
@param[in]	max		desired number of blocks in the free_list
@return number of blocks moved to the free list. */
static ulint buf_free_from_unzip_LRU_list_batch(buf_pool_t *buf_pool,
                                                ulint max) {
  ulint scanned = 0;
  ulint count = 0;
  ulint free_len = UT_LIST_GET_LEN(buf_pool->free);
  ulint lru_len = UT_LIST_GET_LEN(buf_pool->unzip_LRU);

  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

  buf_block_t *block = UT_LIST_GET_LAST(buf_pool->unzip_LRU);

  while (block != NULL && count < max && free_len < srv_LRU_scan_depth &&
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
@param[in]	buf_pool	buffer pool instance
@param[in]	max		desired number for blocks in the free_list
@return number of blocks for which the write request was queued. */
static ulint buf_flush_LRU_list_batch(buf_pool_t *buf_pool, ulint max) {
  buf_page_t *bpage;
  ulint scanned = 0;
  ulint evict_count = 0;
  ulint count = 0;
  ulint free_len = UT_LIST_GET_LEN(buf_pool->free);
  ulint lru_len = UT_LIST_GET_LEN(buf_pool->LRU);
  ulint withdraw_depth;

  ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

  withdraw_depth = buf_get_withdraw_depth(buf_pool);

  for (bpage = UT_LIST_GET_LAST(buf_pool->LRU);
       bpage != NULL && count + evict_count < max &&
       free_len < srv_LRU_scan_depth + withdraw_depth &&
       lru_len > BUF_LRU_MIN_LEN;
       ++scanned, bpage = buf_pool->lru_hp.get()) {
    buf_page_t *prev = UT_LIST_GET_PREV(LRU, bpage);
    buf_pool->lru_hp.set(prev);

    BPageMutex *block_mutex = buf_page_get_mutex(bpage);

    bool acquired = mutex_enter_nowait(block_mutex) == 0;

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
      /* Block is ready for flush. Dispatch an IO
      request. The IO helper thread will put it on
      free list in IO completion routine. */
      mutex_exit(block_mutex);
      buf_flush_page_and_try_neighbors(bpage, BUF_FLUSH_LRU, max, &count);
    } else if (!acquired) {
      ut_ad(buf_pool->lru_hp.is_hp(prev));
    } else {
      /* Can't evict or dispatch this block. Go to
      previous. */
      mutex_exit(block_mutex);
      ut_ad(buf_pool->lru_hp.is_hp(prev));
    }

    ut_ad(!mutex_own(block_mutex));
    ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

    free_len = UT_LIST_GET_LEN(buf_pool->free);
    lru_len = UT_LIST_GET_LEN(buf_pool->LRU);
  }

  buf_pool->lru_hp.set(NULL);

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
@param[in]	buf_pool	buffer pool instance
@param[in]	max		desired number of blocks in the free_list
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
@param[in]	buf_pool	buffer pool instance
@param[in]	min_n		wished minimum mumber of blocks flushed (it is
not guaranteed that the actual number is that big, though)
@param[in]	lsn_limit	all blocks whose oldest_modification is smaller
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
       count < min_n && bpage != NULL && len > 0 &&
       bpage->oldest_modification < lsn_limit;
       bpage = buf_pool->flush_hp.get(), ++scanned) {
    buf_page_t *prev;

    ut_a(bpage->oldest_modification > 0);
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

  buf_pool->flush_hp.set(NULL);
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
@param[in]	buf_pool	buffer pool instance
@param[in]	flush_type	BUF_FLUSH_LRU or BUF_FLUSH_LIST; if
BUF_FLUSH_LIST, then the caller must not own any latches on pages
@param[in]	min_n		wished minimum mumber of blocks flushed (it is
not guaranteed that the actual number is that big, though)
@param[in]	lsn_limit	in the case of BUF_FLUSH_LIST all blocks whose
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
 @param page_count_flush	number of pages flushed from the end of the
 flush_list
 @param page_count_LRU	number of pages flushed from the end of the LRU list
 */
static void buf_flush_stats(ulint page_count_flush, ulint page_count_LRU) {
  DBUG_PRINT("ib_buf", ("flush completed, from flush_list %u pages, "
                        "from LRU_list %u pages",
                        unsigned(page_count_flush), unsigned(page_count_LRU)));

  srv_stats.buf_pool_flushed.add(page_count_flush + page_count_LRU);
}

/** Start a buffer flush batch for LRU or flush list
@param[in]	buf_pool	buffer pool instance
@param[in]	flush_type	BUF_FLUSH_LRU or BUF_FLUSH_LIST */
static ibool buf_flush_start(buf_pool_t *buf_pool, buf_flush_t flush_type) {
  ut_ad(flush_type == BUF_FLUSH_LRU || flush_type == BUF_FLUSH_LIST);

  mutex_enter(&buf_pool->flush_state_mutex);

  if (buf_pool->n_flush[flush_type] > 0 ||
      buf_pool->init_flush[flush_type] == TRUE) {
    /* There is already a flush batch of the same type running */

    mutex_exit(&buf_pool->flush_state_mutex);

    return (FALSE);
  }

  buf_pool->init_flush[flush_type] = TRUE;

  os_event_reset(buf_pool->no_flush[flush_type]);

  mutex_exit(&buf_pool->flush_state_mutex);

  return (TRUE);
}

/** End a buffer flush batch for LRU or flush list
@param[in]	buf_pool	buffer pool instance
@param[in]	flush_type	BUF_FLUSH_LRU or BUF_FLUSH_LIST */
static void buf_flush_end(buf_pool_t *buf_pool, buf_flush_t flush_type) {
  mutex_enter(&buf_pool->flush_state_mutex);

  buf_pool->init_flush[flush_type] = FALSE;

  buf_pool->try_LRU_scan = TRUE;

  if (buf_pool->n_flush[flush_type] == 0) {
    /* The running flush batch has ended */

    os_event_set(buf_pool->no_flush[flush_type]);
  }

  mutex_exit(&buf_pool->flush_state_mutex);

  if (!srv_read_only_mode) {
    buf_dblwr_flush_buffered_writes();
  } else {
    os_aio_simulated_wake_handler_threads();
  }
}

/** Waits until a flush batch of the given type ends */
void buf_flush_wait_batch_end(buf_pool_t *buf_pool, /*!< buffer pool instance */
                              buf_flush_t type)     /*!< in: BUF_FLUSH_LRU
                                                    or BUF_FLUSH_LIST */
{
  ut_ad(type == BUF_FLUSH_LRU || type == BUF_FLUSH_LIST);

  if (buf_pool == NULL) {
    ulint i;

    for (i = 0; i < srv_buf_pool_instances; ++i) {
      buf_pool_t *buf_pool;

      buf_pool = buf_pool_from_array(i);

      thd_wait_begin(NULL, THD_WAIT_DISKIO);
      os_event_wait(buf_pool->no_flush[type]);
      thd_wait_end(NULL);
    }
  } else {
    thd_wait_begin(NULL, THD_WAIT_DISKIO);
    os_event_wait(buf_pool->no_flush[type]);
    thd_wait_end(NULL);
  }
}

/** Do flushing batch of a given type.
NOTE: The calling thread is not allowed to own any latches on pages!
@param[in,out]	buf_pool	buffer pool instance
@param[in]	type		flush type
@param[in]	min_n		wished minimum mumber of blocks flushed
(it is not guaranteed that the actual number is that big, though)
@param[in]	lsn_limit	in the case BUF_FLUSH_LIST all blocks whose
oldest_modification is smaller than this should be flushed (if their number
does not exceed min_n), otherwise ignored
@param[out]	n_processed	the number of pages which were processed is
passed back to caller. Ignored if NULL
@retval true	if a batch was queued successfully.
@retval false	if another batch of same type was already running. */
bool buf_flush_do_batch(buf_pool_t *buf_pool, buf_flush_t type, ulint min_n,
                        lsn_t lsn_limit, ulint *n_processed) {
  ut_ad(type == BUF_FLUSH_LRU || type == BUF_FLUSH_LIST);

  if (n_processed != NULL) {
    *n_processed = 0;
  }

  if (!buf_flush_start(buf_pool, type)) {
    return (false);
  }

  ulint page_count = buf_flush_batch(buf_pool, type, min_n, lsn_limit);

  buf_flush_end(buf_pool, type);

  if (n_processed != NULL) {
    *n_processed = page_count;
  }

  return (true);
}

/** This utility flushes dirty blocks from the end of the flush list of all
buffer pool instances.
NOTE: The calling thread is not allowed to own any latches on pages!
@param[in]	min_n		wished minimum mumber of blocks flushed
                                (it is not guaranteed that the actual number
                                is that big, though)
@param[in]	lsn_limit	in the case BUF_FLUSH_LIST all blocks whose
                                oldest_modification is smaller than this
                                should be flushed (if their number does not
                                exceed min_n), otherwise ignored
@param[out]	n_processed	the number of pages which were processed is
                                passed back to caller. Ignored if NULL.

@return true if a batch was queued successfully for each buffer pool
instance. false if another batch of same type was already running in
at least one of the buffer pool instance */
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
@param[in,out]	buf_pool	buffer pool instance
@return true if success. */
bool buf_flush_single_page_from_LRU(buf_pool_t *buf_pool) {
  ulint scanned;
  buf_page_t *bpage;
  ibool freed;

  mutex_enter(&buf_pool->LRU_list_mutex);

  for (bpage = buf_pool->single_scan_itr.start(), scanned = 0, freed = false;
       bpage != NULL; ++scanned, bpage = buf_pool->single_scan_itr.get()) {
    ut_ad(mutex_own(&buf_pool->LRU_list_mutex));

    buf_page_t *prev = UT_LIST_GET_PREV(LRU, bpage);

    buf_pool->single_scan_itr.set(prev);

    BPageMutex *block_mutex;

    block_mutex = buf_page_get_mutex(bpage);

    mutex_enter(block_mutex);

    if (buf_flush_ready_for_replace(bpage)) {
      /* block is ready for eviction i.e., it is
      clean and is not IO-fixed or buffer fixed. */

      if (buf_LRU_free_page(bpage, true)) {
        freed = true;
        break;
      } else {
        mutex_exit(block_mutex);
      }

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

  if (!freed) {
    /* Can't find a single flushable page. */
    ut_ad(!bpage);
    mutex_exit(&buf_pool->LRU_list_mutex);
  }

  if (scanned) {
    MONITOR_INC_VALUE_CUMULATIVE(MONITOR_LRU_SINGLE_FLUSH_SCANNED,
                                 MONITOR_LRU_SINGLE_FLUSH_SCANNED_NUM_CALL,
                                 MONITOR_LRU_SINGLE_FLUSH_SCANNED_PER_CALL,
                                 scanned);
  }

  ut_ad(!mutex_own(&buf_pool->LRU_list_mutex));

  return (freed);
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
    scan_depth = ut_min(withdraw_depth, scan_depth);
  } else {
    scan_depth = ut_min(static_cast<ulint>(srv_LRU_scan_depth), scan_depth);
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

/** Calculates if flushing is required based on number of dirty pages in
 the buffer pool.
 @return percent of io_capacity to flush to manage dirty page ratio */
static ulint af_get_pct_for_dirty() {
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
static ulint af_get_pct_for_lsn(lsn_t age) /*!< in: current age of LSN. */
{
  lsn_t max_async_age;
  lsn_t lsn_age_factor;
  lsn_t af_lwm = (srv_adaptive_flushing_lwm * log_get_capacity()) / 100;

  if (age < af_lwm) {
    /* No adaptive flushing. */
    return (0);
  }

  max_async_age = log_get_max_modified_age_async();

  if (age < max_async_age && !srv_adaptive_flushing) {
    /* We have still not reached the max_async point and
    the user has disabled adaptive flushing. */
    return (0);
  }

  /* If we are here then we know that either:
  1) User has enabled adaptive flushing
  2) User may have disabled adaptive flushing but we have reached
  max_async_age. */
  lsn_age_factor = (age * 100) / max_async_age;

  ut_ad(srv_max_io_capacity >= srv_io_capacity);
  return (static_cast<ulint>(((srv_max_io_capacity / srv_io_capacity) *
                              (lsn_age_factor * sqrt((double)lsn_age_factor))) /
                             7.5));
}

/** This function is called approximately once every second by the
 page_cleaner thread. Based on various factors it decides if there is a
 need to do flushing.
 @return number of pages recommended to be flushed
 @param lsn_limit	pointer to return LSN up to which flushing must happen
 @param last_pages_in	the number of pages flushed by the last flush_list
                         flushing. */
static ulint page_cleaner_flush_pages_recommendation(lsn_t *lsn_limit,
                                                     ulint last_pages_in) {
  static lsn_t prev_lsn = 0;
  static ulint sum_pages = 0;
  static ulint avg_page_rate = 0;
  static ulint n_iterations = 0;
  static time_t prev_time;
  lsn_t oldest_lsn;
  lsn_t cur_lsn;
  lsn_t age;
  lsn_t lsn_rate;
  ulint n_pages = 0;
  ulint pct_for_dirty = 0;
  ulint pct_for_lsn = 0;
  ulint pct_total = 0;

  cur_lsn = log_buffer_dirty_pages_added_up_to_lsn(*log_sys);

  if (prev_lsn == 0) {
    /* First time around. */
    prev_lsn = cur_lsn;
    prev_time = ut_time();
    return (0);
  }

  if (prev_lsn == cur_lsn) {
    return (0);
  }

  sum_pages += last_pages_in;

  time_t curr_time = ut_time();
  double time_elapsed = difftime(curr_time, prev_time);

  /* We update our variables every srv_flushing_avg_loops
  iterations to smooth out transition in workload. */
  if (++n_iterations >= srv_flushing_avg_loops ||
      time_elapsed >= srv_flushing_avg_loops) {
    if (time_elapsed < 1) {
      time_elapsed = 1;
    }

    avg_page_rate = static_cast<ulint>(
        ((static_cast<double>(sum_pages) / time_elapsed) + avg_page_rate) / 2);

    /* How much LSN we have generated since last call. */
    lsn_rate = static_cast<lsn_t>(static_cast<double>(cur_lsn - prev_lsn) /
                                  time_elapsed);

    lsn_avg_rate = (lsn_avg_rate + lsn_rate) / 2;

    /* aggregate stats of all slots */
    mutex_enter(&page_cleaner->mutex);

    ulint flush_tm = page_cleaner->flush_time;
    ulint flush_pass = page_cleaner->flush_pass;

    page_cleaner->flush_time = 0;
    page_cleaner->flush_pass = 0;

    ulint lru_tm = 0;
    ulint list_tm = 0;
    ulint lru_pass = 0;
    ulint list_pass = 0;

    for (ulint i = 0; i < page_cleaner->n_slots; i++) {
      page_cleaner_slot_t *slot;

      slot = &page_cleaner->slots[i];

      lru_tm += slot->flush_lru_time;
      lru_pass += slot->flush_lru_pass;
      list_tm += slot->flush_list_time;
      list_pass += slot->flush_list_pass;

      slot->flush_lru_time = 0;
      slot->flush_lru_pass = 0;
      slot->flush_list_time = 0;
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

    prev_lsn = cur_lsn;
    prev_time = curr_time;

    n_iterations = 0;

    sum_pages = 0;
  }

  oldest_lsn = buf_pool_get_oldest_modification_approx();

  ut_ad(oldest_lsn <= log_get_lsn(*log_sys));

  age = cur_lsn > oldest_lsn ? cur_lsn - oldest_lsn : 0;

  pct_for_dirty = af_get_pct_for_dirty();
  pct_for_lsn = af_get_pct_for_lsn(age);

  pct_total = ut_max(pct_for_dirty, pct_for_lsn);

  /* Estimate pages to be flushed for the lsn progress */
  ulint sum_pages_for_lsn = 0;
  lsn_t target_lsn = oldest_lsn + lsn_avg_rate * buf_flush_lsn_scan_factor;

  for (ulint i = 0; i < srv_buf_pool_instances; i++) {
    buf_pool_t *buf_pool = buf_pool_from_array(i);
    ulint pages_for_lsn = 0;

    buf_flush_list_mutex_enter(buf_pool);
    for (buf_page_t *b = UT_LIST_GET_LAST(buf_pool->flush_list); b != NULL;
         b = UT_LIST_GET_PREV(list, b)) {
      if (b->oldest_modification > target_lsn) {
        break;
      }
      ++pages_for_lsn;
    }
    buf_flush_list_mutex_exit(buf_pool);

    sum_pages_for_lsn += pages_for_lsn;

    mutex_enter(&page_cleaner->mutex);
    ut_ad(page_cleaner->slots[i].state == PAGE_CLEANER_STATE_NONE);
    page_cleaner->slots[i].n_pages_requested =
        pages_for_lsn / buf_flush_lsn_scan_factor + 1;
    mutex_exit(&page_cleaner->mutex);
  }

  sum_pages_for_lsn /= buf_flush_lsn_scan_factor;
  if (sum_pages_for_lsn < 1) {
    sum_pages_for_lsn = 1;
  }

  /* Cap the maximum IO capacity that we are going to use by
  max_io_capacity. Limit the value to avoid too quick increase */
  ulint pages_for_lsn =
      std::min<ulint>(sum_pages_for_lsn, srv_max_io_capacity * 2);

  n_pages = (PCT_IO(pct_total) + avg_page_rate + pages_for_lsn) / 3;

  if (n_pages > srv_max_io_capacity) {
    n_pages = srv_max_io_capacity;
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
                         : n_pages / srv_buf_pool_instances;
  }
  mutex_exit(&page_cleaner->mutex);

  MONITOR_SET(MONITOR_FLUSH_N_TO_FLUSH_REQUESTED, n_pages);

  MONITOR_SET(MONITOR_FLUSH_N_TO_FLUSH_BY_AGE, sum_pages_for_lsn);

  MONITOR_SET(MONITOR_FLUSH_AVG_PAGE_RATE, avg_page_rate);
  MONITOR_SET(MONITOR_FLUSH_LSN_AVG_RATE, lsn_avg_rate);
  MONITOR_SET(MONITOR_FLUSH_PCT_FOR_DIRTY, pct_for_dirty);
  MONITOR_SET(MONITOR_FLUSH_PCT_FOR_LSN, pct_for_lsn);

  *lsn_limit = LSN_MAX;

  return (n_pages);
}

/** Puts the page_cleaner thread to sleep if it has finished work in less
 than a second
 @retval 0 wake up by event set,
 @retval OS_SYNC_TIME_EXCEEDED if timeout was exceeded
 @param next_loop_time	time when next loop iteration should start
 @param sig_count	zero or the value returned by previous call of
                         os_event_reset() */
static ulint pc_sleep_if_needed(ulint next_loop_time, int64_t sig_count) {
  ulint cur_time = ut_time_ms();

  if (next_loop_time > cur_time) {
    /* Get sleep interval in micro seconds. We use
    ut_min() to avoid long sleep in case of wrap around. */
    ulint sleep_us;

    sleep_us =
        ut_min(static_cast<ulint>(1000000), (next_loop_time - cur_time) * 1000);

    return (os_event_wait_time_low(buf_flush_event, sleep_us, sig_count));
  }

  return (OS_SYNC_TIME_EXCEEDED);
}

/** Initialize page_cleaner.
@param[in]	n_page_cleaners	Number of page cleaner threads to create */
void buf_flush_page_cleaner_init(size_t n_page_cleaners) {
  ut_ad(page_cleaner == NULL);

  page_cleaner =
      static_cast<page_cleaner_t *>(ut_zalloc_nokey(sizeof(*page_cleaner)));

  mutex_create(LATCH_ID_PAGE_CLEANER, &page_cleaner->mutex);

  page_cleaner->is_requested = os_event_create("pc_is_requested");
  page_cleaner->is_finished = os_event_create("pc_is_finished");

  page_cleaner->n_slots = static_cast<ulint>(srv_buf_pool_instances);

  page_cleaner->slots = static_cast<page_cleaner_slot_t *>(
      ut_zalloc_nokey(page_cleaner->n_slots * sizeof(*page_cleaner->slots)));

  ut_d(page_cleaner->n_disabled_debug = 0);

  page_cleaner->is_running = true;

  os_thread_create(page_flush_coordinator_thread_key,
                   buf_flush_page_coordinator_thread, n_page_cleaners);

  /* Make sure page cleaner is active. */

  while (!buf_page_cleaner_is_active) {
    os_thread_sleep(10000);
  }
}

/**
Close page_cleaner. */
static void buf_flush_page_cleaner_close(void) {
  /* waiting for all worker threads exit */
  while (page_cleaner->n_workers > 0) {
    os_thread_sleep(10000);
  }

  mutex_destroy(&page_cleaner->mutex);

  ut_free(page_cleaner->slots);

  os_event_destroy(page_cleaner->is_finished);
  os_event_destroy(page_cleaner->is_requested);

  ut_free(page_cleaner);

  page_cleaner = NULL;
}

/**
Requests for all slots to flush all buffer pool instances.
@param min_n	wished minimum mumber of blocks flushed
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
    page_cleaner_flush_pages_recommendation() */

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
@return	the number of the slots which has not been treated yet. */
static ulint pc_flush_slot(void) {
  ulint lru_tm = 0;
  ulint list_tm = 0;
  int lru_pass = 0;
  int list_pass = 0;

  mutex_enter(&page_cleaner->mutex);

  if (page_cleaner->n_slots_requested > 0) {
    page_cleaner_slot_t *slot = NULL;
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
      goto finish_mutex;
    }

    mutex_exit(&page_cleaner->mutex);

    lru_tm = ut_time_ms();

    /* Flush pages from end of LRU if required */
    slot->n_flushed_lru = buf_flush_LRU_list(buf_pool);

    lru_tm = ut_time_ms() - lru_tm;
    lru_pass++;

    if (!page_cleaner->is_running) {
      slot->n_flushed_list = 0;
      goto finish;
    }

    /* Flush pages from flush_list if required */
    if (page_cleaner->requested) {
      list_tm = ut_time_ms();

      slot->succeeded_list =
          buf_flush_do_batch(buf_pool, BUF_FLUSH_LIST, slot->n_pages_requested,
                             page_cleaner->lsn_limit, &slot->n_flushed_list);

      list_tm = ut_time_ms() - list_tm;
      list_pass++;
    } else {
      slot->n_flushed_list = 0;
      slot->succeeded_list = true;
    }
  finish:
    mutex_enter(&page_cleaner->mutex);
  finish_mutex:
    page_cleaner->n_slots_flushing--;
    page_cleaner->n_slots_finished++;
    slot->state = PAGE_CLEANER_STATE_FINISHED;

    slot->flush_lru_time += lru_tm;
    slot->flush_list_time += list_tm;
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
@param n_flushed_lru	number of pages flushed from the end of the LRU list.
@param n_flushed_list	number of pages flushed from the end of the
                        flush_list.
@return			true if all flush_list flushing batch were success. */
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

  return (all_succeeded);
}

#ifdef UNIV_LINUX
/**
Set priority for page_cleaner threads.
@param[in]	priority	priority intended to set
@return	true if set as intended */
static bool buf_flush_page_cleaner_set_priority(int priority) {
  setpriority(PRIO_PROCESS, (pid_t)syscall(SYS_gettid), priority);
  return (getpriority(PRIO_PROCESS, (pid_t)syscall(SYS_gettid)) == priority);
}
#endif /* UNIV_LINUX */

#ifdef UNIV_DEBUG
/** Loop used to disable page cleaner threads. */
static void buf_flush_page_cleaner_disabled_loop(void) {
  ut_ad(page_cleaner != NULL);

  if (!innodb_page_cleaner_disabled_debug) {
    /* We return to avoid entering and exiting mutex. */
    return;
  }

  mutex_enter(&page_cleaner->mutex);
  page_cleaner->n_disabled_debug++;
  mutex_exit(&page_cleaner->mutex);

  while (innodb_page_cleaner_disabled_debug &&
         srv_shutdown_state == SRV_SHUTDOWN_NONE && page_cleaner->is_running) {
    os_thread_sleep(100000); /* [A] */
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

/** Disables page cleaner threads (coordinator and workers).
It's used by: SET GLOBAL innodb_page_cleaner_disabled_debug = 1 (0).
@param[in]	thd		thread handle
@param[in]	var		pointer to system variable
@param[out]	var_ptr		where the formal string goes
@param[in]	save		immediate result from check function */
void buf_flush_page_cleaner_disabled_debug_update(THD *thd, SYS_VAR *var,
                                                  void *var_ptr,
                                                  const void *save) {
  if (page_cleaner == NULL) {
    return;
  }

  if (!*static_cast<const bool *>(save)) {
    if (!innodb_page_cleaner_disabled_debug) {
      return;
    }

    innodb_page_cleaner_disabled_debug = false;

    /* Enable page cleaner threads. */
    while (srv_shutdown_state == SRV_SHUTDOWN_NONE) {
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

  while (srv_shutdown_state == SRV_SHUTDOWN_NONE) {
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

    os_thread_sleep(100000);
  }
}
#endif /* UNIV_DEBUG */

/** Thread tasked with flushing dirty pages from the buffer pools.
As of now we'll have only one coordinator.
@param[in]	n_page_cleaners	Number of page cleaner threads to create */
static void buf_flush_page_coordinator_thread(size_t n_page_cleaners) {
  ulint next_loop_time = ut_time_ms() + 1000;
  ulint n_flushed = 0;
  ulint last_activity = srv_get_activity_count();
  ulint last_pages = 0;

  my_thread_init();

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

  buf_page_cleaner_is_active = true;

  /* We start from 1 because the coordinator thread is part of the
  same set */

  for (size_t i = 1; i < n_page_cleaners; ++i) {
    os_thread_create(page_flush_thread_key, buf_flush_page_cleaner_thread);
  }

  while (!srv_read_only_mode && srv_shutdown_state == SRV_SHUTDOWN_NONE &&
         recv_sys->spaces != NULL) {
    /* treat flushing requests during recovery. */
    ulint n_flushed_lru = 0;
    ulint n_flushed_list = 0;

    os_event_wait(recv_sys->flush_start);

    if (srv_shutdown_state != SRV_SHUTDOWN_NONE || recv_sys->spaces == NULL) {
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
        ut_ad(0);
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
  int64_t sig_count = os_event_reset(buf_flush_event);

  while (srv_shutdown_state == SRV_SHUTDOWN_NONE) {
    /* The page_cleaner skips sleep if the server is
    idle and there are no pending IOs in the buffer pool
    and there is work to do. */
    if (srv_check_activity(last_activity) || buf_get_n_pending_read_ios() ||
        n_flushed == 0) {
      ret_sleep = pc_sleep_if_needed(next_loop_time, sig_count);

      if (srv_shutdown_state != SRV_SHUTDOWN_NONE) {
        break;
      }
    } else if (ut_time_ms() > next_loop_time) {
      ret_sleep = OS_SYNC_TIME_EXCEEDED;
    } else {
      ret_sleep = 0;
    }

    sig_count = os_event_reset(buf_flush_event);

    if (ret_sleep == OS_SYNC_TIME_EXCEEDED) {
      ulint curr_time = ut_time_ms();

      if (curr_time > next_loop_time + 3000) {
        if (warn_count == 0) {
          ulint us;

          us = 1000 + curr_time - next_loop_time;

          ib::info(ER_IB_MSG_128)
              << "Page cleaner took " << us << "ms to flush " << n_flushed_last
              << " and evict " << n_evicted << " pages";

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

      next_loop_time = curr_time + 1000;
      n_flushed_last = n_evicted = 0;
    }

    if (ret_sleep != OS_SYNC_TIME_EXCEEDED && srv_flush_sync &&
        buf_flush_sync_lsn > 0) {
      /* woke up for flush_sync */
      mutex_enter(&page_cleaner->mutex);
      lsn_t lsn_limit = buf_flush_sync_lsn;
      buf_flush_sync_lsn = 0;
      mutex_exit(&page_cleaner->mutex);

      /* Request flushing for threads */
      pc_request(ULINT_MAX, lsn_limit);

      ulint tm = ut_time_ms();

      /* Coordinator also treats requests */
      while (pc_flush_slot() > 0) {
      }

      /* only coordinator is using these counters,
      so no need to protect by lock. */
      page_cleaner->flush_time += ut_time_ms() - tm;
      page_cleaner->flush_pass++;

      /* Wait for all slots to be finished */
      ulint n_flushed_lru = 0;
      ulint n_flushed_list = 0;
      pc_wait_finished(&n_flushed_lru, &n_flushed_list);

      if (n_flushed_list > 0 || n_flushed_lru > 0) {
        buf_flush_stats(n_flushed_list, n_flushed_lru);

        MONITOR_INC_VALUE_CUMULATIVE(
            MONITOR_FLUSH_SYNC_TOTAL_PAGE, MONITOR_FLUSH_SYNC_COUNT,
            MONITOR_FLUSH_SYNC_PAGES, n_flushed_lru + n_flushed_list);
      }

      n_flushed = n_flushed_lru + n_flushed_list;

    } else if (srv_check_activity(last_activity)) {
      ulint n_to_flush;
      lsn_t lsn_limit = 0;

      /* Estimate pages from flush_list to be flushed */
      if (ret_sleep == OS_SYNC_TIME_EXCEEDED) {
        last_activity = srv_get_activity_count();
        n_to_flush =
            page_cleaner_flush_pages_recommendation(&lsn_limit, last_pages);
      } else {
        n_to_flush = 0;
      }

      /* Request flushing for threads */
      pc_request(n_to_flush, lsn_limit);

      ulint tm = ut_time_ms();

      /* Coordinator also treats requests */
      while (pc_flush_slot() > 0) {
        /* No op */
      }

      /* only coordinator is using these counters,
      so no need to protect by lock. */
      page_cleaner->flush_time += ut_time_ms() - tm;
      page_cleaner->flush_pass++;

      /* Wait for all slots to be finished */
      ulint n_flushed_lru = 0;
      ulint n_flushed_list = 0;

      pc_wait_finished(&n_flushed_lru, &n_flushed_list);

      if (n_flushed_list > 0 || n_flushed_lru > 0) {
        buf_flush_stats(n_flushed_list, n_flushed_lru);
      }

      if (ret_sleep == OS_SYNC_TIME_EXCEEDED) {
        last_pages = n_flushed_list;
      }

      n_evicted += n_flushed_lru;
      n_flushed_last += n_flushed_list;

      n_flushed = n_flushed_lru + n_flushed_list;

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

    } else if (ret_sleep == OS_SYNC_TIME_EXCEEDED) {
      /* no activity, slept enough */
      buf_flush_lists(PCT_IO(100), LSN_MAX, &n_flushed);

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

  ut_ad(srv_shutdown_state > 0);
  if (srv_fast_shutdown == 2 ||
      srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS) {
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
  During shutdown we pass through two stages. In the first stage,
  when SRV_SHUTDOWN_CLEANUP is set other threads like the master
  and the purge threads may be working as well. We start flushing
  the buffer pool but can't be sure that no new pages are being
  dirtied until we enter SRV_SHUTDOWN_FLUSH_PHASE phase. */

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
      os_thread_sleep(100000);
    }
  } while (srv_shutdown_state == SRV_SHUTDOWN_CLEANUP);

  /* At this point all threads including the master and the purge
  thread must have been suspended. */
  ut_a(!srv_master_thread_active());
  ut_a(srv_shutdown_state == SRV_SHUTDOWN_FLUSH_PHASE);

  /* We can now make a final sweep on flushing the buffer pool
  and exit after we have cleaned the whole buffer pool.
  It is important that we wait for any running batch that has
  been triggered by us to finish. Otherwise we can end up
  considering end of that batch as a finish of our final
  sweep and we'll come out of the loop leaving behind dirty pages
  in the flush_list */
  buf_flush_wait_batch_end(NULL, BUF_FLUSH_LIST);
  buf_flush_wait_LRU_batch_end();

  bool success;

  do {
    pc_request(ULINT_MAX, LSN_MAX);

    while (pc_flush_slot() > 0) {
    }

    ulint n_flushed_lru = 0;
    ulint n_flushed_list = 0;
    success = pc_wait_finished(&n_flushed_lru, &n_flushed_list);

    n_flushed = n_flushed_lru + n_flushed_list;

    buf_flush_wait_batch_end(NULL, BUF_FLUSH_LIST);
    buf_flush_wait_LRU_batch_end();

  } while (!success || n_flushed > 0);

  /* Some sanity checks */
  ut_a(!srv_master_thread_active());
  ut_a(srv_shutdown_state == SRV_SHUTDOWN_FLUSH_PHASE);

  for (ulint i = 0; i < srv_buf_pool_instances; i++) {
    buf_pool_t *buf_pool = buf_pool_from_array(i);
    ut_a(UT_LIST_GET_LEN(buf_pool->flush_list) == 0);
  }

  /* We have lived our life. Time to die. */

thread_exit:
  /* All worker threads are waiting for the event here,
  and no more access to page_cleaner structure by them.
  Wakes worker threads up just to make them exit. */
  page_cleaner->is_running = false;
  os_event_set(page_cleaner->is_requested);

  buf_flush_page_cleaner_close();

  buf_page_cleaner_is_active = false;

  my_thread_end();
}

/** Worker thread of page_cleaner. */
static void buf_flush_page_cleaner_thread() {
  my_thread_init();
  mutex_enter(&page_cleaner->mutex);
  ++page_cleaner->n_workers;
  mutex_exit(&page_cleaner->mutex);

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

  mutex_enter(&page_cleaner->mutex);
  --page_cleaner->n_workers;
  mutex_exit(&page_cleaner->mutex);
  my_thread_end();
}

/** Synchronously flush dirty blocks from the end of the flush list of all
 buffer pool instances. NOTE: The calling thread is not allowed to own any
 latches on pages! */
void buf_flush_sync_all_buf_pools(void) {
  bool success;
  ulint n_pages;
  do {
    n_pages = 0;
    success = buf_flush_lists(ULINT_MAX, LSN_MAX, &n_pages);
    buf_flush_wait_batch_end(NULL, BUF_FLUSH_LIST);

    if (!success) {
      MONITOR_INC(MONITOR_FLUSH_SYNC_WAITS);
    }

    MONITOR_INC_VALUE_CUMULATIVE(MONITOR_FLUSH_SYNC_TOTAL_PAGE,
                                 MONITOR_FLUSH_SYNC_COUNT,
                                 MONITOR_FLUSH_SYNC_PAGES, n_pages);
  } while (!success);

  ut_a(success);
}

/** Request IO burst and wake page_cleaner up.
@param[in]	lsn_limit	upper limit of LSN to be flushed */
void buf_flush_request_force(lsn_t lsn_limit) {
  ut_a(buf_page_cleaner_is_active);

  /* adjust based on lsn_avg_rate not to get old */
  lsn_t lsn_target = lsn_limit + lsn_avg_rate * 3;

  mutex_enter(&page_cleaner->mutex);
  if (lsn_target > buf_flush_sync_lsn) {
    buf_flush_sync_lsn = lsn_target;
  }
  mutex_exit(&page_cleaner->mutex);

  os_event_set(buf_flush_event);
}
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG

/** Functor to validate the flush list. */
struct Check {
  void operator()(const buf_page_t *elem) { ut_a(elem->in_flush_list); }
};

/** Validates the flush list.
 @return true if ok */
static ibool buf_flush_validate_low(
    buf_pool_t *buf_pool) /*!< in: Buffer pool instance */
{
  buf_page_t *bpage;
  const ib_rbt_node_t *rnode = NULL;
  Check check;

  ut_ad(buf_flush_list_mutex_own(buf_pool));

  ut_list_validate(buf_pool->flush_list, check);

  bpage = UT_LIST_GET_FIRST(buf_pool->flush_list);

  /* If we are in recovery mode i.e.: flush_rbt != NULL
  then each block in the flush_list must also be present
  in the flush_rbt. */
  if (buf_pool->flush_rbt != NULL) {
    rnode = rbt_first(buf_pool->flush_rbt);
  }

  while (bpage != NULL) {
    const lsn_t om = bpage->oldest_modification;

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

    if (buf_pool->flush_rbt != NULL) {
      buf_page_t **prpage;

      ut_a(rnode != NULL);
      prpage = rbt_value(buf_page_t *, rnode);

      ut_a(*prpage != NULL);
      ut_a(*prpage == bpage);
      rnode = rbt_next(buf_pool->flush_rbt, rnode);
    }

    bpage = UT_LIST_GET_NEXT(list, bpage);

    ut_a(bpage == NULL ||
         buf_flush_list_order_validate(bpage->oldest_modification, om));
  }

  /* By this time we must have exhausted the traversal of
  flush_rbt (if active) as well. */
  ut_a(rnode == NULL);

  return (TRUE);
}

/** Validates the flush list.
 @return true if ok */
ibool buf_flush_validate(buf_pool_t *buf_pool) /*!< buffer pool instance */
{
  ibool ret;

  buf_flush_list_mutex_enter(buf_pool);

  ret = buf_flush_validate_low(buf_pool);

  buf_flush_list_mutex_exit(buf_pool);

  return (ret);
}
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

/** Check if there are any dirty pages that belong to a space id in the flush
 list in a particular buffer pool.
 @return number of dirty pages present in a single buffer pool */
ulint buf_pool_get_dirty_pages_count(
    buf_pool_t *buf_pool,    /*!< in: buffer pool */
    space_id_t id,           /*!< in: space id to check */
    FlushObserver *observer) /*!< in: flush observer to check */

{
  ulint count = 0;

  buf_flush_list_mutex_enter(buf_pool);

  buf_page_t *bpage;

  for (bpage = UT_LIST_GET_FIRST(buf_pool->flush_list); bpage != 0;
       bpage = UT_LIST_GET_NEXT(list, bpage)) {
    ut_ad(buf_page_in_file(bpage) ||
          buf_page_get_state(bpage) == BUF_BLOCK_REMOVE_HASH);
    ut_ad(bpage->in_flush_list);
    ut_ad(bpage->oldest_modification > 0);

    if ((observer != NULL && observer == bpage->flush_observer) ||
        (observer == NULL && id == bpage->id.space())) {
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
    space_id_t id,           /*!< in: space id to check */
    FlushObserver *observer) /*!< in: flush observer to check */
{
  ulint count = 0;

  for (ulint i = 0; i < srv_buf_pool_instances; ++i) {
    buf_pool_t *buf_pool;

    buf_pool = buf_pool_from_array(i);

    count += buf_pool_get_dirty_pages_count(buf_pool, id, observer);
  }

  return (count);
}

/** FlushObserver constructor
@param[in]	space_id	table space id
@param[in]	trx		trx instance
@param[in]	stage		performance schema accounting object,
used by ALTER TABLE. It is passed to log_preflush_pool_modified_pages()
for accounting. */
FlushObserver::FlushObserver(space_id_t space_id, trx_t *trx,
                             ut_stage_alter_t *stage)
    : m_space_id(space_id), m_trx(trx), m_stage(stage), m_interrupted(false) {
  m_flushed = UT_NEW_NOKEY(std::vector<ulint>(srv_buf_pool_instances));
  m_removed = UT_NEW_NOKEY(std::vector<ulint>(srv_buf_pool_instances));

  for (ulint i = 0; i < srv_buf_pool_instances; i++) {
    m_flushed->at(i) = 0;
    m_removed->at(i) = 0;
  }

#ifdef FLUSH_LIST_OBSERVER_DEBUG
  ib::info(ER_IB_MSG_130) << "FlushObserver constructor: " << m_trx->id;
#endif /* FLUSH_LIST_OBSERVER_DEBUG */
}

/** FlushObserver deconstructor */
FlushObserver::~FlushObserver() {
  ut_ad(buf_flush_get_dirty_pages_count(m_space_id, this) == 0);

  UT_DELETE(m_flushed);
  UT_DELETE(m_removed);

#ifdef FLUSH_LIST_OBSERVER_DEBUG
  ib::info(ER_IB_MSG_131) << "FlushObserver deconstructor: " << m_trx->id;
#endif /* FLUSH_LIST_OBSERVER_DEBUG */
}

/** Check whether trx is interrupted
@return true if trx is interrupted */
bool FlushObserver::check_interrupted() {
  if (trx_is_interrupted(m_trx)) {
    interrupted();

    return (true);
  }

  return (false);
}

/** Notify observer of a flush
@param[in]	buf_pool	buffer pool instance
@param[in]	bpage		buffer page to flush */
void FlushObserver::notify_flush(buf_pool_t *buf_pool, buf_page_t *bpage) {
  os_atomic_increment_ulint(&m_flushed->at(buf_pool->instance_no), 1);

  if (m_stage != NULL) {
    m_stage->inc();
  }
}

/** Notify observer of a remove
@param[in]	buf_pool	buffer pool instance
@param[in]	bpage		buffer page flushed */
void FlushObserver::notify_remove(buf_pool_t *buf_pool, buf_page_t *bpage) {
  os_atomic_increment_ulint(&m_removed->at(buf_pool->instance_no), 1);
}

/** Flush dirty pages and wait. */
void FlushObserver::flush() {
  buf_remove_t buf_remove;

  if (m_interrupted) {
    buf_remove = BUF_REMOVE_FLUSH_NO_WRITE;
  } else {
    buf_remove = BUF_REMOVE_FLUSH_WRITE;

    if (m_stage != NULL) {
      ulint pages_to_flush = buf_flush_get_dirty_pages_count(m_space_id, this);

      m_stage->begin_phase_flush(pages_to_flush);
    }
  }

  /* Flush or remove dirty pages. */
  buf_LRU_flush_or_remove_pages(m_space_id, buf_remove, m_trx);

  /* Wait for all dirty pages were flushed. */
  for (ulint i = 0; i < srv_buf_pool_instances; i++) {
    while (!is_complete(i)) {
      os_thread_sleep(2000);
    }
  }
}
#endif /* UNIV_HOTBACKUP */
