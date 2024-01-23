/* Copyright (c) 2006, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/rpl_injector.h"

#include "sql/binlog.h"  // mysql_bin_log
#include "sql/mdl.h"
#include "sql/rpl_write_set_handler.h"  // add_pke
#include "sql/sql_base.h"               // close_thread_tables
#include "sql/sql_class.h"              // THD
#include "sql/transaction.h"            // trans_begin

/*
  injector::transaction - member definitions
*/

injector::transaction::transaction(THD *thd, bool calc_writeset_hash)
    : m_state(START_STATE),
      m_thd(thd),
      m_calc_writeset_hash(calc_writeset_hash) {
  // Remember position where transaction started
  Log_info log_info;
  mysql_bin_log.get_current_log(&log_info);
  strmake(m_start_name_buf, log_info.log_file_name,
          sizeof(m_start_name_buf) - 1);
  m_start_pos.m_file_name = m_start_name_buf;
  m_start_pos.m_file_pos = log_info.pos;

  /*
     Next pos is unknown until after commit of the Binlog transaction
  */
  m_next_pos.m_file_name = nullptr;
  m_next_pos.m_file_pos = 0;

  /*
    Ensure we don't pick up this thd's last written Binlog pos in
    empty-transaction-commit cases.
    This is not ideal, as it zaps this information for any other
    usage (e.g. WL4047)
    Potential improvement : save the 'old' next pos prior to
    commit, and restore on error.
  */
  m_thd->clear_next_event_pos();

  trans_begin(m_thd);
}

/**
   @retval 0 transaction committed
   @retval 1 transaction rolled back
 */
int injector::transaction::commit() {
  DBUG_TRACE;
  const int error = m_thd->binlog_flush_pending_rows_event(true);
  /*
    Cluster replication does not preserve statement or
    transaction boundaries of the master.  Instead, a new
    transaction on replication slave is started when a new GCI
    (global checkpoint identifier) is issued, and is committed
    when the last event of the check point has been received and
    processed. This ensures consistency of each cluster in
    cluster replication, and there is no requirement for stronger
    consistency: MySQL replication is asynchronous with other
    engines as well.

    A practical consequence of that is that row level replication
    stream passed through the injector thread never contains
    COMMIT events.
    Here we should preserve the server invariant that there is no
    outstanding statement transaction when the normal transaction
    is committed by committing the statement transaction
    explicitly.
  */
  trans_commit_stmt(m_thd);
  if (!trans_commit(m_thd)) {
    close_thread_tables(m_thd);
    m_thd->mdl_context.release_transactional_locks();
  }

  /* Copy next position out into our next pos member */
  if (error == 0 && m_thd->binlog_next_event_pos.file_name != nullptr) {
    strmake(m_end_name_buf, m_thd->binlog_next_event_pos.file_name,
            sizeof(m_end_name_buf) - 1);
    m_next_pos.m_file_name = m_end_name_buf;
    m_next_pos.m_file_pos = m_thd->binlog_next_event_pos.pos;
  } else {
    /* Error, problem copying etc. */
    m_next_pos.m_file_name = nullptr;
    m_next_pos.m_file_pos = 0;
  }

  return error;
}

int injector::transaction::rollback() {
  DBUG_TRACE;
  trans_rollback_stmt(m_thd);
  if (!trans_rollback(m_thd)) {
    close_thread_tables(m_thd);
    if (!m_thd->locked_tables_mode)
      m_thd->mdl_context.release_transactional_locks();
  }
  return 0;
}

// Utility class for changing THD::server_id in a limited scope
class Change_server_id_scope {
  THD *const m_thd;
  const uint32 m_save_id;

 public:
  Change_server_id_scope(THD *thd, int32 new_server_id)
      : m_thd(thd), m_save_id(thd->server_id) {
    m_thd->set_server_id(new_server_id);
  }
  ~Change_server_id_scope() { m_thd->set_server_id(m_save_id); }
};

int injector::transaction::use_table(server_id_type sid, table tbl) {
  DBUG_TRACE;

  int error;

  if ((error = check_state(TABLE_STATE))) return error;

  Change_server_id_scope save_id(m_thd, sid);
  return m_thd->binlog_write_table_map(tbl.get_table(), tbl.is_transactional(),
                                       false);
}

int injector::transaction::write_row(server_id_type sid, table tbl,
                                     MY_BITMAP const *cols, record_type record,
                                     const unsigned char *extra_row_info) {
  DBUG_TRACE;

  int error = check_state(ROW_STATE);
  if (error) return error;

  Change_server_id_scope save_id(m_thd, sid);
  table::save_sets saveset(tbl, cols, cols);

  if (m_calc_writeset_hash && !tbl.skip_hash()) {
    try {
      if (add_pke(tbl.get_table(), m_thd, record)) {
        return HA_ERR_RBR_LOGGING_FAILED;
      }
    } catch (const std::bad_alloc &) {
      return HA_ERR_RBR_LOGGING_FAILED;
    }
  }

  return m_thd->binlog_write_row(tbl.get_table(), tbl.is_transactional(),
                                 record, extra_row_info);
}

int injector::transaction::delete_row(server_id_type sid, table tbl,
                                      MY_BITMAP const *cols, record_type record,
                                      const unsigned char *extra_row_info) {
  DBUG_TRACE;

  int error = check_state(ROW_STATE);
  if (error) return error;

  Change_server_id_scope save_id(m_thd, sid);
  table::save_sets saveset(tbl, cols, cols);

  if (m_calc_writeset_hash && !tbl.skip_hash()) {
    try {
      if (add_pke(tbl.get_table(), m_thd, record)) {
        return HA_ERR_RBR_LOGGING_FAILED;
      }
    } catch (const std::bad_alloc &) {
      return HA_ERR_RBR_LOGGING_FAILED;
    }
  }

  return m_thd->binlog_delete_row(tbl.get_table(), tbl.is_transactional(),
                                  record, extra_row_info);
}

int injector::transaction::update_row(server_id_type sid, table tbl,
                                      MY_BITMAP const *before_cols,
                                      MY_BITMAP const *after_cols,
                                      record_type before, record_type after,
                                      const unsigned char *extra_row_info) {
  DBUG_TRACE;

  int error = check_state(ROW_STATE);
  if (error) return error;

  Change_server_id_scope save_id(m_thd, sid);
  table::save_sets saveset(tbl, before_cols, after_cols);

  if (m_calc_writeset_hash && !tbl.skip_hash()) {
    try {
      if (add_pke(tbl.get_table(), m_thd, before)) {
        return HA_ERR_RBR_LOGGING_FAILED;
      }
      if (add_pke(tbl.get_table(), m_thd, after)) {
        return HA_ERR_RBR_LOGGING_FAILED;
      }
    } catch (const std::bad_alloc &) {
      return HA_ERR_RBR_LOGGING_FAILED;
    }
  }

  return m_thd->binlog_update_row(tbl.get_table(), tbl.is_transactional(),
                                  before, after, extra_row_info);
}

injector::transaction::binlog_pos injector::transaction::start_pos() const {
  return m_start_pos;
}

injector::transaction::binlog_pos injector::transaction::next_pos() const {
  return m_next_pos;
}

/*
  injector - member definitions
*/

/* This constructor is called below */
inline injector::injector() = default;

static injector *s_injector = nullptr;
injector *injector::instance() {
  if (s_injector == nullptr) s_injector = new injector;
  /* "There can be only one [instance]" */
  return s_injector;
}

void injector::free_instance() {
  injector *inj = s_injector;

  if (inj != nullptr) {
    s_injector = nullptr;
    delete inj;
  }
}

int injector::record_incident(THD *thd, std::string_view message) {
  return mysql_bin_log.write_incident_commit(thd, message);
}
