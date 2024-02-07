/*****************************************************************************

Copyright (c) 2012, 2024, Oracle and/or its affiliates.

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

/** @file include/dict0stats_bg.h
 Code used for background table and index stats gathering.

 Created Apr 26, 2012 Vasil Dimov
 *******************************************************/

#ifndef dict0stats_bg_h
#define dict0stats_bg_h

#include "univ.i"

#include "dict0types.h"
#include "os0event.h"
#include "os0thread.h"
#include "row0mysql.h"

/** Event to wake up the stats thread */
extern os_event_t dict_stats_event;

#ifdef HAVE_PSI_INTERFACE
extern mysql_pfs_key_t dict_stats_recalc_pool_mutex_key;
#endif /* HAVE_PSI_INTERFACE */

#ifdef UNIV_DEBUG
/** Value of MySQL global used to disable dict_stats thread. */
extern bool innodb_dict_stats_disabled_debug;
#endif /* UNIV_DEBUG */

/** Add a table to the recalc pool, which is processed by the
 background stats gathering thread. Only the table id is added to the
 list, so the table can be closed after being enqueued and it will be
 opened when needed. If the table does not exist later (has been DROPped),
 then it will be removed from the pool and skipped. */
void dict_stats_recalc_pool_add(
    const dict_table_t *table); /*!< in: table to add */

/** Delete a given table from the auto recalc pool.
 dict_stats_recalc_pool_del() */
void dict_stats_recalc_pool_del(
    const dict_table_t *table); /*!< in: table to remove */

/** Yield the data dictionary latch when waiting
for the background thread to stop accessing a table.
@param[in] trx transaction holding the data dictionary locks
@param[in] location location where called */
static inline void DICT_STATS_BG_YIELD(trx_t *trx, ut::Location location) {
  row_mysql_unlock_data_dictionary(trx);
  std::this_thread::sleep_for(std::chrono::milliseconds(250));
  row_mysql_lock_data_dictionary(trx, location);
}

/** Request the background collection of statistics to stop for a table.
 @retval true when no background process is active
 @retval false when it is not safe to modify the table definition */
[[nodiscard]] static inline bool dict_stats_stop_bg(
    dict_table_t *table); /*!< in/out: table */

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
    trx_t *trx);         /*!< in/out: transaction to use for
                         unlocking/locking the data dict */
/** Initialize global variables needed for the operation of dict_stats_thread().
 Must be called before dict_stats_thread() is started. */
void dict_stats_thread_init();

/** Free resources allocated by dict_stats_thread_init(), must be called
 after dict_stats_thread() has exited. */
void dict_stats_thread_deinit();

#ifdef UNIV_DEBUG
/** Disables dict stats thread. It's used by:
        SET GLOBAL innodb_dict_stats_disabled_debug = 1 (0).
@param[in]      thd             thread handle
@param[in]      var             pointer to system variable
@param[out]     var_ptr         where the formal string goes
@param[in]      save            immediate result from check function */
void dict_stats_disabled_debug_update(THD *thd, SYS_VAR *var, void *var_ptr,
                                      const void *save);
#endif /* UNIV_DEBUG */

/** This is the thread for background stats gathering. It pops tables, from
the auto recalc list and proceeds them, eventually recalculating their
statistics. */
void dict_stats_thread();

/** Shutdown the dict stats thread. */
void dict_stats_shutdown();

#include "dict0stats_bg.ic"

#endif /* dict0stats_bg_h */
