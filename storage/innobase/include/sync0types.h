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

/** @file include/sync0types.h
 Global types for sync

 Created 9/5/1995 Heikki Tuuri
 *******************************************************/

#ifndef sync0types_h
#define sync0types_h

#include <iostream>
#include <vector>

#include "sync0sync.h"
#include "univ.i"
#include "ut0counter.h"
#include "ut0log.h"
#include "ut0new.h"

#ifdef UNIV_DEBUG
/** Set when InnoDB has invoked exit(). */
extern bool innodb_calling_exit;
#endif /* UNIV_DEBUG */

#ifdef _WIN32
/** Native mutex */
typedef CRITICAL_SECTION sys_mutex_t;
#else
/** Native mutex */
typedef pthread_mutex_t sys_mutex_t;
#endif /* _WIN32 */

/*
                LATCHING ORDER WITHIN THE DATABASE
                ==================================

The mutex or latch in the central memory object, for instance, a rollback
segment object, must be acquired before acquiring the latch or latches to
the corresponding file data structure. In the latching order below, these
file page object latches are placed immediately below the corresponding
central memory object latch or mutex.

Synchronization object                  Notes
----------------------                  -----

Dictionary mutex                        If we have a pointer to a dictionary
|                                       object, e.g., a table, it can be
|                                       accessed without reserving the
|                                       dictionary mutex. We must have a
|                                       reservation, a memoryfix, to the
|                                       appropriate table object in this case,
|                                       and the table must be explicitly
|                                       released later.
V
Dictionary header
|
V
Secondary index tree latch              The tree latch protects also all
|                                       the B-tree non-leaf pages. These
V                                       can be read with the page only
Secondary index non-leaf                bufferfixed to save CPU time,
|                                       no s-latch is needed on the page.
|                                       Modification of a page requires an
|                                       x-latch on the page, however. If a
|                                       thread owns an x-latch to the tree,
|                                       it is allowed to latch non-leaf pages
|                                       even after it has acquired the fsp
|                                       latch.
V
Secondary index leaf                    The latch on the secondary index leaf
|                                       can be kept while accessing the
|                                       clustered index, to save CPU time.
V
Clustered index tree latch              To increase concurrency, the tree
|                                       latch is usually released when the
|                                       leaf page latch has been acquired.
V
Clustered index non-leaf
|
V
Clustered index leaf
|
V
Transaction system header
|
V
Transaction undo mutex                  The undo log entry must be written
|                                       before any index page is modified.
|                                       Transaction undo mutex is for the undo
|                                       logs the analogue of the tree latch
|                                       for a B-tree. If a thread has the
|                                       trx undo mutex reserved, it is allowed
|                                       to latch the undo log pages in any
|                                       order, and also after it has acquired
|                                       the fsp latch.
V
Rollback segment mutex                  The rollback segment mutex must be
|                                       reserved, if, e.g., a new page must
|                                       be added to an undo log. The rollback
|                                       segment and the undo logs in its
|                                       history list can be seen as an
|                                       analogue of a B-tree, and the latches
|                                       reserved similarly, using a version of
|                                       lock-coupling. If an undo log must be
|                                       extended by a page when inserting an
|                                       undo log record, this corresponds to
|                                       a pessimistic insert in a B-tree.
V
Rollback segment header
|
V
Purge system latch
|
V
Undo log pages                          If a thread owns the trx undo mutex,
|                                       or for a log in the history list, the
|                                       rseg mutex, it is allowed to latch
|                                       undo log pages in any order, and even
|                                       after it has acquired the fsp latch.
|                                       If a thread does not have the
|                                       appropriate mutex, it is allowed to
|                                       latch only a single undo log page in
|                                       a mini-transaction.
V
File space management latch             If a mini-transaction must allocate
|                                       several file pages, it can do that,
|                                       because it keeps the x-latch to the
|                                       file space management in its memo.
V
File system pages
|
V
lock_sys_wait_mutex                     Mutex protecting lock timeout data
|
V
lock_sys->global_sharded_latch          Sharded rw-latch protecting lock_sys_t
|
V
lock_sys->table_mutexes                 Mutexes protecting lock_sys_t table
|                                       lock queues
|
V
lock_sys->page_mutexes                  Mutexes protecting lock_sys_t page
|                                       lock queues
|
V
trx_sys->mutex                          Mutex protecting trx_sys_t
|
V
Threads mutex                           Background thread scheduling mutex
|
V
query_thr_mutex                         Mutex protecting query threads
|
V
trx_mutex                               Mutex protecting trx_t fields
|
V
Search system mutex
|
V
Buffer pool mutexes
|
V
Log mutex
|
Any other latch
|
V
Memory pool mutex */

/** Latching order levels. If you modify these, you have to also update
LatchDebug internals in sync0debug.cc */

enum latch_level_t {
  SYNC_UNKNOWN = 0,

  SYNC_MUTEX = 1,

  RW_LOCK_SX,
  RW_LOCK_X_WAIT,
  RW_LOCK_S,
  RW_LOCK_X,
  RW_LOCK_NOT_LOCKED,

  SYNC_LOCK_FREE_HASH,

  SYNC_MONITOR_MUTEX,

  SYNC_ANY_LATCH,

  SYNC_FIL_SHARD,

  SYNC_PAGE_ARCH_OPER,

  SYNC_BUF_FLUSH_LIST,
  SYNC_BUF_FLUSH_STATE,
  SYNC_BUF_ZIP_HASH,
  SYNC_BUF_FREE_LIST,
  SYNC_BUF_ZIP_FREE,
  SYNC_BUF_BLOCK,
  SYNC_BUF_PAGE_HASH,
  SYNC_BUF_LRU_LIST,
  SYNC_BUF_CHUNKS,

  SYNC_DBLWR,

  SYNC_SEARCH_SYS,

  SYNC_WORK_QUEUE,

  SYNC_FTS_TOKENIZE,
  SYNC_FTS_OPTIMIZE,
  SYNC_FTS_BG_THREADS,
  SYNC_FTS_CACHE_INIT,
  SYNC_RECV,

  SYNC_LOG_LIMITS,
  SYNC_LOG_FLUSHER,
  SYNC_LOG_FILES,
  SYNC_LOG_WRITER,
  SYNC_LOG_WRITE_NOTIFIER,
  SYNC_LOG_FLUSH_NOTIFIER,
  SYNC_LOG_CLOSER,
  SYNC_LOG_CHECKPOINTER,
  SYNC_LOG_SN,
  SYNC_LOG_SN_MUTEX,
  SYNC_PAGE_ARCH,
  SYNC_PAGE_ARCH_CLIENT,
  SYNC_LOG_ARCH,

  SYNC_PAGE_CLEANER,
  SYNC_TRX_SYS_HEADER,
  SYNC_TRX_SYS_SERIALISATION,
  SYNC_PURGE_QUEUE,
  SYNC_THREADS,
  SYNC_TRX,
  SYNC_POOL,
  SYNC_POOL_MANAGER,
  SYNC_TRX_SYS_SHARD,
  SYNC_TRX_SYS,
  SYNC_LOCK_SYS_SHARDED,
  SYNC_LOCK_SYS_GLOBAL,
  SYNC_LOCK_WAIT_SYS,

  SYNC_INDEX_ONLINE_LOG,

  SYNC_IBUF_BITMAP,
  SYNC_IBUF_BITMAP_MUTEX,
  SYNC_IBUF_TREE_NODE,
  SYNC_IBUF_TREE_NODE_NEW,
  SYNC_IBUF_INDEX_TREE,

  SYNC_IBUF_MUTEX,

  SYNC_FSP_PAGE,
  SYNC_FSP,
  SYNC_TEMP_POOL_MANAGER,
  SYNC_EXTERN_STORAGE,
  SYNC_RSEG_ARRAY_HEADER,
  SYNC_TRX_UNDO_PAGE,
  SYNC_RSEG_HEADER,
  SYNC_RSEG_HEADER_NEW,
  SYNC_TEMP_SPACE_RSEG,
  SYNC_UNDO_SPACE_RSEG,
  SYNC_TRX_SYS_RSEG,
  SYNC_TRX_UNDO,
  SYNC_PURGE_LATCH,
  SYNC_TREE_NODE,
  SYNC_TREE_NODE_FROM_HASH,
  SYNC_TREE_NODE_NEW,
  SYNC_INDEX_TREE,
  SYNC_RSEGS,
  SYNC_UNDO_SPACES,

  SYNC_PERSIST_DIRTY_TABLES,
  SYNC_PERSIST_AUTOINC,

  SYNC_IBUF_PESS_INSERT_MUTEX,
  SYNC_IBUF_HEADER,
  SYNC_DICT_HEADER,
  SYNC_TABLE,
  SYNC_STATS_AUTO_RECALC,
  SYNC_DICT_AUTOINC_MUTEX,
  SYNC_DICT,
  SYNC_PARSER,
  SYNC_FTS_CACHE,
  SYNC_UNDO_DDL,

  SYNC_DICT_OPERATION,

  SYNC_AHI_ENABLED,

  SYNC_TRX_I_S_LAST_READ,

  SYNC_TRX_I_S_RWLOCK,

  SYNC_RECV_WRITER,

  /** Level is varying. Only used with buffer pool page locks, which
  do not have a fixed level, but instead have their level set after
  the page is locked; see e.g.  ibuf_bitmap_get_map_page(). */

  SYNC_LEVEL_VARYING,

  /** This can be used to suppress order checking. */
  SYNC_NO_ORDER_CHECK,

  /** Maximum level value */
  SYNC_LEVEL_MAX = SYNC_NO_ORDER_CHECK
};

/** Each latch has an ID. This id is used for creating the latch and to look
up its meta-data. See sync0debug.c. The order does not matter here, but
alphabetical ordering seems useful */
enum latch_id_t {
  LATCH_ID_NONE = 0,
  LATCH_ID_AHI_ENABLED,
  LATCH_ID_AUTOINC,
  LATCH_ID_BUF_BLOCK_MUTEX,
  LATCH_ID_BUF_POOL_CHUNKS,
  LATCH_ID_BUF_POOL_ZIP,
  LATCH_ID_BUF_POOL_LRU_LIST,
  LATCH_ID_BUF_POOL_FREE_LIST,
  LATCH_ID_BUF_POOL_ZIP_FREE,
  LATCH_ID_BUF_POOL_ZIP_HASH,
  LATCH_ID_BUF_POOL_FLUSH_STATE,
  LATCH_ID_DBLWR,
  LATCH_ID_DBLWR_SPACE_CACHE,
  LATCH_ID_DDL_AUTOINC,
  LATCH_ID_CACHE_LAST_READ,
  LATCH_ID_DICT_FOREIGN_ERR,
  LATCH_ID_DICT_SYS,
  LATCH_ID_DICT_TABLE,
  LATCH_ID_FIL_SHARD,
  LATCH_ID_FLUSH_LIST,
  LATCH_ID_FTS_BG_THREADS,
  LATCH_ID_FTS_DELETE,
  LATCH_ID_FTS_OPTIMIZE,
  LATCH_ID_FTS_DOC_ID,
  LATCH_ID_FTS_PLL_TOKENIZE,
  LATCH_ID_HASH_TABLE_MUTEX,
  LATCH_ID_IBUF_BITMAP,
  LATCH_ID_IBUF,
  LATCH_ID_IBUF_PESSIMISTIC_INSERT,
  LATCH_ID_LOCK_FREE_HASH,
  LATCH_ID_LOCK_SYS_GLOBAL,
  LATCH_ID_LOCK_SYS_PAGE,
  LATCH_ID_LOCK_SYS_TABLE,
  LATCH_ID_LOCK_SYS_WAIT,
  LATCH_ID_LOG_SN,
  LATCH_ID_LOG_SN_MUTEX,
  LATCH_ID_LOG_CHECKPOINTER,
  LATCH_ID_LOG_CLOSER,
  LATCH_ID_LOG_WRITER,
  LATCH_ID_LOG_FLUSHER,
  LATCH_ID_LOG_WRITE_NOTIFIER,
  LATCH_ID_LOG_FLUSH_NOTIFIER,
  LATCH_ID_LOG_LIMITS,
  LATCH_ID_LOG_FILES,
  LATCH_ID_PARSER,
  LATCH_ID_LOG_ARCH,
  LATCH_ID_PAGE_ARCH,
  LATCH_ID_PAGE_ARCH_OPER,
  LATCH_ID_PAGE_ARCH_CLIENT,
  LATCH_ID_PERSIST_METADATA_BUFFER,
  LATCH_ID_DICT_PERSIST_DIRTY_TABLES,
  LATCH_ID_PERSIST_AUTOINC,
  LATCH_ID_DICT_PERSIST_CHECKPOINT,
  LATCH_ID_PAGE_CLEANER,
  LATCH_ID_PURGE_SYS_PQ,
  LATCH_ID_RECALC_POOL,
  LATCH_ID_RECV_SYS,
  LATCH_ID_RECV_WRITER,
  LATCH_ID_TEMP_SPACE_RSEG,
  LATCH_ID_UNDO_SPACE_RSEG,
  LATCH_ID_TRX_SYS_RSEG,
  LATCH_ID_RW_LOCK_DEBUG,
  LATCH_ID_RTR_SSN_MUTEX,
  LATCH_ID_RTR_ACTIVE_MUTEX,
  LATCH_ID_RTR_MATCH_MUTEX,
  LATCH_ID_RTR_PATH_MUTEX,
  LATCH_ID_RW_LOCK_LIST,
  LATCH_ID_SRV_INNODB_MONITOR,
  LATCH_ID_SRV_MISC_TMPFILE,
  LATCH_ID_SRV_MONITOR_FILE,
  LATCH_ID_SYNC_THREAD,
  LATCH_ID_TRX_UNDO,
  LATCH_ID_TRX_POOL,
  LATCH_ID_TRX_POOL_MANAGER,
  LATCH_ID_TEMP_POOL_MANAGER,
  LATCH_ID_TRX,
  LATCH_ID_TRX_SYS,
  LATCH_ID_TRX_SYS_SHARD,
  LATCH_ID_TRX_SYS_SERIALISATION,
  LATCH_ID_SRV_SYS,
  LATCH_ID_SRV_SYS_TASKS,
  LATCH_ID_PAGE_ZIP_STAT_PER_INDEX,
  LATCH_ID_EVENT_MANAGER,
  LATCH_ID_EVENT_MUTEX,
  LATCH_ID_SYNC_ARRAY_MUTEX,
  LATCH_ID_ZIP_PAD_MUTEX,
  LATCH_ID_OS_AIO_READ_MUTEX,
  LATCH_ID_OS_AIO_WRITE_MUTEX,
  LATCH_ID_OS_AIO_IBUF_MUTEX,
  LATCH_ID_ROW_DROP_LIST,
  LATCH_ID_INDEX_ONLINE_LOG,
  LATCH_ID_WORK_QUEUE,
  LATCH_ID_BTR_SEARCH,
  LATCH_ID_BUF_BLOCK_LOCK,
  LATCH_ID_BUF_BLOCK_DEBUG,
  LATCH_ID_DICT_OPERATION,
  LATCH_ID_CHECKPOINT,
  LATCH_ID_RSEGS,
  LATCH_ID_UNDO_SPACES,
  LATCH_ID_UNDO_DDL,
  LATCH_ID_FIL_SPACE,
  LATCH_ID_FTS_CACHE,
  LATCH_ID_FTS_CACHE_INIT,
  LATCH_ID_TRX_I_S_CACHE,
  LATCH_ID_TRX_PURGE,
  LATCH_ID_IBUF_INDEX_TREE,
  LATCH_ID_INDEX_TREE,
  LATCH_ID_DICT_TABLE_STATS,
  LATCH_ID_HASH_TABLE_RW_LOCK,
  LATCH_ID_BUF_CHUNK_MAP_LATCH,
  LATCH_ID_SYNC_DEBUG_MUTEX,
  LATCH_ID_MASTER_KEY_ID_MUTEX,
  LATCH_ID_FILE_OPEN,
  LATCH_ID_CLONE_SYS,
  LATCH_ID_CLONE_TASK,
  LATCH_ID_CLONE_SNAPSHOT,
  LATCH_ID_PARALLEL_READ,
  LATCH_ID_DBLR,
  LATCH_ID_REDO_LOG_ARCHIVE_ADMIN_MUTEX,
  LATCH_ID_REDO_LOG_ARCHIVE_QUEUE_MUTEX,
  LATCH_ID_TEST_MUTEX,
  LATCH_ID_MAX = LATCH_ID_TEST_MUTEX
};

/** OS mutex, without any policy. It is a thin wrapper around the
system mutexes. The interface is different from the policy mutexes,
to ensure that it is called directly and not confused with the
policy mutexes. */
struct OSMutex {
  /** Constructor */
  OSMutex() UNIV_NOTHROW { ut_d(m_freed = true); }

  /** Create the mutex by calling the system functions. */
  void init() UNIV_NOTHROW {
    ut_ad(m_freed);

#ifdef _WIN32
    InitializeCriticalSection((LPCRITICAL_SECTION)&m_mutex);
#else
    {
      int ret = pthread_mutex_init(&m_mutex, nullptr);
      ut_a(ret == 0);
    }
#endif /* _WIN32 */

    ut_d(m_freed = false);
  }

  /** Destructor */
  ~OSMutex() = default;

  /** Destroy the mutex */
  void destroy() UNIV_NOTHROW {
    ut_ad(innodb_calling_exit || !m_freed);
#ifdef _WIN32
    DeleteCriticalSection((LPCRITICAL_SECTION)&m_mutex);
#else
    int ret;

    ret = pthread_mutex_destroy(&m_mutex);

    if (ret != 0) {
#ifdef UNIV_NO_ERR_MSGS
      ib::error()
#else
      ib::error(ER_IB_MSG_1372)
#endif
          << "Return value " << ret << " when calling pthread_mutex_destroy().";
    }
#endif /* _WIN32 */
    ut_d(m_freed = true);
  }

  /** Release the mutex. */
  void exit() UNIV_NOTHROW {
    ut_ad(innodb_calling_exit || !m_freed);
#ifdef _WIN32
    LeaveCriticalSection(&m_mutex);
#else
    int ret = pthread_mutex_unlock(&m_mutex);
    ut_a(ret == 0);
#endif /* _WIN32 */
  }

  /** Acquire the mutex. */
  void enter() UNIV_NOTHROW {
    ut_ad(innodb_calling_exit || !m_freed);
#ifdef _WIN32
    EnterCriticalSection((LPCRITICAL_SECTION)&m_mutex);
#else
    int ret = pthread_mutex_lock(&m_mutex);
    ut_a(ret == 0);
#endif /* _WIN32 */
  }

  void lock() { enter(); }
  void unlock() { exit(); }

  /** @return true if locking succeeded */
  bool try_lock() UNIV_NOTHROW {
    ut_ad(innodb_calling_exit || !m_freed);
#ifdef _WIN32
    return (TryEnterCriticalSection(&m_mutex) != 0);
#else
    return (pthread_mutex_trylock(&m_mutex) == 0);
#endif /* _WIN32 */
  }

  /** Required for os_event_t */
  operator sys_mutex_t *() UNIV_NOTHROW { return (&m_mutex); }

#if defined(UNIV_LIBRARY) && defined(UNIV_DEBUG)
  bool is_owned() {
    /* This should never be reached. This is
    added to fix is_owned() compilation errors
    for library. We will never reach here because
    mutexes are disabled in library. */
    ut_error;
    return (false);
  }
#endif /* UNIV_LIBRARY && UNIV_DEBUG */

 private:
#ifdef UNIV_DEBUG
  /** true if the mutex has been freed/destroyed. */
  bool m_freed;
#endif /* UNIV_DEBUG */

  sys_mutex_t m_mutex;
};

#ifndef UNIV_LIBRARY
#ifdef UNIV_PFS_MUTEX
/** Latch element
Used for mutexes which have PFS keys defined under UNIV_PFS_MUTEX.
@param[in]      id              Latch id
@param[in]      level           Latch level
@param[in]      key             PFS key */
#define LATCH_ADD_MUTEX(id, level, key)                      \
  latch_meta[LATCH_ID_##id] = ut::new_withkey<latch_meta_t>( \
      UT_NEW_THIS_FILE_PSI_KEY, LATCH_ID_##id, #id, level, #level, key)

#ifdef UNIV_PFS_RWLOCK
/** Latch element.
Used for rwlocks which have PFS keys defined under UNIV_PFS_RWLOCK.
@param[in]      id              Latch id
@param[in]      level           Latch level
@param[in]      key             PFS key */
#define LATCH_ADD_RWLOCK(id, level, key)                     \
  latch_meta[LATCH_ID_##id] = ut::new_withkey<latch_meta_t>( \
      UT_NEW_THIS_FILE_PSI_KEY, LATCH_ID_##id, #id, level, #level, key)
#else
#define LATCH_ADD_RWLOCK(id, level, key)                                     \
  latch_meta[LATCH_ID_##id] =                                                \
      ut::new_withkey<latch_meta_t>(UT_NEW_THIS_FILE_PSI_KEY, LATCH_ID_##id, \
                                    #id, level, #level, PSI_NOT_INSTRUMENTED)
#endif /* UNIV_PFS_RWLOCK */

#else
#define LATCH_ADD_MUTEX(id, level, key)                      \
  latch_meta[LATCH_ID_##id] = ut::new_withkey<latch_meta_t>( \
      UT_NEW_THIS_FILE_PSI_KEY, LATCH_ID_##id, #id, level, #level)
#define LATCH_ADD_RWLOCK(id, level, key)                     \
  latch_meta[LATCH_ID_##id] = ut::new_withkey<latch_meta_t>( \
      UT_NEW_THIS_FILE_PSI_KEY, LATCH_ID_##id, #id, level, #level)
#endif /* UNIV_PFS_MUTEX */

/** Default latch counter */
class LatchCounter {
 public:
  /** The counts we collect for a mutex */
  struct Count {
    /** Constructor */
    Count() UNIV_NOTHROW : m_spins(), m_waits(), m_calls(), m_enabled() {
      /* No op */
    }

    /** Rest the values to zero */
    void reset() UNIV_NOTHROW {
      m_spins = 0;
      m_waits = 0;
      m_calls = 0;
    }

    /** Number of spins trying to acquire the latch. */
    uint32_t m_spins;

    /** Number of waits trying to acquire the latch */
    uint32_t m_waits;

    /** Number of times it was called */
    uint32_t m_calls;

    /** true if enabled */
    bool m_enabled;
  };

  /** Constructor */
  LatchCounter() UNIV_NOTHROW : m_active(false) { m_mutex.init(); }

  /** Destructor */
  ~LatchCounter() UNIV_NOTHROW {
    m_mutex.destroy();

    for (Count *count : m_counters) {
      ut::delete_(count);
    }
  }

  /** Reset all counters to zero. It is not protected by any
  mutex and we don't care about atomicity. Unless it is a
  demonstrated problem. The information collected is not
  required for the correct functioning of the server. */
  void reset() UNIV_NOTHROW {
    m_mutex.enter();

    for (Count *count : m_counters) {
      count->reset();
    }

    m_mutex.exit();
  }

  /** @return the aggregate counter */
  Count *sum_register() UNIV_NOTHROW {
    m_mutex.enter();

    Count *count;

    if (m_counters.empty()) {
      count = ut::new_withkey<Count>(UT_NEW_THIS_FILE_PSI_KEY);
      m_counters.push_back(count);
    } else {
      ut_a(m_counters.size() == 1);
      count = m_counters[0];
    }

    m_mutex.exit();

    return (count);
  }

  /** Deregister the count. We don't do anything
  @param[in]    count           The count instance to deregister */
  void sum_deregister(Count *count
                      [[maybe_unused]]) const UNIV_NOTHROW { /* Do nothing */
  }

  /** Register a single instance counter */
  void single_register(Count *count) UNIV_NOTHROW {
    m_mutex.enter();

    m_counters.push_back(count);

    m_mutex.exit();
  }

  /** Deregister a single instance counter
  @param[in]    count           The count instance to deregister */
  void single_deregister(Count *count) UNIV_NOTHROW {
    m_mutex.enter();

    m_counters.erase(std::remove(m_counters.begin(), m_counters.end(), count),
                     m_counters.end());

    m_mutex.exit();
  }

  /** Iterate over the counters */
  template <typename Callback>
  void iterate(Callback &&callback) const UNIV_NOTHROW {
    m_mutex.enter();
    for (const Count *count : m_counters) {
      std::forward<Callback>(callback)(count);
    }
    m_mutex.exit();
  }

  /** Disable the monitoring */
  void enable() UNIV_NOTHROW {
    m_mutex.enter();

    for (Count *count : m_counters) {
      count->m_enabled = true;
    }

    m_active = true;

    m_mutex.exit();
  }

  /** Disable the monitoring */
  void disable() UNIV_NOTHROW {
    m_mutex.enter();

    for (Count *count : m_counters) {
      count->m_enabled = false;
    }

    m_active = false;

    m_mutex.exit();
  }

  /** @return if monitoring is active */
  bool is_enabled() const UNIV_NOTHROW { return (m_active); }

 private:
  /* Disable copying */
  LatchCounter(const LatchCounter &);
  LatchCounter &operator=(const LatchCounter &);

 private:
  typedef OSMutex Mutex;
  typedef std::vector<Count *> Counters;

  /** Mutex protecting m_counters */
  mutable Mutex m_mutex;

  /** Counters for the latches */
  Counters m_counters;

  /** if true then we collect the data */
  bool m_active;
};

/** Latch meta data */
template <typename Counter = LatchCounter>
class LatchMeta {
 public:
  typedef Counter CounterType;

#ifdef UNIV_PFS_MUTEX
  typedef mysql_pfs_key_t pfs_key_t;
#endif /* UNIV_PFS_MUTEX */

  /** Constructor */
  LatchMeta()
      : m_id(LATCH_ID_NONE),
        m_name(),
        m_level(SYNC_UNKNOWN),
        m_level_name()
#ifdef UNIV_PFS_MUTEX
        ,
        m_pfs_key()
#endif /* UNIV_PFS_MUTEX */
  {
  }

  /** Destructor */
  ~LatchMeta() = default;

  /** Constructor
  @param[in]    id              Latch id
  @param[in]    name            Latch name
  @param[in]    level           Latch level
  @param[in]    level_name      Latch level text representation
  @param[in]    key             PFS key */
  LatchMeta(latch_id_t id, const char *name, latch_level_t level,
            const char *level_name
#ifdef UNIV_PFS_MUTEX
            ,
            pfs_key_t key
#endif /* UNIV_PFS_MUTEX */
            )
      : m_id(id),
        m_name(name),
        m_level(level),
        m_level_name(level_name)
#ifdef UNIV_PFS_MUTEX
        ,
        m_pfs_key(key)
#endif /* UNIV_PFS_MUTEX */
  {
    /* No op */
  }

  /* Less than operator.
  @param[in]    rhs             Instance to compare against
  @return true if this.get_id() < rhs.get_id() */
  bool operator<(const LatchMeta &rhs) const {
    return (get_id() < rhs.get_id());
  }

  /** @return the latch id */
  latch_id_t get_id() const { return (m_id); }

  /** @return the latch name */
  const char *get_name() const { return (m_name); }

  /** @return the latch level */
  latch_level_t get_level() const { return (m_level); }

  /** @return the latch level name */
  const char *get_level_name() const { return (m_level_name); }

#ifdef UNIV_PFS_MUTEX
  /** @return the PFS key for the latch */
  pfs_key_t get_pfs_key() const { return (m_pfs_key); }
#endif /* UNIV_PFS_MUTEX */

  /** @return the counter instance */
  Counter *get_counter() { return (&m_counter); }

 private:
  /** Latch id */
  latch_id_t m_id;

  /** Latch name */
  const char *m_name;

  /** Latch level in the ordering */
  latch_level_t m_level;

  /** Latch level text representation */
  const char *m_level_name;

#ifdef UNIV_PFS_MUTEX
  /** PFS key */
  pfs_key_t m_pfs_key;
#endif /* UNIV_PFS_MUTEX */

  /** For gathering latch statistics */
  Counter m_counter;
};

typedef LatchMeta<LatchCounter> latch_meta_t;
typedef std::vector<latch_meta_t *, ut::allocator<latch_meta_t *>>
    LatchMetaData;

/** Note: This is accessed without any mutex protection. It is initialised
at startup and elements should not be added to or removed from it after
that.  See sync_latch_meta_init() */
extern LatchMetaData latch_meta;

/** Get the latch meta-data from the latch ID
@param[in]      id              Latch ID
@return the latch meta data */
inline latch_meta_t &sync_latch_get_meta(latch_id_t id) {
  ut_ad(static_cast<size_t>(id) < latch_meta.size());
  ut_ad(id == latch_meta[id]->get_id());

  return (*latch_meta[id]);
}

/** Fetch the counter for the latch
@param[in]      id              Latch ID
@return the latch counter */
inline latch_meta_t::CounterType *sync_latch_get_counter(latch_id_t id) {
  latch_meta_t &meta = sync_latch_get_meta(id);

  return (meta.get_counter());
}

/** Get the latch name from the latch ID
@param[in]      id              Latch ID
@return the name, will assert if not found */
inline const char *sync_latch_get_name(latch_id_t id) {
  const latch_meta_t &meta = sync_latch_get_meta(id);

  return (meta.get_name());
}

/** Get the latch ordering level
@param[in]      id              Latch id to lookup
@return the latch level */
inline latch_level_t sync_latch_get_level(latch_id_t id) {
  const latch_meta_t &meta = sync_latch_get_meta(id);

  return (meta.get_level());
}

#ifdef UNIV_PFS_MUTEX
/** Get the latch PFS key from the latch ID
@param[in]      id              Latch ID
@return the PFS key */
inline mysql_pfs_key_t sync_latch_get_pfs_key(latch_id_t id) {
  const latch_meta_t &meta = sync_latch_get_meta(id);

  return (meta.get_pfs_key());
}
#endif /* UNIV_PFS_MUTEX */

#ifndef UNIV_HOTBACKUP
/** String representation of the filename and line number where the
latch was created
@param[in]      id              Latch ID
@param[in]      created         Filename and line number where it was created
@return the string representation */
std::string sync_mutex_to_string(latch_id_t id, const std::string &created);

/** Print the filename "basename"
@return the basename */
const char *sync_basename(const char *filename);
#endif /* !UNIV_HOTBACKUP */

/** Register a latch, called when it is created
@param[in]      ptr             Latch instance that was created
@param[in]      filename        Filename where it was created
@param[in]      line            Line number in filename */
void sync_file_created_register(const void *ptr, const char *filename,
                                uint16_t line);

/** Deregister a latch, called when it is destroyed
@param[in]      ptr             Latch to be destroyed */
void sync_file_created_deregister(const void *ptr);

/** Get the string where the file was created. Its format is "name:line"
@param[in]      ptr             Latch instance
@return created information or "" if can't be found */
std::string sync_file_created_get(const void *ptr);

#endif /* !UNIV_LIBRARY */

#ifdef UNIV_DEBUG

/** All (ordered) latches, used in debugging, must derive from this class. */
struct latch_t {
  /** Constructor
  @param[in]    id      The latch ID */
  explicit latch_t(latch_id_t id = LATCH_ID_NONE) UNIV_NOTHROW : m_id(id),
                                                                 m_rw_lock(),
                                                                 m_temp_fsp() {}

  latch_t &operator=(const latch_t &) = default;

  /** Destructor */
  virtual ~latch_t() UNIV_NOTHROW = default;

  /** @return the latch ID */
  latch_id_t get_id() const { return (m_id); }

  /** @return true if it is a rw-lock */
  bool is_rw_lock() const UNIV_NOTHROW { return (m_rw_lock); }

  /** Print the latch context
  @return the string representation */
  virtual std::string to_string() const = 0;

  /** @return "filename:line" from where the latch was last locked */
  virtual std::string locked_from() const = 0;

  /** @return the latch level */
  latch_level_t get_level() const UNIV_NOTHROW {
    ut_a(m_id != LATCH_ID_NONE);

#ifndef UNIV_LIBRARY
    return (sync_latch_get_level(m_id));
#else
    /* This should never be reached. This is
    added to fix compilation errors
    for library. We will never reach here because
    mutexes are disabled in library. */
    ut_error;
    return (SYNC_UNKNOWN);
#endif /* !UNIV_LIBRARY */
  }

  /** @return true if the latch is for a temporary file space*/
  bool is_temp_fsp() const UNIV_NOTHROW { return (m_temp_fsp); }

  /** Set the temporary tablespace flag. The latch order constraints
  are different for intrinsic tables. We don't always acquire the
  index->lock. We need to figure out the context and add some special
  rules during the checks. */
  void set_temp_fsp() UNIV_NOTHROW {
    ut_ad(get_id() == LATCH_ID_FIL_SPACE);
    m_temp_fsp = true;
  }

  /** @return the latch name, m_id must be set  */
  const char *get_name() const UNIV_NOTHROW {
    ut_a(m_id != LATCH_ID_NONE);

#ifndef UNIV_LIBRARY
    return (sync_latch_get_name(m_id));
#else
    /* This should never be reached. This is
    added to fix compilation errors
    for library. We will never reach here because
    mutexes are disabled in library. */
    ut_error;
    return (nullptr);
#endif /* !UNIV_LIBRARY */
  }

  /** Latch ID */
  latch_id_t m_id;

  /** true if it is a rw-lock. In debug mode, rw_lock_t derives from
  this class and sets this variable. */
  bool m_rw_lock;

  /** true if it is an temporary space latch */
  bool m_temp_fsp;
};

/** Subclass this to iterate over a thread's acquired latch levels. */
struct sync_check_functor_t {
  virtual ~sync_check_functor_t() = default;
  virtual bool operator()(const latch_level_t) = 0;
  virtual bool result() const = 0;
};

/** Functor to check whether the calling thread owns the btr search mutex. */
struct btrsea_sync_check : public sync_check_functor_t {
  /** Constructor
  @param[in]    has_search_latch        true if owns the latch */
  explicit btrsea_sync_check(bool has_search_latch)
      : m_result(), m_has_search_latch(has_search_latch) {}

  /** Destructor */
  ~btrsea_sync_check() override = default;

  /** Called for every latch owned by the calling thread.
  @param[in]    level           Level of the existing latch
  @return true if the predicate check fails */
  bool operator()(const latch_level_t level) override {
    /* If calling thread doesn't hold search latch then
    check if there are latch level exception provided.

    Note: Optimizer has added InnoDB intrinsic table as an
    alternative to MyISAM intrinsic table. With this a new
    control flow comes into existence, it is:

    Server -> Plugin -> SE

    Plugin in this case is I_S which is sharing the latch vector
    of InnoDB and so there could be lock conflicts. Ideally
    the Plugin should use a difference namespace latch vector
    as it doesn't have any dependency with SE latching protocol.

    Added check that will allow thread to hold I_S latches */

    if (!m_has_search_latch &&
        (level != SYNC_SEARCH_SYS && level != SYNC_DICT &&
         level != SYNC_FTS_CACHE && level != SYNC_UNDO_DDL &&
         level != SYNC_DICT_OPERATION && level != SYNC_TRX_I_S_LAST_READ &&
         level != SYNC_TRX_I_S_RWLOCK)) {
      m_result = true;
#ifdef UNIV_NO_ERR_MSGS
      ib::error()
#else
      ib::error(ER_IB_MSG_1373)
#endif
          << "Debug: Calling thread does not hold search "
             "latch but does hold latch level "
          << level << ".";

      return (m_result);
    }

    return (false);
  }

  /** @return result from the check */
  bool result() const override { return (m_result); }

 private:
  /** True if all OK */
  bool m_result;

  /** If the caller owns the search latch */
  const bool m_has_search_latch;
};

/** Functor to check for dictionary latching constraints. */
struct dict_sync_check : public sync_check_functor_t {
  /** Constructor
  @param[in]    dict_mutex_allowed      true if the dict mutex
                                          is allowed */
  explicit dict_sync_check(bool dict_mutex_allowed)
      : m_result(), m_dict_mutex_allowed(dict_mutex_allowed) {}

  /** Destructor */
  ~dict_sync_check() override = default;

  /** Check the latching constraints
  @param[in]    level           The level held by the thread */
  bool operator()(const latch_level_t level) override {
    if (!m_dict_mutex_allowed ||
        (level != SYNC_DICT && level != SYNC_UNDO_SPACES &&
         level != SYNC_FTS_CACHE && level != SYNC_DICT_OPERATION &&
         /* This only happens in recv_apply_hashed_log_recs. */
         level != SYNC_RECV_WRITER && level != SYNC_NO_ORDER_CHECK)) {
      m_result = true;
#ifdef UNIV_NO_ERR_MSGS
      ib::error()
#else
      ib::error(ER_IB_MSG_1374)
#endif
          << "Debug: Dictionary latch order violation for level " << level
          << ".";

      return (true);
    }

    return (false);
  }

  /** @return the result of the check */
  virtual bool result() const override { return (m_result); }

 private:
  /** True if all OK */
  bool m_result;

  /** True if it is OK to hold the dict mutex */
  const bool m_dict_mutex_allowed;
};

/** Functor to check for given latching constraints. */
struct sync_allowed_latches : public sync_check_functor_t {
  /** Constructor
  @param[in]    from    first element in an array of latch_level_t
  @param[in]    to      last element in an array of latch_level_t */
  sync_allowed_latches(const latch_level_t *from, const latch_level_t *to)
      : m_result(), m_latches(from, to) {}

  /** Default constructor.  The list of allowed latches is empty. */
  sync_allowed_latches() : m_result(), m_latches() {}

  /** Check whether the given latch_t violates the latch constraint.
  This object maintains a list of allowed latch levels, and if the given
  latch belongs to a latch level that is not there in the allowed list,
  then it is a violation.

  @param[in]    level   The latch level to check
  @return true if there is a latch ordering violation */
  virtual bool operator()(const latch_level_t level) override {
    for (latch_level_t allowed_level : m_latches) {
      if (level == allowed_level) {
        m_result = false;

        /* No violation */
        return (m_result);
      }
    }

#ifdef UNIV_NO_ERR_MSGS
    ib::error()
#else
    ib::error(ER_IB_MSG_1375)
#endif
        << "Debug: sync_allowed_latches violation for level=" << level;
    m_result = true;
    return (m_result);
  }

  /** @return the result of the check */
  virtual bool result() const override { return (m_result); }

 private:
  /** Save the result of validation check here
  True if all OK */
  bool m_result;

  typedef std::vector<latch_level_t, ut::allocator<latch_level_t>> latches_t;

  /** List of latch levels that are allowed to be held */
  latches_t m_latches;
};

typedef ulint rw_lock_flags_t;

/* Flags to specify lock types for rw_lock_own_flagged() */
enum rw_lock_flag_t {
  RW_LOCK_FLAG_S = 1 << 0,
  RW_LOCK_FLAG_X = 1 << 1,
  RW_LOCK_FLAG_SX = 1 << 2
};

#endif /* UNIV_DBEUG */

#endif /* sync0types_h */
