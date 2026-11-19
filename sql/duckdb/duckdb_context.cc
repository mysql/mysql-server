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
#include "duckdb_context.h"
#include "mysql/components/services/log_builtins.h"
#include "sql/binlog_ext.h"
#include "sql/debug_sync.h"
#include "sql/derror.h"
#include "sql/duckdb/duckdb_config.h"
#include "sql/duckdb/duckdb_log.h"
#include "sql/duckdb/duckdb_sql_quote.h"
#include "sql/rpl_gtid_persist.h"
#include "sql/rpl_replica.h"
#include "sql/rpl_rli.h"
#include "sql/set_var.h"
#include "sql/sql_lex.h"
#include "sql/transaction.h"

bool duckdb_idempotent_data_import_enabled = false;
#define my_millisecond_getsystime() (my_getsystime() / 10000)

namespace myduck {

ulonglong local_gtid_compression_period = 500;

static void push_warning(THD *thd, std::string &warn_msg) {
  if (warn_msg.empty()) {
    return;
  }
  push_warning_printf(
      thd, Sql_condition::SL_WARNING, ER_DUCKDB_SETTING_SESSION_VARIABLE,
      ER_THD(thd, ER_DUCKDB_SETTING_SESSION_VARIABLE), warn_msg.c_str());
  warn_msg.clear();
}

static std::string get_duckdb_explain_output_name(THD *thd) {
  static const char *duckdb_explain_output_names[] = {"ALL", "OPTIMIZED_ONLY",
                                                      "PHYSICAL_ONLY", NullS};
  duckdb::ExplainOutputType duckdb_explain_output =
      static_cast<duckdb::ExplainOutputType>(
          thd->variables.duckdb_explain_output_type);
  return std::string(
      duckdb_explain_output_names[static_cast<size_t>(duckdb_explain_output)]);
}

std::unique_ptr<duckdb::QueryResult>
DuckdbThdContext::DuckdbSessionConfig::compare_and_config(
    THD *thd, duckdb::Connection &connection) {
  std::vector<std::string> config_sql;

  std::string db;
  if (thd->db().str != nullptr && thd->db().length > 0 &&
      std::string(thd->db().str) != m_database) {
    db = std::string(thd->db().str);
    std::string sql = "USE " + quote_duckdb_ident(db);
    /* Allow 'use db' execution failure */
    auto res = duckdb_query(connection, sql, false);
    m_database.Set(res->HasError() ? "" : db);
  }

  std::string warn_msg;

  std::string tz = get_timezone_according_thd(thd, warn_msg);
  if (tz != m_timezone) {
    config_sql.push_back("SET TimeZone = '" + tz + "'");
    push_warning(thd, warn_msg);
    m_timezone.Set(tz);
  }

  std::string collation =
      get_duckdb_collation(thd->variables.collation_connection, warn_msg);
  if (collation != m_collation) {
    config_sql.push_back("SET default_collation = '" + collation + "'");
    push_warning(thd, warn_msg);
    m_collation.Set(collation);
  }

  bool force_no_collation =
      (thd->slave_thread || thd->variables.duckdb_force_no_collation) ? true
                                                                      : false;
  if (m_force_no_collation != force_no_collation) {
    config_sql.push_back(std::string("SET force_no_collation = ") +
                         (force_no_collation ? "true" : "false"));
    m_force_no_collation.Set(force_no_collation);
  }

  timeval user_time = thd->user_time;
  if (m_user_time != user_time) {
    std::string config_timestamp("SET timestamp = ");
    if (user_time.tv_sec || user_time.tv_usec) {
      config_timestamp.append(
          std::to_string(user_time.tv_sec * 1000000LL + user_time.tv_usec));
    } else {
      config_timestamp.append("-1");
    }
    config_sql.push_back(config_timestamp);
    m_user_time.Set(user_time);
  }

  ulong default_week_format = thd->variables.default_week_format;
  if (default_week_format != m_default_week_format) {
    config_sql.push_back("SET default_week_format = " +
                         std::to_string(default_week_format));
    m_default_week_format.Set(default_week_format);
  }

  ulonglong sql_mode = thd->variables.sql_mode;
  if (sql_mode != m_sql_mode) {
    config_sql.push_back("SET sql_mode = " + std::to_string(sql_mode));
    m_sql_mode.Set(sql_mode);
  }

  ulonglong disabled_optimizers = thd->variables.duckdb_disabled_optimizers;
  if (disabled_optimizers != m_disabled_optimizers) {
    std::string sql = "SET disabled_optimizers = '";
    LEX_STRING tmp;
    if (!duckdb_disabled_optimizers_string_representation(
            thd, disabled_optimizers, &tmp)) {
      sql += to_string(tmp);
      sql += "'";
    }
    config_sql.push_back(sql);
    m_disabled_optimizers.Set(disabled_optimizers);
  }

  ulonglong merge_join_threshold = thd->variables.duckdb_merge_join_threshold;
  if (merge_join_threshold != m_merge_join_threshold) {
    std::string sql = "SET merge_join_threshold = ";
    sql += std::to_string(merge_join_threshold);
    config_sql.push_back(sql);
    m_merge_join_threshold.Set(merge_join_threshold);
  }

  ulonglong max_threads_per_query =
      thd->slave_thread ? duckdb_max_threads_per_query_rpl
                        : thd->variables.duckdb_max_threads_per_query;
  if (max_threads_per_query != m_query_max_threads) {
    std::string sql = "SET max_threads_per_query = ";
    sql += std::to_string(max_threads_per_query);
    config_sql.push_back(sql);
    m_query_max_threads.Set(max_threads_per_query);
  }

  if (thd->lex->is_explain()) {
    std::string cur_explain_output_str = get_duckdb_explain_output_name(thd);
    if (cur_explain_output_str != m_explain_output_str) {
      config_sql.push_back("set explain_output = '" + cur_explain_output_str +
                           "'");
      m_explain_output_str.Set(cur_explain_output_str);
    }
  }

  bool prefer_high_precision = thd->variables.duckdb_prefer_high_precision;
  if (m_prefer_high_precision != prefer_high_precision) {
    config_sql.push_back(std::string("SET prefer_high_precision = ") +
                         (prefer_high_precision ? "true" : "false"));
    m_prefer_high_precision.Set(prefer_high_precision);
  }

  std::unique_ptr<duckdb::QueryResult> res;
  /* Params unchanged, no need to config. */
  if (config_sql.empty()) {
    return res;
  }

  for (auto &sql : config_sql) {
    res = duckdb_query(connection, sql);
    if (res->HasError()) {
      /* New value is cached in advance, need to reset it if failed. */
      reset_cached_config();
      return res;
    }
  }

  return res;
}

DeltaAppender *DuckdbThdContext::get_appender(TABLE *table) {
  if (!m_appenders) {
    m_appenders = std::make_unique<DeltaAppenders>();
  }

  cur_batch_could_be_committed = false;

  std::string db(table->s->db.str, table->s->db.length);
  std::string tb(table->s->table_name.str, table->s->table_name.length);

  return m_appenders->get_appender(m_thd, db, tb,
                                   batch_state == IN_INSERT_ONLY_BATCH,
                                   get_idempotent_flag(), table);
}

int DuckdbThdContext::append_row_insert(TABLE *table,
                                        const MY_BITMAP *blob_map) {
  DeltaAppender *delta = get_appender(table);
  return delta ? delta->append_row_insert(table, cur_trx_no, blob_map)
               : HA_DUCKDB_APPEND_ERROR;
}

int DuckdbThdContext::append_row_update(TABLE *table, const uchar *old_row) {
  DeltaAppender *delta = get_appender(table);
  return delta ? delta->append_row_update(table, cur_trx_no, old_row)
               : HA_DUCKDB_APPEND_ERROR;
}

int DuckdbThdContext::append_row_delete(TABLE *table) {
  DeltaAppender *delta = get_appender(table);
  return delta ? delta->append_row_delete(table, cur_trx_no)
               : HA_DUCKDB_APPEND_ERROR;
}

bool DuckdbThdContext::get_idempotent_flag() {
  if (duckdb_idempotent_data_import_enabled &&
      m_thd->variables.duckdb_data_import_mode) {
    return true;
  }

  return m_thd->rli_slave &&
         m_thd->rli_slave->get_local_duckdb_idempotent_flag();
}

void DuckdbThdContext::add_gtid_to_batch_set() {
  global_tsid_lock->rdlock();
  batch_gtid_set.ensure_sidno(m_thd->owned_gtid.sidno);
  batch_gtid_set._add_gtid(m_thd->owned_gtid);
  ++gtid_compression_counter;
  global_tsid_lock->unlock();
  m_thd->clear_owned_gtids();
  m_thd->variables.gtid_next.set_undefined();
}

bool DuckdbThdContext::duckdb_delay_commit(Log_event *ev) {
  event_seq_state = DuckdbThdContext::INITIAL;

  if (!duckdb_multi_trx_in_batch) return false;
  if (batch_state != IN_MIX_BATCH) return false;
  if (m_thd->owned_gtid.sidno <= 0) return false;

  Relay_log_info *rli = m_thd->rli_slave;
  if (!rli) return false;

  if (myduck::duckdb_log_options & LOG_DUCKDB_MULTI_TRX_BATCH_DETAIL) {
    myduck::log_duckdb_gtid("duckdb batch add gtid", ASSIGNED_GTID,
                            m_thd->owned_gtid.sidno, m_thd->owned_gtid.gno);
  }

  add_gtid_to_batch_set();

  mysql_mutex_lock(&rli->data_lock);
  xid_event_master_log_pos = ev->common_header->log_pos;
  xid_event_relay_log_pos = rli->get_event_relay_log_pos();
  strmake(xid_event_relay_log_name, rli->get_event_relay_log_name(),
          FN_REFLEN - 1);
  xid_future_event_relay_log_pos = rli->get_future_event_relay_log_pos();
  mysql_mutex_unlock(&rli->data_lock);

  if (!batch_multi_trx_started) {
    batch_multi_trx_started = true;
    batch_start_time = my_millisecond_getsystime();
    cur_batch_length = 0;
  }

  cur_batch_could_be_committed = true;
  ++cur_trx_no;
  cur_batch_length += rli->get_transaction_length();
  ulonglong cur_time = my_millisecond_getsystime();

  if (cur_batch_length >= duckdb_multi_trx_max_batch_length) {
    if (myduck::duckdb_log_options & LOG_DUCKDB_MULTI_TRX_BATCH_COMMIT) {
      myduck::log_duckdb_multi_trx_batch_commit("batch length");
    }
    return false;
  }

  if (cur_time - batch_start_time >= duckdb_multi_trx_timeout) {
    if (myduck::duckdb_log_options & LOG_DUCKDB_MULTI_TRX_BATCH_COMMIT) {
      myduck::log_duckdb_multi_trx_batch_commit("timeout");
    }
    return false;
  }

  return true;
}

int DuckdbThdContext::save_batch_gtid_set() {
  bool compress = false;
  if ((gtid_executed_compression_period != 0 &&
       gtid_compression_counter > gtid_executed_compression_period) ||
      gtid_compression_counter > local_gtid_compression_period) {
    gtid_compression_counter = 0;
    compress = true;
  }

  global_tsid_lock->rdlock();
  int ret = gtid_table_persistor->save(&batch_gtid_set, compress);
  global_tsid_lock->unlock();

  return ret;
}
void DuckdbThdContext::reset_batch() {
  if (m_appenders && !m_appenders->is_empty()) m_appenders->reset_all();
  batch_state = UNDEFINED;
  batch_multi_trx_started = false;
  cur_batch_could_be_committed = false;
  cur_trx_no = 0;
  cur_batch_length = 0;
  batch_start_time = 0;
  batch_gtid_set.clear_set();
  xid_event_relay_log_pos = 0;
  memset(xid_event_relay_log_name, 0, FN_REFLEN);
  event_seq_state = DuckdbThdContext::INITIAL;
}

void DuckdbThdContext::prepare_gtids_for_binlog_commit() {
  if (m_thd->owned_gtid.sidno <= 0) {
    m_thd->owned_gtid.sidno = batch_gtid_set.get_max_sidno();
    m_thd->owned_gtid.gno =
        batch_gtid_set.get_last_gno(m_thd->owned_gtid.sidno);
    m_thd->owned_tsid =
        global_tsid_map->sidno_to_tsid(m_thd->owned_gtid.sidno, true);

    batch_gtid_set._remove_gtid(m_thd->owned_gtid);

    m_thd->variables.gtid_next.set(m_thd->owned_gtid);
  }

  global_tsid_lock->rdlock();
  Transaction_context_log_event tcle(server_uuid, true, m_thd->thread_id(),
                                     false, &batch_gtid_set);
  trx_cache_write_event(m_thd, &tcle);
  global_tsid_lock->unlock();
}

void DuckdbThdContext::add_gtid_set(Gtid_set *set) {
  if (!batch_multi_trx_started) {
    batch_multi_trx_started = true;
    batch_start_time = my_millisecond_getsystime();
  }

  Gtid saved_owned_gtid = m_thd->owned_gtid;
  mysql::gtid::Tsid saved_owned_sid = m_thd->owned_tsid;
  m_thd->clear_owned_gtids();

  global_tsid_lock->rdlock();

  Gtid_set::Gtid_iterator git(set);
  Gtid g = git.get();
  rpl_sidno locked_sidno = 0;

  while (g.sidno != 0) {
    g.sidno =
        global_tsid_map->add_tsid(set->get_tsid_map()->sidno_to_tsid(g.sidno));

    if (locked_sidno != g.sidno) {
      if (locked_sidno > 0) gtid_state->unlock_sidno(locked_sidno);
      gtid_state->lock_sidno(g.sidno);
      locked_sidno = g.sidno;
    }

    gtid_state->acquire_ownership(m_thd, g);
    batch_gtid_set.ensure_sidno(g.sidno);
    batch_gtid_set._add_gtid(g);

    git.next();
    g = git.get();

    m_thd->clear_owned_gtids();
  }

  if (locked_sidno > 0) gtid_state->unlock_sidno(locked_sidno);

  global_tsid_lock->unlock();

  m_thd->owned_gtid = saved_owned_gtid;
  m_thd->owned_tsid = saved_owned_sid;
}

void DuckdbThdContext::update_on_commit() {
  Gtid_set::Gtid_iterator git(&batch_gtid_set);
  Gtid g = git.get();

  while (g.sidno != 0) {
    m_thd->owned_gtid = g;
    if (myduck::duckdb_log_options & LOG_DUCKDB_MULTI_TRX_BATCH_DETAIL) {
      myduck::log_duckdb_gtid("duckdb batch update on commit", ASSIGNED_GTID,
                              m_thd->owned_gtid.sidno, m_thd->owned_gtid.gno);
    }
    gtid_state->update_on_commit(m_thd);
    git.next();
    g = git.get();
  }

  reset_batch();
}

void DuckdbThdContext::update_on_rollback() {
  /* When log_bin=on, we retrieved a GTID and put it into 'thd->owned_gtid',
     which needs to be released here */
  gtid_state->update_on_rollback(m_thd);

  Gtid_set::Gtid_iterator git(&batch_gtid_set);
  Gtid g = git.get();

  while (g.sidno != 0) {
    m_thd->owned_gtid = g;
    gtid_state->update_on_rollback(m_thd);
    git.next();
    g = git.get();
  }

  reset_batch();
}

int DuckdbThdContext::commit_if_possible() {
  if (!batch_multi_trx_started || !cur_batch_could_be_committed) return 0;

  Relay_log_info *rli = m_thd->rli_slave;
  if (!rli) return 0;

  my_off_t saved_event_relay_log_pos;
  char saved_event_relay_log_name[FN_REFLEN];
  my_off_t saved_future_event_relay_log_pos;

  mysql_mutex_lock(&rli->data_lock);
  saved_event_relay_log_pos = rli->get_event_relay_log_pos();
  strmake(saved_event_relay_log_name, rli->get_event_relay_log_name(),
          FN_REFLEN - 1);
  saved_future_event_relay_log_pos = rli->get_future_event_relay_log_pos();
  rli->set_event_relay_log_pos(xid_event_relay_log_pos);
  rli->set_event_relay_log_name(xid_event_relay_log_name);
  rli->set_future_event_relay_log_pos(xid_future_event_relay_log_pos);
  mysql_mutex_unlock(&rli->data_lock);

  set_batch_state(UNDEFINED);

  Xid_log_event ev(m_thd, 0);
  ev.common_header->log_pos = xid_event_master_log_pos;
  int ret = ev.do_apply_event(rli);

  mysql_mutex_lock(&rli->data_lock);
  rli->set_event_relay_log_pos(saved_event_relay_log_pos);
  rli->set_event_relay_log_name(saved_event_relay_log_name);
  rli->set_future_event_relay_log_pos(saved_future_event_relay_log_pos);
  mysql_mutex_unlock(&rli->data_lock);

  return ret;
}

bool DuckdbThdContext::need_implicit_commit_batch(Log_event *ev) {
  if (!batch_multi_trx_started || !cur_batch_could_be_committed) return false;

  auto ev_type = ev->get_type_code();

  switch (event_seq_state) {
    case DuckdbThdContext::INITIAL:
      if (ev_type == mysql::binlog::event::GTID_LOG_EVENT ||
          ev_type == mysql::binlog::event::GTID_TAGGED_LOG_EVENT) {
        event_seq_state = DuckdbThdContext::GTID;
      }
      return false;
    case DuckdbThdContext::GTID:
      if (ev_type == mysql::binlog::event::QUERY_EVENT &&
          strcmp("BEGIN", ((Query_log_event *)ev)->query) == 0) {
        event_seq_state = DuckdbThdContext::GTID_BEGIN;
        return false;
      } else {
        if (myduck::duckdb_log_options & LOG_DUCKDB_MULTI_TRX_BATCH_COMMIT) {
          myduck::log_duckdb_multi_trx_batch_commit("DDL");
        }
        return true;
      }
    case DuckdbThdContext::GTID_BEGIN:
      if (ev_type == mysql::binlog::event::TABLE_MAP_EVENT ||
          ev_type == mysql::binlog::event::ROWS_QUERY_LOG_EVENT) {
        event_seq_state = DuckdbThdContext::INITIAL;
        cur_batch_could_be_committed = false;
        return false;
      } else if (ev_type == mysql::binlog::event::XID_EVENT ||
                 (ev_type == mysql::binlog::event::QUERY_EVENT &&
                  strcmp("COMMIT", ((Query_log_event *)ev)->query) == 0)) {
        event_seq_state = DuckdbThdContext::INITIAL;
        return false;
      } else {
        if (myduck::duckdb_log_options & LOG_DUCKDB_MULTI_TRX_BATCH_COMMIT) {
          myduck::log_duckdb_multi_trx_batch_commit("non-Row Format");
        }
        return true;
      }
  }

  return false;
}

int DuckdbThdContext::implicit_commit_batch() {
  auto saved_event_sql_state = event_seq_state;
  auto saved_gtid_next = m_thd->variables.gtid_next;
  auto saved_owned_gtid = m_thd->owned_gtid;
  mysql::gtid::Tsid saved_owned_sid;
  saved_owned_sid = m_thd->owned_tsid;

  if (m_thd->variables.gtid_next.type == ASSIGNED_GTID)
    m_thd->variables.gtid_next.set_undefined();
  m_thd->owned_gtid.clear();
  m_thd->owned_tsid.clear();

  int ret = commit_if_possible();

  m_thd->variables.gtid_next = saved_gtid_next;
  m_thd->owned_gtid = saved_owned_gtid;
  m_thd->owned_tsid = saved_owned_sid;

  /* todo: move out of implicit_commit_batch */
  if (saved_event_sql_state == DuckdbThdContext::GTID_BEGIN) {
    trans_begin(m_thd);
    event_seq_state = DuckdbThdContext::INITIAL;
  }

  return ret;
}

int DuckdbThdContext::commit_partial_batch() {
  if (!batch_multi_trx_started || !m_appenders) return 0;

  if (myduck::duckdb_log_options & LOG_DUCKDB_MULTI_TRX_BATCH_COMMIT) {
    myduck::log_duckdb_multi_trx_batch_commit("rollback");
  }

  /* Rollback last partial Trx */
  int ret = m_appenders->rollback_trx(cur_trx_no);
  if (ret) return ret;

  /* Commit the complete Trx in the batch */
  cur_batch_could_be_committed = true;
  ret = implicit_commit_batch();

  /* Release last owned_gtid if needed */
  gtid_state->update_on_rollback(m_thd);

  return ret;
}

bool DuckdbThdContext::duckdb_trans_commit(std::string &error_msg) {
  assert(m_con);

  DBUG_EXECUTE_IF("simulate_duckdb_commit_failed", {
    error_msg = "DuckDB COMMIT failed.";
    return true;
  });

  DBUG_EXECUTE_IF("debug_sync_when_duckdb_commit", {
    const char act[] = "now SIGNAL commit_signal WAIT_FOR resume_signal";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  };);

  if (flush_appenders(error_msg)) {
    return true;
  }

  set_in_copy_ddl(false);

  if (m_con->HasActiveTransaction()) {
    auto result = duckdb_query(*m_con, "COMMIT");
    if (result->HasError()) {
      error_msg = result->GetError().c_str();
      return true;
    }
  }
  return false;
}

bool DuckdbThdContext::duckdb_trans_rollback(std::string &error_msg) {
  assert(m_con);
  int res = false;

  update_on_rollback();

  if (m_con->HasActiveTransaction()) {
    auto result = duckdb_query(*m_con, "ROLLBACK");
    if (result->HasError()) {
      error_msg = result->GetError().c_str();
      return true;
    }
  }
  return res;
}

void DuckdbThdContext::interrupt() { m_con->Interrupt(); }
}  // namespace myduck
