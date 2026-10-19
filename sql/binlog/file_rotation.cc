// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include "sql/binlog/file_rotation.h"

#include <cassert>

#include "my_dbug.h"
#include "sql/binlog.h"     // MYSQL_BIN_LOG
#include "sql/rpl_gtid.h"   // gtid_state
#include "sql/sql_class.h"  // THD

int Standard_file_rotation::save_gtids_of_last_binlog() {
  return gtid_state->save_gtids_of_last_binlog_into_table();
}

void Standard_file_rotation::rotate_dependency_tracker(MYSQL_BIN_LOG &log) {
  log.m_dependency_tracker.rotate();
}

void Standard_file_rotation::publish_end_position(MYSQL_BIN_LOG &log) {
  log.update_binlog_end_pos();
}

int Spilled_file_rotation::save_gtids_of_last_binlog() {
  THD *const large_trx_thd =
      /* The debug point raises its error on current_thd. */
      DBUG_EVALUATE_IF("gtid_executed_readonly", nullptr, current_thd);
  if (large_trx_thd != nullptr) current_thd = nullptr;
  const int error = gtid_state->save_gtids_of_last_binlog_into_table();
  if (large_trx_thd != nullptr) large_trx_thd->store_globals();
  return error;
}

void Spilled_file_rotation::record_gtid_persistence_fallback_reason() {
  fallback_reason = Large_trx_fallback_reason::kGtidPersistence;
}

bool Spilled_file_rotation::replace_new_file_with_spilled_file(
    MYSQL_BIN_LOG &log) {
  assert(!log.is_relay_log);
  return log.promote_spilled_file(this);
}

void Spilled_file_rotation::rotate_dependency_tracker(MYSQL_BIN_LOG &log) {
  if (!promoted) log.m_dependency_tracker.rotate();
}

void Spilled_file_rotation::publish_end_position(MYSQL_BIN_LOG &log) {
  DBUG_EXECUTE_IF("crash_bolt_after_purge_index_remove", {
    if (promoted) DBUG_SUICIDE();
  });
  if (!promoted) {
    log.update_binlog_end_pos();
    return;
  }
  /*
    The promoted file holds the transaction already, but dump threads must
    not read the transaction before the after_flush hook. The flush stage
    publishes the transaction then, as for any transaction.
  */
  log.lock_binlog_end_pos();
  log.atomic_binlog_end_pos = header_end;
  log.signal_update();
  log.unlock_binlog_end_pos();
}
