/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

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

/**
  @file

  Extends the binlog code for AliSQL
*/

#include "sql/binlog_ext.h"
#include "sql/duckdb/duckdb_config.h"
#include "sql/duckdb/duckdb_context.h"
#include "sql/duckdb/duckdb_query.h"

Binlog_ext mysql_bin_log_ext;

void Binlog_ext::init() {
  m_log_num = 0;
  open_binlog_file();
}

bool Binlog_ext::open_binlog_file() {
  /* m_log_num is stored in big-endian  */
  int4store(reinterpret_cast<uchar *>(&m_log_num),
            static_cast<uint>(atoi(fn_ext(mysql_bin_log.get_log_fname()) + 1)));
  m_log_name =
      mysql_bin_log.log_file_name + dirname_length(mysql_bin_log.log_file_name);
  return false;
}

bool Binlog_ext::duckdb_commit(THD *thd) {
  DBUG_TRACE;
  my_off_t total_bytes = 0;
  bool do_rotate = false;
  bool error = false;

  thd->get_transaction()->m_flags.run_hooks = false;

  DBUG_EXECUTE_IF("crash_commit_before_log", DBUG_SUICIDE(););

  mysql_mutex_lock(&mysql_bin_log.LOCK_log);
  // used for truncate binlog if error happens
  my_off_t before_pos = mysql_bin_log.m_binlog_file->position();

  mysql_bin_log.assign_automatic_gtids_to_flush_group(thd);
  {
    std::pair<int, my_off_t> result = mysql_bin_log.flush_thread_caches(thd);
    error = result.first;
    total_bytes = result.second;
  }

  if (error == 0 && total_bytes > 0) {
    my_off_t flush_end_pos;
    error = mysql_bin_log.flush_cache_to_file(&flush_end_pos);
  }

  if (error == 0 && total_bytes > 0) {
    std::pair<bool, bool> result = mysql_bin_log.sync_binlog_file(false);
    error = result.first;
  }

  if (error) {
    mysql_bin_log.handle_binlog_flush_or_sync_error(
        thd, false /* need_lock_log */,
        (thd->commit_error == THD::CE_FLUSH_GNO_EXHAUSTED_ERROR)
            ? ER_THD(thd, ER_GNO_EXHAUSTED)
            : nullptr);
  }
  DBUG_EXECUTE_IF("crash_after_binlog_sync", DBUG_SUICIDE(););
  DEBUG_SYNC(thd, "after_binlog_sync");

  /*
    For the case DuckDB engine does not envovled into the transaction,
    A duckdb transaction should be started and the handlerton is registered
    to Server layer here for updating duckdb_binlog_position table.
  */

  auto duckdb_ctx = thd->get_duckdb_context();
  if (!duckdb_ctx->has_transaction() && duckdb_ctx->duckdb_trans_begin())
    assert(1);
  if (thd->get_transaction()->is_active(Transaction_ctx::SESSION)) {
    trans_register_ha(thd, true, myduck_hton, nullptr);
  } else if (thd->get_transaction()->is_active(Transaction_ctx::STMT)) {
    /*
      If a SQL does nothing(e.g ALTER TABLE t1), it even doesn't open the
      table. It will be the status that both transaction and stmt is not
      active. In this case, ha_commit_trans will not be called.
      tc_log->commit is called directly. It should not be registered
      in this case.
    */
    trans_register_ha(thd, false, myduck_hton, nullptr);
  }

  std::ostringstream stmt;
  stmt << "INSERT INTO mysql.duckdb_binlog_position VALUES('" << m_log_name
       << "', " << mysql_bin_log.m_binlog_file->get_real_file_size() << ")";

  auto query_res = myduck::duckdb_query(thd, stmt.str(), false);

  // For the case SQL does nothing, commit DuckDB Engine here.
  if (!query_res->HasError() &&
      !thd->get_transaction()->is_active(Transaction_ctx::SESSION) &&
      !thd->get_transaction()->is_active(Transaction_ctx::STMT)) {
    query_res = myduck::duckdb_query(thd, "COMMIT", false);
    DBUG_EXECUTE_IF("crash_after_duckdb_commit", DBUG_SUICIDE(););
  }

  if (query_res->HasError()) {
    thd->commit_error = THD::CE_COMMIT_ERROR;
    my_error(ER_DUCKDB_COMMIT_ERROR, MYF(0), query_res->GetError().c_str());
  }

  if (thd->commit_error == THD::CE_NONE) {
    mysql_bin_log.finish_transaction_in_engines(
        thd, thd->get_transaction()->m_flags.real_commit, false);
  }

  // finish_transaction_in_engines may return CE_COMMIT_ERROR
  if (thd->commit_error == THD::CE_COMMIT_ERROR) {
    mysql_bin_log.m_binlog_file->truncate(before_pos);
    (void)mysql_bin_log.finish_commit(thd);
    mysql_mutex_unlock(&mysql_bin_log.LOCK_log);

    return true;
  }

  mysql_bin_log.update_binlog_end_pos();

  do_rotate = (mysql_bin_log.m_binlog_file->get_real_file_size() >=
               (my_off_t)mysql_bin_log.max_size);
  (void)mysql_bin_log.finish_commit(thd);
  mysql_mutex_unlock(&mysql_bin_log.LOCK_log);

  /*
   If we need to rotate, we do it without commit error.
   Otherwise the thd->commit_error will be possibly reset.
  */
  if (DBUG_EVALUATE_IF("force_rotate", 1, 0) ||
      (do_rotate && thd->commit_error == THD::CE_NONE)) {
    bool check_purge = false;
    mysql_mutex_lock(&mysql_bin_log.LOCK_log);
    /*
      The transaction has committed, thus we ignore rotation error
      here. The server will crash if any severe error happens during
      rotation.
    */
    (void)mysql_bin_log.rotate(false, &check_purge);
    mysql_mutex_unlock(&mysql_bin_log.LOCK_log);

    if (check_purge) mysql_bin_log.auto_purge();
  }
  return false;
}

bool Binlog_ext::duckdb_binlog_rotate() {
  if (!myduck::global_mode_on()) return false;

  // Ignore truncate error here. It should not crash the server.
  auto query_res =
      myduck::duckdb_query("TRUNCATE TABLE mysql.duckdb_binlog_position");

  DBUG_EXECUTE_IF("crash_during_duckdb_binlog_rotate", DBUG_SUICIDE(););

  std::ostringstream stmt;
  stmt << "INSERT INTO mysql.duckdb_binlog_position VALUES('" << m_log_name
       << "', " << mysql_bin_log.m_binlog_file->get_real_file_size() << ")";
  fprintf(stderr, "query:%s\n", stmt.str().c_str());
  query_res = myduck::duckdb_query(stmt.str());

  return DBUG_EVALUATE_IF("simulate_duckdb_binlog_roate_error", true,
                          query_res->HasError());
}

bool Binlog_ext::duckdb_binlog_init() {
  using namespace myduck;
  std::string query =
      "SELECT 1 FROM information_schema.tables "
      "WHERE table_schema = 'mysql' AND table_name = 'duckdb_binlog_position'";
  auto res = duckdb_query(query);
  if (res->HasError()) {
    LogErr(ERROR_LEVEL, ER_DUCKDB,
           "Failed to SELECT information_schema.tables");
    return true;
  }

  auto mres = res->Cast<duckdb::StreamQueryResult>().Materialize();
  if (mres->RowCount() > 0) return false;

  LogErr(INFORMATION_LEVEL, ER_DUCKDB, "Create duckdb_binlog_position table");

  query = "CREATE SCHEMA IF NOT EXISTS mysql";
  res = duckdb_query(query);
  if (res->HasError()) goto err;

  query =
      "CREATE TABLE mysql.duckdb_binlog_position("
      "  file VARCHAR(128) NOT NULL,"
      "  position BIGINT NOT NULL)";
  res = duckdb_query(query);
  if (res->HasError()) goto err;

  return false;
err:
  LogErr(ERROR_LEVEL, ER_DUCKDB, "Failed to initialize duckdb_binlog_position");
  return true;
}

bool Binlog_ext::duckdb_recover(const char *log_name) {
  Binlog_file_reader binlog_file_reader(opt_source_verify_checksum);
  if (binlog_file_reader.open(log_name)) {
    LogErr(ERROR_LEVEL, ER_BINLOG_FILE_OPEN_FAILED,
           binlog_file_reader.get_error_str());
    return true;
  }

  if (!mysql_bin_log.read_binlog_in_use_flag(binlog_file_reader)) return false;

  std::string stmt;
  stmt =
      "SELECT max(position) FROM mysql.duckdb_binlog_position WHERE file = '";
  stmt += log_name + dirname_length(log_name);
  stmt += "'";
  auto res = myduck::duckdb_query(stmt);

  if (res->HasError()) {
    LogErr(ERROR_LEVEL, ER_DUCKDB,
           "Failed to read positon from mysql.duckdb_binlog_position");
    return true;
  }

  auto mres = res->Cast<duckdb::StreamQueryResult>().Materialize();
  LogErr(INFORMATION_LEVEL, ER_DUCKDB, mres->ToString().c_str());

  if (mres->RowCount() == 0) return false;
  if (mres->GetValue(0, 0).IsNull()) return false;

  auto pos = mres->GetValue<int64_t>(0, 0);
  if (pos == 0) return false;

  truncate(log_name, pos);
  std::ostringstream errmsg;
  errmsg << "Truncate last binlog file to position " << pos;
  LogErr(INFORMATION_LEVEL, ER_DUCKDB, errmsg.str().c_str());
  return false;
}

void trx_cache_write_event(THD *thd, Log_event *event) {
  binlog_cache_mngr *cache_mngr = thd_get_cache_mngr(thd);
  cache_mngr->trx_cache.write_event(event);
}
