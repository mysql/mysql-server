/*
   Copyright (c) 2011, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "storage/ndb/plugin/ndb_thd_ndb.h"

#include "my_dbug.h"
#include "mysql/plugin.h"  // thd_get_thread_id
#include "mysqld_error.h"
#include "sql/derror.h"
#include "sql/sql_error.h"
#include "storage/ndb/plugin/ndb_ddl_transaction_ctx.h"
#include "storage/ndb/plugin/ndb_log.h"
#include "storage/ndb/plugin/ndb_thd.h"

/*
  Default value for max number of transactions creatable against NDB from
  the handler. Should really be 2 but there is a transaction to much allocated
  when lock table is used, and one extra to used for global schema lock.
*/
static const int MAX_TRANSACTIONS = 4;

Thd_ndb *Thd_ndb::seize(THD *thd, const char *name) {
  DBUG_TRACE;

  Thd_ndb *thd_ndb = new Thd_ndb(thd, name);
  if (thd_ndb == nullptr) {
    return nullptr;
  }

  if (thd_ndb->ndb->init(MAX_TRANSACTIONS) != 0) {
    delete thd_ndb;
    return nullptr;
  }

  // Save mapping between Ndb and THD
  thd_ndb->ndb->setCustomData64(thd_get_thread_id(thd));

  // Init Applier state (if it will do applier work)
  if (!thd_ndb->init_applier()) {
    delete thd_ndb;
    return nullptr;
  }

  return thd_ndb;
}

void Thd_ndb::release(Thd_ndb *thd_ndb) {
  DBUG_TRACE;
  delete thd_ndb;
}

bool Thd_ndb::recycle_ndb(void) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("ndb: %p", ndb));

  assert(global_schema_lock_trans == nullptr);
  assert(trans == nullptr);

  delete ndb;

  ndb = new Ndb(connection, "");
  if (ndb == nullptr) {
    // Dead code, failed new will terminate
    return false;
  }

  if (ndb->init(MAX_TRANSACTIONS) != 0) {
    // Failed to init Ndb object, release the newly created Ndb
    delete ndb;
    ndb = nullptr;
    return false;
  }

  // Save mapping between Ndb and THD
  ndb->setCustomData64(thd_get_thread_id(m_thd));

  /* Reset last commit epoch for this 'session'. */
  m_last_commit_epoch_session = 0;

  /* Update m_connect_count to avoid false failures of ::valid_ndb() */
  m_connect_count = connection->get_connect_count();

  return true;
}

bool Thd_ndb::valid_ndb(void) const {
  // The ndb object should be valid as long as a
  // global schema lock transaction is ongoing
  if (global_schema_lock_trans) return true;

  // The ndb object should be valid as long as a
  // transaction is ongoing
  if (trans) return true;

  if (unlikely(m_connect_count != connection->get_connect_count()))
    return false;

  return true;
}

void Thd_ndb::Trans_tables::dbug_print_elem(
    const std::pair<NDB_SHARE *, Stats> &elem, bool check_reset) const {
  const Stats &stat = elem.second;

  std::string records("<invalid>");
  if (!stat.invalid()) {
    records = std::to_string(stat.table_rows);
  }

  DBUG_PRINT("share", ("%p = { records: %s, uncommitted: %d }", elem.first,
                       records.c_str(), stat.uncommitted_rows));
  if (check_reset) assert(stat.uncommitted_rows == 0);
}

void Thd_ndb::Trans_tables::dbug_print(bool check_reset) const {
  DBUG_TRACE;
  for (const auto &key_and_value : m_map) {
    dbug_print_elem(key_and_value, check_reset);
  }
}

void Thd_ndb::Trans_tables::clear() { m_map.clear(); }

/**
  Register table stats for NDB_SHARE pointer.

  @param share   The NDB_SHARE pointer to find or create table stats for.

  @note Usage of the NDB_SHARE pointer as key means that all ha_ndbcluster
  instances which has been opened for same table in same transaction uses the
  same table stats instance.

  @return Pointer to table stats or nullptr
*/
Thd_ndb::Trans_tables::Stats *Thd_ndb::Trans_tables::register_stats(
    NDB_SHARE *share) {
  DBUG_TRACE;

  const auto emplace_res = m_map.emplace(share, Stats());
  if (emplace_res.second) {
    // New element inserted, double check initial values
    DBUG_PRINT("info", ("New element inserted for share: %p", share));
    assert(emplace_res.first->first == share);
    assert(emplace_res.first->second.invalid());
    assert(emplace_res.first->second.uncommitted_rows == 0);
  } else {
    DBUG_PRINT("info", ("Existing element found for share: %p", share));
    dbug_print_elem(*emplace_res.first, false);
  }

  dbug_print();

  // Return pointer to table stat
  const auto elem = emplace_res.first;
  Stats *stat_ptr = &(elem->second);
  return stat_ptr;
}

/**
   Reset counters for all Stats registered as taking part in the transaction.

   This is normally done when failure to execute the NDB transaction has
   occurred and caused all changes in whole transaction have been aborted. The
   counters will then be invalid.
 */
void Thd_ndb::Trans_tables::reset_stats() {
  DBUG_TRACE;
  for (auto &key_and_value : m_map) {
    Stats &stat = key_and_value.second;
    stat.uncommitted_rows = 0;
  }
  dbug_print(true);
}

/**
   Update cached tables stats for all NDB_SHARE's registered as taking part in
   the transaction. Reset number of uncommitted rows.
*/
void Thd_ndb::Trans_tables::update_cached_stats_with_committed() {
  DBUG_TRACE;
  dbug_print();
  for (auto &key_and_value : m_map) {
    NDB_SHARE *share = key_and_value.first;
    Stats &stat = key_and_value.second;
    share->cached_stats.add_changed_rows(stat.uncommitted_rows);
    stat.uncommitted_rows = 0;
  }
  dbug_print(true);
}

bool Thd_ndb::check_option(Options option) const { return (options & option); }

void Thd_ndb::set_option(Options option) { options |= option; }

/*
  This function is called after a row operation has been added to
  the transaction. It will update the unsent byte counter and determine if batch
  size threshold has been exceeded.

  @param row_size Estimated number of bytes that has been added to transaction.

  @return true Batch size threshold has been exceeded

*/
bool Thd_ndb::add_row_check_if_batch_full(uint row_size) {
  if (m_unsent_bytes == 0) {
    // Clear batch buffer
    m_batch_mem_root.ClearForReuse();
  }

  // Increment number of unsent bytes. NOTE! The row_size is assumed to be a
  // fairly small number, basically limited by max record size of a table
  m_unsent_bytes += row_size;

  // Return true if unsent bytes has exceeded the batch size threshold.
  return m_unsent_bytes >= m_batch_size;
}

bool Thd_ndb::check_trans_option(Trans_options option) const {
  return (trans_options & option);
}

void Thd_ndb::set_trans_option(Trans_options option) {
#ifndef NDEBUG
  if (check_trans_option(TRANS_TRANSACTIONS_OFF))
    DBUG_PRINT("info", ("Disabling transactions"));
  if (check_trans_option(TRANS_NO_LOGGING))
    DBUG_PRINT("info", ("Statement is not using logging"));
#endif
  trans_options |= option;
}

void Thd_ndb::reset_trans_options(void) {
  DBUG_PRINT("info", ("Resetting trans_options"));
  trans_options = 0;
}

/*
  Push to THD's condition stack

  @param severity    Severity of the pushed condition
  @param code        Error code to use for the pushed condition
  @param[in]  fmt    printf-like format string
  @param[in]  args   Arguments
*/
static void push_condition(THD *thd,
                           Sql_condition::enum_severity_level severity,
                           uint code, const char *fmt, va_list args)
    MY_ATTRIBUTE((format(printf, 4, 0)));

static void push_condition(THD *thd,
                           Sql_condition::enum_severity_level severity,
                           uint code, const char *fmt, va_list args) {
  assert(fmt);

  // Assemble the message
  char msg_buf[512];
  vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);

  push_warning(thd, severity, code, msg_buf);

  // Workaround problem where Ndb_local_connection can't access the
  // warnings produced when running a SQL query, instead detect
  // if this a binlog thread and print the warning also to log.
  // NOTE! This can be removed when BUG#27507543 has been implemented
  // and instead log these warnings in a more controlled/selective manner
  // in Ndb_local_connection.
  if (ndb_thd_is_binlog_thread(thd) || ndb_thd_is_replica_thread(thd)) {
    if (code == ER_REPLICA_SILENT_RETRY_TRANSACTION) {
      // The warning should be handled silently
      return;
    }
    ndb_log_warning("%s", msg_buf);
  }
}

void Thd_ndb::push_warning(const char *fmt, ...) const {
  const uint code = ER_GET_ERRMSG;
  va_list args;
  va_start(args, fmt);
  push_condition(m_thd, Sql_condition::SL_WARNING, code, fmt, args);
  va_end(args);
}

void Thd_ndb::push_warning(uint code, const char *fmt, ...) const {
  va_list args;
  va_start(args, fmt);
  push_condition(m_thd, Sql_condition::SL_WARNING, code, fmt, args);
  va_end(args);
}

void Thd_ndb::push_ndb_error_warning(const NdbError &ndberr) const {
  if (ndberr.status == NdbError::TemporaryError) {
    push_warning_printf(m_thd, Sql_condition::SL_WARNING,
                        ER_GET_TEMPORARY_ERRMSG,
                        ER_THD(m_thd, ER_GET_TEMPORARY_ERRMSG), ndberr.code,
                        ndberr.message, "NDB");
  } else {
    push_warning_printf(m_thd, Sql_condition::SL_WARNING, ER_GET_ERRMSG,
                        ER_THD(m_thd, ER_GET_ERRMSG), ndberr.code,
                        ndberr.message, "NDB");
  }
}

void Thd_ndb::set_ndb_error(const NdbError &ndberr, const char *message) const {
  push_ndb_error_warning(ndberr);
  my_printf_error(ER_GET_ERRMSG, "%s", MYF(0), message);
}

Ndb_DDL_transaction_ctx *Thd_ndb::get_ddl_transaction_ctx(
    bool create_if_not_exist) {
  if (!m_ddl_ctx && create_if_not_exist) {
    /* There is no DDL context yet. Instantiate it. */
    m_ddl_ctx = new Ndb_DDL_transaction_ctx(m_thd);
  }
  return m_ddl_ctx;
}

void Thd_ndb::clear_ddl_transaction_ctx() {
  assert(m_ddl_ctx != nullptr);
  delete m_ddl_ctx;
  m_ddl_ctx = nullptr;
}

std::string Thd_ndb::get_info_str() const {
  bool sep = false;
  std::stringstream ss;
  if (m_thread_name) {
    ss << "name=" << m_thread_name;
    sep = true;
  }
  const ulonglong pfs_thread_id = ndb_thd_get_pfs_thread_id();
  if (pfs_thread_id) {
    if (sep) ss << ", ";
    ss << "pfs_thread_id=" << pfs_thread_id;
  }
  return ss.str();
}
