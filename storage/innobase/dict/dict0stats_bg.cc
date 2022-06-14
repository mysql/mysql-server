/*****************************************************************************

Copyright (c) 2012, 2022, Oracle and/or its affiliates.

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

/** @file dict/dict0stats_bg.cc
 Code used for background table and index stats gathering.

 Created Apr 25, 2012 Vasil Dimov
 *******************************************************/

#include "sql_thd_internal_api.h"

#include <stddef.h>
#include <sys/types.h>
#include <vector>

#include "dict0dd.h"
#include "dict0dict.h"
#include "dict0stats.h"
#include "dict0stats_bg.h"

#include "os0thread-create.h"
#include "row0mysql.h"
#include "srv0start.h"
#include "ut0new.h"

/** Minimum time interval between stats recalc for a given table */
constexpr std::chrono::seconds MIN_RECALC_INTERVAL{10};

static inline bool SHUTTING_DOWN() {
  return srv_shutdown_state.load() >=
         SRV_SHUTDOWN_PRE_DD_AND_SYSTEM_TRANSACTIONS;
}

/** Event to wake up the stats thread */
os_event_t dict_stats_event = nullptr;

#ifdef UNIV_DEBUG
/** Used by SET GLOBAL innodb_dict_stats_disabled_debug = 1; */
bool innodb_dict_stats_disabled_debug;

static os_event_t dict_stats_disabled_event;
#endif /* UNIV_DEBUG */

/** This mutex protects the "recalc_pool" variable. */
static ib_mutex_t recalc_pool_mutex;

/** The number of tables that can be added to "recalc_pool" before
it is enlarged */
static const ulint RECALC_POOL_INITIAL_SLOTS = 128;

/** Allocator type, used by std::vector */
typedef ut::allocator<table_id_t> recalc_pool_allocator_t;

/** The multitude of tables whose stats are to be automatically
recalculated - an STL vector */
typedef std::vector<table_id_t, recalc_pool_allocator_t> recalc_pool_t;

/** Iterator type for iterating over the elements of objects of type
recalc_pool_t. */
typedef recalc_pool_t::iterator recalc_pool_iterator_t;

/** Pool where we store information on which tables are to be processed
by background statistics gathering. */
static recalc_pool_t *recalc_pool;

/** Initialize the recalc pool, called once during thread initialization. */
static void dict_stats_recalc_pool_init() {
  ut_ad(!srv_read_only_mode);

  const PSI_memory_key key = mem_key_dict_stats_bg_recalc_pool_t;

  recalc_pool = ut::new_withkey<recalc_pool_t>(ut::make_psi_memory_key(key),
                                               recalc_pool_allocator_t(key));

  recalc_pool->reserve(RECALC_POOL_INITIAL_SLOTS);
}

/** Free the resources occupied by the recalc pool, called once during
 thread de-initialization. */
static void dict_stats_recalc_pool_deinit() {
  ut_ad(!srv_read_only_mode);

  recalc_pool->clear();

  ut::delete_(recalc_pool);
  recalc_pool = nullptr;
}

/** Add a table to the recalc pool, which is processed by the
 background stats gathering thread. Only the table id is added to the
 list, so the table can be closed after being enqueued and it will be
 opened when needed. If the table does not exist later (has been DROPped),
 then it will be removed from the pool and skipped. */
void dict_stats_recalc_pool_add(
    const dict_table_t *table) /*!< in: table to add */
{
  ut_ad(!srv_read_only_mode);

  mutex_enter(&recalc_pool_mutex);

  /* quit if already in the list */
  for (recalc_pool_iterator_t iter = recalc_pool->begin();
       iter != recalc_pool->end(); ++iter) {
    if (*iter == table->id) {
      mutex_exit(&recalc_pool_mutex);
      return;
    }
  }

  recalc_pool->push_back(table->id);

  mutex_exit(&recalc_pool_mutex);

  os_event_set(dict_stats_event);
}

/** Get a table from the auto recalc pool. The returned table id is removed
 from the pool.
 @return true if the pool was non-empty and "id" was set, false otherwise */
static bool dict_stats_recalc_pool_get(
    table_id_t *id) /*!< out: table id, or unmodified if list is
                    empty */
{
  ut_ad(!srv_read_only_mode);

  mutex_enter(&recalc_pool_mutex);

  if (recalc_pool->empty()) {
    mutex_exit(&recalc_pool_mutex);
    return (false);
  }

  *id = recalc_pool->at(0);

  recalc_pool->erase(recalc_pool->begin());

  mutex_exit(&recalc_pool_mutex);

  return (true);
}

/** Delete a given table from the auto recalc pool.
 dict_stats_recalc_pool_del() */
void dict_stats_recalc_pool_del(
    const dict_table_t *table) /*!< in: table to remove */
{
  ut_ad(!srv_read_only_mode);
  ut_ad(dict_sys_mutex_own());

  mutex_enter(&recalc_pool_mutex);

  ut_ad(table->id > 0);

  for (recalc_pool_iterator_t iter = recalc_pool->begin();
       iter != recalc_pool->end(); ++iter) {
    if (*iter == table->id) {
      /* erase() invalidates the iterator */
      recalc_pool->erase(iter);
      break;
    }
  }

  mutex_exit(&recalc_pool_mutex);
}

/** Wait until background stats thread has stopped using the specified table.
 The caller must have locked the data dictionary using
 row_mysql_lock_data_dictionary() and this function may unlock it temporarily
 and restore the lock before it exits.
 The background stats thread is guaranteed not to start using the specified
 table after this function returns and before the caller unlocks the data
 dictionary because it sets the BG_STAT_IN_PROGRESS bit in table->stats_bg_flag
 under dict_sys->mutex. */
void dict_stats_wait_bg_to_stop_using_table(
    dict_table_t *table, /*!< in/out: table */
    trx_t *trx)          /*!< in/out: transaction to use for
                         unlocking/locking the data dict */
{
  while (!dict_stats_stop_bg(table)) {
    DICT_STATS_BG_YIELD(trx, UT_LOCATION_HERE);
  }
}

/** Initialize global variables needed for the operation of dict_stats_thread()
 Must be called before dict_stats_thread() is started. */
void dict_stats_thread_init() {
  ut_a(!srv_read_only_mode);

  dict_stats_event = os_event_create();

  ut_d(dict_stats_disabled_event = os_event_create());

  /* The recalc_pool_mutex is acquired from:
  1) the background stats gathering thread before any other latch
     and released without latching anything else in between (thus
     any level would do here)
  2) from row_update_statistics_if_needed()
     and released without latching anything else in between. We know
     that dict_sys->mutex (SYNC_DICT) is not acquired when
     row_update_statistics_if_needed() is called and it may be acquired
     inside that function (thus a level <=SYNC_DICT would do).
  3) from row_drop_table_for_mysql() after dict_sys->mutex (SYNC_DICT)
     and dict_operation_lock (SYNC_DICT_OPERATION) have been locked
     (thus a level <SYNC_DICT && <SYNC_DICT_OPERATION would do)
  So we choose SYNC_STATS_AUTO_RECALC to be about below SYNC_DICT. */

  mutex_create(LATCH_ID_RECALC_POOL, &recalc_pool_mutex);

  dict_stats_recalc_pool_init();
}

/** Free resources allocated by dict_stats_thread_init(), must be called
 after dict_stats_thread() has exited. */
void dict_stats_thread_deinit() {
  ut_a(!srv_read_only_mode);
  ut_ad(!srv_thread_is_active(srv_threads.m_dict_stats));

  if (recalc_pool == nullptr) {
    return;
  }

  dict_stats_recalc_pool_deinit();

  mutex_free(&recalc_pool_mutex);

#ifdef UNIV_DEBUG
  os_event_destroy(dict_stats_disabled_event);
  dict_stats_disabled_event = nullptr;
#endif /* UNIV_DEBUG */

  os_event_destroy(dict_stats_event);
  dict_stats_event = nullptr;
}

/** Get the first table that has been added for auto recalc and eventually
update its stats.
@param[in,out]  thd     current thread */
static void dict_stats_process_entry_from_recalc_pool(THD *thd) {
  table_id_t table_id;

  ut_ad(!srv_read_only_mode);

  DBUG_EXECUTE_IF("do_not_meta_lock_in_background", return;);

  /* pop the first table from the auto recalc pool */
  if (!dict_stats_recalc_pool_get(&table_id)) {
    /* no tables for auto recalc */
    return;
  }

  dict_table_t *table;
  MDL_ticket *mdl = nullptr;

  /* We need to enter dict_sys->mutex for setting
  table->stats_bg_flag. This is for blocking other DDL, like drop
  table. */
  dict_sys_mutex_enter();
  table = dd_table_open_on_id(table_id, thd, &mdl, true, true);

  if (table == nullptr) {
    /* table does not exist, must have been DROPped
    after its id was enqueued */
    dict_sys_mutex_exit();
    return;
  }

  /* Check whether table is corrupted */
  if (table->is_corrupted()) {
    dd_table_close(table, thd, &mdl, true);
    dict_sys_mutex_exit();
    return;
  }

  /* Set bg flag. */
  table->stats_bg_flag = BG_STAT_IN_PROGRESS;

  dict_sys_mutex_exit();

  /* ut_time_monotonic() could be expensive, the current function
  is called once every time a table has been changed more than 10% and
  on a system with lots of small tables, this could become hot. If we
  find out that this is a problem, then the check below could eventually
  be replaced with something else, though a time interval is the natural
  approach. */

  if (std::chrono::steady_clock::now() - table->stats_last_recalc <
      MIN_RECALC_INTERVAL) {
    /* Stats were (re)calculated not long ago. To avoid
    too frequent stats updates we put back the table on
    the auto recalc list and do nothing. */

    dict_stats_recalc_pool_add(table);

  } else {
    dict_stats_update(table, DICT_STATS_RECALC_PERSISTENT);
  }

  dict_sys_mutex_enter();

  /* Set back bg flag */
  table->stats_bg_flag = BG_STAT_NONE;

  dict_sys_mutex_exit();

  /* This call can't be moved into dict_sys->mutex protection,
  since it'll cause deadlock while release mdl lock. */
  dd_table_close(table, thd, &mdl, false);
}

#ifdef UNIV_DEBUG
void dict_stats_disabled_debug_update(THD *, SYS_VAR *, void *,
                                      const void *save) {
  /* This method is protected by mutex, as every SET GLOBAL .. */
  ut_ad(dict_stats_disabled_event != nullptr);

  const bool disable = *static_cast<const bool *>(save);

  const int64_t sig_count = os_event_reset(dict_stats_disabled_event);

  innodb_dict_stats_disabled_debug = disable;

  if (disable) {
    os_event_set(dict_stats_event);
    os_event_wait_low(dict_stats_disabled_event, sig_count);
  }
}
#endif /* UNIV_DEBUG */

/** This is the thread for background stats gathering. It pops tables, from
the auto recalc list and proceeds them, eventually recalculating their
statistics. */
void dict_stats_thread() {
  ut_a(!srv_read_only_mode);
  THD *thd = create_internal_thd();

  while (!SHUTTING_DOWN()) {
    /* Wake up periodically even if not signaled. This is
    because we may lose an event - if the below call to
    dict_stats_process_entry_from_recalc_pool() puts the entry back
    in the list, the os_event_set() will be lost by the subsequent
    os_event_reset(). */
    os_event_wait_time(dict_stats_event, MIN_RECALC_INTERVAL);

#ifdef UNIV_DEBUG
    while (innodb_dict_stats_disabled_debug) {
      os_event_set(dict_stats_disabled_event);
      if (SHUTTING_DOWN()) {
        break;
      }
      os_event_wait_time(dict_stats_event, std::chrono::milliseconds{100});
    }
#endif /* UNIV_DEBUG */

    if (SHUTTING_DOWN()) {
      break;
    }

    dict_stats_process_entry_from_recalc_pool(thd);

    os_event_reset(dict_stats_event);
  }

  destroy_internal_thd(thd);
}

/** Shutdown the dict stats thread. */
void dict_stats_shutdown() {
  os_event_set(dict_stats_event);
  srv_threads.m_dict_stats.join();
}
