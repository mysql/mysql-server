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
#ifndef BINLOG_TC_LOG_INCLUDED
#define BINLOG_TC_LOG_INCLUDED

#include "sql/binlog/binlog_tc_log_processing.h"

/**
 * @see Binlog_tc_log_processing for class and its methods documentation
 */
class Binlog_tc_log : public Binlog_tc_log_processing {
 public:
  Binlog_tc_log();
  virtual ~Binlog_tc_log() override;

  [[nodiscard]] virtual int prepare(MYSQL_BIN_LOG *binlog, THD *thd,
                                    bool all) override;
  [[nodiscard]] virtual THD *fetch_and_process_flush_stage_queue(
      bool check_and_skip_flush_logs) override;
  [[nodiscard]] virtual int process_flush_stage_queue(
      MYSQL_BIN_LOG *binlog, my_off_t *total_bytes_var,
      THD **out_queue_var) override;
  [[nodiscard]] virtual bool rollback_in_engines(THD *thd, bool all) override;
  virtual void finish_transaction_in_engines(THD *thd, bool all,
                                             bool run_after_commit) override;

  [[nodiscard]] virtual bool binlog_register_observer() override;
  virtual void binlog_unregister_observer() override;
  [[nodiscard]] virtual bool is_persistence_enabled() override;
};

#endif /* BINLOG_TC_LOG_INCLUDED */
