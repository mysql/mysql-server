/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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
#include "binlog_tc_log.h"
#include "my_dbug.h"
#include "mysql/components/services/log_builtins.h"
#include "sql/binlog.h"
#include "sql/binlog/binlog_ofile.h"
#include "sql/binlog/group_commit/bgc_ticket_manager.h"
#include "sql/binlog/thd_backup_and_restore.h"
#include "sql/clone_handler.h"
#include "sql/debug_sync.h"
#include "sql/derror.h"
#include "sql/mysqld.h"
#include "sql/rpl_commit_stage_manager.h"
#include "sql/rpl_handler.h"
#include "sql/rpl_replica_commit_order_manager.h"
#include "sql/sql_class.h"
#include "sql/xa.h"

Binlog_tc_log::Binlog_tc_log() {}

Binlog_tc_log::~Binlog_tc_log() {}

int Binlog_tc_log::prepare(MYSQL_BIN_LOG *binlog, THD *thd, bool all) {
  DBUG_TRACE;

  assert(opt_bin_log);

  /*
    Set HA_IGNORE_DURABILITY to not flush the prepared record of the
    transaction to the log of storage engine (for example, InnoDB
    redo log) during the prepare phase. So that we can flush prepared
    records of transactions to the log of storage engine in a group
    right before flushing them to binary log during binlog group
    commit flush stage. Reset to HA_REGULAR_DURABILITY at the
    beginning of parsing next command.
  */
  thd->durability_property = HA_IGNORE_DURABILITY;

  CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP("before_prepare_in_engines");

  int error = ha_prepare_low(thd, all);

  CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP("after_ha_prepare_low");
  // Invoke `commit` if we're dealing with `XA PREPARE` in order to use BCG
  // to write the event to file.
  if (!error && all && is_xa_prepare(thd)) return binlog->commit(thd, true);

  return error;
}

THD *Binlog_tc_log::fetch_and_process_flush_stage_queue(
    const bool check_and_skip_flush_logs) {
  /*
    Fetch the entire flush queue and empty it, so that the next batch
    has a leader. We must do this before invoking ha_flush_logs(...)
    for guaranteeing to flush prepared records of transactions before
    flushing them to binary log, which is required by crash recovery.
  */
  Commit_stage_manager::get_instance().lock_queue(
      Commit_stage_manager::BINLOG_FLUSH_STAGE);

  THD *first_seen =
      Commit_stage_manager::get_instance().fetch_queue_skip_acquire_lock(
          Commit_stage_manager::BINLOG_FLUSH_STAGE);
  assert(first_seen != nullptr);

  THD *commit_order_thd =
      Commit_stage_manager::get_instance().fetch_queue_skip_acquire_lock(
          Commit_stage_manager::COMMIT_ORDER_FLUSH_STAGE);

  Commit_stage_manager::get_instance().unlock_queue(
      Commit_stage_manager::BINLOG_FLUSH_STAGE);

  if (!check_and_skip_flush_logs ||
      (check_and_skip_flush_logs && commit_order_thd != nullptr)) {
    /*
      We flush prepared records of transactions to the log of storage
      engine (for example, InnoDB redo log) in a group right before
      flushing them to binary log.
    */
    ha_flush_logs(true);
  }

  /*
    The transactions are flushed to the disk and so threads
    executing slave preserve commit order can be unblocked.
  */
  Commit_stage_manager::get_instance()
      .process_final_stage_for_ordered_commit_group(commit_order_thd);
  return first_seen;
}

int Binlog_tc_log::process_flush_stage_queue(MYSQL_BIN_LOG *binlog
                                             [[maybe_unused]],
                                             my_off_t *total_bytes_var
                                             [[maybe_unused]],
                                             THD **out_queue_var
                                             [[maybe_unused]]) {
  DBUG_TRACE;
#ifndef NDEBUG
  // number of flushes per group.
  int no_flushes = 0;
#endif
  assert(total_bytes_var && out_queue_var);
  my_off_t total_bytes = 0;
  int flush_error = 1;
  mysql_mutex_assert_owner(binlog->get_log_lock());

  THD *first_seen = fetch_and_process_flush_stage_queue(false);
  DBUG_EXECUTE_IF("crash_after_flush_engine_log", DBUG_SUICIDE(););
  CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP("before_write_binlog");

  binlog->assign_automatic_gtids_to_flush_group(first_seen);

  /* Flush thread caches to binary log. */
  for (THD *head = first_seen; head; head = head->next_to_commit) {
    Thd_backup_and_restore switch_thd(current_thd, head);

    const auto [error, flushed_bytes] = binlog->flush_thread_caches(head);
    total_bytes += flushed_bytes;
    if (flush_error == 1) flush_error = error;
#ifndef NDEBUG
    no_flushes++;
#endif
  }

  *out_queue_var = first_seen;
  *total_bytes_var = total_bytes;

  first_seen->rpl_thd_ctx.binlog_group_commit_ctx().set_max_size_exceeded(
      total_bytes > 0 &&
      (binlog->get_binlog_file()->get_real_file_size() >=
           (my_off_t)binlog->get_max_size() ||
       DBUG_EVALUATE_IF("simulate_max_binlog_size", true, false)));
#ifndef NDEBUG
  DBUG_PRINT("info", ("no_flushes:= %d", no_flushes));
#endif
  return flush_error;
}

bool Binlog_tc_log::rollback_in_engines(THD *thd, bool all) {
  return trx_coordinator::rollback_in_engines(thd, all);
}

void Binlog_tc_log::finish_transaction_in_engines(THD *thd, bool all,
                                                  bool run_after_commit) {
  if (thd->get_transaction()->m_flags.commit_low) {
    if (trx_coordinator::commit_in_engines(thd, all, run_after_commit))
      thd->commit_error = THD::CE_COMMIT_ERROR;
  } else if (is_xa_rollback(thd)) {
    if (trx_coordinator::rollback_in_engines(thd, all))
      thd->commit_error = THD::CE_COMMIT_ERROR;
  }
}

bool Binlog_tc_log::binlog_register_observer() { return true; }
void Binlog_tc_log::binlog_unregister_observer() {}

bool Binlog_tc_log::is_persistence_enabled() { return true; }
