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

#pragma once

#include "sql/duckdb/duckdb_charset_collation.h"
#include "sql/duckdb/duckdb_manager.h"
#include "sql/duckdb/duckdb_query.h"
#include "sql/duckdb/duckdb_timezone.h"
#include "sql/log_event.h"
#include "sql/sql_class.h"
#include "storage/duckdb/delta_appender.h"

class THD;
class DeltaAppender;

namespace myduck {

enum BatchState {
  UNDEFINED = 0,
  NOT_IN_BATCH,
  IN_INSERT_ONLY_BATCH,
  IN_MIX_BATCH
};

inline bool operator==(const timeval &lhs, const timeval &rhs) {
  return lhs.tv_sec == rhs.tv_sec && lhs.tv_usec == rhs.tv_usec;
}

inline bool operator!=(const timeval &lhs, const timeval &rhs) {
  return lhs.tv_sec != rhs.tv_sec || lhs.tv_usec != rhs.tv_usec;
}

class DuckdbThdContext {
 public:
  DuckdbThdContext(THD *thd)
      : m_thd(thd),
        in_copy_ddl(false),
        batch_state(UNDEFINED),
        batch_gtid_set(global_tsid_map) {
    m_con = DuckdbManager::CreateConnection();
  }

  ~DuckdbThdContext() {
    if (has_transaction()) {
      std::string error_msg;
      duckdb_trans_rollback(error_msg);
    }
  }

  enum EventSeqState { INITIAL = 0, GTID, GTID_BEGIN };

  bool has_transaction() {
    assert(m_con);
    return m_con->HasActiveTransaction();
  }

  bool duckdb_trans_begin() {
    assert(m_con && !m_con->HasActiveTransaction());

    auto result = duckdb_query(*m_con, "BEGIN");
    if (result->HasError()) {
      return true;
    }
    return false;
  }

  void reset_batch();

  void add_gtid_to_batch_set();

  bool duckdb_delay_commit(Log_event *ev);

  bool multi_trx_in_batch() { return batch_multi_trx_started; }

  int save_batch_gtid_set();

  void prepare_gtids_for_binlog_commit();

  void add_gtid_set(Gtid_set *set);

  void update_on_commit();

  void update_on_rollback();

  int commit_if_possible();

  bool need_implicit_commit_batch(Log_event *ev);

  int implicit_commit_batch();

  int commit_partial_batch();

  bool duckdb_trans_commit(std::string &error_msg);

  bool duckdb_trans_rollback(std::string &error_msg);

  void interrupt();

  class DuckdbSessionConfig {
   public:
    DuckdbSessionConfig()
        : m_database(),
          m_timezone(),
          m_collation(),
          /*
           * the explain output of duckdb is default set to PHYSICAL_ONLY,
           * if it is changed, this also needs changed.
           * */
          m_explain_output_str() {}

    /**
      Compares the current session configuration with the given THD object's
      settings and applies necessary changes to the DuckDB session.

      @param thd THD
      @param connection DuckDB connection

      @return QueryResult of the last executed SQL command.
    */
    std::unique_ptr<duckdb::QueryResult> compare_and_config(
        THD *thd, duckdb::Connection &connection);

    template <typename T>
    class Config {
     private:
      T m_value;
      bool m_valid;

     public:
      Config() : m_value(), m_valid(false) {}

      friend bool operator==(const T &lhs, const Config &rhs) {
        if (!rhs.m_valid) return false;
        return lhs == rhs.m_value;
      }

      friend bool operator==(const Config &lhs, const T &rhs) {
        if (!lhs.m_valid) return false;
        return lhs.m_value == rhs;
      }

      friend bool operator!=(const T &lhs, const Config &rhs) {
        if (!rhs.m_valid) return true;
        return lhs != rhs.m_value;
      }

      friend bool operator!=(const Config &lhs, const T &rhs) {
        if (!lhs.m_valid) return true;
        return lhs.m_value != rhs;
      }

      void Set(const T &lhs) {
        m_value = lhs;
        m_valid = true;
      }

      void SetInvalid() { m_valid = false; }
    };

    void reset_cached_config() {
      m_database.SetInvalid();
      m_timezone.SetInvalid();
      m_collation.SetInvalid();
      m_explain_output_str.SetInvalid();
      m_force_no_collation.SetInvalid();
      m_user_time.SetInvalid();
      m_sql_mode.SetInvalid();
      m_disabled_optimizers.SetInvalid();
      m_merge_join_threshold.SetInvalid();
      m_query_max_threads.SetInvalid();
    }

   private:
    Config<std::string> m_database;
    Config<std::string> m_timezone;
    Config<std::string> m_collation;
    Config<std::string> m_explain_output_str;
    Config<bool> m_force_no_collation;
    Config<timeval> m_user_time;
    Config<ulong> m_default_week_format;
    Config<ulonglong> m_sql_mode;
    Config<ulonglong> m_disabled_optimizers;
    Config<ulonglong> m_merge_join_threshold;
    Config<ulonglong> m_query_max_threads;
    Config<bool> m_prefer_high_precision;
  };

 public:
  duckdb::Connection &get_connection() { return *m_con; }

  /** Appender related functions */
  bool flush_appenders(std::string &error_msg) {
    if (m_appenders && !m_appenders->is_empty()) {
      if (m_appenders->flush_all(error_msg)) {
        return true;
      }
    }
    set_batch_state(UNDEFINED);
    return false;
  }

  DeltaAppender *get_appender(TABLE *table);

  void delete_appender(std::string &db, std::string &tb) {
    if (!m_appenders || m_appenders->is_empty()) {
      return;
    }
    m_appenders->delete_appender(db, tb);
  }

  int append_row_insert(TABLE *table, const MY_BITMAP *blob_map);

  int append_row_update(TABLE *table, const uchar *old_row);

  int append_row_delete(TABLE *table);

  void set_in_copy_ddl(bool in) { in_copy_ddl = in; }
  bool is_in_copy_ddl() { return in_copy_ddl; }

  void set_batch_state(BatchState state) { batch_state = state; }
  BatchState get_batch_state() { return batch_state; }

  bool get_idempotent_flag();

  std::unique_ptr<duckdb::QueryResult> config_duckdb_env(
      THD *thd, duckdb::Connection &connection) {
    return m_session_env.compare_and_config(thd, connection);
  }

 private:
  std::shared_ptr<duckdb::Connection> m_con;

  THD *m_thd;

  DuckdbSessionConfig m_session_env;

  std::unique_ptr<DeltaAppenders> m_appenders;

  bool in_copy_ddl;

  BatchState batch_state;

  bool batch_multi_trx_started = false;

  bool cur_batch_could_be_committed = false;

  ulonglong cur_trx_no = 0;

  ulonglong cur_batch_length = 0;

  ulonglong batch_start_time = 0;

  Gtid_set batch_gtid_set;

  ulonglong gtid_compression_counter = 0;

  my_off_t xid_event_master_log_pos;

  my_off_t xid_event_relay_log_pos;

  char xid_event_relay_log_name[FN_REFLEN];

  my_off_t xid_future_event_relay_log_pos;

  EventSeqState event_seq_state;
};
}  // namespace myduck
