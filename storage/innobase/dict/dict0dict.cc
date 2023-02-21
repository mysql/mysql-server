/*****************************************************************************

Copyright (c) 1996, 2023, Oracle and/or its affiliates.
Copyright (c) 2012, Facebook Inc.

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

/** @file dict/dict0dict.cc
 Data dictionary system

 Created 1/8/1996 Heikki Tuuri
 ***********************************************************************/

#include "debug_sync.h"
#include "my_config.h"

#include <stdlib.h>
#include <strfunc.h>
#include <sys/types.h>
#include <algorithm>
#include <string>

#ifndef UNIV_HOTBACKUP
#include "current_thd.h"
#endif /* !UNIV_HOTBACKUP */
#include "dict0dict.h"
#include "fil0fil.h"
#ifndef UNIV_HOTBACKUP
#include "fts0fts.h"
#endif /* !UNIV_HOTBACKUP */
#include "ha_prototypes.h"
#include "log0chkp.h"
#include "log0write.h"
#include "my_dbug.h"

#ifndef UNIV_HOTBACKUP
#include "clone0api.h"
#include "mysqld.h"  // system_charset_info
#include "que0types.h"
#include "row0sel.h"
#endif /* !UNIV_HOTBACKUP */

#if defined UNIV_HOTBACKUP && defined UNIV_DEBUG
static inline bool dict_non_lru_find_table(const dict_table_t *x) {
  return true;
}
#endif /* UNIV_HOTBACKUP  && UNIV_DEBUG */

/** dummy index for ROW_FORMAT=REDUNDANT supremum and infimum records */
dict_index_t *dict_ind_redundant;

#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
/** Flag to control insert buffer debugging. */
extern uint ibuf_debug;
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

#include <algorithm>
#include <vector>

#include "btr0btr.h"
#include "btr0cur.h"
#include "btr0sea.h"
#include "buf0buf.h"
#include "data0type.h"
#include "dict0boot.h"
#include "dict0crea.h"
#ifndef UNIV_HOTBACKUP
#include "dict0dd.h"
#endif /* !UNIV_HOTBACKUP */
#include "dict0mem.h"
#include "dict0priv.h"
#ifndef UNIV_HOTBACKUP
#include "dict0stats.h"
#endif /* !UNIV_HOTBACKUP */
#include "fsp0sysspace.h"
#ifndef UNIV_HOTBACKUP
#include "fts0fts.h"
#include "fts0types.h"
#include "lock0lock.h"
#endif /* !UNIV_HOTBACKUP */
#include "mach0data.h"
#include "mem0mem.h"
#include "os0once.h"
#include "page0page.h"
#include "page0zip.h"
#ifndef UNIV_HOTBACKUP
#include "pars0pars.h"
#include "pars0sym.h"
#include "que0que.h"
#endif /* !UNIV_HOTBACKUP */
#include "rem0cmp.h"
#include "row0ins.h"
#include "row0log.h"
#ifndef UNIV_HOTBACKUP
#include "ddl0ddl.h"
#include "row0mysql.h"
#endif /* !UNIV_HOTBACKUP */
#include "row0upd.h"
#ifndef UNIV_HOTBACKUP
#include "ha_innodb.h"
#include "srv0mon.h"
#include "srv0start.h"
#include "sync0sync.h"
#include "trx0undo.h"
#include "ut0new.h"
#endif /* !UNIV_HOTBACKUP */

static_assert(DATA_ROW_ID == 0, "DATA_ROW_ID != 0");
static_assert(DATA_TRX_ID == 1, "DATA_TRX_ID != 1");
static_assert(DATA_ROLL_PTR == 2, "DATA_ROLL_PTR != 2");
static_assert(DATA_N_SYS_COLS == 3, "DATA_N_SYS_COLS != 3");
static_assert(DATA_TRX_ID_LEN == 6, "DATA_TRX_ID_LEN != 6");
static_assert(DATA_ITT_N_SYS_COLS == 2, "DATA_ITT_N_SYS_COLS != 2");

/** the dictionary system */
dict_sys_t *dict_sys = nullptr;

/** The set of SE private IDs of DD tables. Used to tell whether a table is
a DD table. Since the DD tables can be rebuilt with new SE private IDs,
this set replaces checks based on ranges of IDs. */
std::set<dd::Object_id> dict_sys_t::s_dd_table_ids = {};

/** The name of the data dictionary tablespace. */
const char *dict_sys_t::s_dd_space_name = "mysql";

/** The file name of the data dictionary tablespace */
const char *dict_sys_t::s_dd_space_file_name = "mysql.ibd";

/** The name of the hard-coded system tablespace. */
const char *dict_sys_t::s_sys_space_name = "innodb_system";

/** The name of the predefined temporary tablespace. */
const char *dict_sys_t::s_temp_space_name = "innodb_temporary";

/** The file name of the predefined temporary tablespace */
const char *dict_sys_t::s_temp_space_file_name = "ibtmp1";

/** The hard-coded tablespace name innodb_file_per_table. */
const char *dict_sys_t::s_file_per_table_name = "innodb_file_per_table";

/** These two undo tablespaces cannot be dropped. */
const char *dict_sys_t::s_default_undo_space_name_1 = "innodb_undo_001";
const char *dict_sys_t::s_default_undo_space_name_2 = "innodb_undo_002";

/** the dictionary persisting structure */
dict_persist_t *dict_persist = nullptr;

/** @brief the data dictionary rw-latch protecting dict_sys

table create, drop, etc. reserve this in X-mode; implicit or
background operations purge, rollback, foreign key checks reserve this
in S-mode; we cannot trust that MySQL protects implicit or background
operations a table drop since MySQL does not know of them; therefore
we need this; NOTE: a transaction which reserves this must keep book
on the mode in trx_t::dict_operation_lock_mode */
rw_lock_t *dict_operation_lock;

/** Percentage of compression failures that are allowed in a single
round */
ulong zip_failure_threshold_pct = 5;

/** Maximum percentage of a page that can be allowed as a pad to avoid
compression failures */
ulong zip_pad_max = 50;

/** buffer pool max size per table  hash table fixed size in bytes */
constexpr uint32_t DICT_POOL_PER_TABLE_HASH = 512;

#ifndef UNIV_HOTBACKUP
/** Identifies generated InnoDB foreign key names */
static char dict_ibfk[] = "_ibfk_";

/** Array to store table_ids of INNODB_SYS_* TABLES */
static table_id_t dict_sys_table_id[SYS_NUM_SYSTEM_TABLES];

/** Tries to find column names for the index and sets the col field of the
index.
@param[in]      table   table
@param[in,out]  index   index
@param[in]      add_v   new virtual columns added along with an add index call
@return true if the column names were found */
static bool dict_index_find_and_set_cols(const dict_table_t *table,
                                         dict_index_t *index,
                                         const dict_add_v_col_t *add_v);
/** Builds the internal dictionary cache representation for a clustered
 index, containing also system fields not defined by the user.
 @return own: the internal representation of the clustered index */
static dict_index_t *dict_index_build_internal_clust(
    const dict_table_t *table, /*!< in: table */
    dict_index_t *index);      /*!< in: user representation of
                               a clustered index */
/** Builds the internal dictionary cache representation for a non-clustered
 index, containing also system fields not defined by the user.
 @return own: the internal representation of the non-clustered index */
static dict_index_t *dict_index_build_internal_non_clust(
    const dict_table_t *table, /*!< in: table */
    dict_index_t *index);      /*!< in: user representation of
                               a non-clustered index */
/** Builds the internal dictionary cache representation for an FTS index.
 @return own: the internal representation of the FTS index */
static dict_index_t *dict_index_build_internal_fts(
    dict_table_t *table,  /*!< in: table */
    dict_index_t *index); /*!< in: user representation of an FTS index */

/** Removes an index from the dictionary cache. */
static void dict_index_remove_from_cache_low(
    dict_table_t *table, /*!< in/out: table */
    dict_index_t *index, /*!< in, own: index */
    bool lru_evict);     /*!< in: true if page being evicted
                         to make room in the table LRU list */

/** Calculate and update the redo log margin for current tables which
have some changed dynamic metadata in memory and have not been written
back to mysql.innodb_dynamic_metadata. Update LSN limit, which is used
to stop user threads when redo log is running out of space and they
do not hold latches (log.free_check_limit_lsn). */
static void dict_persist_update_log_margin(void);

/** Removes a table object from the dictionary cache. */
static void dict_table_remove_from_cache_low(
    dict_table_t *table, /*!< in, own: table */
    bool lru_evict);     /*!< in: true if evicting from LRU */

#ifdef UNIV_DEBUG
/** Validate the dictionary table LRU list.
 @return true if validate OK */
static bool dict_lru_validate();
/** Check if table is in the dictionary table LRU list.
 @param[in] find_table table to find
 @return true if table found in LRU list. */
static bool dict_lru_find_table(const dict_table_t *find_table);
/** Check if a table exists in the dict table non-LRU list.
 @param[in] find_table table to find
 @return true if table found in non-LRU list */
static bool dict_non_lru_find_table(const dict_table_t *find_table);
#endif /* UNIV_DEBUG */

/* Stream for storing detailed information about the latest foreign key
and unique key errors. Only created if !srv_read_only_mode */
FILE *dict_foreign_err_file = nullptr;
/* mutex protecting the foreign and unique error buffers */
ib_mutex_t dict_foreign_err_mutex;

/** Checks if the database name in two table names is the same.
 @return true if same db name */
bool dict_tables_have_same_db(const char *name1, /*!< in: table name in the
                                                  form dbname '/' tablename */
                              const char *name2) /*!< in: table name in the
                                                 form dbname '/' tablename */
{
  for (; *name1 == *name2; name1++, name2++) {
    if (*name1 == '/') {
      return true;
    }
    ut_a(*name1); /* the names must contain '/' */
  }
  return false;
}

/** Return the end of table name where we have removed dbname and '/'.
 @return table name */
const char *dict_remove_db_name(const char *name) /*!< in: table name in the
                                                  form dbname '/' tablename */
{
  const char *s = strchr(name, '/');
  ut_a(s);

  return (s + 1);
}
#endif /* !UNIV_HOTBACKUP */

/** Get the database name length in a table name.
 @return database name length */
ulint dict_get_db_name_len(const char *name) /*!< in: table name in the form
                                             dbname '/' tablename */
{
  const char *s;
  s = strchr(name, '/');
  if (s == nullptr) {
    return (0);
  }
  return (s - name);
}

#ifndef UNIV_HOTBACKUP
/** Reserves the dictionary system mutex for MySQL. */
void dict_mutex_enter_for_mysql(void) { dict_sys_mutex_enter(); }

/** Releases the dictionary system mutex for MySQL. */
void dict_mutex_exit_for_mysql(void) { dict_sys_mutex_exit(); }

/** Allocate and init a dict_table_t's stats latch.
This function must not be called concurrently on the same table object.
@param[in,out]  table_void      table whose stats latch to create */
static void dict_table_stats_latch_alloc(void *table_void) {
  dict_table_t *table = static_cast<dict_table_t *>(table_void);

  /* Note: rw_lock_create() will call the constructor */

  table->stats_latch = static_cast<rw_lock_t *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, sizeof(rw_lock_t)));

  ut_a(table->stats_latch != nullptr);

  rw_lock_create(dict_table_stats_key, table->stats_latch,
                 LATCH_ID_DICT_TABLE_STATS);
}

/** Deinit and free a dict_table_t's stats latch.
This function must not be called concurrently on the same table object.
@param[in,out]  table   table whose stats latch to free */
static void dict_table_stats_latch_free(dict_table_t *table) {
  rw_lock_free(table->stats_latch);
  ut::free(table->stats_latch);
}

/** Create a dict_table_t's stats latch or delay for lazy creation.
This function is only called from either single threaded environment
or from a thread that has not shared the table object with other threads.
@param[in,out]  table   table whose stats latch to create
@param[in]      enabled if false then the latch is disabled
and dict_table_stats_lock()/unlock() become noop on this table. */
void dict_table_stats_latch_create(dict_table_t *table, bool enabled) {
  if (!enabled) {
    table->stats_latch = nullptr;
    table->stats_latch_created = os_once::DONE;
    return;
  }

  /* We create this lazily the first time it is used. */
  table->stats_latch = nullptr;
  table->stats_latch_created = os_once::NEVER_DONE;
}

/** Destroy a dict_table_t's stats latch.
This function is only called from either single threaded environment
or from a thread that has not shared the table object with other threads.
@param[in,out]  table   table whose stats latch to destroy */
void dict_table_stats_latch_destroy(dict_table_t *table) {
  if (table->stats_latch_created == os_once::DONE &&
      table->stats_latch != nullptr) {
    dict_table_stats_latch_free(table);
  }
}

/** Lock the appropriate latch to protect a given table's statistics.
@param[in]      table           table whose stats to lock
@param[in]      latch_mode      RW_S_LATCH or RW_X_LATCH */
void dict_table_stats_lock(dict_table_t *table, ulint latch_mode) {
  ut_ad(table != nullptr);
  ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

  os_once::do_or_wait_for_done(&table->stats_latch_created,
                               dict_table_stats_latch_alloc, table);

  if (table->stats_latch == nullptr) {
    /* This is a dummy table object that is private in the current
    thread and is not shared between multiple threads, thus we
    skip any locking. */
    return;
  }

  switch (latch_mode) {
    case RW_S_LATCH:
      rw_lock_s_lock(table->stats_latch, UT_LOCATION_HERE);
      break;
    case RW_X_LATCH:
      rw_lock_x_lock(table->stats_latch, UT_LOCATION_HERE);
      break;
    case RW_NO_LATCH:
      [[fallthrough]];
    default:
      ut_error;
  }
}

/** Unlock the latch that has been locked by dict_table_stats_lock().
@param[in]      table           table whose stats to unlock
@param[in]      latch_mode      RW_S_LATCH or RW_X_LATCH */
void dict_table_stats_unlock(dict_table_t *table, ulint latch_mode) {
  ut_ad(table != nullptr);
  ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

  if (table->stats_latch == nullptr) {
    /* This is a dummy table object that is private in the current
    thread and is not shared between multiple threads, thus we
    skip any locking. */
    return;
  }

  switch (latch_mode) {
    case RW_S_LATCH:
      rw_lock_s_unlock(table->stats_latch);
      break;
    case RW_X_LATCH:
      rw_lock_x_unlock(table->stats_latch);
      break;
    case RW_NO_LATCH:
      [[fallthrough]];
    default:
      ut_error;
  }
}

/** Try to drop any indexes after an aborted index creation.
 This can also be after a server kill during DROP INDEX. */
static void dict_table_try_drop_aborted(
    dict_table_t *table, /*!< in: table, or NULL if it
                         needs to be looked up again */
    table_id_t table_id, /*!< in: table identifier */
    ulint ref_count)     /*!< in: expected table->n_ref_count */
{
  trx_t *trx;

  trx = trx_allocate_for_background();
  trx->op_info = "try to drop any indexes after an aborted index creation";
  row_mysql_lock_data_dictionary(trx, UT_LOCATION_HERE);
  trx_set_dict_operation(trx, TRX_DICT_OP_INDEX);

  if (table == nullptr) {
    table = dd_table_open_on_id(table_id, nullptr, nullptr, true, true);

    /* Decrement the ref count. The table is MDL locked, so should
    not be dropped */
    if (table) {
      dd_table_close(table, nullptr, nullptr, true);
    }
  } else {
    ut_ad(table->id == table_id);
  }

  if (table && table->get_ref_count() == ref_count && table->drop_aborted) {
    /* Silence a debug assertion in ddl::drop_indexes(). */
    ut_d(table->acquire());
    ddl::drop_indexes(trx, table, true);
    ut_d(table->release());
    ut_ad(table->get_ref_count() == ref_count);
    trx_commit_for_mysql(trx);
  }

  row_mysql_unlock_data_dictionary(trx);
  trx_free_for_background(trx);
}

/** When opening a table,
 try to drop any indexes after an aborted index creation.
 Release the dict_sys->mutex. */
static void dict_table_try_drop_aborted_and_mutex_exit(
    dict_table_t *table, /*!< in: table (may be NULL) */
    bool try_drop)       /*!< in: false if should try to
                         drop indexes whose online creation
                         was aborted */
{
  if (try_drop && table != nullptr && table->drop_aborted &&
      table->get_ref_count() == 1 && table->first_index()) {
    /* Attempt to drop the indexes whose online creation
    was aborted. */
    table_id_t table_id = table->id;

    dict_sys_mutex_exit();

    dict_table_try_drop_aborted(table, table_id, 1);
  } else {
    dict_sys_mutex_exit();
  }
}
#endif /* !UNIV_HOTBACKUP */

/** Decrements the count of open handles to a table.
@param[in,out] table Table
@param[in] dict_locked True=data dictionary locked
@param[in] try_drop True=try to drop any orphan indexes after an aborted online
index creation */
void dict_table_close(dict_table_t *table, bool dict_locked, bool try_drop) {
  ut_a(table->get_ref_count() > 0);

#ifndef UNIV_HOTBACKUP
#ifdef UNIV_DEBUG
  if (!table->is_intrinsic()) {
    /* This is now only for validation in debug mode */
    if (!dict_locked) {
      dict_sys_mutex_enter();
    }

    ut_ad(dict_lru_validate());

    if (table->can_be_evicted) {
      ut_ad(dict_lru_find_table(table));
    } else {
      ut_ad(dict_non_lru_find_table(table));
    }

    if (!dict_locked) {
      dict_sys_mutex_exit();
    }
  }
#endif /* UNIV_DEBUG */
#endif /* !UNIV_HOTBACKUP */

  if (!table->is_intrinsic()) {
    /* Ask for lock to prevent concurrent table open,
    in case the race of n_ref_count and stat_initialized in
    dict_stats_deinit(). See dict_table_t::acquire_with_lock() too.
    We don't actually need dict_sys mutex any more here. */
    table->lock();
  }

  auto drop_aborted = try_drop && table->drop_aborted &&
                      table->get_ref_count() == 1 && table->first_index();

  table->release();

#ifndef UNIV_HOTBACKUP
  /* Intrinsic table is not added to dictionary cache so skip other
  cache specific actions. */
  if (table->is_intrinsic()) {
    return;
  }

  /* Force persistent stats re-read upon next open of the table
  so that FLUSH TABLE can be used to forcibly fetch stats from disk
  if they have been manually modified. We reset table->stat_initialized
  only if table reference count is 0 because we do not want too frequent
  stats re-reads (e.g. in other cases than FLUSH TABLE). */
  if (strchr(table->name.m_name, '/') != nullptr &&
      table->get_ref_count() == 0 && dict_stats_is_persistent_enabled(table)) {
    DEBUG_SYNC(current_thd, "innodb.before_stats_deinit");
    dict_stats_deinit(table);
  }

  if (!dict_locked) {
    if (drop_aborted) {
      ut_d(ut_error);
      ut_o(dict_table_try_drop_aborted(nullptr, table->id, 0));
    }
  }
#endif /* !UNIV_HOTBACKUP */

  if (!table->is_intrinsic()) {
    table->unlock();
  }
}

#ifndef UNIV_HOTBACKUP
/** Closes the only open handle to a table and drops a table while assuring
 that dict_sys->mutex is held the whole time.  This assures that the table
 is not evicted after the close when the count of open handles goes to zero.
 Because dict_sys->mutex is held, we do not need to call
 dict_table_prevent_eviction().  */
void dict_table_close_and_drop(
    trx_t *trx,          /*!< in: data dictionary transaction */
    dict_table_t *table) /*!< in/out: table */
{
  ut_ad(dict_sys_mutex_own());
  ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));
  ut_ad(trx_state_eq(trx, TRX_STATE_ACTIVE));

  dict_table_close(table, true, false);

#if defined UNIV_DEBUG || defined UNIV_DDL_DEBUG
  /* Nobody should have initialized the stats of the newly created
  table when this is called. So we know that it has not been added
  for background stats gathering. */
  ut_a(!table->stat_initialized);
#endif /* UNIV_DEBUG || UNIV_DDL_DEBUG */

  ddl::drop_table(trx, table);
}

/** Check if the table has a given (non_virtual) column.
@param[in]      table           table object
@param[in]      col_name        column name
@param[in]      col_nr          column number guessed, 0 as default
@return column number if the table has the specified column,
otherwise table->n_def */
ulint dict_table_has_column(const dict_table_t *table, const char *col_name,
                            ulint col_nr) {
  ulint col_max = table->n_def;

  ut_ad(table);
  ut_ad(col_name);
  ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

  if (col_nr < col_max &&
      innobase_strcasecmp(col_name, table->get_col_name(col_nr)) == 0) {
    return (col_nr);
  }

  /** The order of column may changed, check it with other columns */
  for (ulint i = 0; i < col_max; i++) {
    if (i != col_nr &&
        innobase_strcasecmp(col_name, table->get_col_name(i)) == 0) {
      return (i);
    }
  }

  return (col_max);
}

/** Returns a virtual column's name.
@param[in]      table   target table
@param[in]      col_nr  virtual column number (nth virtual column)
@return column name or NULL if column number out of range. */
const char *dict_table_get_v_col_name(const dict_table_t *table, ulint col_nr) {
  const char *s;

  ut_ad(table);
  ut_ad(col_nr < table->n_v_def);
  ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

  if (col_nr >= table->n_v_def) {
    return (nullptr);
  }

  s = table->v_col_names;

  if (s != nullptr) {
    for (ulint i = 0; i < col_nr; i++) {
      s += strlen(s) + 1;
    }
  }

  return (s);
}

/** Search virtual column's position in InnoDB according to its position
in original table's position
@param[in]      table   target table
@param[in]      col_nr  column number (nth column in the MySQL table)
@return virtual column's position in InnoDB, ULINT_UNDEFINED if not find */
static ulint dict_table_get_v_col_pos_for_mysql(const dict_table_t *table,
                                                ulint col_nr) {
  ulint i;

  ut_ad(table);
  ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

  for (i = 0; i < table->n_v_def; i++) {
    if (col_nr == dict_get_v_col_mysql_pos(table->v_cols[i].m_col.ind)) {
      break;
    }
  }

  if (i == table->n_v_def) {
    return (ULINT_UNDEFINED);
  }

  ut_ad(col_nr < static_cast<ulint>(table->n_t_def));

  return (i);
}

/** Returns a virtual column's name according to its original
MySQL table position.
@param[in]      table   target table
@param[in]      col_nr  column number (nth column in the table)
@return column name. */
const char *dict_table_get_v_col_name_mysql(const dict_table_t *table,
                                            ulint col_nr) {
  ulint i = dict_table_get_v_col_pos_for_mysql(table, col_nr);

  if (i == ULINT_UNDEFINED) {
    return (nullptr);
  }

  return (dict_table_get_v_col_name(table, i));
}

/** Get nth virtual column according to its original MySQL table position
@param[in]      table   target table
@param[in]      col_nr  column number in MySQL Table definition
@return dict_v_col_t ptr */
dict_v_col_t *dict_table_get_nth_v_col_mysql(const dict_table_t *table,
                                             ulint col_nr) {
  ulint i = dict_table_get_v_col_pos_for_mysql(table, col_nr);

  if (i == ULINT_UNDEFINED) {
    return (nullptr);
  }

  return (dict_table_get_nth_v_col(table, i));
}

/** Allocate and init the autoinc latch of a given table.
This function must not be called concurrently on the same table object.
@param[in,out]  table_void      table whose autoinc latch to create */
static void dict_table_autoinc_alloc(void *table_void) {
  dict_table_t *table = static_cast<dict_table_t *>(table_void);

  table->autoinc_mutex = ut::new_withkey<ib_mutex_t>(UT_NEW_THIS_FILE_PSI_KEY);
  ut_a(table->autoinc_mutex != nullptr);
  mutex_create(LATCH_ID_AUTOINC, table->autoinc_mutex);

  table->autoinc_persisted_mutex =
      ut::new_withkey<ib_mutex_t>(UT_NEW_THIS_FILE_PSI_KEY);
  ut_a(table->autoinc_persisted_mutex != nullptr);
  mutex_create(LATCH_ID_PERSIST_AUTOINC, table->autoinc_persisted_mutex);
}

/** Allocate and init the zip_pad_mutex of a given index.
This function must not be called concurrently on the same index object.
@param[in,out]  index_void      index whose zip_pad_mutex to create */
static void dict_index_zip_pad_alloc(void *index_void) {
  dict_index_t *index = static_cast<dict_index_t *>(index_void);
  index->zip_pad.mutex = ut::new_withkey<SysMutex>(UT_NEW_THIS_FILE_PSI_KEY);
  ut_a(index->zip_pad.mutex != nullptr);
  mutex_create(LATCH_ID_ZIP_PAD_MUTEX, index->zip_pad.mutex);
}

/** Acquire the autoinc lock. */
void dict_table_autoinc_lock(dict_table_t *table) /*!< in/out: table */
{
  os_once::do_or_wait_for_done(&table->autoinc_mutex_created,
                               dict_table_autoinc_alloc, table);

  mutex_enter(table->autoinc_mutex);
}

/** Acquire the zip_pad_mutex latch.
@param[in,out]  index   the index whose zip_pad_mutex to acquire.*/
static void dict_index_zip_pad_lock(dict_index_t *index) {
  os_once::do_or_wait_for_done(&index->zip_pad.mutex_created,
                               dict_index_zip_pad_alloc, index);

  mutex_enter(index->zip_pad.mutex);
}

/** Unconditionally set the autoinc counter.
@param[in,out] table Table
@param[in] value Next value to assign to a row */
void dict_table_autoinc_initialize(dict_table_t *table, uint64_t value) {
  ut_ad(dict_table_autoinc_own(table));

  table->autoinc = value;
}

bool dict_table_autoinc_log(dict_table_t *table, uint64_t value, mtr_t *mtr) {
  bool log = false;

  mutex_enter(table->autoinc_persisted_mutex);

  if (table->autoinc_persisted < value) {
    dict_table_autoinc_persisted_update(table, value);

    /* The only concern here is some concurrent thread may
    change the dirty_status to METADATA_BUFFERED. And the
    only function is dict_table_persist_to_dd_table_buffer_low(),
    which could be called by checkpoint and will first set the
    dirty_status to METADATA_BUFFERED, and then write back
    the latest changes to DDTableBuffer, all of which are under
    protection of dict_persist->mutex.

    If that function sets the dirty_status to METADATA_BUFFERED
    first, below checking will force current thread to wait on
    dict_persist->mutex. Above update to AUTOINC would be either
    written back to DDTableBuffer or not. But the redo logs for
    current change won't be counted into current checkpoint.
    See how log_sys->dict_max_allowed_checkpoint_lsn is set.
    So even a crash after below redo log flushed, no change lost.

    If that function sets the dirty_status after below checking,
    which means current change would be written back to
    DDTableBuffer. It's also safe. */
    if (table->dirty_status.load() == METADATA_DIRTY) {
      ut_ad(table->in_dirty_dict_tables_list);
    } else {
      dict_table_mark_dirty(table);
    }

    log = true;
  }

  mutex_exit(table->autoinc_persisted_mutex);

  if (log) {
    PersistentTableMetadata metadata(table->id, table->version);
    metadata.set_autoinc(value);

    Persister *persister = dict_persist->persisters->get(PM_TABLE_AUTO_INC);
    persister->write_log(table->id, metadata, mtr);
    /* No need to flush due to performance reason */
  }

  /* Check and return if auto increment is to be persisted. */
  return (log && dict_persist->check_persist_immediately());
}

/** Get all the FTS indexes on a table.
@param[in]      table   table
@param[out]     indexes all FTS indexes on this table
@return number of FTS indexes */
ulint dict_table_get_all_fts_indexes(dict_table_t *table,
                                     ib_vector_t *indexes) {
  dict_index_t *index;

  ut_a(ib_vector_size(indexes) == 0);

  for (index = table->first_index(); index; index = index->next()) {
    if (index->type == DICT_FTS) {
      ib_vector_push(indexes, &index);
    }
  }

  return (ib_vector_size(indexes));
}

/** Reads the next autoinc value (== autoinc counter value), 0 if not yet
 initialized.
 @return value for a new row, or 0 */
uint64_t dict_table_autoinc_read(const dict_table_t *table) /*!< in: table */
{
  ut_ad(dict_table_autoinc_own(table));

  return (table->autoinc);
}

/** Updates the autoinc counter if the value supplied is greater than the
 current value.
@param[in,out] table Table
@param[in] value Value which was assigned to a row */
void dict_table_autoinc_update_if_greater(dict_table_t *table, uint64_t value) {
  ut_ad(dict_table_autoinc_own(table));

  if (value > table->autoinc) {
    table->autoinc = value;
  }
}

/** Release the autoinc lock. */
void dict_table_autoinc_unlock(dict_table_t *table) /*!< in/out: table */
{
  mutex_exit(table->autoinc_mutex);
}

bool dict_index_contains_col_or_prefix(const dict_index_t *index, ulint n,
                                       bool is_virtual) {
  const dict_field_t *field;
  const dict_col_t *col;
  ulint pos;
  ulint n_fields;

  ut_ad(index);
  ut_ad(index->magic_n == DICT_INDEX_MAGIC_N);

  if (index->is_clustered()) {
    return true;
  }

  if (is_virtual) {
    col = &dict_table_get_nth_v_col(index->table, n)->m_col;
  } else {
    col = index->table->get_col(n);
  }

  n_fields = dict_index_get_n_fields(index);

  for (pos = 0; pos < n_fields; pos++) {
    field = index->get_field(pos);

    if (col == field->col) {
      return true;
    }
  }

  return false;
}

/** Looks for a matching field in an index. The column has to be the same. The
 column in index must be complete, or must contain a prefix longer than the
 column in index2. That is, we must be able to construct the prefix in index2
 from the prefix in index.
 @return position in internal representation of the index;
 ULINT_UNDEFINED if not contained */
ulint dict_index_get_nth_field_pos(
    const dict_index_t *index,  /*!< in: index from which to search */
    const dict_index_t *index2, /*!< in: index */
    ulint n)                    /*!< in: field number in index2 */
{
  const dict_field_t *field;
  const dict_field_t *field2;
  ulint n_fields;
  ulint pos;

  ut_ad(index);
  ut_ad(index->magic_n == DICT_INDEX_MAGIC_N);

  field2 = index2->get_field(n);

  n_fields = dict_index_get_n_fields(index);

  /* Are we looking for a MBR (Minimum Bound Box) field of
  a spatial index */
  bool is_mbr_fld = (n == 0 && dict_index_is_spatial(index2));

  for (pos = 0; pos < n_fields; pos++) {
    field = index->get_field(pos);

    /* The first field of a spatial index is a transformed
    MBR (Minimum Bound Box) field made out of original column,
    so its field->col still points to original cluster index
    col, but the actual content is different. So we cannot
    consider them equal if neither of them is MBR field */
    if (pos == 0 && dict_index_is_spatial(index) && !is_mbr_fld) {
      continue;
    }

    if (field->col == field2->col &&
        (field->prefix_len == 0 || (field->prefix_len >= field2->prefix_len &&
                                    field2->prefix_len != 0))) {
      return (pos);
    }
  }

  return (ULINT_UNDEFINED);
}

/** Looks for non-virtual column n position in the clustered index.
 @return position in internal representation of the clustered index */
ulint dict_table_get_nth_col_pos(const dict_table_t *table, /*!< in: table */
                                 ulint n) /*!< in: column number */
{
  return (table->first_index()->get_col_pos(n));
}

/** Get the innodb column position for a non-virtual column according to
its original MySQL table position n
@param[in]      table   table
@param[in]      n       MySQL column position
@return column position in InnoDB */
ulint dict_table_mysql_pos_to_innodb(const dict_table_t *table, ulint n) {
  ut_ad(n < table->n_t_cols);

  if (table->n_v_def == 0) {
    /* No virtual columns, the MySQL position is the same
    as InnoDB position */
    return (n);
  }

  /* Find out how many virtual columns are stored in front of 'n' */
  ulint v_before = 0;
  for (ulint i = 0; i < table->n_v_def; ++i) {
    if (table->v_cols[i].m_col.ind > n) {
      break;
    }

    ++v_before;
  }

  ut_ad(n >= v_before);

  return (n - v_before);
}

/** Checks if a column is in the ordering columns of the clustered index of a
 table. Column prefixes are treated like whole columns.
 @return true if the column, or its prefix, is in the clustered key */
bool dict_table_col_in_clustered_key(
    const dict_table_t *table, /*!< in: table */
    ulint n)                   /*!< in: column number */
{
  const dict_index_t *index;
  const dict_field_t *field;
  const dict_col_t *col;
  ulint pos;
  ulint n_fields;

  ut_ad(table);

  col = table->get_col(n);

  index = table->first_index();

  n_fields = dict_index_get_n_unique(index);

  for (pos = 0; pos < n_fields; pos++) {
    field = index->get_field(pos);

    if (col == field->col) {
      return true;
    }
  }

  return false;
}
#endif /* !UNIV_HOTBACKUP */

/** Inits the data dictionary module. */
void dict_init(void) {
  dict_operation_lock = static_cast<rw_lock_t *>(ut::zalloc_withkey(
      UT_NEW_THIS_FILE_PSI_KEY, sizeof(*dict_operation_lock)));

  dict_sys = static_cast<dict_sys_t *>(
      ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, sizeof(*dict_sys)));

  UT_LIST_INIT(dict_sys->table_LRU);
  UT_LIST_INIT(dict_sys->table_non_LRU);

  mutex_create(LATCH_ID_DICT_SYS, &dict_sys->mutex);

  dict_sys->table_hash = ut::new_<hash_table_t>(
      buf_pool_get_curr_size() / (DICT_POOL_PER_TABLE_HASH * UNIV_WORD_SIZE));

  dict_sys->table_id_hash = ut::new_<hash_table_t>(
      buf_pool_get_curr_size() / (DICT_POOL_PER_TABLE_HASH * UNIV_WORD_SIZE));

  rw_lock_create(dict_operation_lock_key, dict_operation_lock,
                 LATCH_ID_DICT_OPERATION);

#ifndef UNIV_HOTBACKUP
  if (!srv_read_only_mode) {
    dict_foreign_err_file = os_file_create_tmpfile();
    ut_a(dict_foreign_err_file);
  }
#endif /* !UNIV_HOTBACKUP */

  mutex_create(LATCH_ID_DICT_FOREIGN_ERR, &dict_foreign_err_mutex);
}

#ifndef UNIV_HOTBACKUP
/** Move to the most recently used segment of the LRU list. */
void dict_move_to_mru(dict_table_t *table) /*!< in: table to move to MRU */
{
  ut_ad(dict_sys_mutex_own());
  ut_ad(dict_lru_validate());
  ut_ad(dict_lru_find_table(table));

  ut_a(table->can_be_evicted);

  UT_LIST_REMOVE(dict_sys->table_LRU, table);

  UT_LIST_ADD_FIRST(dict_sys->table_LRU, table);

  ut_ad(dict_lru_validate());
}

/** Returns a table object and increment its open handle count.
 NOTE! This is a high-level function to be used mainly from outside the
 'dict' module. Inside this directory dict_table_get_low
 is usually the appropriate function.
 @return table, NULL if does not exist */
dict_table_t *dict_table_open_on_name(
    const char *table_name,       /*!< in: table name */
    bool dict_locked,             /*!< in: true=data dictionary locked */
    bool try_drop,                /*!< in: true=try to drop any orphan
                                  indexes after an aborted online
                                  index creation */
    dict_err_ignore_t ignore_err) /*!< in: error to be ignored when
                                  loading a table definition */
{
  dict_table_t *table;
  DBUG_TRACE;
  DBUG_PRINT("dict_table_open_on_name", ("table: '%s'", table_name));

  if (!dict_locked) {
    dict_sys_mutex_enter();
  }

  ut_ad(table_name);
  ut_ad(dict_sys_mutex_own());

  std::string table_str(table_name);
  /* Check and convert 5.7 table name. We always keep 8.0 format name in cache
  during upgrade. */
  if (dict_name::is_partition(table_name)) {
    dict_name::rebuild(table_str);
  }
  table = dict_table_check_if_in_cache_low(table_str.c_str());

  if (table == nullptr) {
    table = dict_load_table(table_name, true, ignore_err);
  }

  ut_ad(!table || table->cached);

  if (table != nullptr) {
    if (ignore_err == DICT_ERR_IGNORE_NONE && table->is_corrupted()) {
      /* Make life easy for drop table. */
      dict_table_prevent_eviction(table);

      if (!dict_locked) {
        dict_sys_mutex_exit();
      }

      ib::info(ER_IB_MSG_175) << "Table " << table->name
                              << " is corrupted. Please drop the table"
                                 " and recreate it";
      return nullptr;
    }

    if (table->can_be_evicted) {
      dict_move_to_mru(table);
    }

    table->acquire();
  }

  ut_ad(dict_lru_validate());

  if (!dict_locked) {
    dict_table_try_drop_aborted_and_mutex_exit(table, try_drop);
  }

  return table;
}
#endif /* !UNIV_HOTBACKUP */

/** Adds system columns to a table object.
@param[in,out] table Table
@param[in] heap Temporary heap */
void dict_table_add_system_columns(dict_table_t *table, mem_heap_t *heap) {
  ut_ad(table);
  /* INSTANT DROP columns are added after system columns, so no need to
  consider drop columns count in following assert. */
  ut_ad(table->n_def == (table->n_cols - table->get_n_sys_cols()));
  ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);
  ut_ad(!table->cached);

  /* NOTE: the system columns MUST be added in the following order
  (so that they can be indexed by the numerical value of DATA_ROW_ID,
  etc.) and as the last columns of the table memory object.
  The clustered index will not always physically contain all system
  columns.
  Intrinsic table don't need DB_ROLL_PTR as UNDO logging is turned off
  for these tables. */

  const uint32_t phy_pos = UINT32_UNDEFINED;
  const uint8_t v_added = 0;
  const uint8_t v_dropped = 0;

  dict_mem_table_add_col(table, heap, "DB_ROW_ID", DATA_SYS,
                         DATA_ROW_ID | DATA_NOT_NULL, DATA_ROW_ID_LEN, false,
                         phy_pos, v_added, v_dropped);

  dict_mem_table_add_col(table, heap, "DB_TRX_ID", DATA_SYS,
                         DATA_TRX_ID | DATA_NOT_NULL, DATA_TRX_ID_LEN, false,
                         phy_pos, v_added, v_dropped);

  if (!table->is_intrinsic()) {
    dict_mem_table_add_col(table, heap, "DB_ROLL_PTR", DATA_SYS,
                           DATA_ROLL_PTR | DATA_NOT_NULL, DATA_ROLL_PTR_LEN,
                           false, phy_pos, v_added, v_dropped);

    /* This check reminds that if a new system column is added to
    the program, it should be dealt with here */
  }
}

#ifndef UNIV_HOTBACKUP
/** Mark if table has big rows.
@param[in,out]  table   table handler */
void dict_table_set_big_rows(dict_table_t *table) {
  ulint row_len = 0;
  for (ulint i = 0; i < table->n_def; i++) {
    ulint col_len = table->get_col(i)->get_max_size();

    row_len += col_len;

    /* If we have a single unbounded field, or several gigantic
    fields, mark the maximum row size as BIG_ROW_SIZE. */
    if (row_len >= BIG_ROW_SIZE || col_len >= BIG_ROW_SIZE) {
      row_len = BIG_ROW_SIZE;

      break;
    }
  }

  table->big_rows = (row_len >= BIG_ROW_SIZE) ? true : false;
}

void dict_table_add_to_cache(dict_table_t *table, bool can_be_evicted) {
  ut_ad(dict_lru_validate());
  ut_ad(dict_sys_mutex_own());

  table->cached = true;

  const auto name_hash_value = ut::hash_string(table->name.m_name);
  const auto index_id_hash_value = ut::hash_uint64(table->id);

  dict_table_set_big_rows(table);

  /* Look for a table with the same name: error if such exists */
  {
    dict_table_t *table2;
    HASH_SEARCH(name_hash, dict_sys->table_hash, name_hash_value,
                dict_table_t *, table2, ut_ad(table2->cached),
                !strcmp(table2->name.m_name, table->name.m_name));
    ut_a(table2 == nullptr);

#ifdef UNIV_DEBUG
    /* Look for the same table pointer with a different name */
    HASH_SEARCH_ALL(name_hash, dict_sys->table_hash, dict_table_t *, table2,
                    ut_ad(table2->cached), table2 == table);
    ut_ad(table2 == nullptr);
#endif /* UNIV_DEBUG */
  }

  /* Look for a table with the same id: error if such exists */
  {
    dict_table_t *table2;
    HASH_SEARCH(id_hash, dict_sys->table_id_hash, index_id_hash_value,
                dict_table_t *, table2, ut_ad(table2->cached),
                table2->id == table->id);
    ut_a(table2 == nullptr);

#ifdef UNIV_DEBUG
    /* Look for the same table pointer with a different id */
    HASH_SEARCH_ALL(id_hash, dict_sys->table_id_hash, dict_table_t *, table2,
                    ut_ad(table2->cached), table2 == table);
    ut_ad(table2 == nullptr);
#endif /* UNIV_DEBUG */
  }

  /* Add table to hash table of tables */
  HASH_INSERT(dict_table_t, name_hash, dict_sys->table_hash, name_hash_value,
              table);

  /* Add table to hash table of tables based on table id */
  HASH_INSERT(dict_table_t, id_hash, dict_sys->table_id_hash,
              index_id_hash_value, table);

  table->can_be_evicted = can_be_evicted;

  if (table->can_be_evicted) {
    UT_LIST_ADD_FIRST(dict_sys->table_LRU, table);
  } else {
    UT_LIST_ADD_FIRST(dict_sys->table_non_LRU, table);
  }

  ut_ad(dict_lru_validate());

  table->dirty_status.store(METADATA_CLEAN);

  dict_sys->size +=
      mem_heap_get_size(table->heap) + strlen(table->name.m_name) + 1;
  DBUG_EXECUTE_IF(
      "dd_upgrade", if (srv_is_upgrade_mode && srv_upgrade_old_undo_found) {
        ib::info(ER_IB_MSG_176) << "Adding table to cache: " << table->name;
      });
}

/** Test whether a table can be evicted from the LRU cache.
 @return true if table can be evicted. */
static bool dict_table_can_be_evicted(
    dict_table_t *table) /*!< in: table to test */
{
  ut_ad(dict_sys_mutex_own());
  ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));

  ut_a(table->can_be_evicted);
  ut_a(table->foreign_set.empty());
  ut_a(table->referenced_set.empty());

  if (table->get_ref_count() == 0) {
    const dict_index_t *index;

    /* The transaction commit and rollback are called from
    outside the handler interface. This means that there is
    a window where the table->n_ref_count can be zero but
    the table instance is in "use". */

    if (lock_table_has_locks(table)) {
      return false;
    }

    for (index = table->first_index(); index != nullptr;
         index = index->next()) {
      /* We are not allowed to free the in-memory index
      struct dict_index_t until all entries in the adaptive
      hash index that point to any of the page belonging to
      his b-tree index are dropped. This is so because
      dropping of these entries require access to
      dict_index_t struct. To avoid such scenario we keep
      a count of number of such pages in the search_info and
      only free the dict_index_t struct when this count
      drops to zero.

      See also: dict_index_remove_from_cache_low() */

      if (index->search_info->ref_count > 0) {
        return false;
      }
    }

    return true;
  }

  return false;
}

/** Make room in the table cache by evicting an unused table. The unused table
 should not be part of FK relationship and currently not used in any user
 transaction. There is no guarantee that it will remove a table.
 @return number of tables evicted. If the number of tables in the dict_LRU
 is less than max_tables it will not do anything. */
ulint dict_make_room_in_cache(
    ulint max_tables, /*!< in: max tables allowed in cache */
    ulint pct_check)  /*!< in: max percent to check */
{
  ulint i;
  ulint len;
  dict_table_t *table;
  ulint check_up_to;
  ulint n_evicted = 0;

  ut_a(pct_check > 0);
  ut_a(pct_check <= 100);
  ut_ad(dict_sys_mutex_own());
  ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));
  ut_ad(dict_lru_validate());

  i = len = UT_LIST_GET_LEN(dict_sys->table_LRU);

  if (len < max_tables) {
    return (0);
  }

  check_up_to = len - ((len * pct_check) / 100);

  /* Check for overflow */
  ut_a(i == 0 || check_up_to <= i);

  /* Find a suitable candidate to evict from the cache. Don't scan the
  entire LRU list. Only scan pct_check list entries. */

  for (table = UT_LIST_GET_LAST(dict_sys->table_LRU);
       table != nullptr && i > check_up_to && (len - n_evicted) > max_tables;
       --i) {
    dict_table_t *prev_table;

    prev_table = UT_LIST_GET_PREV(table_LRU, table);

    table->lock();

    if (dict_table_can_be_evicted(table)) {
      table->unlock();
      DBUG_EXECUTE_IF("crash_if_fts_table_is_evicted", {
        if (table->fts && dict_table_has_fts_index(table)) {
          ut_d(ut_error);
        }
      };);
      dict_table_remove_from_cache_low(table, true);

      ++n_evicted;
    } else {
      table->unlock();
    }

    table = prev_table;
  }

  return (n_evicted);
}

/** Move a table to the non-LRU list from the LRU list. */
void dict_table_move_from_lru_to_non_lru(
    dict_table_t *table) /*!< in: table to move from LRU to non-LRU */
{
  ut_ad(dict_sys_mutex_own());
  ut_ad(dict_lru_find_table(table));

  ut_a(table->can_be_evicted);

  UT_LIST_REMOVE(dict_sys->table_LRU, table);

  UT_LIST_ADD_LAST(dict_sys->table_non_LRU, table);

  table->can_be_evicted = false;
}
#endif /* !UNIV_HOTBACKUP */

/** Move a table to the LRU end from the non LRU list.
@param[in]      table   InnoDB table object */
void dict_table_move_from_non_lru_to_lru(dict_table_t *table) {
  ut_ad(dict_sys_mutex_own());
  ut_ad(dict_non_lru_find_table(table));

  ut_a(!table->can_be_evicted);

  UT_LIST_REMOVE(dict_sys->table_non_LRU, table);

  UT_LIST_ADD_LAST(dict_sys->table_LRU, table);

  table->can_be_evicted = true;
}

#ifndef UNIV_HOTBACKUP

/** Look up an index in a table.
@param[in]      table   table
@param[in]      id      index identifier
@return index
@retval NULL if not found */
static const dict_index_t *dict_table_find_index_on_id(
    const dict_table_t *table, const index_id_t &id) {
  for (const dict_index_t *index = table->first_index(); index != nullptr;
       index = index->next()) {
    if (index->space == id.m_space_id && index->id == id.m_index_id) {
      return (index);
    }
  }

  return (nullptr);
}

const dict_index_t *dict_index_find(const index_id_t &id) {
  ut_ad(dict_sys_mutex_own());

  for (auto table : dict_sys->table_LRU) {
    const dict_index_t *index = dict_table_find_index_on_id(table, id);
    if (index != nullptr) {
      return (index);
    }
  }

  for (auto table : dict_sys->table_non_LRU) {
    const dict_index_t *index = dict_table_find_index_on_id(table, id);
    if (index != nullptr) {
      return (index);
    }
  }

  return (nullptr);
}

/** Function object to remove a foreign key constraint from the
referenced_set of the referenced table.  The foreign key object is
also removed from the dictionary cache.  The foreign key constraint
is not removed from the foreign_set of the table containing the
constraint. */
struct dict_foreign_remove_partial {
  void operator()(dict_foreign_t *foreign) {
    dict_table_t *table = foreign->referenced_table;
    if (table != nullptr) {
      table->referenced_set.erase(foreign);
    }
    dict_foreign_free(foreign);
  }
};

/** Renames a table object.
 @return true if success */
dberr_t dict_table_rename_in_cache(
    dict_table_t *table,       /*!< in/out: table */
    const char *new_name,      /*!< in: new name */
    bool rename_also_foreigns) /*!< in: in ALTER TABLE we want
                           to preserve the original table name
                           in constraints which reference it */
{
  dberr_t err;
  dict_foreign_t *foreign;
  dict_index_t *index;
  char old_name[MAX_FULL_NAME_LEN + 1];

  ut_ad(dict_sys_mutex_own());

  /* store the old/current name to an automatic variable */
  if (strlen(table->name.m_name) + 1 <= sizeof(old_name)) {
    strcpy(old_name, table->name.m_name);
  } else {
    ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_177)
        << "Too long table name: " << table->name << ", max length is "
        << MAX_FULL_NAME_LEN;
  }

  const auto hash_value = ut::hash_string(new_name);

  /* Look for a table with the same name: error if such exists */
  dict_table_t *table2;
  HASH_SEARCH(name_hash, dict_sys->table_hash, hash_value, dict_table_t *,
              table2, ut_ad(table2->cached),
              (ut_strcmp(table2->name.m_name, new_name) == 0));

  DBUG_EXECUTE_IF(
      "dict_table_rename_in_cache_failure",
      if (table2 == nullptr) { table2 = (dict_table_t *)-1; });

  if (table2 != nullptr) {
    ib::error(ER_IB_MSG_178)
        << "Cannot rename table '" << old_name << "' to '" << new_name
        << "' since the"
           " dictionary cache already contains '"
        << new_name << "'.";

    return (DB_ERROR);
  }

  /* If the table is stored in a single-table tablespace,
  rename the tablespace file. */

  if (dict_table_is_discarded(table)) {
    char *filepath;

    ut_ad(dict_table_is_file_per_table(table));
    ut_ad(!table->is_temporary());

    /* Make sure the data_dir_path is set. */
    dd_get_and_save_data_dir_path<dd::Table>(table, nullptr, true);

    std::string path = dict_table_get_datadir(table);

    filepath = Fil_path::make(path, table->name.m_name, IBD, true);

    if (filepath == nullptr) {
      return (DB_OUT_OF_MEMORY);
    }

    err = fil_delete_tablespace(table->space, BUF_REMOVE_NONE);

    ut_a(err == DB_SUCCESS || err == DB_TABLESPACE_NOT_FOUND ||
         err == DB_IO_ERROR);

    if (err == DB_IO_ERROR) {
      ib::info(ER_IB_MSG_179) << "IO error while deleting: " << table->space
                              << " during rename of '" << old_name << "' to"
                              << " '" << new_name << "'";
    }

    /* Delete any temp file hanging around. */
    os_file_type_t ftype;
    bool exists;
    if (os_file_status(filepath, &exists, &ftype) && exists &&
        !os_file_delete_if_exists(innodb_temp_file_key, filepath, nullptr)) {
      ib::info(ER_IB_MSG_180) << "Delete of " << filepath << " failed.";
    }

    ut::free(filepath);

  } else if (dict_table_is_file_per_table(table)) {
    char *new_path = nullptr;
    char *old_path = fil_space_get_first_path(table->space);

    ut_ad(!table->is_temporary());

    if (DICT_TF_HAS_DATA_DIR(table->flags)) {
      std::string new_ibd;

      new_ibd = Fil_path::make_new_path(old_path, new_name, IBD);

      new_path = mem_strdup(new_ibd.c_str());

      /* InnoDB adds the db directory to the data directory.
      If the RENAME changes database, then it is possible that
      the a directory named for the new db does not exist
      in this remote location. */
      err = os_file_create_subdirs_if_needed(new_path);
      if (err != DB_SUCCESS) {
        ut::free(old_path);
        ut::free(new_path);
        return (err);
      }
    } else {
      new_path = Fil_path::make_ibd_from_table_name(new_name);
    }

    /* New filepath must not exist. */
    err = fil_rename_tablespace_check(table->space, old_path, new_path, false);
    if (err != DB_SUCCESS) {
      ut::free(old_path);
      ut::free(new_path);
      return (err);
    }

    std::string new_tablespace_name(new_name);
    dict_name::convert_to_space(new_tablespace_name);

    dberr_t err = fil_rename_tablespace(table->space, old_path,
                                        new_tablespace_name.c_str(), new_path);

    ut::free(old_path);
    ut::free(new_path);

    if (err != DB_SUCCESS) {
      return (err);
    }
  }

  err = log_ddl->write_rename_table_log(table, new_name, table->name.m_name);
  if (err != DB_SUCCESS) {
    return (err);
  }

  /* Remove table from the hash tables of tables */
  HASH_DELETE(dict_table_t, name_hash, dict_sys->table_hash,
              ut::hash_string(old_name), table);

  if (strlen(new_name) > strlen(table->name.m_name)) {
    /* We allocate MAX_FULL_NAME_LEN + 1 bytes here to avoid
    memory fragmentation, we assume a repeated calls of
    ut_realloc() with the same size do not cause fragmentation */
    ut_a(strlen(new_name) <= MAX_FULL_NAME_LEN);

    table->name.m_name = static_cast<char *>(ut::realloc_withkey(
        UT_NEW_THIS_FILE_PSI_KEY, table->name.m_name, MAX_FULL_NAME_LEN + 1));
  }
  strcpy(table->name.m_name, new_name);

  /* Add table to hash table of tables */
  HASH_INSERT(dict_table_t, name_hash, dict_sys->table_hash, hash_value, table);

  dict_sys->size += strlen(new_name) - strlen(old_name);
  ut_a(dict_sys->size > 0);

  /* Update the table_name field in indexes */
  for (index = table->first_index(); index != nullptr; index = index->next()) {
    index->table_name = table->name.m_name;
  }

  if (!rename_also_foreigns) {
    /* In ALTER TABLE we think of the rename table operation
    in the direction table -> temporary table (#sql...)
    as dropping the table with the old name and creating
    a new with the new name. Thus we kind of drop the
    constraints from the dictionary cache here. The foreign key
    constraints will be inherited to the new table from the
    system tables through a call of dict_load_foreigns. */

    /* Remove the foreign constraints from the cache */
    std::for_each(table->foreign_set.begin(), table->foreign_set.end(),
                  dict_foreign_remove_partial());
    table->foreign_set.clear();

    /* Reset table field in referencing constraints */
    for (dict_foreign_set::iterator it = table->referenced_set.begin();
         it != table->referenced_set.end(); ++it) {
      foreign = *it;
      foreign->referenced_table = nullptr;
      foreign->referenced_index = nullptr;
    }

    /* Make the set of referencing constraints empty */
    table->referenced_set.clear();

    return (DB_SUCCESS);
  }

  /* Update the table name fields in foreign constraints, and update also
  the constraint id of new format >= 4.0.18 constraints. Note that at
  this point we have already changed table->name to the new name. */

  dict_foreign_set fk_set;

  for (;;) {
    dict_foreign_set::iterator it = table->foreign_set.begin();

    if (it == table->foreign_set.end()) {
      break;
    }

    foreign = *it;

    if (foreign->referenced_table) {
      foreign->referenced_table->referenced_set.erase(foreign);
    }

    if (ut_strlen(foreign->foreign_table_name) <
        ut_strlen(table->name.m_name)) {
      /* Allocate a longer name buffer;
      TODO: store buf len to save memory */

      foreign->foreign_table_name =
          mem_heap_strdup(foreign->heap, table->name.m_name);
      dict_mem_foreign_table_name_lookup_set(foreign, true);
    } else {
      strcpy(foreign->foreign_table_name, table->name.m_name);
      dict_mem_foreign_table_name_lookup_set(foreign, false);
    }
    if (strchr(foreign->id, '/')) {
      /* This is a >= 4.0.18 format id */

      ulint db_len;
      char *old_id;
      char old_name_cs_filename[MAX_FULL_NAME_LEN + 1];
      uint errors = 0;

      /* All table names are internally stored in charset
      my_charset_filename (except the temp tables and the
      partition identifier suffix in partition tables). The
      foreign key constraint names are internally stored
      in UTF-8 charset.  The variable fkid here is used
      to store foreign key constraint name in charset
      my_charset_filename for comparison further below. */
      char fkid[MAX_TABLE_NAME_LEN + 20];
      bool on_tmp = false;

      /* The old table name in my_charset_filename is stored
      in old_name_cs_filename */

      strncpy(old_name_cs_filename, old_name, sizeof(old_name_cs_filename));
      if (strstr(old_name, TEMP_TABLE_PATH_PREFIX) == nullptr) {
        innobase_convert_to_system_charset(
            strchr(old_name_cs_filename, '/') + 1, strchr(old_name, '/') + 1,
            MAX_TABLE_NAME_LEN, &errors);

        if (errors) {
          /* There has been an error to convert
          old table into UTF-8.  This probably
          means that the old table name is
          actually in UTF-8. */
          innobase_convert_to_filename_charset(
              strchr(old_name_cs_filename, '/') + 1, strchr(old_name, '/') + 1,
              MAX_TABLE_NAME_LEN);
        } else {
          /* Old name already in
          my_charset_filename */
          strncpy(old_name_cs_filename, old_name, sizeof(old_name_cs_filename));
        }
      }

      strncpy(fkid, foreign->id, MAX_TABLE_NAME_LEN);

      if (strstr(fkid, TEMP_TABLE_PATH_PREFIX) == nullptr) {
        innobase_convert_to_filename_charset(strchr(fkid, '/') + 1,
                                             strchr(foreign->id, '/') + 1,
                                             MAX_TABLE_NAME_LEN + 20);
      } else {
        on_tmp = true;
      }

      old_id = mem_strdup(foreign->id);

      if (ut_strlen(fkid) >
              ut_strlen(old_name_cs_filename) + ((sizeof dict_ibfk) - 1) &&
          !memcmp(fkid, old_name_cs_filename,
                  ut_strlen(old_name_cs_filename)) &&
          !memcmp(fkid + ut_strlen(old_name_cs_filename), dict_ibfk,
                  (sizeof dict_ibfk) - 1)) {
        /* This is a generated >= 4.0.18 format id */

        char table_name[MAX_TABLE_NAME_LEN + 1] = "";
        uint errors = 0;

        if (strlen(table->name.m_name) > strlen(old_name)) {
          foreign->id = static_cast<char *>(mem_heap_alloc(
              foreign->heap, strlen(table->name.m_name) + strlen(old_id) + 1));
        }

        /* Convert the table name to UTF-8 */
        strncpy(table_name, table->name.m_name, MAX_TABLE_NAME_LEN);
        innobase_convert_to_system_charset(strchr(table_name, '/') + 1,
                                           strchr(table->name.m_name, '/') + 1,
                                           MAX_TABLE_NAME_LEN, &errors);

        if (errors) {
          /* Table name could not be converted
          from charset my_charset_filename to
          UTF-8. This means that the table name
          is already in UTF-8 (#mysql#50). */
          strncpy(table_name, table->name.m_name, MAX_TABLE_NAME_LEN);
        }

        /* Replace the prefix 'databasename/tablename'
        with the new names */
        strcpy(foreign->id, table_name);
        if (on_tmp) {
          strcat(foreign->id, old_id + ut_strlen(old_name));
        } else {
          sprintf(strchr(foreign->id, '/') + 1, "%s%s",
                  strchr(table_name, '/') + 1, strstr(old_id, "_ibfk_"));
        }

      } else {
        /* This is a >= 4.0.18 format id where the user
        gave the id name */
        db_len = dict_get_db_name_len(table->name.m_name) + 1;

        if (db_len - 1 > dict_get_db_name_len(foreign->id)) {
          foreign->id = static_cast<char *>(
              mem_heap_alloc(foreign->heap, db_len + strlen(old_id) + 1));
        }

        /* Replace the database prefix in id with the
        one from table->name */

        ut_memcpy(foreign->id, table->name.m_name, db_len);

        strcpy(foreign->id + db_len, dict_remove_db_name(old_id));
      }

      ut::free(old_id);
    }

    table->foreign_set.erase(it);
    fk_set.insert(foreign);

    if (foreign->referenced_table) {
      foreign->referenced_table->referenced_set.insert(foreign);
    }
  }

  ut_a(table->foreign_set.empty());
  table->foreign_set.swap(fk_set);

  for (dict_foreign_set::iterator it = table->referenced_set.begin();
       it != table->referenced_set.end(); ++it) {
    foreign = *it;

    if (ut_strlen(foreign->referenced_table_name) <
        ut_strlen(table->name.m_name)) {
      /* Allocate a longer name buffer;
      TODO: store buf len to save memory */

      foreign->referenced_table_name =
          mem_heap_strdup(foreign->heap, table->name.m_name);

      dict_mem_referenced_table_name_lookup_set(foreign, true);
    } else {
      /* Use the same buffer */
      strcpy(foreign->referenced_table_name, table->name.m_name);

      dict_mem_referenced_table_name_lookup_set(foreign, false);
    }
  }

  return (DB_SUCCESS);
}

/** Change the id of a table object in the dictionary cache. This is used in
 DISCARD TABLESPACE. */
void dict_table_change_id_in_cache(
    dict_table_t *table, /*!< in/out: table object already in cache */
    table_id_t new_id)   /*!< in: new id to set */
{
  ut_ad(table);
  ut_ad(dict_sys_mutex_own());
  ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

#ifdef UNIV_DEBUG

  auto id_fold = ut::hash_uint64(table->id);
  /* Table with the same id should not exist in cache */
  {
    dict_table_t *table2;
    HASH_SEARCH(id_hash, dict_sys->table_id_hash, id_fold, dict_table_t *,
                table2, ut_ad(table2->cached), table2->id == new_id);
    ut_ad(table2 == nullptr);

    if (table2 != nullptr) {
      return;
    }
  }
#endif /*UNIV_DEBUG*/

  /* Remove the table from the hash table of id's */

  HASH_DELETE(dict_table_t, id_hash, dict_sys->table_id_hash,
              ut::hash_uint64(table->id), table);

  table->id = new_id;

  /* Add the table back to the hash table */
  HASH_INSERT(dict_table_t, id_hash, dict_sys->table_id_hash,
              ut::hash_uint64(table->id), table);
}

/** Removes a table object from the dictionary cache. */
static void dict_table_remove_from_cache_low(
    dict_table_t *table, /*!< in, own: table */
    bool lru_evict)      /*!< in: true if table being evicted
                         to make room in the table LRU list */
{
  dict_foreign_t *foreign;
  dict_index_t *index;

  ut_ad(table);
  ut_ad(dict_lru_validate());
  ut_a(table->get_ref_count() == 0);
  ut_a(table->n_rec_locks.load() == 0);
  ut_ad(dict_sys_mutex_own());
  ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

  /* We first dirty read the status which could be changed from
  METADATA_DIRTY to METADATA_BUFFERED by checkpoint, and check again
  when persistence is necessary */
  switch (table->dirty_status.load()) {
    case METADATA_DIRTY:
      /* Write back the dirty metadata to DDTableBuffer */
      dict_table_persist_to_dd_table_buffer(table);
      ut_ad(table->dirty_status.load() != METADATA_DIRTY);
      [[fallthrough]];
    case METADATA_BUFFERED:
      /* We have to remove it away here, since it's evicted.
      And we will add it again once it's re-loaded if possible */
      mutex_enter(&dict_persist->mutex);
      ut_ad(table->in_dirty_dict_tables_list);
      UT_LIST_REMOVE(dict_persist->dirty_dict_tables, table);
      mutex_exit(&dict_persist->mutex);
      break;
    case METADATA_CLEAN:
      break;
  }

  /* Remove the foreign constraints from the cache */
  std::for_each(table->foreign_set.begin(), table->foreign_set.end(),
                dict_foreign_remove_partial());
  table->foreign_set.clear();

  /* Reset table field in referencing constraints */
  for (dict_foreign_set::iterator it = table->referenced_set.begin();
       it != table->referenced_set.end(); ++it) {
    foreign = *it;
    foreign->referenced_table = nullptr;
    foreign->referenced_index = nullptr;
  }

  /* Remove the indexes from the cache */

  for (index = UT_LIST_GET_LAST(table->indexes); index != nullptr;
       index = UT_LIST_GET_LAST(table->indexes)) {
    dict_index_remove_from_cache_low(table, index, lru_evict);
  }

  /* Remove table from the hash tables of tables */

  HASH_DELETE(dict_table_t, name_hash, dict_sys->table_hash,
              ut::hash_string(table->name.m_name), table);

  HASH_DELETE(dict_table_t, id_hash, dict_sys->table_id_hash,
              ut::hash_uint64(table->id), table);

  /* Remove table from LRU or non-LRU list. */
  if (table->can_be_evicted) {
    ut_ad(dict_lru_find_table(table));
    UT_LIST_REMOVE(dict_sys->table_LRU, table);
  } else {
    ut_ad(dict_non_lru_find_table(table));
    UT_LIST_REMOVE(dict_sys->table_non_LRU, table);
  }

  ut_ad(dict_lru_validate());

  /* Free virtual column template if any */
  if (table->vc_templ != nullptr) {
    dict_free_vc_templ(table->vc_templ);
    ut::delete_(table->vc_templ);
  }

  const auto size =
      mem_heap_get_size(table->heap) + strlen(table->name.m_name) + 1;

  ut_ad(dict_sys->size >= size);

  dict_sys->size -= size;

  dict_mem_table_free(table);
}

/** Removes a table object from the dictionary cache. */
void dict_table_remove_from_cache(dict_table_t *table) /*!< in, own: table */
{
  dict_table_remove_from_cache_low(table, false);
}

/** Try to invalidate an entry from the dict cache, for a partitioned table,
if any table found.
@param[in]      name    Table name */
void dict_partitioned_table_remove_from_cache(const char *name) {
  ut_ad(dict_sys_mutex_own());

  size_t name_len = strlen(name);

  for (uint32_t i = 0; i < hash_get_n_cells(dict_sys->table_id_hash); ++i) {
    dict_table_t *table;

    table =
        static_cast<dict_table_t *>(hash_get_first(dict_sys->table_hash, i));

    while (table != nullptr) {
      dict_table_t *prev_table = table;

      table = static_cast<dict_table_t *>(HASH_GET_NEXT(name_hash, prev_table));
      ut_ad(prev_table->magic_n == DICT_TABLE_MAGIC_N);

      if (prev_table->is_dd_table) {
        continue;
      }

      if ((strncmp(name, prev_table->name.m_name, name_len) == 0) &&
          dict_table_is_partition(prev_table)) {
        btr_drop_ahi_for_table(prev_table);
        dict_table_remove_from_cache(prev_table);
      }
    }
  }
}

#ifdef UNIV_DEBUG
/** Removes a table object from the dictionary cache, for debug purpose
@param[in,out]  table           table object
@param[in]      lru_evict       true if table being evicted to make room
                                in the table LRU list */
void dict_table_remove_from_cache_debug(dict_table_t *table, bool lru_evict) {
  dict_table_remove_from_cache_low(table, lru_evict);
}
#endif /* UNIV_DEBUG */

/** If the given column name is reserved for InnoDB system columns, return
 true.
 @return true if name is reserved */
bool dict_col_name_is_reserved(const char *name) /*!< in: column name */
{
  /* This check reminds that if a new system column is added to
  the program, it should be dealt with here. */
  static_assert(DATA_N_SYS_COLS == 3, "DATA_N_SYS_COLS != 3");

  static const char *reserved_names[] = {"DB_ROW_ID", "DB_TRX_ID",
                                         "DB_ROLL_PTR"};

  ulint i;

  for (i = 0; i < UT_ARR_SIZE(reserved_names); i++) {
    if (innobase_strcasecmp(name, reserved_names[i]) == 0) {
      return true;
    }
  }

  return false;
}

/** Return maximum size of the node pointer record.
 @return maximum size of the record in bytes */
ulint dict_index_node_ptr_max_size(const dict_index_t *index) /*!< in: index */
{
  ulint comp;
  ulint i;
  /* maximum possible storage size of a record */
  ulint rec_max_size;

  if (dict_index_is_ibuf(index)) {
    /* cannot estimate accurately */
    /* This is universal index for change buffer.
    The max size of the entry is about max key length * 2.
    (index key + primary key to be inserted to the index)
    (The max key length is UNIV_PAGE_SIZE / 16 * 3 at
     ha_innobase::max_supported_key_length(),
     considering MAX_KEY_LENGTH = 3072 at MySQL imposes
     the 3500 historical InnoDB value for 16K page size case.)
    For the universal index, node_ptr contains most of the entry.
    And 512 is enough to contain ibuf columns and meta-data */
    return (UNIV_PAGE_SIZE / 8 * 3 + 512);
  }

  comp = dict_table_is_comp(index->table);

  /* Each record has page_no, length of page_no and header. */
  rec_max_size = comp ? REC_NODE_PTR_SIZE + 1 + REC_N_NEW_EXTRA_BYTES
                      : REC_NODE_PTR_SIZE + 2 + REC_N_OLD_EXTRA_BYTES;

  if (comp) {
    /* Include the "null" flags in the
    maximum possible record size. */
    rec_max_size += UT_BITS_IN_BYTES(index->n_nullable);
  } else {
    /* For each column, include a 2-byte offset and a
    "null" flag. */
    rec_max_size += 2 * index->n_fields;
  }

  /* Compute the maximum possible record size. */
  for (i = 0; i < dict_index_get_n_unique_in_tree(index); i++) {
    const dict_field_t *field = index->get_field(i);
    const dict_col_t *col = field->col;
    ulint field_max_size;
    ulint field_ext_max_size;

    /* Determine the maximum length of the index field. */

    field_max_size = col->get_fixed_size(comp);
    if (field_max_size) {
      /* dict_index_add_col() should guarantee this */
      ut_ad(!field->prefix_len || field->fixed_len == field->prefix_len);
      /* Fixed lengths are not encoded
      in ROW_FORMAT=COMPACT. */
      rec_max_size += field_max_size;
      continue;
    }

    field_max_size = col->get_max_size();
    field_ext_max_size = field_max_size < 256 ? 1 : 2;

    if (field->prefix_len && field->prefix_len < field_max_size) {
      field_max_size = field->prefix_len;
    }

    if (comp) {
      /* Add the extra size for ROW_FORMAT=COMPACT.
      For ROW_FORMAT=REDUNDANT, these bytes were
      added to rec_max_size before this loop. */
      rec_max_size += field_ext_max_size;
    }

    rec_max_size += field_max_size;
  }

  return (rec_max_size);
}

void get_permissible_max_size(const dict_table_t *table,
                              const dict_index_t *index, size_t &page_rec_max,
                              size_t &page_ptr_max) {
  const bool comp = dict_table_is_comp(table);
  const page_size_t page_size(dict_table_page_size(table));

  if (page_size.is_compressed() &&
      page_size.physical() < univ_page_size.physical()) {
    /* On a compressed page, two records must fit in the
    uncompressed page modification log. On compressed pages
    with size.physical() == univ_page_size.physical(),
    this limit will never be reached. */
    ut_ad(comp);
    /* The maximum allowed record size is the size of
    an empty page, minus a byte for recoding the heap
    number in the page modification log.  The maximum
    allowed node pointer size is half that. */
    page_rec_max = page_zip_empty_size(index->n_fields, page_size.physical());
    if (page_rec_max) {
      page_rec_max--;
    }
    page_ptr_max = page_rec_max / 2;
  } else {
    /* The maximum allowed record size is half a B-tree
    page(16k for 64k page size).  No additional sparse
    page directory entry will be generated for the first
    few user records. */
    page_rec_max = srv_page_size == UNIV_PAGE_SIZE_MAX
                       ? REC_MAX_DATA_SIZE - 1
                       : page_get_free_space_of_empty(comp) / 2;
    page_ptr_max = page_rec_max;
  }
}

void get_field_max_size(const dict_table_t *table, const dict_index_t *index,
                        const dict_field_t *field, size_t &rec_max_size) {
  const bool comp = dict_table_is_comp(table);
  const dict_col_t *col = field->col;
  ulint field_max_size;
  ulint field_ext_max_size;

  /* In dtuple_convert_big_rec(), variable-length columns
  that are longer than BTR_EXTERN_LOCAL_STORED_MAX_SIZE
  may be chosen for external storage.

  Fixed-length columns, and all columns of secondary
  index records are always stored inline. */

  /* Determine the maximum length of the index field.
  The field_ext_max_size should be computed as the worst
  case in rec_get_converted_size_comp() for
  REC_STATUS_ORDINARY records. */

  field_max_size = col->get_fixed_size(comp);
  if (field_max_size && field->fixed_len != 0) {
    /* dict_index_add_col() should guarantee this */
    ut_ad(!field->prefix_len || field->fixed_len == field->prefix_len);
    /* Fixed lengths are not encoded in ROW_FORMAT=COMPACT. */
    field_ext_max_size = 0;
    rec_max_size += field_max_size;
    return;
  }

  field_max_size = col->get_max_size();
  field_ext_max_size = field_max_size < 256 ? 1 : 2;

  if (field->prefix_len) {
    if (field->prefix_len < field_max_size) {
      field_max_size = field->prefix_len;
    }
  } else if (field_max_size > BTR_EXTERN_LOCAL_STORED_MAX_SIZE &&
             index->is_clustered()) {
    if (dict_table_has_atomic_blobs(table)) {
      /* In the worst case, we have a locally stored column of
      BTR_EXTERN_LOCAL_STORED_MAX_SIZE bytes. The length can be stored in one
      byte. If the column were stored externally, the lengths in the clustered
      index page would be BTR_EXTERN_FIELD_REF_SIZE and 2. */
      field_max_size = BTR_EXTERN_LOCAL_STORED_MAX_SIZE;
      field_ext_max_size = 1;
    } else {
      /* In this case, externally stored fields would have 768 byte prefix
      stored in-line followed by fields reference. */
      size_t local_stored_length =
          BTR_EXTERN_FIELD_REF_SIZE + DICT_ANTELOPE_MAX_INDEX_COL_LEN;
      if (field_max_size > local_stored_length) {
        field_max_size = local_stored_length;
        /* Prefix length will be stored in 2 bytes. */
        field_ext_max_size = 2;
      }
    }
  }

  if (comp) {
    /* Add the extra size for ROW_FORMAT=COMPACT.
    For ROW_FORMAT=REDUNDANT, these bytes were
    added to rec_max_size before this loop. */
    rec_max_size += field_ext_max_size;
  }

  rec_max_size += field_max_size;
}

/** A B-tree page should accommodate at least two records. This function finds
out if this is violated for records of maximum possible length of this index.
@param[in]   table         table
@param[in]   new_index     index
@param[in]   strict        true=report error if records could be too big to fit
                           in a B-tree page
@return true if the index record could become too big */
static bool dict_index_too_big_for_tree(const dict_table_t *table,
                                        const dict_index_t *new_index,
                                        bool strict) {
  /* FTS index consists of auxiliary tables, they shall be excluded from index
  row size check */
  if (new_index->type & DICT_FTS) {
    return (false);
  }

  DBUG_EXECUTE_IF("ib_force_create_table", return (false););

  /* maximum allowed size of a record on a leaf page */
  size_t page_rec_max;
  /* maximum allowed size of a node pointer record */
  size_t page_ptr_max;
  get_permissible_max_size(table, new_index, page_rec_max, page_ptr_max);

  size_t rec_max_size;
  bool res = dict_index_validate_max_rec_size(
      table, new_index, strict, page_rec_max, page_ptr_max, rec_max_size);
  ut_a(res || rec_max_size < page_rec_max);
  return (res);
}

bool dict_index_validate_max_rec_size(const dict_table_t *table,
                                      const dict_index_t *index, bool strict,
                                      const size_t page_rec_max,
                                      const size_t page_ptr_max,
                                      size_t &rec_max_size) {
  const bool comp = dict_table_is_comp(table);

  const page_size_t page_size(dict_table_page_size(table));
  if (page_size.is_compressed() &&
      page_size.physical() < univ_page_size.physical()) {
    /* On a compressed page, there is a two-byte entry in the dense page
    directory for every record. But there is no record header. */
    rec_max_size = 2;
  } else {
    /* Each record has a header. */
    rec_max_size = comp ? REC_N_NEW_EXTRA_BYTES : REC_N_OLD_EXTRA_BYTES;
  }

  if (comp) {
    /* Include the "null" flags in the maximum possible record size. */
    rec_max_size += UT_BITS_IN_BYTES(index->n_nullable);
  } else {
    /* For each column, include a 2-byte offset and a "null" flag.  The 1-byte
    format is only used in short records that do not contain externally stored
    columns. Such records could never exceed the page limit, even when using
    the 2-byte format. */
    rec_max_size += 2 * index->n_fields;
  }

  /* Each record would have version stored in 1 byte */
  if (index->has_row_versions()) {
    rec_max_size += 1;
  }

  /* Compute the maximum possible record size. */
  for (size_t i = 0; i < index->n_fields; i++) {
    const dict_field_t *field = index->get_field(i);
    get_field_max_size(table, index, field, rec_max_size);

    /* Check the size limit on leaf pages. */
    if (rec_max_size >= page_rec_max) {
      ib::error_or_warn(strict)
          << "Cannot add field " << field->name << " in table " << table->name
          << " because after adding it, the row size is " << rec_max_size
          << " which is greater than maximum allowed"
             " size ("
          << page_rec_max << ") for a record on index leaf page.";

      return (true);
    }

    /* Check the size limit on non-leaf pages. Records stored in non-leaf
    B-tree pages consist of the unique columns of the record (the key columns
    of the B-tree) and a node pointer field. When we have processed the unique
    columns, rec_max_size equals the size of the node pointer record minus the
    node pointer column. */
    if (i + 1 == dict_index_get_n_unique_in_tree(index) &&
        rec_max_size + REC_NODE_PTR_SIZE >= page_ptr_max) {
      return (true);
    }
  }

  return (false);
}

/** Adds an index to the dictionary cache.
@param[in,out]  table   table on which the index is
@param[in,out]  index   index; NOTE! The index memory
                        object is freed in this function!
@param[in]      page_no root page number of the index
@param[in]      strict  true=refuse to create the index
                        if records could be too big to fit in
                        an B-tree page
@return DB_SUCCESS, DB_TOO_BIG_RECORD, or DB_CORRUPTION */
dberr_t dict_index_add_to_cache(dict_table_t *table, dict_index_t *index,
                                page_no_t page_no, bool strict) {
  ut_ad(!dict_sys_mutex_own());
  return (
      dict_index_add_to_cache_w_vcol(table, index, nullptr, page_no, strict));
}

/** Clears the virtual column's index list before index is being freed.
@param[in]  index   Index being freed */
void dict_index_remove_from_v_col_list(dict_index_t *index) {
  /* Index is not completely formed */
  if (!index->cached) {
    return;
  }
  if (dict_index_has_virtual(index)) {
    const dict_col_t *col;
    const dict_v_col_t *vcol;

    for (ulint i = 0; i < dict_index_get_n_fields(index); i++) {
      col = index->get_col(i);
      if (col->is_virtual()) {
        vcol = reinterpret_cast<const dict_v_col_t *>(col);
        /* This could be NULL, when we do add
        virtual column, add index together. We do not
        need to track this virtual column's index */
        if (vcol->v_indexes == nullptr) {
          continue;
        }
        dict_v_idx_list::iterator it;
        for (it = vcol->v_indexes->begin(); it != vcol->v_indexes->end();
             ++it) {
          dict_v_idx_t v_index = *it;
          if (v_index.index == index) {
            vcol->v_indexes->erase(it);
            break;
          }
        }
      }
    }
  }
}
/** Check if key part of the index has fixed len and thus rec offsets can be
cached. If so, it initializes rec_cache.offsets and rec_cache.nullable_cols, so
that we can avoid calling the costly rec_get_offsets() in many cases. That is:
- offsets will contain offsets of the first n columns within a rec, assuming
  that none of them contains NULL, where n is chosen to be sufficient for the
  most common use case, which is a binary search within a B-tree page,
- nullable_cols will be the number of columns among those first n columns, which
  are NULLable at all, so that before using the rec_cache.offset we know how
  many initial bits of the rec's NULL mask need to be zero, to ensure that the
  rec doesn't have NULLs in the n first columns and thus rec_cache.offsets is
  indeed corresponding to its layout and can be used for this rec.
@param[in]  index   The index instance for which rec_cache should be computed
*/
static void dict_index_try_cache_rec_offsets(dict_index_t *index) {
  ut_ad(index->rec_cache.offsets == nullptr);
  ut_ad(index->rec_cache.nullable_cols == 0);

  const auto n_unique_in_tree = dict_index_get_n_unique_in_tree(index);
  ut_a(0 < n_unique_in_tree);
#ifdef UNIV_DEBUG
  /* Check for backward compatibility - in the past we used cache only for
  intrinsic tables, and assumed that it is sufficient to check only if the
  first n_uniq fields are fixed length. We'd like to ensure that the new code
  uses cache for intrinsic tables at least in these cases it used to before.
  But the loop below now checks n_unique_in_tree columns, not just n_unique.
  Following assert makes sure that when n_unique < n_unique_in_tree, we still
  use cache, because it only happens when n_unique_in_tree has one more column
  which is the primary key reference and has a fixed length, so should not
  affect the decision of the loop.
  Note: in general primary keys can have more than one column, don't have to be
  fixed length, etc. It just never happens for intrinsic tables in our testing.
  This is not an assert crucial for correctness. It's just to show that there's
  no obvious regression w.r.t intrinsic tables. */
  if (index->table->is_intrinsic() && index->n_uniq != n_unique_in_tree) {
    ut_a(index->n_uniq == n_unique_in_tree - 1);
    ut_a(!index->is_clustered());
    ut_a(index->get_field(n_unique_in_tree - 1)->fixed_len);
  }
#endif
  for (size_t i = 0; i < n_unique_in_tree; i++) {
    if (!index->get_field(i)->fixed_len) {
      return;
    }
  }
  /* inlined rec_get_offsets() and rec_init_offsets()'s fast-path without
  referencing rec follows: */

  const auto offsets_len = n_unique_in_tree + (1 + REC_OFFS_HEADER_SIZE);
  auto *const offsets = static_cast<ulint *>(
      mem_heap_alloc(index->heap, sizeof(ulint) * offsets_len));

  index->rec_cache.offsets = offsets;
  rec_offs_set_n_alloc(offsets, offsets_len);
  rec_offs_set_n_fields(offsets, n_unique_in_tree);
  rec_init_fixed_offsets(index, offsets);

  for (size_t i = 0; i < n_unique_in_tree; i++) {
    if (!(index->get_field(i)->col->prtype & DATA_NOT_NULL)) {
      index->rec_cache.nullable_cols++;
    }
  }
  ut_a(index->rec_cache.nullable_cols <= index->n_nullable);
}

/** Adds an index to the dictionary cache, with possible indexing newly
added column.
@param[in,out]  table   table on which the index is
@param[in,out]  index   index; NOTE! The index memory
                        object is freed in this function!
@param[in]      add_v   new virtual column that being added along with
                        an add index call
@param[in]      page_no root page number of the index
@param[in]      strict  true=refuse to create the index
                        if records could be too big to fit in
                        an B-tree page
@return DB_SUCCESS, DB_TOO_BIG_RECORD, or DB_CORRUPTION */
dberr_t dict_index_add_to_cache_w_vcol(dict_table_t *table, dict_index_t *index,
                                       const dict_add_v_col_t *add_v,
                                       page_no_t page_no, bool strict) {
  dict_index_t *new_index;
  ulint n_ord;
  ulint i;

  ut_ad(index);
  ut_ad(!dict_sys_mutex_own());
  ut_ad(index->n_def == index->n_fields);
  ut_ad(index->magic_n == DICT_INDEX_MAGIC_N);
  ut_ad(!dict_index_is_online_ddl(index));
  ut_ad(!dict_index_is_ibuf(index));

  ut_d(mem_heap_validate(index->heap));
  ut_a(!index->is_clustered() || UT_LIST_GET_LEN(table->indexes) == 0);

  if (!dict_index_find_and_set_cols(table, index, add_v)) {
    dict_mem_index_free(index);
    return (DB_CORRUPTION);
  }

  /* Build the cache internal representation of the index,
  containing also the added system fields */

  if (index->type == DICT_FTS) {
    new_index = dict_index_build_internal_fts(table, index);
  } else if (index->is_clustered()) {
    new_index = dict_index_build_internal_clust(table, index);
  } else {
    new_index = dict_index_build_internal_non_clust(table, index);
  }

  /* Set the n_fields value in new_index to the actual defined
  number of fields in the cache internal representation */

  new_index->n_fields = new_index->n_def;

  if (index->is_clustered() && table->has_row_versions()) {
    new_index->n_fields = new_index->n_def - table->get_n_instant_drop_cols();
  }
  new_index->n_total_fields = new_index->n_def;
  new_index->trx_id = index->trx_id;
  new_index->set_committed(index->is_committed());
  new_index->allow_duplicates = index->allow_duplicates;
  new_index->nulls_equal = index->nulls_equal;
  new_index->disable_ahi = index->disable_ahi;
  new_index->srid_is_valid = index->srid_is_valid;
  new_index->srid = index->srid;

  new_index->srid = index->srid;
  new_index->srid_is_valid = index->srid_is_valid;
  if (index->rtr_srs.get() != nullptr)
    new_index->rtr_srs.reset(index->rtr_srs->clone());

  if (dict_index_too_big_for_tree(table, new_index, strict)) {
    if (strict) {
      dict_mem_index_free(new_index);
      dict_mem_index_free(index);
      return (DB_TOO_BIG_RECORD);
    } else if (current_thd != nullptr) {
      /* Avoid the warning to be printed
      during recovery. */
      ib_warn_row_too_big(table);
    }
  }

  n_ord = new_index->n_uniq;

  /* Flag the ordering columns and also set column max_prefix */

  for (i = 0; i < n_ord; i++) {
    const dict_field_t *field = new_index->get_field(i);

    /* Check the column being added in the index for
    the first time and flag the ordering column. */
    if (field->col->ord_part == 0) {
      field->col->max_prefix = field->prefix_len;
      field->col->ord_part = 1;
    } else if (field->prefix_len == 0) {
      /* Set the max_prefix for a column to 0 if
      its prefix length is 0 (for this index)
      even if it was a part of any other index
      with some prefix length. */
      field->col->max_prefix = 0;
    } else if (field->col->max_prefix != 0 &&
               field->prefix_len > field->col->max_prefix) {
      /* Set the max_prefix value based on the
      prefix_len. */
      field->col->max_prefix = field->prefix_len;
    }
    ut_ad(field->col->ord_part == 1);
  }

  new_index->stat_n_diff_key_vals = static_cast<uint64_t *>(mem_heap_zalloc(
      new_index->heap, dict_index_get_n_unique(new_index) *
                           sizeof(*new_index->stat_n_diff_key_vals)));

  new_index->stat_n_sample_sizes = static_cast<uint64_t *>(mem_heap_zalloc(
      new_index->heap, dict_index_get_n_unique(new_index) *
                           sizeof(*new_index->stat_n_sample_sizes)));

  new_index->stat_n_non_null_key_vals = static_cast<uint64_t *>(mem_heap_zalloc(
      new_index->heap, dict_index_get_n_unique(new_index) *
                           sizeof(*new_index->stat_n_non_null_key_vals)));

  new_index->stat_index_size = 1;
  new_index->stat_n_leaf_pages = 1;

  new_index->table = table;
  new_index->table_name = table->name.m_name;
  new_index->search_info = btr_search_info_create(new_index->heap);

  new_index->page = page_no;
  rw_lock_create(index_tree_rw_lock_key, &new_index->lock, LATCH_ID_INDEX_TREE);

  /* The conditions listed here correspond to the simplest call-path through
  rec_get_offsets(). If they are met now, we can cache rec offsets and use the
  cache as long they are met. We assert before using the rec_cache that they are
  still met, and clear the rec_cache.offsets when they change. */
  if (dict_table_is_comp(table) && !dict_index_has_virtual(index) &&
      (!table->has_instant_cols() && !table->has_row_versions()) &&
      !dict_index_is_spatial(index)) {
    dict_index_try_cache_rec_offsets(new_index);
  } else {
    /* The rules should not prevent caching for intrinsic tables */
    ut_ad(!table->is_intrinsic());
  }

  dict_sys_mutex_enter();

  /* Add the new index as the last index for the table */
  UT_LIST_ADD_LAST(table->indexes, new_index);

  /* Intrinsic table are not added to dictionary cache instead are
  cached to session specific thread cache. */
  if (!table->is_intrinsic()) {
    dict_sys->size += mem_heap_get_size(new_index->heap);
  }

  dict_sys_mutex_exit();

  if (dict_index_has_virtual(index)) {
    const dict_col_t *col;
    const dict_v_col_t *vcol;

    for (ulint i = 0; i < dict_index_get_n_fields(index); i++) {
      col = index->get_col(i);
      if (col->is_virtual()) {
        vcol = reinterpret_cast<const dict_v_col_t *>(col);

        /* This could be NULL, when we do add virtual
        column, add index together. We do not need to
        track this virtual column's index */
        if (vcol->v_indexes == nullptr) {
          continue;
        }

        dict_v_idx_list::iterator it;

        for (it = vcol->v_indexes->begin(); it != vcol->v_indexes->end();) {
          dict_v_idx_t v_index = *it;
          if (v_index.index == index) {
            vcol->v_indexes->erase(it++);
          } else {
            it++;
          }
        }
      }
    }
  }

  new_index->instant_cols = false;
  new_index->row_versions = false;
  new_index->set_instant_nullable(new_index->n_nullable);

  if (new_index->is_clustered()) {
    if (new_index->table->has_row_versions()) {
      new_index->row_versions = true;
    }

    if (new_index->table->has_instant_cols()) {
      ut_ad(new_index->table->is_upgraded_instant());
      new_index->instant_cols = true;
      const size_t n_instant_fields = new_index->get_instant_fields();
      size_t new_n_nullable =
          new_index->calculate_n_instant_nullable(n_instant_fields);
      new_index->set_instant_nullable(new_n_nullable);
    }
  }

  dict_mem_index_free(index);

  return (DB_SUCCESS);
}

/** Removes an index from the dictionary cache. */
static void dict_index_remove_from_cache_low(
    dict_table_t *table, /*!< in/out: table */
    dict_index_t *index, /*!< in, own: index */
    bool lru_evict)      /*!< in: true if index being evicted
                         to make room in the table LRU list */
{
  ut_ad(table && index);
  ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);
  ut_ad(index->magic_n == DICT_INDEX_MAGIC_N);
  ut_ad(dict_sys_mutex_own());

  /* No need to acquire the dict_index_t::lock here because
  there can't be any active operations on this index (or table). */

  if (index->online_log) {
    ut_ad(index->online_status == ONLINE_INDEX_CREATION);
    row_log_free(index->online_log);
  }

  btr_search_await_no_reference(table, index, !lru_evict);

  rw_lock_free(&index->lock);

  /* The index is being dropped, remove any compression stats for it. */
  if (!lru_evict && DICT_TF_GET_ZIP_SSIZE(index->table->flags) &&
      !index->table->discard_after_ddl) {
    index_id_t id(index->space, index->id);
    mutex_enter(&page_zip_stat_per_index_mutex);
    page_zip_stat_per_index.erase(id);
    mutex_exit(&page_zip_stat_per_index_mutex);
  }

  /* Remove the index from the list of indexes of the table */
  UT_LIST_REMOVE(table->indexes, index);

  /* Remove the index from affected virtual column index list */
  if (dict_index_has_virtual(index)) {
    const dict_col_t *col;
    const dict_v_col_t *vcol;

    for (ulint i = 0; i < dict_index_get_n_fields(index); i++) {
      col = index->get_col(i);
      if (col->is_virtual()) {
        vcol = reinterpret_cast<const dict_v_col_t *>(col);

        /* This could be NULL, when we do add virtual
        column, add index together. We do not need to
        track this virtual column's index */
        if (vcol->v_indexes == nullptr) {
          continue;
        }

        dict_v_idx_list::iterator it;

        for (it = vcol->v_indexes->begin(); it != vcol->v_indexes->end();
             ++it) {
          dict_v_idx_t v_index = *it;
          if (v_index.index == index) {
            vcol->v_indexes->erase(it);
            break;
          }
        }
      }
    }
  }

  const auto size = mem_heap_get_size(index->heap);

  ut_ad(!table->is_intrinsic());
  ut_ad(dict_sys->size >= size);

  dict_sys->size -= size;

  dict_mem_index_free(index);
}

/** Removes an index from the dictionary cache. */
void dict_index_remove_from_cache(dict_table_t *table, /*!< in/out: table */
                                  dict_index_t *index) /*!< in, own: index */
{
  dict_index_remove_from_cache_low(table, index, false);
}

/** Duplicate a virtual column information
@param[in]      v_col   virtual column information to duplicate
@param[in,out]  heap    memory heap
@return the duplicated virtual column */
static dict_v_col_t *dict_duplicate_v_col(const dict_v_col_t *v_col,
                                          mem_heap_t *heap) {
  dict_v_col_t *new_v_col =
      static_cast<dict_v_col_t *>(mem_heap_zalloc(heap, sizeof(*v_col)));

  ut_ad(v_col->v_indexes == nullptr);

  /* Currently, only m_col and v_indexes would be cared in future use,
  and v_indexes is always nullptr. So the memcpy can work for it */
  memcpy(new_v_col, v_col, sizeof(*v_col));

  return (new_v_col);
}

/** Tries to find column names for the index and sets the col field of the
index.
@param[in]      table   table
@param[in,out]  index   index
@param[in]      add_v   new virtual columns added along with an add index call
@return true if the column names were found */
static bool dict_index_find_and_set_cols(const dict_table_t *table,
                                         dict_index_t *index,
                                         const dict_add_v_col_t *add_v) {
  std::vector<ulint, ut::allocator<ulint>> col_added;
  std::vector<ulint, ut::allocator<ulint>> v_col_added;

  ut_ad(table != nullptr && index != nullptr);
  ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);
  ut_ad(!dict_sys_mutex_own());

  for (ulint i = 0; i < index->n_fields; i++) {
    ulint j;
    dict_field_t *field = index->get_field(i);

    for (j = 0; j < table->n_cols; j++) {
      if (!strcmp(table->get_col_name(j), field->name)) {
        /* Check if same column is being assigned again
        which suggest that column has duplicate name. */
        bool exists =
            std::find(col_added.begin(), col_added.end(), j) != col_added.end();

        if (exists) {
          /* Duplicate column found. */
          goto dup_err;
        }

        field->col = table->get_col(j);

        col_added.push_back(j);

        goto found;
      }
    }

    /* Let's check if it is a virtual column */
    for (j = 0; j < table->n_v_cols; j++) {
      if (!strcmp(dict_table_get_v_col_name(table, j), field->name)) {
        /* Check if same column is being assigned again
        which suggest that column has duplicate name. */
        bool exists = std::find(v_col_added.begin(), v_col_added.end(), j) !=
                      v_col_added.end();

        if (exists) {
          /* Duplicate column found. */
          break;
        }

        field->col =
            reinterpret_cast<dict_col_t *>(dict_table_get_nth_v_col(table, j));

        v_col_added.push_back(j);

        goto found;
      }
    }

    if (add_v) {
      for (j = 0; j < add_v->n_v_col; j++) {
        if (!strcmp(add_v->v_col_name[j], field->name)) {
          /* Once add_v is not nullptr, it comes from ALTER TABLE.
          To make sure the index can work after ALTER TABLE path,
          which may happen when the ALTER TABLE gets rolled back,
          it is a must to duplicate the virtual column information,
          in case the passed in object would be freed after ALTER TABLE. */

          dict_sys_mutex_enter();
          uint64_t old_size = mem_heap_get_size(table->heap);
          dict_v_col_t *vcol =
              dict_duplicate_v_col(&add_v->v_col[j], table->heap);
          field->col = &vcol->m_col;
          dict_sys->size += mem_heap_get_size(table->heap) - old_size;
          dict_sys_mutex_exit();

          goto found;
        }
      }
    }

  dup_err:
#ifdef UNIV_DEBUG
    /* It is an error not to find a matching column. */
    ib::error(ER_IB_MSG_182)
        << "No matching column for " << field->name << " in index "
        << index->name << " of table " << table->name;
#endif /* UNIV_DEBUG */
    return false;

  found:;
  }

  return true;
}

/** Copies fields contained in index2 to index1. */
static void dict_index_copy(dict_index_t *index1, /*!< in: index to copy to */
                            dict_index_t *index2, /*!< in: index to copy from */
                            const dict_table_t *table, /*!< in: table */
                            ulint start, /*!< in: first position to copy */
                            ulint end)   /*!< in: last position to copy */
{
  dict_field_t *field;
  ulint i;

  /* Copy fields contained in index2 */

  for (i = start; i < end; i++) {
    field = index2->get_field(i);

    dict_index_add_col(index1, table, field->col, field->prefix_len,
                       field->is_ascending);
  }
}

/** Copies types of fields contained in index to tuple.
@param[in,out] tuple Data tuple
@param[in] index Index
@param[in] n_fields Number of field types to copy */
void dict_index_copy_types(dtuple_t *tuple, const dict_index_t *index,
                           ulint n_fields) {
  ulint i;

  if (dict_index_is_ibuf(index)) {
    /* For IBUF index set field types explicitly. */
    for (ulint i = 0; i < n_fields; i++) {
      dtype_t *dfield_type = dfield_get_type(dtuple_get_nth_field(tuple, i));
      dtype_set(dfield_type, DATA_BINARY, 0, 0);
    }

    return;
  }

  for (i = 0; i < n_fields; i++) {
    const dict_field_t *ifield;
    dtype_t *dfield_type;

    ifield = index->get_field(i);
    dfield_type = dfield_get_type(dtuple_get_nth_field(tuple, i));
    ifield->col->copy_type(dfield_type);
    if (dict_index_is_spatial(index) &&
        DATA_GEOMETRY_MTYPE(dfield_type->mtype)) {
      dfield_type->prtype |= DATA_GIS_MBR;
    }
  }
}

/** Copies types of virtual columns contained in table to tuple and sets all
fields of the tuple to the SQL NULL value.  This function should
be called right after dtuple_create().
@param[in,out]  tuple   data tuple
@param[in]      table   table
*/
void dict_table_copy_v_types(dtuple_t *tuple, const dict_table_t *table) {
  /* tuple could have more virtual columns than existing table,
  if we are calling this for creating index along with adding
  virtual columns */
  ulint n_fields = std::min(dtuple_get_n_v_fields(tuple),
                            static_cast<ulint>(table->n_v_def));

  for (ulint i = 0; i < n_fields; i++) {
    dfield_t *dfield = dtuple_get_nth_v_field(tuple, i);
    dtype_t *dtype = dfield_get_type(dfield);

    dfield_set_null(dfield);
    dict_table_get_nth_v_col(table, i)->m_col.copy_type(dtype);
  }
}
/** Copies types of columns contained in table to tuple and sets all
 fields of the tuple to the SQL NULL value.  This function should
 be called right after dtuple_create(). */
void dict_table_copy_types(dtuple_t *tuple,           /*!< in/out: data tuple */
                           const dict_table_t *table) /*!< in: table */
{
  ulint i;

  for (i = 0; i < dtuple_get_n_fields(tuple); i++) {
    dfield_t *dfield = dtuple_get_nth_field(tuple, i);
    dtype_t *dtype = dfield_get_type(dfield);

    dfield_set_null(dfield);
    table->get_col(i)->copy_type(dtype);
  }

  dict_table_copy_v_types(tuple, table);
}

void dict_table_wait_for_bg_threads_to_exit(dict_table_t *table,
                                            std::chrono::microseconds delay) {
  fts_t *fts = table->fts;

  ut_ad(mutex_own(&fts->bg_threads_mutex));

  while (fts->bg_threads > 0) {
    mutex_exit(&fts->bg_threads_mutex);

    std::this_thread::sleep_for(delay);

    mutex_enter(&fts->bg_threads_mutex);
  }
}

/** Builds the internal dictionary cache representation for a clustered
 index, containing also system fields not defined by the user.
 @return own: the internal representation of the clustered index */
static dict_index_t *dict_index_build_internal_clust(
    const dict_table_t *table, /*!< in: table */
    dict_index_t *index)       /*!< in: user representation of
                               a clustered index */
{
  ut_ad(table && index);
  ut_ad(index->is_clustered());
  ut_ad(!dict_index_is_ibuf(index));

  ut_ad(!dict_sys_mutex_own());
  ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

  uint32_t total_cols_in_table = table->get_total_cols();

  /* Create a new index object with certainly enough fields */
  dict_index_t *new_index =
      dict_mem_index_create(table->name.m_name, index->name, table->space,
                            index->type, index->n_fields + total_cols_in_table);

  /* Copy other relevant data from the old index struct to the new
  struct: it inherits the values */

  new_index->n_user_defined_cols = index->n_fields;

  new_index->id = index->id;

  /* Copy the fields of index */
  dict_index_copy(new_index, index, table, 0, index->n_fields);

  if (dict_index_is_unique(index)) {
    /* Only the fields defined so far are needed to identify
    the index entry uniquely */

    new_index->n_uniq = new_index->n_def;
  } else {
    /* Also the row id is needed to identify the entry */
    new_index->n_uniq = 1 + new_index->n_def;
  }

  new_index->trx_id_offset = 0;

  uint16_t n_fields_processed = 0;

  /* Add phy_pos for fields defined so far */
  while (n_fields_processed < new_index->n_def) {
    dict_field_t *field = new_index->get_field(n_fields_processed);
    dict_col_t *col = field->col;
    if (!table->has_row_versions()) {
      if (field->prefix_len != 0) {
        /* This column prefix is used in PK */
        col->set_prefix_phy_pos(n_fields_processed);
      } else {
        col->set_phy_pos(n_fields_processed);
      }
    } else {
      ut_ad(col->get_phy_pos() != UINT32_UNDEFINED);
    }
    n_fields_processed++;
  }

  auto set_phy_pos = [&](const dict_col_t *cmp_col) {
    dict_col_t *col =
        const_cast<dict_col_t *>(new_index->get_col(n_fields_processed));
    ut_a(col == cmp_col);

    if (!table->has_row_versions()) {
      if (col->get_phy_pos() != UINT32_UNDEFINED && col->has_prefix_phy_pos()) {
        /* This column prefix is used in PK */
        ut_ad(new_index->get_col(col->get_prefix_phy_pos()) == col);
        col->set_col_phy_pos(n_fields_processed);
      } else {
        col->set_phy_pos(n_fields_processed);
      }
    } else {
      ut_ad(col->get_phy_pos() != UINT32_UNDEFINED);
    }

    n_fields_processed++;
  };

  /* Add system columns */

  size_t trx_id_pos = new_index->n_def;

  /* Add ROW ID */
  if (!dict_index_is_unique(index)) {
    dict_index_add_col(new_index, table, table->get_sys_col(DATA_ROW_ID), 0,
                       true);
    set_phy_pos(table->get_sys_col(DATA_ROW_ID));
    trx_id_pos++;
  }

  /* Add TRX ID */
  dict_index_add_col(new_index, table, table->get_sys_col(DATA_TRX_ID), 0,
                     true);
  set_phy_pos(table->get_sys_col(DATA_TRX_ID));

  for (size_t i = 0; i < trx_id_pos; i++) {
    ulint fixed_size =
        new_index->get_col(i)->get_fixed_size(dict_table_is_comp(table));

    if (fixed_size == 0) {
      new_index->trx_id_offset = 0;

      break;
    }

    dict_field_t *field = new_index->get_field(i);
    if (field->prefix_len > 0) {
      new_index->trx_id_offset = 0;

      break;
    }

    /* Add fixed_size to new_index->trx_id_offset.
    Because the latter is a bit-field, an overflow
    can theoretically occur. Check for it. */
    fixed_size += new_index->trx_id_offset;

    new_index->trx_id_offset = fixed_size;

    if (new_index->trx_id_offset != fixed_size) {
      /* Overflow. Pretend that this is a
      variable-length PRIMARY KEY. */
      ut_d(ut_error);
      ut_o(new_index->trx_id_offset = 0);
      ut_o(break);
    }
  }

  /* Add ROLL PTR. UNDO logging is turned-off for intrinsic table and so
  DATA_ROLL_PTR system columns are not added as default system columns to such
  tables. */
  if (!table->is_intrinsic()) {
    dict_index_add_col(new_index, table, table->get_sys_col(DATA_ROLL_PTR), 0,
                       true);
    set_phy_pos(table->get_sys_col(DATA_ROLL_PTR));
  }

  /* Remember the table columns already contained in new_index */
  bool *indexed = static_cast<bool *>(ut::zalloc_withkey(
      UT_NEW_THIS_FILE_PSI_KEY, total_cols_in_table * sizeof *indexed));

  /* Mark the table columns already contained in new_index */
  for (size_t i = 0; i < new_index->n_def; i++) {
    dict_field_t *field = new_index->get_field(i);

    /* If there is only a prefix of the column in the index
    field, do not mark the column as contained in the index */

    if (field->prefix_len == 0) {
      indexed[field->col->ind] = true;
    }
  }

  /* Add to new_index non-system columns of table not yet included there */
  for (size_t i = 0; i < table->get_n_user_cols(); i++) {
    dict_col_t *col = table->get_col(i);
    ut_ad(col->mtype != DATA_SYS);

    if (indexed[col->ind]) {
      continue;
    }

    dict_index_add_col(new_index, table, col, 0, true);
    set_phy_pos(new_index->get_col(n_fields_processed));
  }

  /* Add INSTANT DROP column */
  for (size_t i = table->n_cols; i < total_cols_in_table; i++) {
    dict_col_t *col = table->get_col(i);
    ut_ad(col->mtype != DATA_SYS);

    ut_ad(!indexed[col->ind]);
    dict_index_add_col(new_index, table, col, 0, true);
    /* Physical position must have already been set */
    ut_ad(col->get_phy_pos() != UINT32_UNDEFINED);
    n_fields_processed++;
  }

  ut_ad(n_fields_processed == new_index->n_def);

  ut::free(indexed);

  if (!table->is_system_table) {
    if (table->has_row_versions()) {
      new_index->create_fields_array();
    }
    new_index->create_nullables(table->current_row_version);
  }

  ut_ad(UT_LIST_GET_LEN(table->indexes) == 0);

  new_index->cached = true;

  return (new_index);
}

/** Builds the internal dictionary cache representation for a non-clustered
 index, containing also system fields not defined by the user.
 @return own: the internal representation of the non-clustered index */
static dict_index_t *dict_index_build_internal_non_clust(
    const dict_table_t *table, /*!< in: table */
    dict_index_t *index)       /*!< in: user representation of
                               a non-clustered index */
{
  dict_field_t *field;
  dict_index_t *new_index;
  dict_index_t *clust_index;
  ulint i;
  bool *indexed;

  ut_ad(table && index);
  ut_ad(!index->is_clustered());
  ut_ad(!dict_index_is_ibuf(index));
  ut_ad(!dict_sys_mutex_own());
  ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

  /* The clustered index should be the first in the list of indexes */
  clust_index = UT_LIST_GET_FIRST(table->indexes);

  ut_ad(clust_index);
  ut_ad(clust_index->is_clustered());
  ut_ad(!dict_index_is_ibuf(clust_index));

  /* Create a new index */
  new_index = dict_mem_index_create(table->name.m_name, index->name,
                                    index->space, index->type,
                                    index->n_fields + 1 + clust_index->n_uniq);

  /* Copy other relevant data from the old index
  struct to the new struct: it inherits the values */

  new_index->n_user_defined_cols = index->n_fields;

  new_index->id = index->id;

  /* Copy fields from index to new_index */
  dict_index_copy(new_index, index, table, 0, index->n_fields);

  /* Remember the table columns already contained in new_index */
  indexed = static_cast<bool *>(ut::zalloc_withkey(
      UT_NEW_THIS_FILE_PSI_KEY, table->n_cols * sizeof *indexed));

  /* Mark the table columns already contained in new_index */
  for (i = 0; i < new_index->n_def; i++) {
    field = new_index->get_field(i);

    if (field->col->is_virtual()) {
      continue;
    }

    /* If there is only a prefix of the column in the index
    field, do not mark the column as contained in the index */

    if (field->prefix_len == 0) {
      indexed[field->col->ind] = true;
    }
  }

  /* Add to new_index the columns necessary to determine the clustered
  index entry uniquely */

  for (i = 0; i < clust_index->n_uniq; i++) {
    field = clust_index->get_field(i);

    if (!indexed[field->col->ind]) {
      dict_index_add_col(new_index, table, field->col, field->prefix_len,
                         field->is_ascending);
    } else if (dict_index_is_spatial(index)) {
      /*For spatial index, we still need to add the
      field to index. */
      dict_index_add_col(new_index, table, field->col, field->prefix_len,
                         field->is_ascending);
    }
  }

  ut::free(indexed);

  if (dict_index_is_unique(index)) {
    new_index->n_uniq = index->n_fields;
  } else {
    new_index->n_uniq = new_index->n_def;
  }

  /* Set the n_fields value in new_index to the actual defined
  number of fields */

  new_index->n_fields = new_index->n_def;

  new_index->cached = true;

  return (new_index);
}

/***********************************************************************
Builds the internal dictionary cache representation for an FTS index.
@return own: the internal representation of the FTS index */
static dict_index_t *dict_index_build_internal_fts(
    dict_table_t *table, /*!< in: table */
    dict_index_t *index) /*!< in: user representation of an FTS index */
{
  dict_index_t *new_index;

  ut_ad(table && index);
  ut_ad(index->type == DICT_FTS);
  ut_ad(!dict_sys_mutex_own());
  ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

  /* Create a new index */
  new_index = dict_mem_index_create(table->name.m_name, index->name,
                                    index->space, index->type, index->n_fields);

  /* Copy other relevant data from the old index struct to the new
  struct: it inherits the values */

  new_index->n_user_defined_cols = index->n_fields;

  new_index->id = index->id;

  /* Copy fields from index to new_index */
  dict_index_copy(new_index, index, table, 0, index->n_fields);

  new_index->n_uniq = 0;
  new_index->cached = true;

  if (table->fts->cache == nullptr) {
    table->fts->cache = fts_cache_create(table);
  }

  rw_lock_x_lock(&table->fts->cache->init_lock, UT_LOCATION_HERE);
  /* Notify the FTS cache about this index. */
  fts_cache_index_cache_create(table, new_index);
  rw_lock_x_unlock(&table->fts->cache->init_lock);

  return (new_index);
}
/*====================== FOREIGN KEY PROCESSING ========================*/

/** Checks if a table is referenced by foreign keys.
 @return true if table is referenced by a foreign key */
bool dict_table_is_referenced_by_foreign_key(
    const dict_table_t *table) /*!< in: InnoDB table */
{
  return (!table->referenced_set.empty());
}

/** Removes a foreign constraint struct from the dictionary cache. */
void dict_foreign_remove_from_cache(
    dict_foreign_t *foreign) /*!< in, own: foreign constraint */
{
  ut_ad(dict_sys_mutex_own());
  ut_a(foreign);

  if (foreign->referenced_table != nullptr) {
    foreign->referenced_table->referenced_set.erase(foreign);
  }

  if (foreign->foreign_table != nullptr) {
    foreign->foreign_table->foreign_set.erase(foreign);
  }

  dict_foreign_free(foreign);
}

/** Looks for the foreign constraint from the foreign and referenced lists
 of a table.
 @return foreign constraint */
static dict_foreign_t *dict_foreign_find(
    dict_table_t *table,     /*!< in: table object */
    dict_foreign_t *foreign) /*!< in: foreign constraint */
{
  ut_ad(dict_sys_mutex_own());

  ut_ad(dict_foreign_set_validate(table->foreign_set));
  ut_ad(dict_foreign_set_validate(table->referenced_set));

  dict_foreign_set::iterator it = table->foreign_set.find(foreign);

  if (it != table->foreign_set.end()) {
    return (*it);
  }

  it = table->referenced_set.find(foreign);

  if (it != table->referenced_set.end()) {
    return (*it);
  }

  return (nullptr);
}

/** Tries to find an index whose first fields are the columns in the array,
 in the same order and is not marked for deletion and is not the same
 as types_idx.
 @return matching index, NULL if not found */
dict_index_t *dict_foreign_find_index(
    const dict_table_t *table, /*!< in: table */
    const char **col_names,
    /*!< in: column names, or NULL
    to use table->col_names */
    const char **columns, /*!< in: array of column names */
    ulint n_cols,         /*!< in: number of columns */
    const dict_index_t *types_idx,
    /*!< in: NULL or an index
    whose types the column types
    must match */
    bool check_charsets,
    /*!< in: whether to check
    charsets.  only has an effect
    if types_idx != NULL */
    ulint check_null)
/*!< in: nonzero if none of
the columns must be declared
NOT NULL */
{
  const dict_index_t *index;

  ut_ad(dict_sys_mutex_own());

  index = table->first_index();

  while (index != nullptr) {
    if (types_idx != index && !(index->type & DICT_FTS) &&
        !dict_index_is_spatial(index) && !index->to_be_dropped &&
        (!(index->uncommitted &&
           ((index->online_status == ONLINE_INDEX_ABORTED_DROPPED) ||
            (index->online_status == ONLINE_INDEX_ABORTED)))) &&
        dict_foreign_qualify_index(table, col_names, columns, n_cols, index,
                                   types_idx, check_charsets, check_null)) {
      return const_cast<dict_index_t *>(index);
    }

    index = index->next();
  }

  return (nullptr);
}

/** Report an error in a foreign key definition. */
static void dict_foreign_error_report_low(
    FILE *file,       /*!< in: output stream */
    const char *name) /*!< in: table name */
{
  rewind(file);
  ut_print_timestamp(file);
  fprintf(file, " Error in foreign key constraint of table %s:\n", name);
}

/** Report an error in a foreign key definition. */
static void dict_foreign_error_report(
    FILE *file,         /*!< in: output stream */
    dict_foreign_t *fk, /*!< in: foreign key constraint */
    const char *msg)    /*!< in: the error message */
{
  mutex_enter(&dict_foreign_err_mutex);
  dict_foreign_error_report_low(file, fk->foreign_table_name);
  fputs(msg, file);
  fputs(" Constraint:\n", file);
  dict_print_info_on_foreign_key_in_create_format(file, nullptr, fk, true);
  putc('\n', file);
  if (fk->foreign_index) {
    fprintf(file,
            "The index in the foreign key in table is"
            " %s\n%s\n",
            fk->foreign_index->name(), FOREIGN_KEY_CONSTRAINTS_MSG);
  }
  mutex_exit(&dict_foreign_err_mutex);
}

/** Adds a foreign key constraint object to the dictionary cache. May free
 the object if there already is an object with the same identifier in.
 At least one of the foreign table and the referenced table must already
 be in the dictionary cache!
 @return DB_SUCCESS or error code */
dberr_t dict_foreign_add_to_cache(dict_foreign_t *foreign,
                                  /*!< in, own: foreign key constraint */
                                  const char **col_names,
                                  /*!< in: column names, or NULL to use
                                  foreign->foreign_table->col_names */
                                  bool check_charsets,
                                  /*!< in: whether to check charset
                                  compatibility */
                                  bool can_free_fk,
                                  /*!< in: whether free existing FK */
                                  dict_err_ignore_t ignore_err)
/*!< in: error to be ignored */
{
  dict_table_t *for_table;
  dict_table_t *ref_table;
  dict_foreign_t *for_in_cache = nullptr;
  dict_index_t *index;
  bool added_to_referenced_list = false;
  FILE *ef = dict_foreign_err_file;

  DBUG_TRACE;
  DBUG_PRINT("dict_foreign_add_to_cache", ("id: %s", foreign->id));

  ut_ad(dict_sys_mutex_own());

  for_table =
      dict_table_check_if_in_cache_low(foreign->foreign_table_name_lookup);

  ref_table =
      dict_table_check_if_in_cache_low(foreign->referenced_table_name_lookup);
  ut_a(for_table || ref_table);

  if (for_table) {
    for_in_cache = dict_foreign_find(for_table, foreign);
  }

  if (!for_in_cache && ref_table) {
    for_in_cache = dict_foreign_find(ref_table, foreign);
  }

  if (for_in_cache && for_in_cache != foreign) {
    /* Free the foreign object */
    dict_foreign_free(foreign);
  } else {
    for_in_cache = foreign;
  }

  if (ref_table && !for_in_cache->referenced_table) {
    index = dict_foreign_find_index(
        ref_table, nullptr, for_in_cache->referenced_col_names,
        for_in_cache->n_fields, for_in_cache->foreign_index, check_charsets,
        false);

    if (index == nullptr && !(ignore_err & DICT_ERR_IGNORE_FK_NOKEY)) {
      dict_foreign_error_report(ef, for_in_cache,
                                "there is no index in referenced table"
                                " which would contain\n"
                                "the columns as the first columns,"
                                " or the data types in the\n"
                                "referenced table do not match"
                                " the ones in table.");

      if (for_in_cache == foreign && can_free_fk) {
        mem_heap_free(foreign->heap);
      }

      return DB_CANNOT_ADD_CONSTRAINT;
    }

    for_in_cache->referenced_table = ref_table;
    for_in_cache->referenced_index = index;

    std::pair<dict_foreign_set::iterator, bool> ret =
        ref_table->referenced_set.insert(for_in_cache);

    ut_a(ret.second); /* second is true if the insertion
                      took place */
    added_to_referenced_list = true;
  }

  if (for_table && !for_in_cache->foreign_table) {
    index = dict_foreign_find_index(
        for_table, col_names, for_in_cache->foreign_col_names,
        for_in_cache->n_fields, for_in_cache->referenced_index, check_charsets,
        for_in_cache->type & (DICT_FOREIGN_ON_DELETE_SET_NULL |
                              DICT_FOREIGN_ON_UPDATE_SET_NULL));

    if (index == nullptr && !(ignore_err & DICT_ERR_IGNORE_FK_NOKEY)) {
      dict_foreign_error_report(ef, for_in_cache,
                                "there is no index in the table"
                                " which would contain\n"
                                "the columns as the first columns,"
                                " or the data types in the\n"
                                "table do not match"
                                " the ones in the referenced table\n"
                                "or one of the ON ... SET NULL columns"
                                " is declared NOT NULL.");

      if (for_in_cache == foreign) {
        if (added_to_referenced_list) {
          const dict_foreign_set::size_type n =
              ref_table->referenced_set.erase(for_in_cache);

          ut_a(n == 1); /* the number of
                        elements removed must
                        be one */
        }
        mem_heap_free(foreign->heap);
      }

      return DB_CANNOT_ADD_CONSTRAINT;
    }

    for_in_cache->foreign_table = for_table;
    for_in_cache->foreign_index = index;

    std::pair<dict_foreign_set::iterator, bool> ret =
        for_table->foreign_set.insert(for_in_cache);

    ut_a(ret.second); /* second is true if the insertion
                      took place */
  }

  /* We need to move the table to the non-LRU end of the table LRU
  list. Otherwise it will be evicted from the cache. */

  if (ref_table != nullptr) {
    dict_table_prevent_eviction(ref_table);
  }

  if (for_table != nullptr) {
    dict_table_prevent_eviction(for_table);
  }

  ut_ad(dict_lru_validate());
  return DB_SUCCESS;
}

/** Finds the highest [number] for foreign key constraints of the table. Looks
 only at the >= 4.0.18-format id's, which are of the form
 databasename/tablename_ibfk_[number].
 TODO: Remove this function once in-place ALTER TABLE code is
 updated to avoid its use.
 @return highest number, 0 if table has no new format foreign key constraints */
ulint dict_table_get_highest_foreign_id(
    dict_table_t *table) /*!< in: table in the dictionary memory cache */
{
  dict_foreign_t *foreign;
  char *endp;
  ulint biggest_id = 0;
  ulint id;
  ulint len;

  DBUG_TRACE;

  ut_a(table);

  len = ut_strlen(table->name.m_name);

  for (dict_foreign_set::iterator it = table->foreign_set.begin();
       it != table->foreign_set.end(); ++it) {
    foreign = *it;

    if (ut_strlen(foreign->id) > ((sizeof dict_ibfk) - 1) + len &&
        0 == ut_memcmp(foreign->id, table->name.m_name, len) &&
        0 == ut_memcmp(foreign->id + len, dict_ibfk, (sizeof dict_ibfk) - 1) &&
        foreign->id[len + ((sizeof dict_ibfk) - 1)] != '0') {
      /* It is of the >= 4.0.18 format */

      id = strtoul(foreign->id + len + ((sizeof dict_ibfk) - 1), &endp, 10);
      if (*endp == '\0') {
        ut_a(id != biggest_id);

        if (id > biggest_id) {
          biggest_id = id;
        }
      }
    }
  }

  ulint size = table->foreign_set.size();

  biggest_id = (size > biggest_id) ? size : biggest_id;

  DBUG_PRINT("dict_table_get_highest_foreign_id", ("id: %lu", biggest_id));

  return biggest_id;
}

/*==================== END OF FOREIGN KEY PROCESSING ====================*/

#ifdef UNIV_DEBUG
/** Checks that a tuple has n_fields_cmp value in a sensible range, so that
 no comparison can occur with the page number field in a node pointer.
 @return true if ok */
bool dict_index_check_search_tuple(
    const dict_index_t *index, /*!< in: index tree */
    const dtuple_t *tuple)     /*!< in: tuple used in a search */
{
  ut_a(index);
  ut_a(dtuple_get_n_fields_cmp(tuple) <=
       dict_index_get_n_unique_in_tree(index));
  ut_ad(index->page != FIL_NULL);
  ut_ad(index->page >= FSP_FIRST_INODE_PAGE_NO);
  ut_ad(dtuple_check_typed(tuple));
  ut_ad(!(index->type & DICT_FTS));
  return true;
}
#endif /* UNIV_DEBUG */

/** Builds a node pointer out of a physical record and a page number.
 @return own: node pointer */
dtuple_t *dict_index_build_node_ptr(const dict_index_t *index, /*!< in: index */
                                    const rec_t *rec,  /*!< in: record for which
                                                       to build node  pointer */
                                    page_no_t page_no, /*!< in: page number to
                                                       put in node pointer */
                                    mem_heap_t *heap, /*!< in: memory heap where
                                                      pointer created */
                                    ulint level) /*!< in: level of rec in tree:
                                                 0 means leaf level */
{
  dtuple_t *tuple;
  dfield_t *field;
  byte *buf;
  ulint n_unique;

  if (dict_index_is_ibuf(index)) {
    /* In a universal index tree, we take the whole record as
    the node pointer if the record is on the leaf level,
    on non-leaf levels we remove the last field, which
    contains the page number of the child page */

    ut_a(!dict_table_is_comp(index->table));
    n_unique = rec_get_n_fields_old_raw(rec);

    if (level > 0) {
      ut_a(n_unique > 1);
      n_unique--;
    }
  } else {
    n_unique = dict_index_get_n_unique_in_tree_nonleaf(index);
  }

  tuple = dtuple_create(heap, n_unique + 1);

  /* When searching in the tree for the node pointer, we must not do
  comparison on the last field, the page number field, as on upper
  levels in the tree there may be identical node pointers with a
  different page number; therefore, we set the n_fields_cmp to one
  less: */

  dtuple_set_n_fields_cmp(tuple, n_unique);

  dict_index_copy_types(tuple, index, n_unique);

  buf = static_cast<byte *>(mem_heap_alloc(heap, 4));

  mach_write_to_4(buf, page_no);

  field = dtuple_get_nth_field(tuple, n_unique);
  dfield_set_data(field, buf, 4);

  dtype_set(dfield_get_type(field), DATA_SYS_CHILD, DATA_NOT_NULL, 4);

  rec_copy_prefix_to_dtuple(tuple, rec, index, n_unique, heap);
  dtuple_set_info_bits(tuple,
                       dtuple_get_info_bits(tuple) | REC_STATUS_NODE_PTR);

  ut_ad(dtuple_check_typed(tuple));

  return (tuple);
}

rec_t *dict_index_copy_rec_order_prefix(const dict_index_t *index,
                                        const rec_t *rec, ulint *n_fields,

                                        byte **buf, size_t *buf_size) {
  ulint n;

  UNIV_PREFETCH_R(rec);

  if (dict_index_is_ibuf(index)) {
    ut_a(!dict_table_is_comp(index->table));
    n = rec_get_n_fields_old_raw(rec);
  } else {
    if (page_is_leaf(page_align(rec))) {
      n = dict_index_get_n_unique_in_tree(index);
    } else {
      n = dict_index_get_n_unique_in_tree_nonleaf(index);
      /* For internal node of R-tree, since we need to
      compare the page no field, so, we need to copy this
      field as well. */
      if (dict_index_is_spatial(index)) {
        n++;
      }
    }
  }

  *n_fields = n;
  return (rec_copy_prefix_to_buf(rec, index, n, buf, buf_size));
}

/** Builds a typed data tuple out of a physical record.
 @return own: data tuple */
dtuple_t *dict_index_build_data_tuple(
    dict_index_t *index, /*!< in: index tree */
    rec_t *rec,          /*!< in: record for which to build data tuple */
    ulint n_fields,      /*!< in: number of data fields */
    mem_heap_t *heap)    /*!< in: memory heap where tuple created */
{
  dtuple_t *tuple;

  ut_ad(dict_table_is_comp(index->table) ||
        n_fields <= rec_get_n_fields_old(rec, index));

  tuple = dtuple_create(heap, n_fields);

  dict_index_copy_types(tuple, index, n_fields);

  rec_copy_prefix_to_dtuple(tuple, rec, index, n_fields, heap);

  ut_ad(dtuple_check_typed(tuple));

  return (tuple);
}

/** Calculates the minimum record length in an index. */
ulint dict_index_calc_min_rec_len(const dict_index_t *index) /*!< in: index */
{
  ulint sum = 0;
  ulint i;
  ulint comp = dict_table_is_comp(index->table);

  if (comp) {
    ulint nullable = 0;
    sum = REC_N_NEW_EXTRA_BYTES;
    for (i = 0; i < dict_index_get_n_fields(index); i++) {
      const dict_col_t *col = index->get_col(i);
      ulint size = col->get_fixed_size(comp);
      sum += size;
      if (!size) {
        size = col->len;
        sum += size < 128 ? 1 : 2;
      }
      if (!(col->prtype & DATA_NOT_NULL)) {
        nullable++;
      }
    }

    /* round the NULL flags up to full bytes */
    sum += UT_BITS_IN_BYTES(nullable);

    return (sum);
  }

  for (i = 0; i < dict_index_get_n_fields(index); i++) {
    sum += index->get_col(i)->get_fixed_size(comp);
  }

  if (sum > 127) {
    sum += 2 * dict_index_get_n_fields(index);
  } else {
    sum += dict_index_get_n_fields(index);
  }

  sum += REC_N_OLD_EXTRA_BYTES;

  return (sum);
}

/** Outputs info on a foreign key of a table in a format suitable for
 CREATE TABLE.
@param[in] file File where to print
@param[in] trx Transaction
@param[in] foreign Foreign key constraint
@param[in] add_newline Whether to add a newline */
void dict_print_info_on_foreign_key_in_create_format(FILE *file, trx_t *trx,
                                                     dict_foreign_t *foreign,
                                                     bool add_newline) {
  const char *stripped_id;
  ulint i;

  if (strchr(foreign->id, '/')) {
    /* Strip the preceding database name from the constraint id */
    stripped_id = foreign->id + 1 + dict_get_db_name_len(foreign->id);
  } else {
    stripped_id = foreign->id;
  }

  putc(',', file);

  if (add_newline) {
    /* SHOW CREATE TABLE wants constraints each printed nicely
    on its own line, while error messages want no newlines
    inserted. */
    fputs("\n ", file);
  }

  fputs(" CONSTRAINT ", file);
  innobase_quote_identifier(file, trx, stripped_id);
  fputs(" FOREIGN KEY (", file);

  for (i = 0;;) {
    innobase_quote_identifier(file, trx, foreign->foreign_col_names[i]);
    if (++i < foreign->n_fields) {
      fputs(", ", file);
    } else {
      break;
    }
  }

  fputs(") REFERENCES ", file);

  if (dict_tables_have_same_db(foreign->foreign_table_name_lookup,
                               foreign->referenced_table_name_lookup)) {
    /* Do not print the database name of the referenced table */
    ut_print_name(file, trx,
                  dict_remove_db_name(foreign->referenced_table_name));
  } else {
    ut_print_name(file, trx, foreign->referenced_table_name);
  }

  putc(' ', file);
  putc('(', file);

  for (i = 0;;) {
    innobase_quote_identifier(file, trx, foreign->referenced_col_names[i]);
    if (++i < foreign->n_fields) {
      fputs(", ", file);
    } else {
      break;
    }
  }

  putc(')', file);

  if (foreign->type & DICT_FOREIGN_ON_DELETE_CASCADE) {
    fputs(" ON DELETE CASCADE", file);
  }

  if (foreign->type & DICT_FOREIGN_ON_DELETE_SET_NULL) {
    fputs(" ON DELETE SET NULL", file);
  }

  if (!(foreign->type & DICT_FOREIGN_ON_DELETE_NO_ACTION) &&
      !(foreign->type & DICT_FOREIGN_ON_DELETE_CASCADE) &&
      !(foreign->type & DICT_FOREIGN_ON_DELETE_SET_NULL)) {
    fputs(" ON DELETE RESTRICT", file);
  }

  if (foreign->type & DICT_FOREIGN_ON_UPDATE_CASCADE) {
    fputs(" ON UPDATE CASCADE", file);
  }

  if (foreign->type & DICT_FOREIGN_ON_UPDATE_SET_NULL) {
    fputs(" ON UPDATE SET NULL", file);
  }

  if (!(foreign->type & DICT_FOREIGN_ON_UPDATE_NO_ACTION) &&
      !(foreign->type & DICT_FOREIGN_ON_UPDATE_CASCADE) &&
      !(foreign->type & DICT_FOREIGN_ON_UPDATE_SET_NULL)) {
    fputs(" ON UPDATE RESTRICT", file);
  }
}
#endif /* !UNIV_HOTBACKUP */

/** Inits the structure for persisting dynamic metadata */
void dict_persist_init(void) {
  dict_persist = static_cast<dict_persist_t *>(
      ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, sizeof(*dict_persist)));

  mutex_create(LATCH_ID_DICT_PERSIST_DIRTY_TABLES, &dict_persist->mutex);

#ifndef UNIV_HOTBACKUP
  UT_LIST_INIT(dict_persist->dirty_dict_tables);
#endif /* !UNIV_HOTBACKUP */

  dict_persist->num_dirty_tables = 0;

  dict_persist->m_persist_immediately.store(false);

  dict_persist->persisters =
      ut::new_withkey<Persisters>(UT_NEW_THIS_FILE_PSI_KEY);
  dict_persist->persisters->add(PM_INDEX_CORRUPTED);
  dict_persist->persisters->add(PM_TABLE_AUTO_INC);

#ifndef UNIV_HOTBACKUP
  dict_persist_update_log_margin();
#endif /* !UNIV_HOTBACKUP */
}

/** Clear the structure */
void dict_persist_close(void) {
  ut::delete_(dict_persist->persisters);

#ifndef UNIV_HOTBACKUP
  ut::delete_(dict_persist->table_buffer);
#endif /* !UNIV_HOTBACKUP */

  mutex_free(&dict_persist->mutex);

  ut::free(dict_persist);
}

#ifndef UNIV_HOTBACKUP
/** Initialize the dynamic metadata according to the table object
@param[in]      table           table object
@param[in,out]  metadata        metadata to be initialized */
static void dict_init_dynamic_metadata(dict_table_t *table,
                                       PersistentTableMetadata *metadata) {
  ut_ad(mutex_own(&dict_persist->mutex));

  ut_ad(metadata->get_table_id() == table->id);

  for (const dict_index_t *index = table->first_index(); index != nullptr;
       index = index->next()) {
    if (index->is_corrupted()) {
      metadata->add_corrupted_index(index_id_t(index->space, index->id));
    }
  }

  if (table->autoinc_persisted != 0) {
    metadata->set_autoinc(table->autoinc_persisted);
  }

  /* Will initialize other metadata here */
}

/** Apply the persistent dynamic metadata read from redo logs or
DDTableBuffer to corresponding table during recovery.
@param[in,out]  table           table
@param[in]      metadata        structure of persistent metadata
@return true if we do apply something to the in-memory table object,
otherwise false */
static bool dict_table_apply_dynamic_metadata(
    dict_table_t *table, const PersistentTableMetadata *metadata) {
  bool get_dirty = false;

  ut_ad(dict_sys_mutex_own());

  /* Apply corrupted index ids first */
  const corrupted_ids_t corrupted_ids = metadata->get_corrupted_indexes();

  for (corrupted_ids_t::const_iterator iter = corrupted_ids.begin();
       iter != corrupted_ids.end(); ++iter) {
    const index_id_t index_id = *iter;
    dict_index_t *index;

    index = const_cast<dict_index_t *>(
        dict_table_find_index_on_id(table, index_id));

    if (index != nullptr) {
      ut_ad(index->space == index_id.m_space_id);

      if (!index->is_corrupted()) {
        index->type |= DICT_CORRUPT;
        get_dirty = true;
      }

    } else {
      /* In some cases, we could only load some indexes
      of a table but not all(See dict_load_indexes()).
      So we might not find it here */
      ib::info(ER_IB_MSG_184)
          << "Failed to find the index: " << index_id.m_index_id
          << " in space: " << index_id.m_space_id
          << " of table: " << table->name << "(table id: " << table->id
          << "). The index should have been dropped"
          << " or couldn't be loaded.";
    }
  }

  /* FIXME: Move this to the beginning of this function once corrupted
  index IDs are also written back to dd::Table::se_private_data. */
  /* Here is how version play role. Basically, version would be increased
  by one during every DDL. So applying metadata here should only be
  done when the versions match. One reason for this version is that
  autoinc counter may not be applied if it's bigger if the version is
  older.
  If the version of metadata is older than current table,
  then table already has the latest metadata, the old one should be
  discarded.
  If the metadata version is bigger than the one in table.
  it could be that an ALTER TABLE has been rolled back, so metadata
  in new version should be ignored too. */
  if (table->version != metadata->get_version()) {
    return (get_dirty);
  }

  uint64_t autoinc = metadata->get_autoinc();

  /* This happens during recovery, so no locks are needed. */
  if (autoinc > table->autoinc_persisted) {
    table->autoinc = autoinc;
    table->autoinc_persisted = autoinc;

    get_dirty = true;
  }

  /* Will apply other persistent metadata here */

  return (get_dirty);
}

/** Read persistent dynamic metadata stored in a buffer
@param[in]      buffer          buffer to read
@param[in]      size            size of data in buffer
@param[in]      metadata        where we store the metadata from buffer */
void dict_table_read_dynamic_metadata(const byte *buffer, ulint size,
                                      PersistentTableMetadata *metadata) {
  const byte *pos = buffer;
  persistent_type_t type;
  Persister *persister;
  ulint consumed;
  bool corrupt;

  while (size > 0) {
    type = static_cast<persistent_type_t>(pos[0]);
    ut_ad(type > PM_SMALLEST_TYPE && type < PM_BIGGEST_TYPE);

    persister = dict_persist->persisters->get(type);
    ut_ad(persister != nullptr);

    consumed = persister->read(*metadata, pos, size, &corrupt);
    ut_ad(consumed != 0);
    ut_ad(size >= consumed);
    ut_ad(!corrupt);

    size -= consumed;
    pos += consumed;
  }

  ut_ad(size == 0);
}

/** Check if there is any latest persistent dynamic metadata recorded
in DDTableBuffer table of the specific table. If so, read the metadata and
update the table object accordingly. It's used when loading table.
@param[in]      table           table object */
void dict_table_load_dynamic_metadata(dict_table_t *table) {
  DDTableBuffer *table_buffer;

  ut_ad(dict_sys != nullptr);
  ut_ad(dict_sys_mutex_own());
  ut_ad(!table->is_temporary());

  table_buffer = dict_persist->table_buffer;

  mutex_enter(&dict_persist->mutex);

  std::string *readmeta;
  uint64_t version;
  readmeta = table_buffer->get(table->id, &version);

  if (readmeta->length() != 0) {
    /* Persistent dynamic metadata of this table have changed
    recently, we need to update them to in-memory table */
    PersistentTableMetadata metadata(table->id, version);

    dict_table_read_dynamic_metadata(
        reinterpret_cast<const byte *>(readmeta->data()), readmeta->length(),
        &metadata);

    bool is_dirty = dict_table_apply_dynamic_metadata(table, &metadata);

    /* If !is_dirty, it could be either:
    1. It's first time to load this table, and the corrupted
    index marked has been dropped. Current dirty_status should
    be METADATA_CLEAN.
    2. It's the second time to apply dynamic metadata to this
    table, current in-memory dynamic metadata is up-to-date.
    Current dirty_status should be METADATA_BUFFERED.
    In both cases, we don't have to change the dirty_status */
    if (is_dirty) {
      UT_LIST_ADD_LAST(dict_persist->dirty_dict_tables, table);
      table->dirty_status.store(METADATA_BUFFERED);
      ut_d(table->in_dirty_dict_tables_list = true);
    }
  }

  mutex_exit(&dict_persist->mutex);

  ut::delete_(readmeta);
}

/** Mark the dirty_status of a table as METADATA_DIRTY, and add it to the
dirty_dict_tables list if necessary.
@param[in,out]  table           table */
void dict_table_mark_dirty(dict_table_t *table) {
  ut_ad(!table->is_temporary());

  /* We should not adding dynamic metadata so late in shutdown phase and
  this data would only be retrieved during recovery. */
  ut_ad(srv_shutdown_state.load() < SRV_SHUTDOWN_FLUSH_PHASE);

  mutex_enter(&dict_persist->mutex);

  switch (table->dirty_status.load()) {
    case METADATA_DIRTY:
      break;
    case METADATA_CLEAN:
      /* Not in dirty_tables list, add it now */
      UT_LIST_ADD_LAST(dict_persist->dirty_dict_tables, table);
      ut_d(table->in_dirty_dict_tables_list = true);
      [[fallthrough]];
    case METADATA_BUFFERED:
      table->dirty_status.store(METADATA_DIRTY);
      ++dict_persist->num_dirty_tables;

      dict_persist_update_log_margin();
  }

  ut_ad(table->in_dirty_dict_tables_list);

  mutex_exit(&dict_persist->mutex);
}

/** Flags an index corrupted in the data dictionary cache only. This
is used to mark a corrupted index when index's own dictionary
is corrupted, and we would force to load such index for repair purpose.
Besides, we have to write a redo log.
We don't want to hold dict_sys->mutex here, so that we can set index as
corrupted in some low-level functions. We would only set the flags from
not corrupted to corrupted when server is running, so it should be safe
to set it directly.
@param[in,out]  index           index, must not be NULL */
void dict_set_corrupted(dict_index_t *index) {
  dict_table_t *table = index->table;

  if (index->type & DICT_CORRUPT) {
    return;
  }

  index->type |= DICT_CORRUPT;

  if (!srv_read_only_mode && !table->is_temporary()) {
    PersistentTableMetadata metadata(table->id, table->version);
    metadata.add_corrupted_index(index_id_t(index->space, index->id));

    Persister *persister = dict_persist->persisters->get(PM_INDEX_CORRUPTED);
    ut_ad(persister != nullptr);

    mtr_t mtr;

    mtr.start();
    persister->write_log(table->id, metadata, &mtr);
    mtr.commit();

    /* Make sure the corruption bit won't be lost */
    log_write_up_to(*log_sys, mtr.commit_lsn(), true);

    DBUG_INJECT_CRASH("log_corruption_crash", 1);

    dict_table_mark_dirty(table);
  }
}

/** Write the dirty persistent dynamic metadata for a table to
DD TABLE BUFFER table. This is the low level function to write back.
@param[in,out]  table   table to write */
static void dict_table_persist_to_dd_table_buffer_low(dict_table_t *table) {
  ut_ad(dict_sys != nullptr);
  ut_ad(mutex_own(&dict_persist->mutex));
  ut_ad(table->dirty_status.load() == METADATA_DIRTY);
  ut_ad(table->in_dirty_dict_tables_list);
  ut_ad(!table->is_temporary());

  DDTableBuffer *table_buffer = dict_persist->table_buffer;
  PersistentTableMetadata metadata(table->id, table->version);
  byte buffer[REC_MAX_DATA_SIZE];
  ulint size;

  /* Here the status gets changed first, to make concurrent
  update to this table to wait on dict_persist_t::mutex.
  See dict_table_autoinc_log(), etc. */
  table->dirty_status.store(METADATA_BUFFERED);

  dict_init_dynamic_metadata(table, &metadata);

  size = dict_persist->persisters->write(metadata, buffer);

  dberr_t error =
      table_buffer->replace(table->id, table->version, buffer, size);
  ut_a(error == DB_SUCCESS);

  ut_ad(dict_persist->num_dirty_tables > 0);
  --dict_persist->num_dirty_tables;
  dict_persist_update_log_margin();
}

/** Write back the dirty persistent dynamic metadata of the table
to DDTableBuffer
@param[in,out]  table   table object */
void dict_table_persist_to_dd_table_buffer(dict_table_t *table) {
  mutex_enter(&dict_persist->mutex);

  if (table->dirty_status.load() != METADATA_DIRTY) {
    /* Double check the status, since a concurrent checkpoint
    may have already changed the status to not dirty */
    mutex_exit(&dict_persist->mutex);
    return;
  }

  ut_ad(table->in_dirty_dict_tables_list);

  dict_table_persist_to_dd_table_buffer_low(table);

  mutex_exit(&dict_persist->mutex);
}

/** Check if any table has any dirty persistent data, if so
write dirty persistent data of table to mysql.innodb_dynamic_metadata
accordingly. */
void dict_persist_to_dd_table_buffer() {
  bool persisted = false;

  if (dict_sys == nullptr) {
    log_set_dict_max_allowed_checkpoint_lsn(*log_sys, 0);
    return;
  }

  mutex_enter(&dict_persist->mutex);

  for (dict_table_t *table : dict_persist->dirty_dict_tables.removable()) {
    ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);
    ut_ad(table->dirty_status.load() == METADATA_DIRTY ||
          table->dirty_status.load() == METADATA_BUFFERED);

    if (table->dirty_status.load() == METADATA_DIRTY) {
      /* We should not attempt to write to data pages while shutting down
      page cleaners. */
      if (srv_shutdown_state.load() >= SRV_SHUTDOWN_FLUSH_PHASE) {
        ut_d(ut_error);
      } else {
        dict_table_persist_to_dd_table_buffer_low(table);
        persisted = true;
      }
    }
  }

  ut_ad(dict_persist->num_dirty_tables == 0);

  /* Get this lsn with dict_persist->mutex held,
  so no other concurrent dynamic metadata change logs
  would be before this lsn. */
  const lsn_t persisted_lsn = log_get_lsn(*log_sys);

  /* As soon as we release the dict_persist->mutex, new dynamic
  metadata changes could happen. They would be not persisted
  until next call to dict_persist_to_dd_table_buffer.
  We must not remove redo which could allow to deduce them.
  Therefore the maximum allowed lsn for checkpoint is the
  current lsn. */
  log_set_dict_max_allowed_checkpoint_lsn(*log_sys, persisted_lsn);

  mutex_exit(&dict_persist->mutex);

  if (persisted) {
    log_write_up_to(*log_sys, persisted_lsn, true);
  }
}

/** Calculate and update the redo log margin for current tables which
have some changed dynamic metadata in memory and have not been written
back to mysql.innodb_dynamic_metadata. Update LSN limit, which is used
to stop user threads when redo log is running out of space and they
do not hold latches (log.free_check_limit_lsn). */
static void dict_persist_update_log_margin() {
  /* Below variables basically considers only the AUTO_INCREMENT counter
  and a small margin for corrupted indexes. */

  /* Every table will generate less than 80 bytes without
  considering page split */
  static constexpr uint32_t log_margin_per_table_no_split = 80;

  /* Every table metadata log may roughly consume such many bytes. */
  static constexpr uint32_t record_size_per_table = 50;

  /* How many tables may generate one page split */
  static const uint32_t tables_per_split =
      (univ_page_size.physical() - PAGE_NEW_SUPREMUM_END) /
      record_size_per_table / 2;

  /* Every page split needs at most this log margin, if not root split. */
  static const uint32_t log_margin_per_split_no_root = 500;

  /* Extra margin for root split, we always leave this margin,
  since we don't know exactly it will split root or not */
  static const uint32_t log_margin_per_split_root =
      univ_page_size.physical() / 2 * 3; /* Add 50% margin. */

  /* Read without holding the dict_persist_t::mutex */
  uint32_t num_dirty_tables = dict_persist->num_dirty_tables;
  uint32_t total_splits = 0;
  uint32_t num_tables = num_dirty_tables;

  while (num_tables > 0) {
    total_splits += num_tables / tables_per_split + 1;
    num_tables = num_tables / tables_per_split;
  }

  const auto margin = (num_dirty_tables * log_margin_per_table_no_split +
                       total_splits * log_margin_per_split_no_root +
                       (num_dirty_tables == 0 ? 0 : log_margin_per_split_root));

  if (log_sys != nullptr) {
    /* Update margin for redo log */
    log_set_dict_persist_margin(*log_sys, margin);
  }
}

#ifdef UNIV_DEBUG
/** Sets merge_threshold for all indexes in the list of tables
@param[in]      list                    pointer to the list of tables
@param[in]      merge_threshold_all     value to set for all indexes */
inline void dict_set_merge_threshold_list_debug(
    dict_sys_t::Table_LRU_list_base *list, uint merge_threshold_all) {
  for (auto table : *list) {
    for (auto index : table->indexes) {
      rw_lock_x_lock(dict_index_get_lock(index), UT_LOCATION_HERE);
      index->merge_threshold = merge_threshold_all;
      rw_lock_x_unlock(dict_index_get_lock(index));
    }
  }
}

/** Sets merge_threshold for all indexes in dictionary cache for debug.
@param[in]      merge_threshold_all     value to set for all indexes */
void dict_set_merge_threshold_all_debug(uint merge_threshold_all) {
  dict_sys_mutex_enter();

  dict_set_merge_threshold_list_debug(&dict_sys->table_LRU,
                                      merge_threshold_all);
  dict_set_merge_threshold_list_debug(&dict_sys->table_non_LRU,
                                      merge_threshold_all);

  dict_sys_mutex_exit();
}
#endif /* UNIV_DEBUG */

/** Inits dict_ind_redundant. */
void dict_ind_init(void) {
  dict_table_t *table;

  /* create dummy table and index for REDUNDANT infimum and supremum */
  table = dict_mem_table_create("SYS_DUMMY1", DICT_HDR_SPACE, 1, 0, 0, 0, 0);
  dict_mem_table_add_col(table, nullptr, nullptr, DATA_CHAR,
                         DATA_ENGLISH | DATA_NOT_NULL, 8, true);

  dict_ind_redundant =
      dict_mem_index_create("SYS_DUMMY1", "SYS_DUMMY1", DICT_HDR_SPACE, 0, 1);
  dict_index_add_col(dict_ind_redundant, table, table->get_col(0), 0, true);
  dict_ind_redundant->table = table;
  /* avoid ut_ad(index->cached) in dict_index_get_n_unique_in_tree */
  dict_ind_redundant->cached = true;
}

/** Frees dict_ind_redundant. */
void dict_ind_free(void) {
  dict_table_t *table;

  table = dict_ind_redundant->table;
  dict_mem_index_free(dict_ind_redundant);
  dict_ind_redundant = nullptr;
  dict_mem_table_free(table);
}

/** Get an index by name.
@param[in]      table           the table where to look for the index
@param[in]      name            the index name to look for
@param[in]      committed       true=search for committed,
false=search for uncommitted
@return index, NULL if does not exist */
dict_index_t *dict_table_get_index_on_name(dict_table_t *table,
                                           const char *name, bool committed) {
  dict_index_t *index;

  index = table->first_index();

  while (index != nullptr) {
    if (index->is_committed() == committed &&
        innobase_strcasecmp(index->name, name) == 0) {
      return (index);
    }

    index = index->next();
  }

  return (nullptr);
}

/** Replace the index passed in with another equivalent index in the
 foreign key lists of the table.
 @return whether all replacements were found */
bool dict_foreign_replace_index(
    dict_table_t *table, /*!< in/out: table */
    const char **col_names,
    /*!< in: column names, or NULL
    to use table->col_names */
    const dict_index_t *index) /*!< in: index to be replaced */
{
  bool found = true;
  dict_foreign_t *foreign;

  ut_ad(index->to_be_dropped);
  ut_ad(index->table == table);

  for (dict_foreign_set::iterator it = table->foreign_set.begin();
       it != table->foreign_set.end(); ++it) {
    foreign = *it;
    if (foreign->foreign_index == index) {
      ut_ad(foreign->foreign_table == index->table);

      dict_index_t *new_index = dict_foreign_find_index(
          foreign->foreign_table, col_names, foreign->foreign_col_names,
          foreign->n_fields, index,
          /*check_charsets=*/true, /*check_null=*/false);
      if (new_index) {
        ut_ad(new_index->table == index->table);
        ut_ad(!new_index->to_be_dropped);
      } else {
        found = false;
      }

      foreign->foreign_index = new_index;
    }
  }

  for (dict_foreign_set::iterator it = table->referenced_set.begin();
       it != table->referenced_set.end(); ++it) {
    foreign = *it;
    if (foreign->referenced_index == index) {
      ut_ad(foreign->referenced_table == index->table);

      dict_index_t *new_index = dict_foreign_find_index(
          foreign->referenced_table, nullptr, foreign->referenced_col_names,
          foreign->n_fields, index,
          /*check_charsets=*/true, /*check_null=*/false);
      /* There must exist an alternative index,
      since this must have been checked earlier. */
      if (new_index) {
        ut_ad(new_index->table == index->table);
        ut_ad(!new_index->to_be_dropped);
      } else {
        found = false;
      }

      foreign->referenced_index = new_index;
    }
  }

  return (found);
}

#ifdef UNIV_DEBUG
/** Check for duplicate index entries in a table [using the index name]
@param[in] table Check for dup indexes in this table
@param[in] check Whether and when to allow temporary index names */
void dict_table_check_for_dup_indexes(const dict_table_t *table,
                                      enum check_name check) {
  /* Check for duplicates, ignoring indexes that are marked
  as to be dropped */

  const dict_index_t *index2;

  ut_ad(dict_sys_mutex_own());

  /* The primary index _must_ exist */
  ut_a(UT_LIST_GET_LEN(table->indexes) > 0);

  for (auto index1 : table->indexes) {
    if (!index1->is_committed()) {
      ut_a(!index1->is_clustered());

      switch (check) {
        case CHECK_ALL_COMPLETE:
          ut_error;
        case CHECK_ABORTED_OK:
          switch (dict_index_get_online_status(index1)) {
            case ONLINE_INDEX_COMPLETE:
            case ONLINE_INDEX_CREATION:
              ut_error;
              break;
            case ONLINE_INDEX_ABORTED:
            case ONLINE_INDEX_ABORTED_DROPPED:
              break;
          }
          [[fallthrough]];
        case CHECK_PARTIAL_OK:
          break;
      }
    }

    for (index2 = UT_LIST_GET_NEXT(indexes, index1); index2 != nullptr;
         index2 = UT_LIST_GET_NEXT(indexes, index2)) {
      ut_ad(index1->is_committed() != index2->is_committed() ||
            strcmp(index1->name, index2->name) != 0);
    }
  }
}
#endif /* UNIV_DEBUG */

/** Converts a database and table name from filesystem encoding (e.g.
"@code d@i1b/a@q1b@1Kc @endcode", same format as used in  dict_table_t::name)
in two strings in UTF8MB3 encoding (e.g. dцb and aюbØc). The output buffers must
be at least MAX_DB_UTF8MB3_LEN and MAX_TABLE_UTF8MB3_LEN bytes.
@param[in]      db_and_table    database and table names,
                                e.g. "@code d@i1b/a@q1b@1Kc @endcode"
@param[out]     db_utf8mb3         database name, e.g. dцb
@param[in]      db_utf8mb3_size    db_utf8mb3 size
@param[out]     table_utf8mb3      table name, e.g. aюbØc
@param[in]      table_utf8mb3_size table_utf8mb3 size */
void dict_fs2utf8(const char *db_and_table, char *db_utf8mb3,
                  size_t db_utf8mb3_size, char *table_utf8mb3,
                  size_t table_utf8mb3_size) {
  char db[MAX_DATABASE_NAME_LEN + 1];
  ulint db_len;
  uint errors;

  db_len = dict_get_db_name_len(db_and_table);

  ut_a(db_len <= sizeof(db));

  memcpy(db, db_and_table, db_len);
  db[db_len] = '\0';

  strconvert(&my_charset_filename, db, system_charset_info, db_utf8mb3,
             db_utf8mb3_size, &errors);

  /* convert each # to @0023 in table name and store the result in buf */
  const char *table = dict_remove_db_name(db_and_table);
  const char *table_p;
  char buf[MAX_TABLE_NAME_LEN * 5 + 1];
  char *buf_p;
  for (table_p = table, buf_p = buf; table_p[0] != '\0'; table_p++) {
    if (table_p[0] != '#') {
      buf_p[0] = table_p[0];
      buf_p++;
    } else {
      buf_p[0] = '@';
      buf_p[1] = '0';
      buf_p[2] = '0';
      buf_p[3] = '2';
      buf_p[4] = '3';
      buf_p += 5;
    }
    ut_a((size_t)(buf_p - buf) < sizeof(buf));
  }
  buf_p[0] = '\0';

  errors = 0;
  strconvert(&my_charset_filename, buf, system_charset_info, table_utf8mb3,
             table_utf8mb3_size, &errors);

  if (errors != 0) {
    snprintf(table_utf8mb3, table_utf8mb3_size, "%s", table);
  }
}

/** Resize the hash tables based on the current buffer pool size. */
void dict_resize() {
  dict_sys_mutex_enter();

  /* all table entries are in table_LRU and table_non_LRU lists */
  ut::delete_(dict_sys->table_hash);
  ut::delete_(dict_sys->table_id_hash);

  dict_sys->table_hash = ut::new_<hash_table_t>(
      buf_pool_get_curr_size() / (DICT_POOL_PER_TABLE_HASH * UNIV_WORD_SIZE));

  dict_sys->table_id_hash = ut::new_<hash_table_t>(
      buf_pool_get_curr_size() / (DICT_POOL_PER_TABLE_HASH * UNIV_WORD_SIZE));

  for (auto table : dict_sys->table_LRU) {
    const auto name_hash_value = ut::hash_string(table->name.m_name);
    const auto index_id_hash_value = ut::hash_uint64(table->id);

    HASH_INSERT(dict_table_t, name_hash, dict_sys->table_hash, name_hash_value,
                table);

    HASH_INSERT(dict_table_t, id_hash, dict_sys->table_id_hash,
                index_id_hash_value, table);
  }

  for (auto table : dict_sys->table_non_LRU) {
    const auto name_hash_value = ut::hash_string(table->name.m_name);
    const auto index_id_hash_value = ut::hash_uint64(table->id);

    HASH_INSERT(dict_table_t, name_hash, dict_sys->table_hash, name_hash_value,
                table);

    HASH_INSERT(dict_table_t, id_hash, dict_sys->table_id_hash,
                index_id_hash_value, table);
  }

  dict_sys_mutex_exit();
}
#else
/* Dummy implementation for satisfying compiler for MEB. This can be removed
once the callers in dict0dd.cc is factored out from MEB compilation. */
/* purecov: begin deadcode */
void dict_set_corrupted(dict_index_t *index [[maybe_unused]]) {}
/* purecov: end */
#endif /* !UNIV_HOTBACKUP */

/** Closes the data dictionary module. */
void dict_close(void) {
  if (dict_sys == nullptr) {
    /* This should only happen if a failure occurred
    during redo log processing. */
    return;
  }

  /* Acquire only because it's a pre-condition. */
  dict_sys_mutex_enter();

  if (dict_sys->table_stats != nullptr) {
    dict_table_close(dict_sys->table_stats, true, false);
  }
  if (dict_sys->index_stats != nullptr) {
    dict_table_close(dict_sys->index_stats, true, false);
  }
  if (dict_sys->dynamic_metadata != nullptr) {
    dict_table_close(dict_sys->dynamic_metadata, true, false);
  }
  if (dict_sys->ddl_log) {
    dict_table_close(dict_sys->ddl_log, true, false);
  }

#ifndef UNIV_HOTBACKUP
  /* Free the hash elements. We don't remove them from the table
  because we are going to destroy the table anyway. */
  for (ulint i = 0; i < hash_get_n_cells(dict_sys->table_id_hash); i++) {
    dict_table_t *table;

    table =
        static_cast<dict_table_t *>(hash_get_first(dict_sys->table_hash, i));

    while (table) {
      dict_table_t *prev_table = table;

      table = static_cast<dict_table_t *>(HASH_GET_NEXT(name_hash, prev_table));
      ut_ad(prev_table->magic_n == DICT_TABLE_MAGIC_N);
      dict_table_remove_from_cache(prev_table);
    }
  }
#endif /* !UNIV_HOTBACKUP */

  ut::delete_(dict_sys->table_hash);

  /* The elements are the same instance as in dict_sys->table_hash,
  therefore we don't delete the individual elements. */
  ut::delete_(dict_sys->table_id_hash);

#ifndef UNIV_HOTBACKUP
  dict_ind_free();
#endif /* !UNIV_HOTBACKUP */

  dict_sys_mutex_exit();
  dict_sys_mutex_free();

  rw_lock_free(dict_operation_lock);

  ut::free(dict_operation_lock);
  dict_operation_lock = nullptr;

  mutex_free(&dict_foreign_err_mutex);

#ifndef UNIV_HOTBACKUP
  if (dict_foreign_err_file != nullptr) {
    fclose(dict_foreign_err_file);
    dict_foreign_err_file = nullptr;
  }
#endif /* !UNIV_HOTBACKUP */

  ut_ad(dict_sys->size == 0);

  ut::free(dict_sys);
  dict_sys = nullptr;
}

#ifndef UNIV_HOTBACKUP
#ifdef UNIV_DEBUG
/** Validate the dictionary table LRU list.
 @return true if valid */
static bool dict_lru_validate() {
  ut_ad(dict_sys_mutex_own());

  for (auto table : dict_sys->table_LRU) {
    ut_a(table->can_be_evicted);
  }

  for (auto table : dict_sys->table_non_LRU) {
    ut_a(!table->can_be_evicted);
  }

  return true;
}

static bool dict_lru_find_table(const dict_table_t *find_table) {
  ut_ad(find_table != nullptr);
  ut_ad(dict_sys_mutex_own());

  for (auto table : dict_sys->table_LRU) {
    ut_a(table->can_be_evicted);

    if (table == find_table) {
      return true;
    }
  }

  return false;
}

static bool dict_non_lru_find_table(const dict_table_t *find_table) {
  ut_ad(find_table != nullptr);
  ut_ad(dict_sys_mutex_own());

  for (auto table : dict_sys->table_non_LRU) {
    ut_a(!table->can_be_evicted);

    if (table == find_table) {
      return true;
    }
  }

  return false;
}
#endif /* UNIV_DEBUG */
/** Check an index to see whether its first fields are the columns in the array,
 in the same order and is not marked for deletion and is not the same
 as types_idx.
 @return true if the index qualifies, otherwise false */
bool dict_foreign_qualify_index(
    const dict_table_t *table, /*!< in: table */
    const char **col_names,
    /*!< in: column names, or NULL
    to use table->col_names */
    const char **columns,      /*!< in: array of column names */
    ulint n_cols,              /*!< in: number of columns */
    const dict_index_t *index, /*!< in: index to check */
    const dict_index_t *types_idx,
    /*!< in: NULL or an index
    whose types the column types
    must match */
    bool check_charsets,
    /*!< in: whether to check
    charsets.  only has an effect
    if types_idx != NULL */
    ulint check_null)
/*!< in: nonzero if none of
the columns must be declared
NOT NULL */
{
  if (dict_index_get_n_fields(index) < n_cols) {
    return (false);
  }

  for (ulint i = 0; i < n_cols; i++) {
    dict_field_t *field;
    const char *col_name;
    ulint col_no;

    field = index->get_field(i);
    col_no = dict_col_get_no(field->col);

    if (field->prefix_len != 0) {
      /* We do not accept column prefix
      indexes here */
      return (false);
    }

    if (check_null && (field->col->prtype & DATA_NOT_NULL)) {
      return (false);
    }

    col_name = col_names ? col_names[col_no]
                         : (field->col->is_virtual()
                                ? dict_table_get_v_col_name_mysql(table, col_no)
                                : table->get_col_name(col_no));

    if (0 != innobase_strcasecmp(columns[i], col_name)) {
      return (false);
    }

    if (types_idx &&
        !cmp_cols_are_equal(index->get_col(i), types_idx->get_col(i),
                            check_charsets)) {
      return (false);
    }
  }

  return (true);
}

/** Update the state of compression failure padding heuristics. This is
 called whenever a compression operation succeeds or fails.
 The caller must be holding info->mutex */
static void dict_index_zip_pad_update(
    zip_pad_info_t *info, /*!< in/out: info to be updated */
    ulint zip_threshold)  /*!< in: zip threshold value */
{
  ulint total;
  ulint fail_pct;

  ut_ad(info);

  total = info->success + info->failure;

  ut_ad(total > 0);

  if (zip_threshold == 0) {
    /* User has just disabled the padding. */
    return;
  }

  if (total < ZIP_PAD_ROUND_LEN) {
    /* We are in middle of a round. Do nothing. */
    return;
  }

  /* We are at a 'round' boundary. Reset the values but first
  calculate fail rate for our heuristic. */
  fail_pct = (info->failure * 100) / total;
  info->failure = 0;
  info->success = 0;

  if (fail_pct > zip_threshold) {
    /* Compression failures are more then user defined
    threshold. Increase the pad size to reduce chances of
    compression failures. */
    ut_ad(info->pad % ZIP_PAD_INCR == 0);

    /* Only do increment if it won't increase padding
    beyond max pad size. */
    if (info->pad + ZIP_PAD_INCR < (UNIV_PAGE_SIZE * zip_pad_max) / 100) {
      /* Use atomics even though we have the mutex.
      This is to ensure that we are able to read
      info->pad atomically. */
      info->pad.fetch_add(ZIP_PAD_INCR);

      MONITOR_INC(MONITOR_PAD_INCREMENTS);
    }

    info->n_rounds = 0;

  } else {
    /* Failure rate was OK. Another successful round
    completed. */
    ++info->n_rounds;

    /* If enough successful rounds are completed with
    compression failure rate in control, decrease the
    padding. */
    if (info->n_rounds >= ZIP_PAD_SUCCESSFUL_ROUND_LIMIT && info->pad > 0) {
      ut_ad(info->pad % ZIP_PAD_INCR == 0);
      /* Use atomics even though we have the mutex.
      This is to ensure that we are able to read
      info->pad atomically. */
      info->pad.fetch_sub(ZIP_PAD_INCR);

      info->n_rounds = 0;

      MONITOR_INC(MONITOR_PAD_DECREMENTS);
    }
  }
}

/** This function should be called whenever a page is successfully
 compressed. Updates the compression padding information. */
void dict_index_zip_success(
    dict_index_t *index) /*!< in/out: index to be updated. */
{
  ut_ad(index);

  ulint zip_threshold = zip_failure_threshold_pct;
  if (!zip_threshold) {
    /* Disabled by user. */
    return;
  }

  dict_index_zip_pad_lock(index);
  ++index->zip_pad.success;
  dict_index_zip_pad_update(&index->zip_pad, zip_threshold);
  dict_index_zip_pad_unlock(index);
}

/** This function should be called whenever a page compression attempt
 fails. Updates the compression padding information. */
void dict_index_zip_failure(
    dict_index_t *index) /*!< in/out: index to be updated. */
{
  ut_ad(index);

  ulint zip_threshold = zip_failure_threshold_pct;
  if (!zip_threshold) {
    /* Disabled by user. */
    return;
  }

  dict_index_zip_pad_lock(index);
  ++index->zip_pad.failure;
  dict_index_zip_pad_update(&index->zip_pad, zip_threshold);
  dict_index_zip_pad_unlock(index);
}

/** Return the optimal page size, for which page will likely compress.
 @return page size beyond which page might not compress */
ulint dict_index_zip_pad_optimal_page_size(
    dict_index_t *index) /*!< in: index for which page size
                         is requested */
{
  ulint pad;
  ulint min_sz;
  ulint sz;

  ut_ad(index);

  if (!zip_failure_threshold_pct) {
    /* Disabled by user. */
    return (UNIV_PAGE_SIZE);
  }

  /* We use atomics to read index->zip_pad.pad. Here we use zero
  as increment as are not changing the value of the 'pad'. */

  pad = index->zip_pad.pad.load();

  ut_ad(pad < UNIV_PAGE_SIZE);
  sz = UNIV_PAGE_SIZE - pad;

  /* Min size allowed by user. */
  ut_ad(zip_pad_max < 100);
  min_sz = (UNIV_PAGE_SIZE * (100 - zip_pad_max)) / 100;

  return (std::max(sz, min_sz));
}

/** Convert a 32 bit integer table flags to the 32 bit FSP Flags.
Fsp Flags are written into the tablespace header at the offset
FSP_SPACE_FLAGS and are also stored in the fil_space_t::flags field.
The following chart shows the translation of the low order bit.
Other bits are the same.
========================= Low order bit ==========================
                    | REDUNDANT | COMPACT | COMPRESSED | DYNAMIC
dict_table_t::flags |     0     |    1    |     1      |    1
fil_space_t::flags  |     0     |    0    |     1      |    1
==================================================================
@param[in]      table_flags     dict_table_t::flags
@return tablespace flags (fil_space_t::flags) */
uint32_t dict_tf_to_fsp_flags(uint32_t table_flags) {
  DBUG_EXECUTE_IF("dict_tf_to_fsp_flags_failure", return (UINT32_UNDEFINED););

  bool has_atomic_blobs = DICT_TF_HAS_ATOMIC_BLOBS(table_flags);
  page_size_t page_size = dict_tf_get_page_size(table_flags);
  bool has_data_dir = DICT_TF_HAS_DATA_DIR(table_flags);
  bool is_shared = DICT_TF_HAS_SHARED_SPACE(table_flags);

  ut_ad(!page_size.is_compressed() || has_atomic_blobs);

  /* General tablespaces that are not compressed do not get the
  flags for dynamic row format (ATOMIC_BLOBS) */
  if (is_shared && !page_size.is_compressed()) {
    has_atomic_blobs = false;
  }

  uint32_t fsp_flags = fsp_flags_init(page_size, has_atomic_blobs, has_data_dir,
                                      is_shared, false);

  return (fsp_flags);
}

/** Convert table flag to row format string.
 @return row format name. */
const char *dict_tf_to_row_format_string(
    uint32_t table_flag) /*!< in: row format setting */
{
  switch (dict_tf_get_rec_format(table_flag)) {
    case REC_FORMAT_REDUNDANT:
      return ("ROW_TYPE_REDUNDANT");
    case REC_FORMAT_COMPACT:
      return ("ROW_TYPE_COMPACT");
    case REC_FORMAT_COMPRESSED:
      return ("ROW_TYPE_COMPRESSED");
    case REC_FORMAT_DYNAMIC:
      return ("ROW_TYPE_DYNAMIC");
  }

  ut_error;
}

/** Determine the extent size (in pages) for the given table
@param[in]      table   the table whose extent size is being
                        calculated.
@return extent size in pages (256, 128 or 64) */
page_no_t dict_table_extent_size(const dict_table_t *table) {
  const ulint mb_1 = 1024 * 1024;
  const ulint mb_2 = 2 * mb_1;
  const ulint mb_4 = 4 * mb_1;

  page_size_t page_size = dict_table_page_size(table);
  page_no_t pages_in_extent = FSP_EXTENT_SIZE;

  if (page_size.is_compressed()) {
    ulint disk_page_size = page_size.physical();

    switch (disk_page_size) {
      case 1024:
        pages_in_extent = mb_1 / 1024;
        break;
      case 2048:
        pages_in_extent = mb_1 / 2048;
        break;
      case 4096:
        pages_in_extent = mb_1 / 4096;
        break;
      case 8192:
        pages_in_extent = mb_1 / 8192;
        break;
      case 16384:
        pages_in_extent = mb_1 / 16384;
        break;
      case 32768:
        pages_in_extent = mb_2 / 32768;
        break;
      case 65536:
        pages_in_extent = mb_4 / 65536;
        break;
      default:
        ut_d(ut_error);
    }
  }

  return (pages_in_extent);
}

/** Default constructor */
DDTableBuffer::DDTableBuffer() {
  init();

  /* Check if we need to recover it, in case of crash */
  btr_truncate_recover(m_index);
}

/** Destructor */
DDTableBuffer::~DDTableBuffer() { close(); }

/* Create the search and replace tuples */
void DDTableBuffer::create_tuples() {
  const dict_col_t *col;
  dfield_t *dfield;
  byte *sys_buf;
  byte *id_buf;

  id_buf = static_cast<byte *>(mem_heap_alloc(m_heap, 8));
  memset(id_buf, 0, sizeof *id_buf);

  m_search_tuple = dtuple_create(m_heap, 1);
  dict_index_copy_types(m_search_tuple, m_index, 1);

  dfield = dtuple_get_nth_field(m_search_tuple, 0);
  dfield_set_data(dfield, id_buf, 8);

  /* Allocate another memory for this tuple */
  id_buf = static_cast<byte *>(mem_heap_alloc(m_heap, 8));
  memset(id_buf, 0, sizeof *id_buf);

  m_replace_tuple = dtuple_create(m_heap, N_COLS);
  dict_table_copy_types(m_replace_tuple, m_index->table);

  dfield = dtuple_get_nth_field(m_replace_tuple, TABLE_ID_FIELD_NO);
  dfield_set_data(dfield, id_buf, 8);

  /* Initialize system fields, we always write fake value. */
  sys_buf = static_cast<byte *>(mem_heap_alloc(m_heap, 8));
  memset(sys_buf, 0xFF, 8);

  col = m_index->table->get_sys_col(DATA_ROW_ID);
  dfield = dtuple_get_nth_field(m_replace_tuple, dict_col_get_no(col));
  dfield_set_data(dfield, sys_buf, DATA_ROW_ID_LEN);

  col = m_index->table->get_sys_col(DATA_TRX_ID);
  dfield = dtuple_get_nth_field(m_replace_tuple, dict_col_get_no(col));
  dfield_set_data(dfield, sys_buf, DATA_TRX_ID_LEN);

  col = m_index->table->get_sys_col(DATA_ROLL_PTR);
  dfield = dtuple_get_nth_field(m_replace_tuple, dict_col_get_no(col));
  dfield_set_data(dfield, sys_buf, DATA_ROLL_PTR_LEN);
}

/** Initialize the in-memory index */
void DDTableBuffer::init() {
  if (dict_sys->dynamic_metadata != nullptr) {
    ut_ad(dict_table_is_comp(dict_sys->dynamic_metadata));
    m_index = dict_sys->dynamic_metadata->first_index();
  } else {
    open();
    dict_sys->dynamic_metadata = m_index->table;
  }

  ut_ad(m_index->next() == nullptr);
  ut_ad(m_index->n_uniq == 1);
  ut_ad(N_FIELDS == m_index->n_fields);
  ut_ad(m_index->table->n_cols == N_COLS);

  /* We don't need AHI for this table */
  m_index->disable_ahi = true;
  m_index->cached = true;

  m_heap = mem_heap_create(500, UT_LOCATION_HERE);
  m_dynamic_heap = mem_heap_create(100, UT_LOCATION_HERE);
  m_replace_heap = mem_heap_create(100, UT_LOCATION_HERE);

  create_tuples();
}

/** Open the mysql.innodb_dynamic_metadata when DD is not fully up */
void DDTableBuffer::open() {
  ut_ad(dict_sys->dynamic_metadata == nullptr);

  dict_table_t *table = nullptr;
  /* Keep it the same with definition of mysql/innodb_dynamic_metadata */
  const char *table_name = "mysql/innodb_dynamic_metadata";
  const char *table_id_name = "table_id";
  const char *version_name = "version";
  const char *metadata_name = "metadata";
  ulint prtype = 0;
  mem_heap_t *heap = mem_heap_create(256, UT_LOCATION_HERE);

  /* Get the root page number according to index id, this is
  same with what we do in ha_innobsae::get_se_private_data() */
  page_no_t root = 4;
  space_index_t index_id = 0;
  while (true) {
    if (fsp_is_inode_page(root)) {
      ++root;
      ut_ad(!fsp_is_inode_page(root));
    }

    if (++index_id == dict_sys_t::s_dynamic_meta_index_id) {
      break;
    }

    ++root;
  }

  table = dict_mem_table_create(table_name, dict_sys_t::s_dict_space_id,
                                N_USER_COLS, 0, 0, 0, 0);

  table->id = dict_sys_t::s_dynamic_meta_table_id;
  table->is_dd_table = true;
  table->dd_space_id = dict_sys_t::s_dd_dict_space_id;
  table->flags |= DICT_TF_COMPACT | (1 << DICT_TF_POS_SHARED_SPACE) |
                  (1 << DICT_TF_POS_ATOMIC_BLOBS);

  prtype = dtype_form_prtype(
      MYSQL_TYPE_LONGLONG | DATA_NOT_NULL | DATA_UNSIGNED | DATA_BINARY_TYPE,
      0);

  dict_mem_table_add_col(table, heap, table_id_name, DATA_INT, prtype, 8, true);
  dict_mem_table_add_col(table, heap, version_name, DATA_INT, prtype, 8, true);

  prtype =
      dtype_form_prtype(MYSQL_TYPE_BLOB | DATA_NOT_NULL | DATA_BINARY_TYPE, 63);

  dict_mem_table_add_col(table, heap, metadata_name, DATA_BLOB, prtype, 10,
                         true);

  dict_table_add_system_columns(table, heap);

  m_index =
      dict_mem_index_create(table_name, "PRIMARY", dict_sys_t::s_dict_space_id,
                            DICT_CLUSTERED | DICT_UNIQUE, 1);

  dict_index_add_col(m_index, table, &table->cols[0], 0, true);

  m_index->id = dict_sys_t::s_dynamic_meta_index_id;
  m_index->n_uniq = 1;

  dberr_t err = dict_index_add_to_cache(table, m_index, root, false);
  if (err != DB_SUCCESS) {
    ut_d(ut_error);
  }

  m_index = table->first_index();

  dict_sys_mutex_enter();

  dict_table_add_to_cache(table, true);

  table->acquire();

  dict_sys_mutex_exit();

  mem_heap_free(heap);
}

/** Initialize the id field of tuple
@param[out]     tuple   the tuple to be initialized
@param[in]      id      table id */
void DDTableBuffer::init_tuple_with_id(dtuple_t *tuple, table_id_t id) {
  dfield_t *dfield = dtuple_get_nth_field(tuple, TABLE_ID_FIELD_NO);
  void *data = dfield->data;

  mach_write_to_8(data, id);
  dfield_set_data(dfield, data, 8);
}

/** Free the things initialized in init() */
void DDTableBuffer::close() {
  mem_heap_free(m_heap);
  mem_heap_free(m_dynamic_heap);
  mem_heap_free(m_replace_heap);

  m_search_tuple = nullptr;
  m_replace_tuple = nullptr;
}

/** Prepare for a update on METADATA field
@param[in]      entry   clustered index entry to replace rec
@param[in]      rec     clustered index record
@return update vector of differing fields without system columns,
or NULL if there isn't any different field */
upd_t *DDTableBuffer::update_set_metadata(const dtuple_t *entry,
                                          const rec_t *rec) {
  ulint offsets[N_FIELDS + 1 + REC_OFFS_HEADER_SIZE];
  upd_field_t *upd_field;
  const dfield_t *version_field;
  const dfield_t *metadata_dfield;
  const byte *metadata;
  const byte *version;
  ulint len;
  upd_t *update;

  rec_offs_init(offsets);
  rec_offs_set_n_fields(offsets, N_FIELDS);
  rec_init_offsets_comp_ordinary(rec, false, m_index, offsets);
  ut_ad(!rec_get_deleted_flag(rec, 1));

  version = rec_get_nth_field(nullptr, rec, offsets, VERSION_FIELD_NO, &len);
  ut_ad(len == 8);
  version_field = dtuple_get_nth_field(entry, VERSION_FIELD_NO);

  metadata = rec_get_nth_field(nullptr, rec, offsets, METADATA_FIELD_NO, &len);
  metadata_dfield = dtuple_get_nth_field(entry, METADATA_FIELD_NO);

  if (dfield_data_is_binary_equal(version_field, 8, version) &&
      dfield_data_is_binary_equal(metadata_dfield, len, metadata)) {
    return (nullptr);
  }

  update = upd_create(2, m_replace_heap);

  upd_field = upd_get_nth_field(update, 0);
  dfield_copy(&upd_field->new_val, version_field);
  upd_field_set_field_no(upd_field, VERSION_FIELD_NO, m_index);

  upd_field = upd_get_nth_field(update, 1);
  dfield_copy(&upd_field->new_val, metadata_dfield);
  upd_field_set_field_no(upd_field, METADATA_FIELD_NO, m_index);

  ut_ad(update->validate());

  return (update);
}

/** Replace the dynamic metadata for a specific table
@param[in]      id              table id
@param[in]      version         table dynamic metadata version
@param[in]      metadata        the metadata we want to replace
@param[in]      len             the metadata length
@return DB_SUCCESS or error code */
dberr_t DDTableBuffer::replace(table_id_t id, uint64_t version,
                               const byte *metadata, size_t len) {
  dtuple_t *entry;
  dfield_t *dfield;
  btr_pcur_t pcur;
  mtr_t mtr;
  byte ver[8];
  dberr_t error;

  ut_ad(mutex_own(&dict_persist->mutex));

  init_tuple_with_id(m_search_tuple, id);

  init_tuple_with_id(m_replace_tuple, id);
  mach_write_to_8(ver, version);
  dfield = dtuple_get_nth_field(m_replace_tuple, VERSION_COL_NO);
  dfield_set_data(dfield, ver, sizeof ver);
  dfield = dtuple_get_nth_field(m_replace_tuple, METADATA_COL_NO);
  dfield_set_data(dfield, metadata, len);
  /* Other system fields have been initialized */

  entry =
      row_build_index_entry(m_replace_tuple, nullptr, m_index, m_replace_heap);

  /* Start to search for the to-be-replaced tuple */
  mtr.start();

  pcur.open(m_index, 0, m_search_tuple, PAGE_CUR_LE, BTR_MODIFY_TREE, &mtr,
            UT_LOCATION_HERE);

  if (page_rec_is_infimum(pcur.get_rec()) ||
      pcur.get_low_match() < m_index->n_uniq) {
    /* The record was not found, so it's the first time we
    add the row for this table of id, we need to insert it */
    static const ulint flags = (BTR_CREATE_FLAG | BTR_NO_LOCKING_FLAG |
                                BTR_NO_UNDO_LOG_FLAG | BTR_KEEP_SYS_FLAG);

    mtr.commit();

    error =
        row_ins_clust_index_entry_low(flags, BTR_MODIFY_TREE, m_index,
                                      m_index->n_uniq, entry, nullptr, false);
    ut_a(error == DB_SUCCESS);

    mem_heap_empty(m_dynamic_heap);
    mem_heap_empty(m_replace_heap);

    return (DB_SUCCESS);
  }

  ut_ad(!rec_get_deleted_flag(pcur.get_rec(), true));

  /* Prepare to update the record. */
  upd_t *update = update_set_metadata(entry, pcur.get_rec());

  if (update != nullptr) {
    ulint *cur_offsets = nullptr;
    big_rec_t *big_rec;
    static const ulint flags =
        (BTR_CREATE_FLAG | BTR_NO_LOCKING_FLAG | BTR_NO_UNDO_LOG_FLAG |
         BTR_KEEP_POS_FLAG | BTR_KEEP_SYS_FLAG);

    error = btr_cur_pessimistic_update(
        flags, pcur.get_btr_cur(), &cur_offsets, &m_dynamic_heap,
        m_replace_heap, &big_rec, update, 0, nullptr, 0, 0, &mtr);
    ut_a(error == DB_SUCCESS);
    /* We don't have big rec in this table */
    ut_ad(!big_rec);
  }

  mtr.commit();
  mem_heap_empty(m_dynamic_heap);
  mem_heap_empty(m_replace_heap);

  return (DB_SUCCESS);
}

/** Remove the whole row for a specific table
@param[in]      id      table id
@return DB_SUCCESS or error code */
dberr_t DDTableBuffer::remove(table_id_t id) {
  btr_pcur_t pcur;
  mtr_t mtr;
  dberr_t error;

  ut_ad(mutex_own(&dict_persist->mutex));

  init_tuple_with_id(m_search_tuple, id);

  mtr.start();

  pcur.open(m_index, 0, m_search_tuple, PAGE_CUR_LE,
            BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE, &mtr, UT_LOCATION_HERE);

  if (!page_rec_is_infimum(pcur.get_rec()) &&
      pcur.get_low_match() == m_index->n_uniq) {
    DEBUG_SYNC_C("delete_metadata_before");

    btr_cur_pessimistic_delete(&error, false, pcur.get_btr_cur(),
                               BTR_CREATE_FLAG, false, 0, 0, 0, &mtr, nullptr,
                               nullptr);
    ut_ad(error == DB_SUCCESS);
  }

  mtr.commit();

  return (DB_SUCCESS);
}

/** Truncate the table. We can call it after all the dynamic metadata
has been written back to DD table */
void DDTableBuffer::truncate() {
  ut_ad(mutex_own(&dict_persist->mutex));

  btr_truncate(m_index);
}

/** Get the buffered metadata for a specific table, the caller
has to delete the returned std::string object by ut::delete_
@param[in]      id      table id
@param[out]     version table dynamic metadata version
@return the metadata saved in a string object, if nothing, the
string would be of length 0 */
std::string *DDTableBuffer::get(table_id_t id, uint64_t *version) {
  btr_cur_t cursor;
  mtr_t mtr;
  ulint len;
  const byte *field = nullptr;

  ut_ad(mutex_own(&dict_persist->mutex));

  init_tuple_with_id(m_search_tuple, id);

  mtr.start();

  btr_cur_search_to_nth_level(m_index, 0, m_search_tuple, PAGE_CUR_LE,
                              BTR_SEARCH_LEAF, &cursor, 0, __FILE__, __LINE__,
                              &mtr);

  if (cursor.low_match == dtuple_get_n_fields(m_search_tuple)) {
    ulint offsets[N_FIELDS + 1 + REC_OFFS_HEADER_SIZE];
    rec_offs_init(offsets);
    rec_offs_set_n_fields(offsets, N_FIELDS);
    rec_t *rec = btr_cur_get_rec(&cursor);
    rec_init_offsets_comp_ordinary(rec, false, m_index, offsets);
    ut_ad(!rec_get_deleted_flag(rec, true));

    const byte *rec_version =
        rec_get_nth_field(nullptr, rec, offsets, VERSION_FIELD_NO, &len);
    ut_ad(len == 8);
    *version = mach_read_from_8(rec_version);

    field = rec_get_nth_field(nullptr, rec, offsets, METADATA_FIELD_NO, &len);

    ut_ad(len != UNIV_SQL_NULL);
  } else {
    len = 0;
    *version = 0;
  }

  std::string *metadata = ut::new_withkey<std::string>(
      UT_NEW_THIS_FILE_PSI_KEY, reinterpret_cast<const char *>(field), len);

  mtr.commit();

  return (metadata);
}

/** Write MLOG_TABLE_DYNAMIC_META for persistent dynamic metadata of table
@param[in]      id              Table id
@param[in]      metadata        Metadata used to write the log
@param[in,out]  mtr             Mini-transaction */
void Persister::write_log(table_id_t id,
                          const PersistentTableMetadata &metadata,
                          mtr_t *mtr) const {
  byte *log_ptr;
  ulint size = get_write_size(metadata);
  /* Both table id and version would be written in a compressed format,
  each of which would cost 1..11 bytes, and MLOG_TABLE_DYNAMIC_META costs
  1 byte. Refer to mlog_write_initial_dict_log_record() as well */
  static constexpr uint8_t metadata_log_header_size = 23;

  ut_ad(size > 0);

  if (!mlog_open_metadata(mtr, metadata_log_header_size + size, log_ptr)) {
    /* Currently possible only when global redo logging is not enabled. */
    ut_ad(!mtr_t::s_logging.is_enabled());
    return;
  }

  log_ptr = mlog_write_initial_dict_log_record(
      MLOG_TABLE_DYNAMIC_META, id, metadata.get_version(), log_ptr, mtr);

  ulint consumed = write(metadata, log_ptr, size);
  log_ptr += consumed;

  mlog_close(mtr, log_ptr);
}
#endif /* !UNIV_HOTBACKUP */

/** Write the corrupted indexes of a table, we can pre-calculate the size
by calling get_write_size()
@param[in]      metadata        persistent data
@param[out]     buffer          write buffer
@param[in]      size            size of write buffer, should be at least
                                get_write_size()
@return the length of bytes written */
ulint CorruptedIndexPersister::write(const PersistentTableMetadata &metadata,
                                     byte *buffer, ulint size) const {
  ulint length = 0;
  corrupted_ids_t corrupted_ids = metadata.get_corrupted_indexes();
  ulint num = corrupted_ids.size();

  ut_ad(num < MAX_INDEXES);

  if (corrupted_ids.empty()) {
    return (0);
  }

  /* Write the PM_INDEX_CORRUPTED mark first */
  mach_write_to_1(buffer, static_cast<byte>(PM_INDEX_CORRUPTED));
  ++length;
  ++buffer;

  mach_write_to_1(buffer, num);
  DBUG_EXECUTE_IF("log_corruption_1", mach_write_to_1(buffer, 100););
  ++length;
  ++buffer;

  for (ulint i = 0; i < num; ++i) {
    mach_write_to_4(buffer, corrupted_ids[i].m_space_id);
    mach_write_to_8(buffer + 4, corrupted_ids[i].m_index_id);
    length += INDEX_ID_LENGTH;
    buffer += INDEX_ID_LENGTH;
    ut_ad(length <= size);
  }

  return (length);
}

/** Pre-calculate the size of metadata to be written
@param[in]      metadata        metadata to be written
@return the size of metadata */
ulint CorruptedIndexPersister::get_write_size(
    const PersistentTableMetadata &metadata) const {
  ulint length = 0;
  corrupted_ids_t corrupted_ids = metadata.get_corrupted_indexes();

  ut_ad(corrupted_ids.size() < MAX_INDEXES);

  if (corrupted_ids.empty()) {
    return (0);
  }

  /* PM_INDEX_CORRUPTED mark and number of corrupted indexes' ids */
  length += 1 + 1;
  length += corrupted_ids.size() * INDEX_ID_LENGTH;

  return (length);
}

/** Read the corrupted indexes from buffer, and store them to
metadata object
@param[out]     metadata        metadata where we store the read data
@param[in]      buffer          buffer to read
@param[in]      size            size of buffer
@param[out]     corrupt         true if we found something wrong in
                                the buffer except incomplete buffer,
                                otherwise false
@return the bytes we read from the buffer if the buffer data
is complete and we get everything, 0 if the buffer is incompleted */
ulint CorruptedIndexPersister::read(PersistentTableMetadata &metadata,
                                    const byte *buffer, ulint size,
                                    bool *corrupt) const {
  const byte *end = buffer + size;
  ulint consumed = 0;
  byte type;
  ulint num;

  *corrupt = false;

  /* It should contain PM_INDEX_CORRUPTED and number at least */
  if (size <= 2) {
    return (0);
  }

  type = *buffer++;
  ++consumed;

  if (type != PM_INDEX_CORRUPTED) {
    *corrupt = true;
    return (consumed);
  }

  num = mach_read_from_1(buffer);
  ++consumed;
  ++buffer;

  if (num == 0 || num > MAX_INDEXES) {
    *corrupt = true;
    return (consumed);
  }

  if (buffer + num * INDEX_ID_LENGTH > end) {
    return (0);
  }

  for (ulint i = 0; i < num; ++i) {
    space_id_t space_id = mach_read_from_4(buffer);
    space_index_t index_id = mach_read_from_8(buffer + 4);
    metadata.add_corrupted_index(index_id_t(space_id, index_id));

    buffer += INDEX_ID_LENGTH;
    consumed += INDEX_ID_LENGTH;
  }

  return (consumed);
}

/** Write the autoinc counter of a table, we can pre-calculate
the size by calling get_write_size()
@param[in]      metadata        persistent metadata
@param[out]     buffer          write buffer
@param[in]      size            size of write buffer, should be
                                at least get_write_size()
@return the length of bytes written */
ulint AutoIncPersister::write(const PersistentTableMetadata &metadata,
                              byte *buffer, ulint size) const {
  ulint length = 0;
  uint64_t autoinc = metadata.get_autoinc();

  mach_write_to_1(buffer, static_cast<byte>(PM_TABLE_AUTO_INC));
  ++length;
  ++buffer;

  ulint len = mach_u64_write_much_compressed(buffer, autoinc);
  length += len;
  buffer += len;

  ut_ad(length <= size);
  return (length);
}

/** Read the autoinc counter from buffer, and store them to
metadata object
@param[out]     metadata        metadata where we store the read data
@param[in]      buffer          buffer to read
@param[in]      size            size of buffer
@param[out]     corrupt         true if we found something wrong in
                                the buffer except incomplete buffer,
                                otherwise false
@return the bytes we read from the buffer if the buffer data
is complete and we get everything, 0 if the buffer is incomplete */
ulint AutoIncPersister::read(PersistentTableMetadata &metadata,
                             const byte *buffer, ulint size,
                             bool *corrupt) const {
  const byte *end = buffer + size;
  ulint consumed = 0;
  byte type;
  uint64_t autoinc;

  *corrupt = false;

  /* It should contain PM_TABLE_AUTO_INC and the counter at least */
  if (size < 2) {
    return (0);
  }

  type = *buffer++;
  ++consumed;

  if (type != PM_TABLE_AUTO_INC) {
    *corrupt = true;
    return (consumed);
  }

  const byte *start = buffer;
  autoinc = mach_parse_u64_much_compressed(&start, end);

  if (start == nullptr) {
    /* Just incomplete data, not corrupted */
    return (0);
  }

  if (autoinc == 0) {
    metadata.set_autoinc(autoinc);
  } else {
    metadata.set_autoinc_if_bigger(autoinc);
  }

  consumed += start - buffer;
  ut_ad(consumed <= size);
  return (consumed);
}

/** Destructor */
Persisters::~Persisters() {
  persisters_t::iterator iter;
  for (iter = m_persisters.begin(); iter != m_persisters.end(); ++iter) {
    ut::delete_(iter->second);
  }
}

/** Get the persister object with specified type
@param[in]      type    persister type
@return Persister object required or NULL if not found */
Persister *Persisters::get(persistent_type_t type) const {
  ut_ad(type > PM_SMALLEST_TYPE);
  ut_ad(type < PM_BIGGEST_TYPE);

  persisters_t::const_iterator iter = m_persisters.find(type);

  return (iter == m_persisters.end() ? NULL : iter->second);
}

/** Add a specified persister of type, we will allocate the Persister
if there is no such persister exist, otherwise do nothing and return
the existing one
@param[in]      type    persister type
@return the persister of type */
Persister *Persisters::add(persistent_type_t type) {
  ut_ad(type > PM_SMALLEST_TYPE);
  ut_ad(type < PM_BIGGEST_TYPE);

  Persister *persister = get(type);

  if (persister != nullptr) {
    return (persister);
  }

  switch (type) {
    case PM_INDEX_CORRUPTED:
      persister =
          ut::new_withkey<CorruptedIndexPersister>(UT_NEW_THIS_FILE_PSI_KEY);
      break;
    case PM_TABLE_AUTO_INC:
      persister = ut::new_withkey<AutoIncPersister>(UT_NEW_THIS_FILE_PSI_KEY);
      break;
    default:
      ut_d(ut_error);
      ut_o(break);
  }

  m_persisters.insert(std::make_pair(type, persister));

  return (persister);
}

/** Remove a specified persister of type, we will free the Persister
@param[in]      type    persister type */
void Persisters::remove(persistent_type_t type) {
  persisters_t::iterator iter = m_persisters.find(type);

  if (iter != m_persisters.end()) {
    ut::delete_(iter->second);
    m_persisters.erase(iter);
  }
}

#ifndef UNIV_HOTBACKUP
/** Serialize the metadata to a buffer
@param[in]      metadata        metadata to serialize
@param[out]     buffer          buffer to store the serialized metadata
@return the length of serialized metadata */
size_t Persisters::write(PersistentTableMetadata &metadata, byte *buffer) {
  size_t size = 0;
  byte *pos = buffer;
  persistent_type_t type;

  for (type = static_cast<persistent_type_t>(PM_SMALLEST_TYPE + 1);
       type < PM_BIGGEST_TYPE;
       type = static_cast<persistent_type_t>(type + 1)) {
    ut_ad(size <= REC_MAX_DATA_SIZE);

    Persister *persister = get(type);
    ulint consumed = persister->write(metadata, pos, REC_MAX_DATA_SIZE - size);

    pos += consumed;
    size += consumed;
  }

  return (size);
}

void dict_sdi_close_table(dict_table_t *table) {
  ut_ad(dict_table_is_sdi(table->id));
  dict_table_close(table, true, false);
}

/** Retrieve in-memory index for SDI table.
@param[in]      tablespace_id   innodb tablespace ID
@return dict_index_t structure or NULL*/
dict_index_t *dict_sdi_get_index(space_id_t tablespace_id) {
  dict_table_t *table = dd_table_open_on_id(
      dict_sdi_get_table_id(tablespace_id), nullptr, nullptr, true, true);

  if (table != nullptr) {
    dict_sdi_close_table(table);
    return (table->first_index());
  }
  return (nullptr);
}

/** Retrieve in-memory table object for SDI table.
@param[in]      tablespace_id   innodb tablespace ID
@param[in]      dict_locked     true if dict_sys mutex is acquired
@param[in]      is_create       true when creating SDI Index
@return dict_table_t structure */
dict_table_t *dict_sdi_get_table(space_id_t tablespace_id, bool dict_locked,
                                 bool is_create) {
  if (is_create) {
    if (!dict_locked) {
      dict_sys_mutex_enter();
    }

    dict_sdi_create_idx_in_mem(tablespace_id, false, 0, true);

    if (!dict_locked) {
      dict_sys_mutex_exit();
    }
  }
  dict_table_t *table =
      dd_table_open_on_id(dict_sdi_get_table_id(tablespace_id), nullptr,
                          nullptr, dict_locked, true);

  return (table);
}

/** Remove the SDI table from table cache.
@param[in]      space_id        InnoDB tablespace ID
@param[in]      sdi_table       SDI table
@param[in]      dict_locked     true if dict_sys mutex acquired */
void dict_sdi_remove_from_cache(space_id_t space_id, dict_table_t *sdi_table,
                                bool dict_locked) {
  if (sdi_table == nullptr) {
    /* Remove SDI table from table cache */
    /* We already have MDL protection on tablespace as well
    as MDL on SDI table */
    sdi_table = dd_table_open_on_id_in_mem(dict_sdi_get_table_id(space_id),
                                           dict_locked);
    if (sdi_table) {
      dd_table_close(sdi_table, nullptr, nullptr, dict_locked);
    }
  } else {
    dd_table_close(sdi_table, nullptr, nullptr, dict_locked);
  }

  if (sdi_table) {
    if (!dict_locked) {
      dict_sys_mutex_enter();
    }

    dict_table_remove_from_cache(sdi_table);

    if (!dict_locked) {
      dict_sys_mutex_exit();
    }
  }
}

/** Change the table_id of SYS_* tables if they have been created after
an earlier upgrade. This will update the table_id by adding DICT_MAX_DD_TABLES
*/
void dict_table_change_id_sys_tables() {
  ut_ad(dict_sys_mutex_own());

  /* On upgrading from 5.6 to 5.7, new system table SYS_VIRTUAL is given table
   id after the last created user table. So, if last user table was created
   with table_id as 1027, SYS_VIRTUAL would get id 1028. On upgrade to 8.0,
   all these tables are shifted by 1024. On 5.7, the SYS_FIELDS has table id
   4, which gets updated to 1028 on upgrade. This would later assert when we
   try to open the SYS_VIRTUAL table (having id 1028) for upgrade. Hence, we
   need to upgrade system tables in reverse order to avoid that.
   These tables are created on boot. And hence this issue can only be caused by
   a new table being added at a later stage - the SYS_VIRTUAL table being added
   on upgrading to 5.7. */

  for (int i = SYS_NUM_SYSTEM_TABLES - 1; i >= 0; i--) {
    dict_table_t *system_table = dict_table_get_low(SYSTEM_TABLE_NAME[i]);

    ut_a(system_table != nullptr);
    ut_ad(dict_sys_table_id[i] == system_table->id);

    /* During upgrade, table_id of user tables is also
    moved by DICT_MAX_DD_TABLES. See dict_load_table_one()*/
    table_id_t new_table_id = system_table->id + DICT_MAX_DD_TABLES;

    dict_table_change_id_in_cache(system_table, new_table_id);

    dict_sys_table_id[i] = system_table->id;

    dict_table_prevent_eviction(system_table);
  }
}

/** Evict all tables that are loaded for applying purge.
Since we move the offset of all table ids during upgrade,
these tables cannot exist in cache. Also change table_ids
of SYS_* tables if they are upgraded from earlier versions */
void dict_upgrade_evict_tables_cache() {
  dict_table_t *table;

  rw_lock_x_lock(dict_operation_lock, UT_LOCATION_HERE);
  dict_sys_mutex_enter();

  ut_ad(dict_lru_validate());
  ut_ad(srv_is_upgrade_mode);

  /* Move all tables from non-LRU to LRU */
  for (table = UT_LIST_GET_LAST(dict_sys->table_non_LRU); table != nullptr;) {
    dict_table_t *prev_table;

    prev_table = UT_LIST_GET_PREV(table_LRU, table);

    if (!dict_table_is_system(table->id)) {
      DBUG_EXECUTE_IF("dd_upgrade", ib::info(ER_IB_MSG_185)
                                        << "Moving table " << table->name
                                        << " from non-LRU to LRU";);

      dict_table_move_from_non_lru_to_lru(table);
    }

    table = prev_table;
  }

  for (table = UT_LIST_GET_LAST(dict_sys->table_LRU); table != nullptr;) {
    dict_table_t *prev_table;

    prev_table = UT_LIST_GET_PREV(table_LRU, table);

    ut_ad(dict_table_can_be_evicted(table));

    DBUG_EXECUTE_IF("dd_upgrade", ib::info(ER_IB_MSG_186)
                                      << "Evicting table: LRU: "
                                      << table->name;);

    dict_table_remove_from_cache_low(table, true);

    table = prev_table;
  }

  dict_table_change_id_sys_tables();

  dict_sys_mutex_exit();
  rw_lock_x_unlock(dict_operation_lock);
}

/** Build the table_id array of SYS_* tables. This
array is used to determine if a table is InnoDB SYSTEM
table or not.
@return true if successful, false otherwise */
bool dict_sys_table_id_build() {
  dict_sys_mutex_enter();
  for (uint32_t i = 0; i < SYS_NUM_SYSTEM_TABLES; i++) {
    dict_table_t *system_table = dict_table_get_low(SYSTEM_TABLE_NAME[i]);

    if (system_table == nullptr) {
      /* Cannot find a system table, this happens only if user trying
      to boot server earlier than 5.7 */
      dict_sys_mutex_exit();
      LogErr(ERROR_LEVEL, ER_IB_MSG_1271);
      return (false);
    }
    dict_sys_table_id[i] = system_table->id;
  }
  dict_sys_mutex_exit();
  return (true);
}

/** @return true if table is InnoDB SYS_* table
@param[in]      table_id        table id  */
bool dict_table_is_system(table_id_t table_id) {
  for (uint32_t i = 0; i < SYS_NUM_SYSTEM_TABLES; i++) {
    if (table_id == dict_sys_table_id[i]) {
      return (true);
    }
  }
  return (false);
}

dberr_t dd_sdi_acquire_exclusive_mdl(THD *thd, space_id_t space_id,
                                     MDL_ticket **sdi_mdl) {
  /* Exclusive MDL always need trx context and is
  released on trx commit. So check if thd & trx
  exists */
  ut_ad(thd != nullptr);
  ut_ad(check_trx_exists(current_thd) != nullptr);
  ut_ad(sdi_mdl != nullptr);
  ut_ad(!dict_sys_mutex_own());

  char tbl_buf[NAME_LEN + 1];
  const char *db_buf = "dummy_sdi_db";

  snprintf(tbl_buf, sizeof(tbl_buf), "SDI_" SPACE_ID_PF, space_id);

  /* Submit a higher than default lock wait timeout, the timeout accepted by
  dd::acquire_exclusive_table_mdl is in seconds. */
  std::chrono::seconds lock_wait_timeout = thd_lock_wait_timeout(thd);
  if (lock_wait_timeout < std::chrono::hours(27)) {
    lock_wait_timeout += std::chrono::hours(27);
  }
  if (dd::acquire_exclusive_table_mdl(
          thd, db_buf, tbl_buf, (unsigned long int)lock_wait_timeout.count(),
          sdi_mdl)) {
    /* MDL failure can happen with lower timeout
    values chosen by user */
    return (DB_LOCK_WAIT_TIMEOUT);
  }

  /* MDL creation failed */
  if (*sdi_mdl == nullptr) {
    ut_d(ut_error);
    ut_o(return (DB_LOCK_WAIT_TIMEOUT));
  }

  return (DB_SUCCESS);
}

dberr_t dd_sdi_acquire_shared_mdl(THD *thd, space_id_t space_id,
                                  MDL_ticket **sdi_mdl) {
  ut_ad(sdi_mdl != nullptr);
  ut_ad(!dict_sys_mutex_own());

  char tbl_buf[NAME_LEN + 1];
  const char *db_buf = "dummy_sdi_db";

  snprintf(tbl_buf, sizeof(tbl_buf), "SDI_" SPACE_ID_PF, space_id);

  if (dd::acquire_shared_table_mdl(thd, db_buf, tbl_buf, false, sdi_mdl)) {
    /* MDL failure can happen with lower timeout
    values chosen by user */
    return (DB_LOCK_WAIT_TIMEOUT);
  }

  /* MDL creation failed */
  if (*sdi_mdl == nullptr) {
    ut_d(ut_error);
    ut_o(return (DB_LOCK_WAIT_TIMEOUT));
  }

  return (DB_SUCCESS);
}

/** Get the tablespace data directory if set, otherwise empty string.
@return the data directory */
std::string dict_table_get_datadir(const dict_table_t *table) {
  std::string path;

  if (DICT_TF_HAS_DATA_DIR(table->flags) && table->data_dir_path != nullptr) {
    path.assign(table->data_dir_path);
  }

  return (path);
}

dberr_t dict_set_compression(dict_table_t *table, const char *algorithm,
                             bool is_import_op) {
  ut_ad(table != nullptr);

  /* We don't support Page Compression for the system tablespace, the temporary
  tablespace, or any general tablespace because COMPRESSION is set by TABLE
  DDL, not TABLESPACE DDL (there is no other technical reason). And neither do
  we support it for tables with compressed row_format.

  Also, do not use it for missing tables, unless it's an import operation as in
  the case of import we would still be importing the tablespace at this stage
  and would need to set the compression option for further processing. */
  if ((table->ibd_file_missing && !is_import_op) ||
      !DICT_TF2_FLAG_IS_SET(table, DICT_TF2_USE_FILE_PER_TABLE) ||
      DICT_TF2_FLAG_IS_SET(table, DICT_TF2_TEMPORARY) ||
      page_size_t(table->flags).is_compressed()) {
    return (DB_IO_NO_PUNCH_HOLE_TABLESPACE);
  }

  auto err = fil_set_compression(table->space, algorithm);

  return err;
}

dict_persist_t::Enable_immediate::Enable_immediate(dict_persist_t *persister)
    : m_persister(persister) {
  /* 1. Make sure all new auto increment operations save metadata to
  DD buffer table immediately. */
  m_persister->m_persist_immediately.store(true);

  /* 2. Save all existing metadata to DD buffer table. */
  dict_persist_to_dd_table_buffer();
}

dict_persist_t::Enable_immediate::~Enable_immediate() {
  m_persister->m_persist_immediately.store(false);
  m_persister = nullptr;
}

uint32_t dict_vcol_base_is_foreign_key(dict_v_col_t *vcol,
                                       dict_foreign_t *foreign) {
  const dict_table_t *table = foreign->foreign_table;
  uint32_t foreign_col_count = 0;

  for (uint32_t i = 0; i < foreign->n_fields; i++) {
    const char *foreign_col_name = foreign->foreign_col_names[i];
    for (uint32_t j = 0; j < vcol->num_base; j++) {
      if (strcmp(foreign_col_name,
                 table->get_col_name(vcol->base_col[j]->ind)) == 0) {
        foreign_col_count++;
      }
    }
  }
  return foreign_col_count;
}

#ifdef UNIV_DEBUG
/** Validate no active background threads to cause purge or rollback
 operations. */
void dict_validate_no_purge_rollback_threads() {
  /* No concurrent background threads to access to the table */
  ut_ad(trx_purge_state() == PURGE_STATE_STOP ||
        trx_purge_state() == PURGE_STATE_DISABLED);
  ut_ad(!srv_thread_is_active(srv_threads.m_trx_recovery_rollback));
}
#endif /* UNIV_DEBUG */
#endif /* !UNIV_HOTBACKUP */
