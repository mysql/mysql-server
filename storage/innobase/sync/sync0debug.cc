/*****************************************************************************

Copyright (c) 2012, 2022, Oracle and/or its affiliates.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

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

/** @file sync/sync0debug.cc
 Debug checks for latches.

 Created 2012-08-21 Sunny Bains
 *******************************************************/

#include "sync0debug.h"

#include <stddef.h>
#include <algorithm>
#include <atomic>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "sync0rw.h"
#include "ut0mutex.h"

#include <scope_guard.h>

#ifndef UNIV_NO_ERR_MSGS
#include "srv0start.h"
#endif /* !UNIV_NO_ERR_MSGS */

#include "ut0new.h"

#ifdef UNIV_DEBUG
#ifndef UNIV_NO_ERR_MSGS
#include <current_thd.h>
#endif /* !UNIV_NO_ERR_MSGS */

bool srv_sync_debug;

/** The global mutex which protects debug info lists of all rw-locks.
To modify the debug info list of an rw-lock, this mutex has to be
acquired in addition to the mutex protecting the lock. */
static ib_mutex_t rw_lock_debug_mutex;

/** If deadlock detection does not get immediately the mutex,
it may wait for this event */
static os_event_t rw_lock_debug_event;

/** This is set to true, if there may be waiters for the event */
static std::atomic<bool> rw_lock_debug_waiters{false};

/** The latch held by a thread */
struct Latched {
  /** Constructor */
  Latched() : m_latch(), m_level(SYNC_UNKNOWN) {}

  /** Constructor
  @param[in]    latch           Latch instance
  @param[in]    level           Level of latch held */
  Latched(const latch_t *latch, latch_level_t level)
      : m_latch(latch), m_level(level) {
    /* No op */
  }

  /** @return the latch level */
  latch_level_t get_level() const { return (m_level); }

  /** Check if the rhs latch and level match
  @param[in]    rhs             instance to compare with
  @return true on match */
  bool operator==(const Latched &rhs) const {
    return (m_latch == rhs.m_latch && m_level == rhs.m_level);
  }

  /** The latch instance */
  const latch_t *m_latch;

  /** The latch level. For buffer blocks we can pass a separate latch
  level to check against, see buf_block_dbg_add_level() */
  latch_level_t m_level;
};

/** Thread specific latches. This is ordered on level in descending order. */
typedef std::vector<Latched, ut::allocator<Latched>> Latches;

/** The deadlock detector. */
struct LatchDebug {
  /** Debug mutex for control structures, should not be tracked
  by this module. */
  typedef OSMutex Mutex;

  /** For tracking a thread's latches. */
  typedef std::map<std::thread::id, Latches *, std::less<std::thread::id>,
                   ut::allocator<std::pair<const std::thread::id, Latches *>>>
      ThreadMap;

  /** Constructor */
  LatchDebug() UNIV_NOTHROW;

  /** Destructor */
  ~LatchDebug() UNIV_NOTHROW { m_mutex.destroy(); }

  /** Create a new instance if one doesn't exist else return
  the existing one.
  @param[in]    add             add an empty entry if one is not
                                  found (default no)
  @return       pointer to a thread's acquired latches. */
  Latches *thread_latches(bool add = false) UNIV_NOTHROW;

  /** Check that all the latches already owned by a thread have a higher
  level than limit and returns the latch which violates this expectation if any.
  @param[in]    latches         the thread's existing (acquired) latches
  @param[in]    limit           to check against
  @return latched if there is one with a level <= limit . */
  const Latched *find_lower_or_equal(const Latches *latches,
                                     latch_level_t limit) const UNIV_NOTHROW;

  /** Checks if the level value exists in the thread's acquired latches.
  @param[in]    latches         the thread's existing (acquired) latches
  @param[in]    level           to lookup
  @return       latch if found or 0 */
  const latch_t *find(const Latches *latches,
                      latch_level_t level) const UNIV_NOTHROW;

  /** Checks if the level value exists in the thread's acquired latches.
  @param[in]    level           The level to lookup
  @return       latch if found or NULL */
  const latch_t *find(latch_level_t level) UNIV_NOTHROW;

  /** Report error and abort.
  @param[in]    latches         thread's existing latches
  @param[in]    latched         The existing latch causing the
                                  invariant to fail
  @param[in]    level           The new level request that breaks
                                  the order */
  [[noreturn]] void crash(const Latches *latches, const Latched *latched,
                          latch_level_t level) const UNIV_NOTHROW;

  /** Do a basic ordering check. Asserts that all the existing latches have a
  level higher than the forbidden_level.
  @param[in]    latches         thread's existing latches
  @param[in]    requested_level
  the level of the requested latch, used in error message only, to provide the
  context of the check to the user. For actual comparisons in asserts the value
  of forbidden_level will be used.
  @param[in]    forbidden_level
  Exclusive lower bound on levels of currently held latches - i.e. each latch in
  @p latches must have level strictly grater than @p forbidden_level. Usually,
  @p forbidden_level equals @p requested_level, but if we allow a thread to hold
  more than one latch at @p requested_level (avoiding deadlock between them in
  some other way), then it will be @p requested_level - 1. It can have a
  value other than these two, in rare cases, where deadlock avoidance rules are
  even more complicated, but this should be avoided.
  */
  void assert_all_held_are_above(const Latches *latches,
                                 latch_level_t requested_level,
                                 ulint forbidden_level) const UNIV_NOTHROW;

  /** Asserts that all the latches already acquired by the thread have a
  level higher than the newly requested latch. This is the most typical latching
  order rule ensuring no deadlock cycle: strictly descending sequence can not
  have a loop.
  @param[in]    latches           thread's existing latches
  @param[in]    requested_level   the level of the requested latch
  */
  void assert_requested_is_lower_than_held(latch_level_t requested_level,
                                           const Latches *latches) const
      UNIV_NOTHROW {
    assert_all_held_are_above(latches, requested_level, requested_level);
  }

  /** Asserts that all the latches already acquired by the thread have a
  level higher or equal to the newly requested latch. This is a rule used for
  latches which can have multiple instances and a thread is allowed to latch
  more than one such instance, when we can somehow prove there's no deadlock due
  to threads requesting more than one. It's the responsibility of the developer
  to document and prove this additional property, ideally encoding it in
  LatchDebug::check_order. One example of such case would be when we know that
  to latch more than one instance the thread must first acquire exclusive right
  to another singleton latch. Another example would be if we always follow some
  natural ordering on the latches of this kind, like by increasing address in an
  array.
  @param[in]    latches           thread's existing latches
  @param[in]    requested_level   the level of the requested latch
  */
  void assert_requested_is_lower_or_equal_to_held(latch_level_t requested_level,
                                                  const Latches *latches) const
      UNIV_NOTHROW {
    assert_all_held_are_above(latches, requested_level, requested_level - 1);
  }

  /** Adds a latch and its level in the thread level array. Allocates
  the memory for the array if called for the first time for this
  OS thread.  Makes the checks against other latch levels stored
  in the array for this thread.

  @param[in]    latch   latch that the thread wants to acqire.
  @param[in]    level   latch level to check against */
  void lock_validate(const latch_t *latch, latch_level_t level) UNIV_NOTHROW {
    /* Ignore diagnostic latches, starting with '.' */

    if (*latch->get_name() != '.' && latch->get_level() != SYNC_LEVEL_VARYING) {
      ut_ad(level != SYNC_LEVEL_VARYING);

      Latches *latches = check_order(latch, level);

      if (!(latches->empty() || level == SYNC_LEVEL_VARYING ||
            level == SYNC_NO_ORDER_CHECK ||
            latches->back().get_level() == SYNC_NO_ORDER_CHECK ||
            latches->back().m_latch->get_level() == SYNC_LEVEL_VARYING ||
            latches->back().get_level() >= level)) {
        const auto latest_latch_level = latches->back().m_latch->get_level();
        const auto latest_level = latches->back().get_level();

#ifdef UNIV_NO_ERR_MSGS
        ib::error()
#else
        ib::error(ER_IB_LOCK_VALIDATE_LATCH_ORDER_VIOLATION)
#endif
            << "LatchDebug::lock_validate() latch order violation. level="
            << level << ", latest_latch_level=" << latest_latch_level
            << ", latest_level=" << latest_level << ".";
        ut_error;
      }
    }
  }

  /** Adds a latch and its level in the thread level array. Allocates
  the memory for the array if called for the first time for this
  OS thread.  Makes the checks against other latch levels stored
  in the array for this thread.

  @param[in]    latch   latch that the thread wants to acquire.
  @param[in]    level   latch level to check against */
  void lock_granted(const latch_t *latch, latch_level_t level) UNIV_NOTHROW {
    /* Ignore diagnostic latches, starting with '.' */

    if (*latch->get_name() != '.' && latch->get_level() != SYNC_LEVEL_VARYING) {
      Latches *latches = thread_latches(true);

      latches->push_back(Latched(latch, level));
    }
  }

  /** For recursive X rw-locks.
  @param[in]    latch           The RW-Lock to relock  */
  void relock(const latch_t *latch) UNIV_NOTHROW {
    ut_a(latch->m_rw_lock);

    latch_level_t level = latch->get_level();

    /* Ignore diagnostic latches, starting with '.' */

    if (*latch->get_name() != '.' && latch->get_level() != SYNC_LEVEL_VARYING) {
      Latches *latches = thread_latches(true);

      Latches::iterator it =
          std::find(latches->begin(), latches->end(), Latched(latch, level));

      if (!(latches->empty() || level == SYNC_LEVEL_VARYING ||
            level == SYNC_NO_ORDER_CHECK ||
            latches->back().m_latch->get_level() == SYNC_LEVEL_VARYING ||
            latches->back().m_latch->get_level() == SYNC_NO_ORDER_CHECK ||
            latches->back().get_level() >= level || it != latches->end())) {
        const auto latest_latch_level = latches->back().m_latch->get_level();
        const auto latest_level = latches->back().get_level();

#ifdef UNIV_NO_ERR_MSGS
        ib::error()
#else
        ib::error(ER_IB_RELOCK_LATCH_ORDER_VIOLATION)
#endif
            << "LatchDebug::relock() latch order violation. level=" << level
            << ", latest_latch_level=" << latest_latch_level
            << ", latest_level=" << latest_level << ".";
        ut_error;
      }

      if (it == latches->end()) {
        latches->push_back(Latched(latch, level));
      } else {
        latches->insert(it, Latched(latch, level));
      }
    }
  }

  /** Iterate over a thread's latches.
  @param[in,out]        functor         The callback
  @return true if the functor returns true. */
  bool for_each(sync_check_functor_t &functor) UNIV_NOTHROW {
    const Latches *latches = thread_latches();

    if (latches == nullptr) {
      return (functor.result());
    }

    Latches::const_iterator end = latches->end();

    for (Latches::const_iterator it = latches->begin(); it != end; ++it) {
      if (functor(it->m_level)) {
        break;
      }
    }

    return (functor.result());
  }

  /** Removes a latch from the thread level array if it is found there.
  @param[in]    latch           The latch that was released */
  void unlock(const latch_t *latch) UNIV_NOTHROW;

  /** Get the level name
  @param[in]    level           The level ID to lookup
  @return level name */
  const std::string &get_level_name(latch_level_t level) const UNIV_NOTHROW {
    Levels::const_iterator it = m_levels.find(level);

    ut_ad(it != m_levels.end());

    return (it->second);
  }

  /** Initialise the debug data structures */
  static void init() UNIV_NOTHROW;

  /** Shutdown the latch debug checking */
  static void shutdown() UNIV_NOTHROW;

  /** @return the singleton instance */
  static LatchDebug *instance() UNIV_NOTHROW { return (s_instance); }

  /** Create the singleton instance */
  static void create_instance() UNIV_NOTHROW {
    ut_ad(s_instance == nullptr);

    s_instance = ut::new_withkey<LatchDebug>(UT_NEW_THIS_FILE_PSI_KEY);
  }

 private:
  /** Disable copying */
  LatchDebug(const LatchDebug &);
  LatchDebug &operator=(const LatchDebug &);

  /** Adds a latch and its level in the thread level array. Allocates
  the memory for the array if called first time for this OS thread.
  Makes the checks against other latch levels stored in the array
  for this thread.

  @param[in]    latch    pointer to a mutex or an rw-lock
  @param[in]    level   level in the latching order
  @return the thread's latches */
  Latches *check_order(const latch_t *latch, latch_level_t level) UNIV_NOTHROW;

  /** Print the latches acquired by a thread
  @param[in]    latches         Latches acquired by a thread */
  void print_latches(const Latches *latches) const UNIV_NOTHROW;

  /** Special handling for the RTR mutexes. We need to add proper
  levels for them if possible.
  @param[in]    latch           Latch to check
  @return true if it is a an _RTR_ mutex */
  bool is_rtr_mutex(const latch_t *latch) const UNIV_NOTHROW {
    return (latch->get_id() == LATCH_ID_RTR_ACTIVE_MUTEX ||
            latch->get_id() == LATCH_ID_RTR_PATH_MUTEX ||
            latch->get_id() == LATCH_ID_RTR_MATCH_MUTEX ||
            latch->get_id() == LATCH_ID_RTR_SSN_MUTEX);
  }

 private:
  /** Comparator for the Levels . */
  struct latch_level_less {
    /** @return true if lhs < rhs */
    bool operator()(const latch_level_t &lhs,
                    const latch_level_t &rhs) const UNIV_NOTHROW {
      return (lhs < rhs);
    }
  };

  typedef std::map<latch_level_t, std::string, latch_level_less,
                   ut::allocator<std::pair<const latch_level_t, std::string>>>
      Levels;

  /** Mutex protecting the deadlock detector data structures. */
  Mutex m_mutex;

  /** Thread specific data. Protected by m_mutex. */
  ThreadMap m_threads;

  /** Mapping from latche level to its string representation. */
  Levels m_levels;

  /** The singleton instance. Must be created in single threaded mode. */
  static LatchDebug *s_instance;

 public:
  /** For checking whether this module has been initialised or not. */
  static bool s_initialized;
};

/** The latch order checking infra-structure */
LatchDebug *LatchDebug::s_instance = nullptr;
bool LatchDebug::s_initialized = false;

#define LEVEL_MAP_INSERT(T)                         \
  do {                                              \
    std::pair<Levels::iterator, bool> result =      \
        m_levels.insert(Levels::value_type(T, #T)); \
    ut_ad(result.second);                           \
  } while (0)

/** Setup the mapping from level ID to level name mapping */
LatchDebug::LatchDebug() {
  m_mutex.init();

  LEVEL_MAP_INSERT(SYNC_UNKNOWN);
  LEVEL_MAP_INSERT(SYNC_MUTEX);
  LEVEL_MAP_INSERT(RW_LOCK_SX);
  LEVEL_MAP_INSERT(RW_LOCK_X_WAIT);
  LEVEL_MAP_INSERT(RW_LOCK_S);
  LEVEL_MAP_INSERT(RW_LOCK_X);
  LEVEL_MAP_INSERT(RW_LOCK_NOT_LOCKED);
  LEVEL_MAP_INSERT(SYNC_LOCK_FREE_HASH);
  LEVEL_MAP_INSERT(SYNC_MONITOR_MUTEX);
  LEVEL_MAP_INSERT(SYNC_ANY_LATCH);
  LEVEL_MAP_INSERT(SYNC_FIL_SHARD);
  LEVEL_MAP_INSERT(SYNC_DBLWR);
  LEVEL_MAP_INSERT(SYNC_BUF_CHUNKS);
  LEVEL_MAP_INSERT(SYNC_BUF_FLUSH_LIST);
  LEVEL_MAP_INSERT(SYNC_BUF_FLUSH_STATE);
  LEVEL_MAP_INSERT(SYNC_BUF_ZIP_HASH);
  LEVEL_MAP_INSERT(SYNC_BUF_FREE_LIST);
  LEVEL_MAP_INSERT(SYNC_BUF_ZIP_FREE);
  LEVEL_MAP_INSERT(SYNC_BUF_BLOCK);
  LEVEL_MAP_INSERT(SYNC_BUF_PAGE_HASH);
  LEVEL_MAP_INSERT(SYNC_BUF_LRU_LIST);
  LEVEL_MAP_INSERT(SYNC_POOL);
  LEVEL_MAP_INSERT(SYNC_POOL_MANAGER);
  LEVEL_MAP_INSERT(SYNC_TEMP_POOL_MANAGER);
  LEVEL_MAP_INSERT(SYNC_SEARCH_SYS);
  LEVEL_MAP_INSERT(SYNC_WORK_QUEUE);
  LEVEL_MAP_INSERT(SYNC_FTS_TOKENIZE);
  LEVEL_MAP_INSERT(SYNC_FTS_OPTIMIZE);
  LEVEL_MAP_INSERT(SYNC_FTS_BG_THREADS);
  LEVEL_MAP_INSERT(SYNC_FTS_CACHE_INIT);
  LEVEL_MAP_INSERT(SYNC_RECV);
  LEVEL_MAP_INSERT(SYNC_RECV_WRITER);
  LEVEL_MAP_INSERT(SYNC_LOG_SN);
  LEVEL_MAP_INSERT(SYNC_LOG_SN_MUTEX);
  LEVEL_MAP_INSERT(SYNC_LOG_LIMITS);
  LEVEL_MAP_INSERT(SYNC_LOG_FLUSHER);
  LEVEL_MAP_INSERT(SYNC_LOG_FILES);
  LEVEL_MAP_INSERT(SYNC_LOG_WRITER);
  LEVEL_MAP_INSERT(SYNC_LOG_WRITE_NOTIFIER);
  LEVEL_MAP_INSERT(SYNC_LOG_FLUSH_NOTIFIER);
  LEVEL_MAP_INSERT(SYNC_LOG_CLOSER);
  LEVEL_MAP_INSERT(SYNC_LOG_CHECKPOINTER);
  LEVEL_MAP_INSERT(SYNC_LOG_ARCH);
  LEVEL_MAP_INSERT(SYNC_PAGE_ARCH);
  LEVEL_MAP_INSERT(SYNC_PAGE_ARCH_OPER);
  LEVEL_MAP_INSERT(SYNC_PAGE_ARCH_CLIENT);
  LEVEL_MAP_INSERT(SYNC_PAGE_CLEANER);
  LEVEL_MAP_INSERT(SYNC_PURGE_QUEUE);
  LEVEL_MAP_INSERT(SYNC_TRX_SYS_HEADER);
  LEVEL_MAP_INSERT(SYNC_THREADS);
  LEVEL_MAP_INSERT(SYNC_TRX);
  LEVEL_MAP_INSERT(SYNC_TRX_SYS);
  LEVEL_MAP_INSERT(SYNC_TRX_SYS_SHARD);
  LEVEL_MAP_INSERT(SYNC_TRX_SYS_SERIALISATION);
  LEVEL_MAP_INSERT(SYNC_LOCK_SYS_GLOBAL);
  LEVEL_MAP_INSERT(SYNC_LOCK_SYS_SHARDED);
  LEVEL_MAP_INSERT(SYNC_LOCK_WAIT_SYS);
  LEVEL_MAP_INSERT(SYNC_INDEX_ONLINE_LOG);
  LEVEL_MAP_INSERT(SYNC_IBUF_BITMAP);
  LEVEL_MAP_INSERT(SYNC_IBUF_BITMAP_MUTEX);
  LEVEL_MAP_INSERT(SYNC_IBUF_TREE_NODE);
  LEVEL_MAP_INSERT(SYNC_IBUF_TREE_NODE_NEW);
  LEVEL_MAP_INSERT(SYNC_IBUF_INDEX_TREE);
  LEVEL_MAP_INSERT(SYNC_IBUF_MUTEX);
  LEVEL_MAP_INSERT(SYNC_FSP_PAGE);
  LEVEL_MAP_INSERT(SYNC_FSP);
  LEVEL_MAP_INSERT(SYNC_EXTERN_STORAGE);
  LEVEL_MAP_INSERT(SYNC_RSEG_ARRAY_HEADER);
  LEVEL_MAP_INSERT(SYNC_TRX_UNDO_PAGE);
  LEVEL_MAP_INSERT(SYNC_RSEG_HEADER);
  LEVEL_MAP_INSERT(SYNC_RSEG_HEADER_NEW);
  LEVEL_MAP_INSERT(SYNC_TEMP_SPACE_RSEG);
  LEVEL_MAP_INSERT(SYNC_UNDO_SPACE_RSEG);
  LEVEL_MAP_INSERT(SYNC_TRX_SYS_RSEG);
  LEVEL_MAP_INSERT(SYNC_RSEGS);
  LEVEL_MAP_INSERT(SYNC_UNDO_SPACES);
  LEVEL_MAP_INSERT(SYNC_UNDO_DDL);
  LEVEL_MAP_INSERT(SYNC_TRX_UNDO);
  LEVEL_MAP_INSERT(SYNC_PURGE_LATCH);
  LEVEL_MAP_INSERT(SYNC_TREE_NODE);
  LEVEL_MAP_INSERT(SYNC_TREE_NODE_FROM_HASH);
  LEVEL_MAP_INSERT(SYNC_TREE_NODE_NEW);
  LEVEL_MAP_INSERT(SYNC_INDEX_TREE);
  LEVEL_MAP_INSERT(SYNC_PERSIST_DIRTY_TABLES);
  LEVEL_MAP_INSERT(SYNC_PERSIST_AUTOINC);
  LEVEL_MAP_INSERT(SYNC_IBUF_PESS_INSERT_MUTEX);
  LEVEL_MAP_INSERT(SYNC_IBUF_HEADER);
  LEVEL_MAP_INSERT(SYNC_DICT_HEADER);
  LEVEL_MAP_INSERT(SYNC_TABLE);
  LEVEL_MAP_INSERT(SYNC_STATS_AUTO_RECALC);
  LEVEL_MAP_INSERT(SYNC_DICT_AUTOINC_MUTEX);
  LEVEL_MAP_INSERT(SYNC_DICT);
  LEVEL_MAP_INSERT(SYNC_AHI_ENABLED);
  LEVEL_MAP_INSERT(SYNC_PARSER);
  LEVEL_MAP_INSERT(SYNC_FTS_CACHE);
  LEVEL_MAP_INSERT(SYNC_DICT_OPERATION);
  LEVEL_MAP_INSERT(SYNC_TRX_I_S_LAST_READ);
  LEVEL_MAP_INSERT(SYNC_TRX_I_S_RWLOCK);
  LEVEL_MAP_INSERT(SYNC_LEVEL_VARYING);
  LEVEL_MAP_INSERT(SYNC_NO_ORDER_CHECK);

  /* Enum count starts from 0 */
  ut_ad(m_levels.size() == SYNC_LEVEL_MAX + 1);
}

/** Print the latches acquired by a thread
@param[in]      latches         Latches acquired by a thread */
void LatchDebug::print_latches(const Latches *latches) const UNIV_NOTHROW {
#ifdef UNIV_NO_ERR_MSGS
  ib::error()
#else
  ib::error(ER_IB_MSG_1161)
#endif /* UNIV_NO_ERR_MSGS */
      << "Latches already owned by this thread: ";

  Latches::const_iterator end = latches->end();

  for (Latches::const_iterator it = latches->begin(); it != end; ++it) {
#ifdef UNIV_NO_ERR_MSGS
    ib::error()
#else
    ib::error(ER_IB_MSG_1162)
#endif /* UNIV_NO_ERR_MSGS */
        << sync_latch_get_name(it->m_latch->get_id()) << " -> " << it->m_level
        << " "
        << "(" << get_level_name(it->m_level) << ")";
  }
}

/** Report error and abort
@param[in]      latches         thread's existing latches
@param[in]      latched         The existing latch causing the invariant to fail
@param[in]      level           The new level request that breaks the order */
void LatchDebug::crash(const Latches *latches, const Latched *latched,
                       latch_level_t level) const UNIV_NOTHROW {
  const latch_t *latch = latched->m_latch;
  const std::string &in_level_name = get_level_name(level);

  const std::string &latch_level_name = get_level_name(latched->m_level);

#ifdef UNIV_NO_ERR_MSGS
  ib::error()
#else
  ib::error(ER_IB_MSG_1163)
#endif /* UNIV_NO_ERR_MSGS */
      << "Thread " << to_string(std::this_thread::get_id())
      << " already owns a latch " << sync_latch_get_name(latch->m_id)
      << " at level"
      << " " << latched->m_level << " (" << latch_level_name
      << " ), which is at a lower/same level than the"
      << " requested latch: " << level << " (" << in_level_name << "). "
      << latch->to_string();

  print_latches(latches);

  ut_error;
}

const Latched *LatchDebug::find_lower_or_equal(
    const Latches *latches, latch_level_t limit) const UNIV_NOTHROW {
  Latches::const_iterator end = latches->end();

  for (Latches::const_iterator it = latches->begin(); it != end; ++it) {
    if (it->m_level <= limit) {
      return (&(*it));
    }
  }

  return (nullptr);
}

void LatchDebug::assert_all_held_are_above(
    const Latches *latches, latch_level_t requested_level,
    ulint forbidden_level) const UNIV_NOTHROW {
  latch_level_t level = latch_level_t(forbidden_level);

  ut_ad(level < SYNC_LEVEL_MAX);

  const Latched *latched = find_lower_or_equal(latches, level);

  if (latched != nullptr) {
    crash(latches, latched, requested_level);
  }
}

/** Create a new instance if one doesn't exist else return the existing one.
@param[in]      add             add an empty entry if one is not found
                                (default no)
@return pointer to a thread's acquired latches. */
Latches *LatchDebug::thread_latches(bool add) UNIV_NOTHROW {
  m_mutex.enter();
  auto mutex_guard = create_scope_guard([this]() { m_mutex.exit(); });

  auto thread_id = std::this_thread::get_id();
  ThreadMap::iterator lb = m_threads.lower_bound(thread_id);

  if (lb != m_threads.end() && !(m_threads.key_comp()(thread_id, lb->first))) {
    Latches *latches = lb->second;

    return (latches);

  } else if (!add) {
    return (nullptr);

  } else {
    typedef ThreadMap::value_type value_type;

    Latches *latches = ut::new_withkey<Latches>(UT_NEW_THIS_FILE_PSI_KEY);

    ut_a(latches != nullptr);

    latches->reserve(32);

    m_threads.insert(lb, value_type(thread_id, latches));

    return (latches);
  }
}

/** Checks if the level value exists in the thread's acquired latches.
@param[in]      latches         the thread's existing (acquired) latches
@param[in]      level           to lookup
@return latch if found or 0 */
const latch_t *LatchDebug::find(const Latches *latches,
                                latch_level_t level) const UNIV_NOTHROW {
  Latches::const_iterator end = latches->end();

  for (Latches::const_iterator it = latches->begin(); it != end; ++it) {
    if (it->m_level == level) {
      return (it->m_latch);
    }
  }

  return (nullptr);
}

/** Checks if the level value exists in the thread's acquired latches.
@param[in]      level           The level to lookup
@return latch if found or NULL */
const latch_t *LatchDebug::find(latch_level_t level) UNIV_NOTHROW {
  return (find(thread_latches(), level));
}

/**
Adds a latch and its level in the thread level array. Allocates the memory
for the array if called first time for this OS thread. Makes the checks
against other latch levels stored in the array for this thread.
@param[in]      latch   pointer to a mutex or an rw-lock
@param[in]      level   level in the latching order
@return the thread's latches */
Latches *LatchDebug::check_order(const latch_t *latch,
                                 latch_level_t level) UNIV_NOTHROW {
  ut_ad(latch->get_level() != SYNC_LEVEL_VARYING);

  Latches *latches = thread_latches(true);

  /* NOTE that there is a problem with _NODE and _LEAF levels: if the
  B-tree height changes, then a leaf can change to an internal node
  or the other way around. We do not know at present if this can cause
  unnecessary assertion failures below. */

  switch (level) {
    case SYNC_NO_ORDER_CHECK:
    case SYNC_EXTERN_STORAGE:
    case SYNC_TREE_NODE_FROM_HASH:
      /* Do no order checking */
      break;

    case SYNC_LOG_SN:
    case SYNC_LOG_SN_MUTEX:
    case SYNC_TRX_SYS_HEADER:
    case SYNC_LOCK_FREE_HASH:
    case SYNC_MONITOR_MUTEX:
    case SYNC_RECV:
    case SYNC_RECV_WRITER:
    case SYNC_FTS_BG_THREADS:
    case SYNC_WORK_QUEUE:
    case SYNC_FTS_TOKENIZE:
    case SYNC_FTS_OPTIMIZE:
    case SYNC_FTS_CACHE:
    case SYNC_FTS_CACHE_INIT:
    case SYNC_PAGE_CLEANER:
    case SYNC_LOG_CHECKPOINTER:
    case SYNC_LOG_CLOSER:
    case SYNC_LOG_WRITER:
    case SYNC_LOG_FLUSHER:
    case SYNC_LOG_WRITE_NOTIFIER:
    case SYNC_LOG_FLUSH_NOTIFIER:
    case SYNC_LOG_LIMITS:
    case SYNC_LOG_FILES:
    case SYNC_LOG_ARCH:
    case SYNC_PAGE_ARCH:
    case SYNC_PAGE_ARCH_OPER:
    case SYNC_PAGE_ARCH_CLIENT:
    case SYNC_THREADS:
    case SYNC_LOCK_WAIT_SYS:
    case SYNC_TRX_SYS:
    case SYNC_TRX_SYS_SHARD:
    case SYNC_TRX_SYS_SERIALISATION:
    case SYNC_IBUF_BITMAP_MUTEX:
    case SYNC_TEMP_SPACE_RSEG:
    case SYNC_UNDO_SPACE_RSEG:
    case SYNC_TRX_SYS_RSEG:
    case SYNC_RSEGS:
    case SYNC_UNDO_SPACES:
    case SYNC_UNDO_DDL:
    case SYNC_TRX_UNDO:
    case SYNC_PURGE_LATCH:
    case SYNC_PURGE_QUEUE:
    case SYNC_DICT_AUTOINC_MUTEX:
    case SYNC_DICT_OPERATION:
    case SYNC_DICT_HEADER:
    case SYNC_TABLE:
    case SYNC_TRX_I_S_RWLOCK:
    case SYNC_TRX_I_S_LAST_READ:
    case SYNC_IBUF_MUTEX:
    case SYNC_INDEX_ONLINE_LOG:
    case SYNC_STATS_AUTO_RECALC:
    case SYNC_POOL:
    case SYNC_POOL_MANAGER:
    case SYNC_TEMP_POOL_MANAGER:
    case SYNC_PARSER:
    case SYNC_DICT:
    case SYNC_AHI_ENABLED:

      /* This is the most typical case, in which we expect requested<held. */
      assert_requested_is_lower_than_held(level, latches);
      break;

    case SYNC_ANY_LATCH:

      /* Temporary workaround for LATCH_ID_RTR_*_MUTEX */
      if (is_rtr_mutex(latch)) {
        const Latched *latched = find_lower_or_equal(latches, level);

        if (latched != nullptr && !is_rtr_mutex(latched->m_latch)) {
          crash(latches, latched, level);
        }

      } else {
        assert_requested_is_lower_than_held(level, latches);
      }

      break;

    case SYNC_TRX:

      /* Either the thread must own the lock_sys global latch, or
      it is allowed to own only ONE trx_t::mutex. There are additional rules
      for holding more than one trx_t::mutex @see trx_before_mutex_enter(). */

      if (find_lower_or_equal(latches, level) != nullptr) {
        assert_requested_is_lower_or_equal_to_held(level, latches);
        ut_a(find(latches, SYNC_LOCK_SYS_GLOBAL) != nullptr);
      }
      break;

    case SYNC_FIL_SHARD:
    case SYNC_DBLWR:
    case SYNC_BUF_CHUNKS:
    case SYNC_BUF_FLUSH_LIST:
    case SYNC_BUF_LRU_LIST:
    case SYNC_BUF_FREE_LIST:
    case SYNC_BUF_ZIP_FREE:
    case SYNC_BUF_ZIP_HASH:
    case SYNC_BUF_FLUSH_STATE:
    case SYNC_RSEG_ARRAY_HEADER:
    case SYNC_LOCK_SYS_GLOBAL:
    case SYNC_LOCK_SYS_SHARDED:
    case SYNC_BUF_PAGE_HASH:
    case SYNC_BUF_BLOCK:
    case SYNC_FSP:
    case SYNC_SEARCH_SYS:

      /* We can have multiple latches of this type therefore we
      can only check whether the requested<=held condition holds. */
      assert_requested_is_lower_or_equal_to_held(level, latches);
      break;

    case SYNC_IBUF_BITMAP:

      /* Either the thread must own the master mutex to all
      the bitmap pages, or it is allowed to latch only ONE
      bitmap page. */

      if (find(latches, SYNC_IBUF_BITMAP_MUTEX) != nullptr) {
        assert_requested_is_lower_or_equal_to_held(level, latches);
      } else {
        assert_requested_is_lower_than_held(level, latches);
      }
      break;

    case SYNC_FSP_PAGE:
      ut_a(find(latches, SYNC_FSP) != nullptr);
      break;

    case SYNC_TRX_UNDO_PAGE:

      /* Purge is allowed to read in as many UNDO pages as it likes.
      The purge thread can read the UNDO pages without any covering
      mutex. */

      if (find(latches, SYNC_TRX_UNDO) == nullptr &&
          find(latches, SYNC_TEMP_SPACE_RSEG) == nullptr &&
          find(latches, SYNC_UNDO_SPACE_RSEG) == nullptr &&
          find(latches, SYNC_TRX_SYS_RSEG) == nullptr) {
        assert_requested_is_lower_or_equal_to_held(level, latches);
      }
      break;

    case SYNC_RSEG_HEADER:

      ut_a(find(latches, SYNC_TEMP_SPACE_RSEG) != nullptr ||
           find(latches, SYNC_UNDO_SPACE_RSEG) != nullptr ||
           find(latches, SYNC_TRX_SYS_RSEG) != nullptr);
      break;

    case SYNC_RSEG_HEADER_NEW:

      ut_a(find(latches, SYNC_FSP_PAGE) != nullptr);
      break;

    case SYNC_TREE_NODE:

    {
      const latch_t *fsp_latch;

      fsp_latch = find(latches, SYNC_FSP);

      if ((fsp_latch == nullptr || !fsp_latch->is_temp_fsp()) &&
          find(latches, SYNC_INDEX_TREE) == nullptr &&
          find(latches, SYNC_DICT_OPERATION) == nullptr) {
        assert_requested_is_lower_or_equal_to_held(level, latches);
      }
    }

    break;

    case SYNC_TREE_NODE_NEW:

      ut_a(find(latches, SYNC_FSP_PAGE) != nullptr);
      break;

    case SYNC_INDEX_TREE:

      assert_all_held_are_above(latches, level, SYNC_TREE_NODE - 1);
      break;

    case SYNC_IBUF_TREE_NODE:

      if (find(latches, SYNC_IBUF_INDEX_TREE) == nullptr) {
        assert_requested_is_lower_or_equal_to_held(level, latches);
      }
      break;

    case SYNC_IBUF_TREE_NODE_NEW:

      /* ibuf_add_free_page() allocates new pages for the change
      buffer while only holding the tablespace x-latch. These
      pre-allocated new pages may only be used while holding
      ibuf_mutex, in btr_page_alloc_for_ibuf(). */

      ut_a(find(latches, SYNC_IBUF_MUTEX) != nullptr ||
           find(latches, SYNC_FSP) != nullptr);
      break;

    case SYNC_IBUF_INDEX_TREE:

      if (find(latches, SYNC_FSP) != nullptr) {
        assert_requested_is_lower_or_equal_to_held(level, latches);
      } else {
        assert_all_held_are_above(latches, level, SYNC_IBUF_TREE_NODE - 1);
      }
      break;

    case SYNC_IBUF_PESS_INSERT_MUTEX:

      assert_all_held_are_above(latches, level, SYNC_FSP - 1);
      ut_a(find(latches, SYNC_IBUF_MUTEX) == nullptr);
      break;

    case SYNC_IBUF_HEADER:

      assert_all_held_are_above(latches, level, SYNC_FSP - 1);
      ut_a(find(latches, SYNC_IBUF_MUTEX) == nullptr);
      ut_a(find(latches, SYNC_IBUF_PESS_INSERT_MUTEX) == nullptr);
      break;

    case SYNC_PERSIST_DIRTY_TABLES:

      assert_all_held_are_above(latches, level, SYNC_IBUF_MUTEX);
      break;

    case SYNC_PERSIST_AUTOINC:

      assert_all_held_are_above(latches, level, SYNC_IBUF_MUTEX);
      ut_a(find(latches, SYNC_PERSIST_DIRTY_TABLES) == nullptr);
      break;

    case SYNC_MUTEX:
    case SYNC_UNKNOWN:
    case SYNC_LEVEL_VARYING:
    case RW_LOCK_X:
    case RW_LOCK_X_WAIT:
    case RW_LOCK_S:
    case RW_LOCK_SX:
    case RW_LOCK_NOT_LOCKED:
      /* These levels should never be set for a latch. */
      ut_error;
      break;
  }

  return (latches);
}

/** Removes a latch from the thread level array if it is found there.
@param[in]      latch           The latch that was released */
void LatchDebug::unlock(const latch_t *latch) UNIV_NOTHROW {
  if (latch->get_level() == SYNC_LEVEL_VARYING) {
    // We don't have varying level mutexes
    ut_ad(latch->m_rw_lock);
  }

  Latches *latches;

  if (*latch->get_name() == '.') {
    /* Ignore diagnostic latches, starting with '.' */

  } else if ((latches = thread_latches()) != nullptr) {
    Latches::reverse_iterator rend = latches->rend();

    for (Latches::reverse_iterator it = latches->rbegin(); it != rend; ++it) {
      if (it->m_latch != latch) {
        continue;
      }

      Latches::iterator i = it.base();

      latches->erase(--i);

      /* If this thread doesn't own any more
      latches remove from the map.

      FIXME: Perhaps use the master thread
      to do purge. Or, do it from close connection.
      This could be expensive. */

      if (latches->empty()) {
        m_mutex.enter();

        m_threads.erase(std::this_thread::get_id());

        m_mutex.exit();

        ut::delete_(latches);
      }

      return;
    }

    if (latch->get_level() != SYNC_LEVEL_VARYING) {
#ifdef UNIV_NO_ERR_MSGS
      ib::error()
#else
      ib::error(ER_IB_MSG_1164)
#endif /* UNIV_NO_ERR_MSGS */
          << "Couldn't find latch " << sync_latch_get_name(latch->get_id());

      print_latches(latches);

      /** Must find the latch. */
      ut_error;
    }
  }
}

/** Check if it is OK to acquire the latch.
@param[in]      latch   latch type */
void sync_check_lock_validate(const latch_t *latch) {
  if (LatchDebug::instance() != nullptr) {
    LatchDebug::instance()->lock_validate(latch, latch->get_level());
  }
}

/** Note that the lock has been granted
@param[in]      latch   latch type */
void sync_check_lock_granted(const latch_t *latch) {
  if (LatchDebug::instance() != nullptr) {
    LatchDebug::instance()->lock_granted(latch, latch->get_level());
  }
}

/** Check if it is OK to acquire the latch.
@param[in]      latch   latch type
@param[in]      level   the level of the mutex */
void sync_check_lock(const latch_t *latch, latch_level_t level) {
  if (LatchDebug::instance() != nullptr) {
    ut_ad(latch->get_level() == SYNC_LEVEL_VARYING);
    ut_ad(latch->get_id() == LATCH_ID_BUF_BLOCK_LOCK);

    LatchDebug::instance()->lock_validate(latch, level);
    LatchDebug::instance()->lock_granted(latch, level);
  }
}

/** Check if it is OK to re-acquire the lock.
@param[in]      latch           RW-LOCK to relock (recursive X locks) */
void sync_check_relock(const latch_t *latch) {
  if (LatchDebug::instance() != nullptr) {
    LatchDebug::instance()->relock(latch);
  }
}

/** Removes a latch from the thread level array if it is found there.
@param[in]      latch           The latch to unlock */
void sync_check_unlock(const latch_t *latch) {
  if (LatchDebug::instance() != nullptr) {
    LatchDebug::instance()->unlock(latch);
  }
}

/** Checks if the level array for the current thread contains a
mutex or rw-latch at the specified level.
@param[in]      level           to find
@return a matching latch, or NULL if not found */
const latch_t *sync_check_find(latch_level_t level) {
  if (LatchDebug::instance() != nullptr) {
    return (LatchDebug::instance()->find(level));
  }

  return (nullptr);
}

/** Checks that the level array for the current thread is empty.
Terminate iteration if the functor returns true.
@param[in,out]   functor        called for each element.
@return true if the functor returns true */
bool sync_check_iterate(sync_check_functor_t &functor) {
  if (LatchDebug::instance() != nullptr) {
    return (LatchDebug::instance()->for_each(functor));
  }

  return (false);
}

/** Enable sync order checking.

Note: We don't enforce any synchronisation checks. The caller must ensure
that no races can occur */
void sync_check_enable() {
  if (!srv_sync_debug) {
    return;
  }

  /* We should always call this before we create threads. */

  LatchDebug::create_instance();
}

/** Initialise the debug data structures */
void LatchDebug::init() UNIV_NOTHROW {
  ut_a(rw_lock_debug_event == nullptr);

  mutex_create(LATCH_ID_RW_LOCK_DEBUG, &rw_lock_debug_mutex);

  rw_lock_debug_event = os_event_create();

  rw_lock_debug_waiters.store(false, std::memory_order_relaxed);
}

/** Shutdown the latch debug checking

Note: We don't enforce any synchronisation checks. The caller must ensure
that no races can occur */
void LatchDebug::shutdown() UNIV_NOTHROW {
  ut_a(rw_lock_debug_event != nullptr);

  os_event_destroy(rw_lock_debug_event);

  rw_lock_debug_event = nullptr;

  mutex_free(&rw_lock_debug_mutex);

  ut_a(s_initialized);

  s_initialized = false;

  if (instance() == nullptr) {
    return;
  }

  ut::delete_(s_instance);

  LatchDebug::s_instance = nullptr;
}

/** Acquires the debug mutex. We cannot use the mutex defined in sync0sync,
because the debug mutex is also acquired in sync0arr while holding the OS
mutex protecting the sync array, and the ordinary mutex_enter might
recursively call routines in sync0arr, leading to a deadlock on the OS
mutex. */
void rw_lock_debug_mutex_enter() {
  for (;;) {
    if (0 == mutex_enter_nowait(&rw_lock_debug_mutex)) {
      return;
    }

    const auto sig_count = os_event_reset(rw_lock_debug_event);
    /* We need to set rw_lock_debug_waiters to true AFTER we have reset the
    event, and got the sig_count, as doing it in opposite order might mean that
    we will miss the wakeup occurring in between, and will wait forever as our
    latest sig_count value will indicate we are waiting for a next wakeup. */
    rw_lock_debug_waiters.exchange(true, std::memory_order_acq_rel);
    /* We need to make sure we read the state of the rw_lock_debug_mutex AFTER
    we have set rw_lock_debug_waiters to true. Otherwise, if we might first
    observe a latched mutex, then the other thread releases it without waking up
    anyone, because we haven't yet set rw_lock_debug_waiters to true, and then
    we go to os_event_wait_low() forever as there is no one to wake us up.*/
    if (0 == mutex_enter_nowait(&rw_lock_debug_mutex)) {
      return;
    }

    os_event_wait_low(rw_lock_debug_event, sig_count);
  }
}

/** Releases the debug mutex. */
void rw_lock_debug_mutex_exit() {
  mutex_exit(&rw_lock_debug_mutex);
  /* It is crucial that we read rw_lock_debug_waiters AFTER we have released
  the rw_lock_debug_mutex. If we check it too soon, we might miss a thread
  which decided to wait on the mutex we hold just after we have checked and
  never wake it up.
  Also, we want to establish a causal relation: if this thread sees
  rw_lock_debug_waiters set to true, then the os_event_set() from this thread
  happens after the thread setting rw_lock_debug_mutex to true has obtained the
  sig_count from os_event_reset(). */
  if (rw_lock_debug_waiters.exchange(false, std::memory_order_acq_rel)) {
    /* We want the rw_lock_debug_waiter to be set to false BEFORE the call to
    os_event_set() below. Otherwise we could overwrite true set by a new waiter
    waiting for a new lock owner (note we have already released the mutex!) */
    os_event_set(rw_lock_debug_event);
  }
}
#endif /* UNIV_DEBUG */

/* Meta data for all the InnoDB latches. If the latch is not in recorded
here then it will be be considered for deadlock checks.  */
LatchMetaData latch_meta;

/** Load the latch meta data. */
static void sync_latch_meta_init() UNIV_NOTHROW {
  latch_meta.resize(LATCH_ID_MAX + 1);

  /* The latches should be ordered on latch_id_t. So that we can
  index directly into the vector to update and fetch meta-data. */

  LATCH_ADD_MUTEX(LOCK_FREE_HASH, SYNC_LOCK_FREE_HASH,
                  lock_free_hash_mutex_key);

  LATCH_ADD_MUTEX(AHI_ENABLED, SYNC_AHI_ENABLED, ahi_enabled_mutex_key);

  LATCH_ADD_MUTEX(AUTOINC, SYNC_DICT_AUTOINC_MUTEX, autoinc_mutex_key);

  LATCH_ADD_MUTEX(DDL_AUTOINC, SYNC_NO_ORDER_CHECK, ddl_autoinc_mutex_key);

#ifdef PFS_SKIP_BUFFER_MUTEX_RWLOCK
  LATCH_ADD_MUTEX(BUF_BLOCK_MUTEX, SYNC_BUF_BLOCK, PFS_NOT_INSTRUMENTED);
#else
  LATCH_ADD_MUTEX(BUF_BLOCK_MUTEX, SYNC_BUF_BLOCK, buffer_block_mutex_key);
#endif /* PFS_SKIP_BUFFER_MUTEX_RWLOCK */
  LATCH_ADD_MUTEX(BUF_POOL_CHUNKS, SYNC_BUF_CHUNKS, buf_pool_chunks_mutex_key);

  LATCH_ADD_MUTEX(BUF_POOL_LRU_LIST, SYNC_BUF_LRU_LIST,
                  buf_pool_LRU_list_mutex_key);

  LATCH_ADD_MUTEX(BUF_POOL_FREE_LIST, SYNC_BUF_FREE_LIST,
                  buf_pool_free_list_mutex_key);

  LATCH_ADD_MUTEX(BUF_POOL_ZIP_FREE, SYNC_BUF_ZIP_FREE,
                  buf_pool_zip_free_mutex_key);

  LATCH_ADD_MUTEX(BUF_POOL_ZIP_HASH, SYNC_BUF_ZIP_HASH,
                  buf_pool_zip_hash_mutex_key);

  LATCH_ADD_MUTEX(BUF_POOL_FLUSH_STATE, SYNC_BUF_FLUSH_STATE,
                  buf_pool_flush_state_mutex_key);

  LATCH_ADD_MUTEX(BUF_POOL_ZIP, SYNC_BUF_BLOCK, buf_pool_zip_mutex_key);

  LATCH_ADD_MUTEX(DICT_FOREIGN_ERR, SYNC_NO_ORDER_CHECK,
                  dict_foreign_err_mutex_key);

  LATCH_ADD_MUTEX(DICT_PERSIST_DIRTY_TABLES, SYNC_PERSIST_DIRTY_TABLES,
                  dict_persist_dirty_tables_mutex_key);

  LATCH_ADD_MUTEX(PERSIST_AUTOINC, SYNC_PERSIST_AUTOINC,
                  autoinc_persisted_mutex_key);

  LATCH_ADD_MUTEX(DICT_SYS, SYNC_DICT, dict_sys_mutex_key);

  LATCH_ADD_MUTEX(DICT_TABLE, SYNC_TABLE, dict_table_mutex_key);

  LATCH_ADD_MUTEX(PARSER, SYNC_PARSER, parser_mutex_key);

  LATCH_ADD_MUTEX(FIL_SHARD, SYNC_FIL_SHARD, fil_system_mutex_key);

  LATCH_ADD_MUTEX(FLUSH_LIST, SYNC_BUF_FLUSH_LIST, flush_list_mutex_key);

  LATCH_ADD_MUTEX(FTS_BG_THREADS, SYNC_FTS_BG_THREADS,
                  fts_bg_threads_mutex_key);

  LATCH_ADD_MUTEX(FTS_DELETE, SYNC_FTS_OPTIMIZE, fts_delete_mutex_key);

  LATCH_ADD_MUTEX(FTS_OPTIMIZE, SYNC_FTS_OPTIMIZE, fts_optimize_mutex_key);

  LATCH_ADD_MUTEX(FTS_DOC_ID, SYNC_FTS_OPTIMIZE, fts_doc_id_mutex_key);

  LATCH_ADD_MUTEX(FTS_PLL_TOKENIZE, SYNC_FTS_TOKENIZE,
                  fts_pll_tokenize_mutex_key);

  LATCH_ADD_MUTEX(HASH_TABLE_MUTEX, SYNC_BUF_PAGE_HASH, hash_table_mutex_key);

  LATCH_ADD_MUTEX(IBUF_BITMAP, SYNC_IBUF_BITMAP_MUTEX, ibuf_bitmap_mutex_key);

  LATCH_ADD_MUTEX(IBUF, SYNC_IBUF_MUTEX, ibuf_mutex_key);

  LATCH_ADD_MUTEX(IBUF_PESSIMISTIC_INSERT, SYNC_IBUF_PESS_INSERT_MUTEX,
                  ibuf_pessimistic_insert_mutex_key);

  LATCH_ADD_MUTEX(LOG_CHECKPOINTER, SYNC_LOG_CHECKPOINTER,
                  log_checkpointer_mutex_key);

  LATCH_ADD_MUTEX(LOG_CLOSER, SYNC_LOG_CLOSER, log_closer_mutex_key);

  LATCH_ADD_MUTEX(LOG_WRITER, SYNC_LOG_WRITER, log_writer_mutex_key);

  LATCH_ADD_MUTEX(LOG_FLUSHER, SYNC_LOG_FLUSHER, log_flusher_mutex_key);

  LATCH_ADD_MUTEX(LOG_WRITE_NOTIFIER, SYNC_LOG_WRITE_NOTIFIER,
                  log_write_notifier_mutex_key);

  LATCH_ADD_MUTEX(LOG_FLUSH_NOTIFIER, SYNC_LOG_FLUSH_NOTIFIER,
                  log_flush_notifier_mutex_key);

  LATCH_ADD_MUTEX(LOG_LIMITS, SYNC_LOG_LIMITS, log_limits_mutex_key);

  LATCH_ADD_MUTEX(LOG_FILES, SYNC_LOG_FILES, log_files_mutex_key);

  LATCH_ADD_RWLOCK(LOG_SN, SYNC_LOG_SN, log_sn_lock_key);

  LATCH_ADD_MUTEX(LOG_SN_MUTEX, SYNC_LOG_SN_MUTEX, log_sn_mutex_key);

  LATCH_ADD_MUTEX(LOG_ARCH, SYNC_LOG_ARCH, log_sys_arch_mutex_key);

  LATCH_ADD_MUTEX(PAGE_ARCH, SYNC_PAGE_ARCH, page_sys_arch_mutex_key);

  LATCH_ADD_MUTEX(PAGE_ARCH_OPER, SYNC_PAGE_ARCH_OPER,
                  page_sys_arch_oper_mutex_key);

  LATCH_ADD_MUTEX(PAGE_ARCH_CLIENT, SYNC_PAGE_ARCH_CLIENT,
                  page_sys_arch_client_mutex_key);

  LATCH_ADD_MUTEX(PAGE_CLEANER, SYNC_PAGE_CLEANER, page_cleaner_mutex_key);

  LATCH_ADD_MUTEX(PURGE_SYS_PQ, SYNC_PURGE_QUEUE, purge_sys_pq_mutex_key);

  LATCH_ADD_MUTEX(RECALC_POOL, SYNC_STATS_AUTO_RECALC, recalc_pool_mutex_key);

  LATCH_ADD_MUTEX(RECV_SYS, SYNC_RECV, recv_sys_mutex_key);

  LATCH_ADD_MUTEX(RECV_WRITER, SYNC_RECV_WRITER, recv_writer_mutex_key);

  LATCH_ADD_MUTEX(TEMP_SPACE_RSEG, SYNC_TEMP_SPACE_RSEG,
                  temp_space_rseg_mutex_key);

  LATCH_ADD_MUTEX(UNDO_SPACE_RSEG, SYNC_UNDO_SPACE_RSEG,
                  undo_space_rseg_mutex_key);

  LATCH_ADD_MUTEX(TRX_SYS_RSEG, SYNC_TRX_SYS_RSEG, trx_sys_rseg_mutex_key);

#ifdef UNIV_DEBUG
  /* Mutex names starting with '.' are not tracked. They are assumed
  to be diagnostic mutexes used in debugging. */
  latch_meta[LATCH_ID_RW_LOCK_DEBUG] = LATCH_ADD_MUTEX(
      RW_LOCK_DEBUG, SYNC_NO_ORDER_CHECK, rw_lock_debug_mutex_key);
#endif /* UNIV_DEBUG */

  LATCH_ADD_MUTEX(RTR_SSN_MUTEX, SYNC_ANY_LATCH, rtr_ssn_mutex_key);

  LATCH_ADD_MUTEX(RTR_ACTIVE_MUTEX, SYNC_ANY_LATCH, rtr_active_mutex_key);

  LATCH_ADD_MUTEX(RTR_MATCH_MUTEX, SYNC_ANY_LATCH, rtr_match_mutex_key);

  LATCH_ADD_MUTEX(RTR_PATH_MUTEX, SYNC_ANY_LATCH, rtr_path_mutex_key);

  LATCH_ADD_MUTEX(RW_LOCK_LIST, SYNC_NO_ORDER_CHECK, rw_lock_list_mutex_key);

  LATCH_ADD_MUTEX(SRV_INNODB_MONITOR, SYNC_NO_ORDER_CHECK,
                  srv_innodb_monitor_mutex_key);

  LATCH_ADD_MUTEX(SRV_MISC_TMPFILE, SYNC_ANY_LATCH, srv_misc_tmpfile_mutex_key);

  LATCH_ADD_MUTEX(SRV_MONITOR_FILE, SYNC_NO_ORDER_CHECK,
                  srv_monitor_file_mutex_key);

#ifdef UNIV_DEBUG
  LATCH_ADD_MUTEX(SYNC_THREAD, SYNC_NO_ORDER_CHECK, sync_thread_mutex_key);
#else
  LATCH_ADD_MUTEX(SYNC_THREAD, SYNC_NO_ORDER_CHECK, PFS_NOT_INSTRUMENTED);
#endif /* UNIV_DEBUG */

  LATCH_ADD_MUTEX(TRX_UNDO, SYNC_TRX_UNDO, trx_undo_mutex_key);

  LATCH_ADD_MUTEX(TRX_POOL, SYNC_POOL, trx_pool_mutex_key);

  LATCH_ADD_MUTEX(TRX_POOL_MANAGER, SYNC_POOL_MANAGER,
                  trx_pool_manager_mutex_key);

  LATCH_ADD_MUTEX(TEMP_POOL_MANAGER, SYNC_TEMP_POOL_MANAGER,
                  temp_pool_manager_mutex_key);

  LATCH_ADD_MUTEX(TRX, SYNC_TRX, trx_mutex_key);

  LATCH_ADD_MUTEX(LOCK_SYS_PAGE, SYNC_LOCK_SYS_SHARDED,
                  lock_sys_page_mutex_key);

  LATCH_ADD_MUTEX(LOCK_SYS_TABLE, SYNC_LOCK_SYS_SHARDED,
                  lock_sys_table_mutex_key);

  LATCH_ADD_MUTEX(LOCK_SYS_WAIT, SYNC_LOCK_WAIT_SYS, lock_wait_mutex_key);

  LATCH_ADD_MUTEX(TRX_SYS, SYNC_TRX_SYS, trx_sys_mutex_key);

  LATCH_ADD_MUTEX(TRX_SYS_SHARD, SYNC_TRX_SYS_SHARD, trx_sys_shard_mutex_key);

  LATCH_ADD_MUTEX(TRX_SYS_SERIALISATION, SYNC_TRX_SYS_SERIALISATION,
                  trx_sys_serialisation_mutex_key);

  LATCH_ADD_MUTEX(SRV_SYS, SYNC_THREADS, srv_sys_mutex_key);

  LATCH_ADD_MUTEX(SRV_SYS_TASKS, SYNC_ANY_LATCH, srv_threads_mutex_key);

  LATCH_ADD_MUTEX(PAGE_ZIP_STAT_PER_INDEX, SYNC_ANY_LATCH,
                  page_zip_stat_per_index_mutex_key);

#ifndef PFS_SKIP_EVENT_MUTEX
  LATCH_ADD_MUTEX(EVENT_MANAGER, SYNC_NO_ORDER_CHECK, event_manager_mutex_key);
  LATCH_ADD_MUTEX(EVENT_MUTEX, SYNC_NO_ORDER_CHECK, event_mutex_key);
#else
  LATCH_ADD_MUTEX(EVENT_MANAGER, SYNC_NO_ORDER_CHECK, PFS_NOT_INSTRUMENTED);
  LATCH_ADD_MUTEX(EVENT_MUTEX, SYNC_NO_ORDER_CHECK, PFS_NOT_INSTRUMENTED);
#endif /* !PFS_SKIP_EVENT_MUTEX */

  LATCH_ADD_MUTEX(SYNC_ARRAY_MUTEX, SYNC_NO_ORDER_CHECK, sync_array_mutex_key);

  LATCH_ADD_MUTEX(ZIP_PAD_MUTEX, SYNC_NO_ORDER_CHECK, zip_pad_mutex_key);

  LATCH_ADD_MUTEX(OS_AIO_READ_MUTEX, SYNC_NO_ORDER_CHECK, PFS_NOT_INSTRUMENTED);

  LATCH_ADD_MUTEX(OS_AIO_WRITE_MUTEX, SYNC_NO_ORDER_CHECK,
                  PFS_NOT_INSTRUMENTED);

  LATCH_ADD_MUTEX(OS_AIO_LOG_MUTEX, SYNC_NO_ORDER_CHECK, PFS_NOT_INSTRUMENTED);

  LATCH_ADD_MUTEX(OS_AIO_IBUF_MUTEX, SYNC_NO_ORDER_CHECK, PFS_NOT_INSTRUMENTED);

  LATCH_ADD_MUTEX(ROW_DROP_LIST, SYNC_NO_ORDER_CHECK, row_drop_list_mutex_key);

  LATCH_ADD_MUTEX(INDEX_ONLINE_LOG, SYNC_INDEX_ONLINE_LOG,
                  index_online_log_key);

  LATCH_ADD_MUTEX(WORK_QUEUE, SYNC_WORK_QUEUE, PFS_NOT_INSTRUMENTED);

  // Add the RW locks
  LATCH_ADD_RWLOCK(BTR_SEARCH, SYNC_SEARCH_SYS, btr_search_latch_key);

#ifndef PFS_SKIP_BUFFER_MUTEX_RWLOCK
  LATCH_ADD_RWLOCK(BUF_BLOCK_LOCK, SYNC_LEVEL_VARYING, buf_block_lock_key);
#else
  LATCH_ADD_RWLOCK(BUF_BLOCK_LOCK, SYNC_LEVEL_VARYING, PFS_NOT_INSTRUMENTED);
#endif /* !PFS_SKIP_BUFFER_MUTEX_RWLOCK */

#ifdef UNIV_DEBUG
  LATCH_ADD_RWLOCK(BUF_BLOCK_DEBUG, SYNC_NO_ORDER_CHECK,
                   buf_block_debug_latch_key);
#else
  LATCH_ADD_RWLOCK(BUF_BLOCK_DEBUG, SYNC_NO_ORDER_CHECK, PFS_NOT_INSTRUMENTED);
#endif /* UNIV_DEBUG */

  LATCH_ADD_RWLOCK(DICT_OPERATION, SYNC_DICT_OPERATION,
                   dict_operation_lock_key);

  LATCH_ADD_RWLOCK(RSEGS, SYNC_RSEGS, rsegs_lock_key);

  LATCH_ADD_RWLOCK(LOCK_SYS_GLOBAL, SYNC_LOCK_SYS_GLOBAL,
                   lock_sys_global_rw_lock_key);

  LATCH_ADD_RWLOCK(UNDO_SPACES, SYNC_UNDO_SPACES, undo_spaces_lock_key);

  LATCH_ADD_MUTEX(UNDO_DDL, SYNC_UNDO_DDL, PFS_NOT_INSTRUMENTED);

  LATCH_ADD_RWLOCK(FIL_SPACE, SYNC_FSP, fil_space_latch_key);

  LATCH_ADD_RWLOCK(FTS_CACHE, SYNC_FTS_CACHE, fts_cache_rw_lock_key);

  LATCH_ADD_RWLOCK(FTS_CACHE_INIT, SYNC_FTS_CACHE_INIT,
                   fts_cache_init_rw_lock_key);

  LATCH_ADD_RWLOCK(TRX_I_S_CACHE, SYNC_TRX_I_S_RWLOCK, trx_i_s_cache_lock_key);

  LATCH_ADD_RWLOCK(TRX_PURGE, SYNC_PURGE_LATCH, trx_purge_latch_key);

  LATCH_ADD_RWLOCK(IBUF_INDEX_TREE, SYNC_IBUF_INDEX_TREE,
                   index_tree_rw_lock_key);

  LATCH_ADD_RWLOCK(INDEX_TREE, SYNC_INDEX_TREE, index_tree_rw_lock_key);

  LATCH_ADD_RWLOCK(DICT_TABLE_STATS, SYNC_INDEX_TREE, dict_table_stats_key);

  LATCH_ADD_RWLOCK(HASH_TABLE_RW_LOCK, SYNC_BUF_PAGE_HASH,
                   hash_table_locks_key);

  LATCH_ADD_RWLOCK(SYNC_DEBUG_MUTEX, SYNC_NO_ORDER_CHECK, PFS_NOT_INSTRUMENTED);

  LATCH_ADD_MUTEX(FILE_OPEN, SYNC_NO_ORDER_CHECK, file_open_mutex_key);

  LATCH_ADD_MUTEX(MASTER_KEY_ID_MUTEX, SYNC_NO_ORDER_CHECK,
                  master_key_id_mutex_key);

  LATCH_ADD_MUTEX(CLONE_SYS, SYNC_NO_ORDER_CHECK, clone_sys_mutex_key);

  LATCH_ADD_MUTEX(CLONE_TASK, SYNC_NO_ORDER_CHECK, clone_task_mutex_key);

  LATCH_ADD_MUTEX(CLONE_SNAPSHOT, SYNC_NO_ORDER_CHECK,
                  clone_snapshot_mutex_key);

  LATCH_ADD_MUTEX(PARALLEL_READ, SYNC_NO_ORDER_CHECK, parallel_read_mutex_key);

  LATCH_ADD_MUTEX(REDO_LOG_ARCHIVE_ADMIN_MUTEX, SYNC_NO_ORDER_CHECK,
                  PFS_NOT_INSTRUMENTED);

  LATCH_ADD_MUTEX(REDO_LOG_ARCHIVE_QUEUE_MUTEX, SYNC_NO_ORDER_CHECK,
                  PFS_NOT_INSTRUMENTED);

  LATCH_ADD_MUTEX(DBLWR, SYNC_DBLWR, dblwr_mutex_key);

  LATCH_ADD_MUTEX(TEST_MUTEX, SYNC_NO_ORDER_CHECK, PFS_NOT_INSTRUMENTED);

  latch_id_t id = LATCH_ID_NONE;

  /* The array should be ordered on latch ID.We need to
  index directly into it from the mutex policy to update
  the counters and access the meta-data. */

  LatchMetaData::iterator it = latch_meta.begin();

  /* Skip the first entry, it is always NULL (LATCH_ID_NONE) */

  for (++it; it != latch_meta.end(); ++it) {
    const latch_meta_t *meta = *it;

    /* Debug latches will be missing */

    if (meta == nullptr) {
      continue;
    }

    ut_a(meta->get_id() != LATCH_ID_NONE);
    ut_a(id < meta->get_id());

    id = meta->get_id();
  }
}

/** Destroy the latch meta data */
static void sync_latch_meta_destroy() {
  for (LatchMetaData::iterator it = latch_meta.begin(); it != latch_meta.end();
       ++it) {
    ut::delete_(*it);
  }

  latch_meta.clear();
}

/** Track mutex file creation name and line number. This is to avoid storing
{ const char* name; uint16_t line; } in every instance. This results in the
sizeof(Mutex) > 64. We use a lookup table to store it separately. Fetching
the values is very rare, only required for diagnostic purposes. And, we
don't create/destroy mutexes that frequently. */
struct CreateTracker {
  /** Constructor */
  CreateTracker() UNIV_NOTHROW { m_mutex.init(); }

  /** Destructor */
  ~CreateTracker() UNIV_NOTHROW {
    ut_ad(m_files.empty());

    m_mutex.destroy();
  }

  /** Register where the latch was created
  @param[in]    ptr             Latch instance
  @param[in]    filename        Where created
  @param[in]    line            Line number in filename */
  void register_latch(const void *ptr, const char *filename,
                      uint16_t line) UNIV_NOTHROW {
    m_mutex.enter();

    Files::iterator lb = m_files.lower_bound(ptr);

    ut_ad(lb == m_files.end() || m_files.key_comp()(ptr, lb->first));

    typedef Files::value_type value_type;

    m_files.insert(lb, value_type(ptr, File(filename, line)));

    m_mutex.exit();
  }

  /** Deregister a latch - when it is destroyed
  @param[in]    ptr             Latch instance being destroyed */
  void deregister_latch(const void *ptr) UNIV_NOTHROW {
    m_mutex.enter();

    Files::iterator lb = m_files.lower_bound(ptr);

    ut_ad(lb != m_files.end() && !(m_files.key_comp()(ptr, lb->first)));

    m_files.erase(lb);

    m_mutex.exit();
  }

  /** Get the create string, format is "name:line"
  @param[in]    ptr             Latch instance
  @return the create string or "" if not found */
  std::string get(const void *ptr) UNIV_NOTHROW {
    m_mutex.enter();

    std::string created;

    Files::iterator lb = m_files.lower_bound(ptr);

    if (lb != m_files.end() && !(m_files.key_comp()(ptr, lb->first))) {
      std::ostringstream msg;

      msg << lb->second.m_name << ":" << lb->second.m_line;

      created = msg.str();
    }

    m_mutex.exit();

    return (created);
  }

 private:
  /** For tracking the filename and line number */
  struct File {
    /** Constructor */
    File() UNIV_NOTHROW : m_name(), m_line() {}

    /** Constructor
    @param[in]  name            Filename where created
    @param[in]  line            Line number where created */
    File(const char *name, uint16_t line) UNIV_NOTHROW
        : m_name(sync_basename(name)),
          m_line(line) {
      /* No op */
    }

    /** Filename where created */
    std::string m_name;

    /** Line number where created */
    uint16_t m_line;
  };

  /** Map the mutex instance to where it was created */
  typedef std::map<const void *, File, std::less<const void *>,
                   ut::allocator<std::pair<const void *const, File>>>
      Files;

  typedef OSMutex Mutex;

  /** Mutex protecting m_files */
  Mutex m_mutex;

  /** Track the latch creation */
  Files m_files;
};

/** Track latch creation location. For reducing the size of the latches */
static CreateTracker *create_tracker;

/** Register a latch, called when it is created
@param[in]      ptr             Latch instance that was created
@param[in]      filename        Filename where it was created
@param[in]      line            Line number in filename */
void sync_file_created_register(const void *ptr, const char *filename,
                                uint16_t line) {
  create_tracker->register_latch(ptr, filename, line);
}

/** Deregister a latch, called when it is destroyed
@param[in]      ptr             Latch to be destroyed */
void sync_file_created_deregister(const void *ptr) {
  create_tracker->deregister_latch(ptr);
}

/** Get the string where the file was created. Its format is "name:line"
@param[in]      ptr             Latch instance
@return created information or "" if can't be found */
std::string sync_file_created_get(const void *ptr) {
  return (create_tracker->get(ptr));
}

/** Initializes the synchronization data structures.
@param[in]      max_threads     Maximum threads that can be created. */
void sync_check_init(size_t max_threads) {
  ut_ad(!LatchDebug::s_initialized);
  ut_d(LatchDebug::s_initialized = true);

  /** For collecting latch statistic - SHOW ... MUTEX */
  mutex_monitor = ut::new_withkey<MutexMonitor>(UT_NEW_THIS_FILE_PSI_KEY);

  /** For trcking mutex creation location */
  create_tracker = ut::new_withkey<CreateTracker>(UT_NEW_THIS_FILE_PSI_KEY);

  sync_latch_meta_init();

  /* Init the mutex list and create the mutex to protect it. */

  mutex_create(LATCH_ID_RW_LOCK_LIST, &rw_lock_list_mutex);

  ut_d(LatchDebug::init());

  sync_array_init(max_threads);
}

/** Frees the resources in InnoDB's own synchronization data structures. Use
os_sync_free() after calling this. */
void sync_check_close() {
  ut_d(LatchDebug::shutdown());

  mutex_free(&rw_lock_list_mutex);

  sync_array_close();

  ut::delete_(mutex_monitor);

  mutex_monitor = nullptr;

  ut::delete_(create_tracker);

  create_tracker = nullptr;

  sync_latch_meta_destroy();
}

#ifdef UNIV_DEBUG
std::mutex Sync_point::s_mutex{};
Sync_point::Sync_points Sync_point::s_sync_points{};

void Sync_point::add(const THD *thd, const std::string &target) noexcept {
  const std::lock_guard<std::mutex> lock(s_mutex);

  auto r1 = std::find_if(
      std::begin(s_sync_points), std::end(s_sync_points),
      [=](const Sync_point &sync_point) { return thd == sync_point.m_thd; });

  if (r1 != s_sync_points.end()) {
    const auto &b = std::begin(r1->m_targets);
    const auto &e = std::end(r1->m_targets);
    const auto r2 = std::find(b, e, target);

    if (r2 == e) {
      r1->m_targets.push_back(target);
    }
  } else {
    s_sync_points.push_back(Sync_point{thd});
    s_sync_points.back().m_targets.push_back(target);
  }
}

bool Sync_point::enabled(const THD *thd, const std::string &target) noexcept {
  const std::lock_guard<std::mutex> lock(s_mutex);

  auto r1 = std::find_if(
      std::begin(s_sync_points), std::end(s_sync_points),
      [=](const Sync_point &sync_point) { return thd == sync_point.m_thd; });

  if (r1 == s_sync_points.end()) {
    return false;
  }

  const auto &b = std::begin(r1->m_targets);
  const auto &e = std::end(r1->m_targets);
  const auto r2 = std::find(b, e, target);

  return r2 != e;
}

bool Sync_point::enabled(const std::string &target) noexcept {
#ifndef UNIV_NO_ERR_MSGS
  return enabled(current_thd, target);
#else
  return false;
#endif /* !UNIV_NO_ERR_MSGS */
}

void Sync_point::erase(const THD *thd, const std::string &target) noexcept {
  const std::lock_guard<std::mutex> lock(s_mutex);

  auto r1 = std::find_if(
      std::begin(s_sync_points), std::end(s_sync_points),
      [=](const Sync_point &sync_point) { return thd == sync_point.m_thd; });

  if (r1 != s_sync_points.end()) {
    const auto &b = std::begin(r1->m_targets);
    const auto &e = std::end(r1->m_targets);
    const auto r2 = std::find(b, e, target);

    if (r2 != e) {
      r1->m_targets.erase(r2);
      if (r1->m_targets.empty()) {
        s_sync_points.erase(r1);
      }
    }
  }
}
#endif /* UNIV_DEBUG */
