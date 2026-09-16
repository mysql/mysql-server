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
#ifndef BINLOG_TC_LOG_PROCESSING_INCLUDED
#define BINLOG_TC_LOG_PROCESSING_INCLUDED

#include "my_inttypes.h"

class MYSQL_BIN_LOG;
class THD;

/**
 *  An interface to delegate responsibility from MYSQL_BIN_LOG class.
 *  Specific implementation is injected into an instance of MYSQL_BIN_LOG class
 *  using `set_tc_log_processing` method. See Binlog_tc_log class for
 *  specific generic implementation of the below interface.
 */
class Binlog_tc_log_processing {
 public:
  Binlog_tc_log_processing() = default;
  virtual ~Binlog_tc_log_processing() = default;

  /**
   * Prepare the transaction in the transaction coordinator.
   *
   * @param binlog An instance of MYSQL_BIN_LOG upon which the prepare method
   *               executes
   * @param thd The THD session object holding the transaction to be prepared.
   * @param all Whether or not the prepare regards a full transaction or the
   *            statement being executed..
   * @retval 0  success
   * @retval 1  error
   */
  [[nodiscard]] virtual int prepare(MYSQL_BIN_LOG *binlog, THD *thd,
                                    bool all) = 0;

  /**
   *  Fetch and empty BINLOG_FLUSH_STAGE and COMMIT_ORDER_FLUSH_STAGE flush
   *  queues and flush transactions to the disk, and unblock threads executing
   *  slave preserve commit order.
   *
   *  @param[in] check_and_skip_flush_logs
   *               if false then flush prepared records of transactions to the
   *  log of storage engine. if true then flush prepared records of transactions
   *  to the log of storage engine only if COMMIT_ORDER_FLUSH_STAGE queue is
   *               non-empty.
   *  @return Pointer to the first session of the BINLOG_FLUSH_STAGE stage
   * queue.
   */
  [[nodiscard]] virtual THD *fetch_and_process_flush_stage_queue(
      bool check_and_skip_flush_logs) = 0;

  /**
   * Execute the flush stage.
   *
   * @param binlog An instance of MYSQL_BIN_LOG upon which the
   *               process_flush_stage_queue method executes
   * @param[out] total_bytes_var Pointer to variable that will be set to total
   *                             number of bytes flushed, or NULL.
   *
   * @param[out] out_queue_var  Pointer to the sessions queue in flush stage.
   * @return Error code on error, zero on success
   */
  [[nodiscard]] virtual int process_flush_stage_queue(MYSQL_BIN_LOG *binlog,
                                                      my_off_t *total_bytes_var,
                                                      THD **out_queue_var) = 0;

  /**
   * Rolls back the underlying transaction in storage engines.
   * Determines if the transaction to rollback is attached to the `thd`
   * parameter or, instead, the `thd` parameter holds the XID for a detached
   * transaction to be rolled back.
   *
   * @param thd   THD session object.
   * @param all   Is set in case of explicit commit (COMMIT statement), or
   *              implicit commit issued by DDL. Is not set when called at the
   *              end of statement, even if autocommit=1.
   * @return false if the transaction was rolled back, true if an error
   *         occurred.
   */
  [[nodiscard]] virtual bool rollback_in_engines(THD *thd, bool all) = 0;

  /**
   *  Finishes the transaction in the engines. If the `commit_low` flag is set,
   *  will commit in the engines, otherwise, if the underlying statement is an
   *  `XA ROLLBACK`, it will rollback in the engines.
   *
   * @param thd The THD session object holding the transaction to finalize.
   * @param all Finalizing a transaction (i.e. true) or a statement
   *           (i.e. false).
   * @param run_after_commit In the case of a commit being issued, whether or
   *                         not to run the `after_commit` hook.
   */
  virtual void finish_transaction_in_engines(THD *thd, bool all,
                                             bool run_after_commit) = 0;

  /**
   * Register binlog observer
   *
   * @retval true It was possible to register binlog observer
   * @retval false It was not possible to register binlog observer
   */
  [[nodiscard]] virtual bool binlog_register_observer() = 0;

  /**
   * Unregister binlog observer
   */
  virtual void binlog_unregister_observer() = 0;

  /**
   * Is persistence enabled
   *
   * @retval true Persistence is enabled
   * @retval false Persistence is disabled
   */
  [[nodiscard]] virtual bool is_persistence_enabled() = 0;
};

#endif /* BINLOG_TC_LOG_PROCESSING_INCLUDED */
