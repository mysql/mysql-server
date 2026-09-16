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

#ifndef MYSQL_SCHEDULER_TRANSACTION_ORDER_SCHEDULE_H
#define MYSQL_SCHEDULER_TRANSACTION_ORDER_SCHEDULE_H

#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include "mysql/scheduler/task_id.h"
#include "mysql/scheduler/task_schedule.h"
#include "mysql/scheduler/time.h"

namespace mysql::scheduler {

/// @class Transaction_order_schedule
/// @brief Represents schedule for a transaction executing in two phases:
/// apply and committing, where commit must follow the commit order
class Transaction_order_schedule : public Task_schedule {
 public:
  enum class Phase { prepare = 0, commit = 1 };
  /// @brief Constructor
  /// @details constructs schedule for tranctions executing commit order
  /// @param task_id Task identifier
  /// @param trx_clock Clock transaction schedule is based on
  /// @param trx_time Delay w.r.t. trx_clock start point
  /// @param commit_clock Clock transaction commit schedule is based on
  /// @param commit_time Delay w.r.t. trx_clock start point
  Transaction_order_schedule(Task_id task_id, Scheduler_clock_ptr trx_clock,
                             uint64_t trx_time,
                             Scheduler_clock_ptr commit_clock,
                             uint64_t commit_time);

  /// @brief This function modifies the state of the schedule to next state
  /// @returns false - Task is one-shot task, won't be executed in the future
  bool next() override;

  /// @brief Gets information about next execution time
  /// @returns Current phase task delay
  const Time_delay_type &get_task_delay() const override;

  bool is_finished() const override;

  /// @brief Obtain task clock
  const Scheduler_clock_ptr &get_clock() const override;

  /// @brief Obtain task id
  const Task_id &get_id() const override;

  /// @brief Gets information about execution time in specific phase
  /// @param phase_id Selected phase sequence number
  const Time_delay_type &get_phase_delay(unsigned int phase_id) const override;

  /// @brief Obtain task clock for the given phase id
  /// @param phase_id Selected phase sequence number
  const Scheduler_clock_ptr &get_phase_clock(
      unsigned int phase_id) const override;

  /// @brief Returns current phase id (sequence number)
  /// @return Phase sequence number
  unsigned int get_phase_id() const override;

 protected:
 private:
  /// This task id
  Task_id m_task_id;
  /// Clock whole task schedule is based on
  Scheduler_clock_ptr m_trx_clock;
  /// Transaction start execution time, relative to the m_trx_clock
  uint64_t m_trx_time;
  /// Commit clock, dependency clock for commit phase
  Scheduler_clock_ptr m_commit_clock;
  /// Transaction commit start time, relative to the commit clock
  uint64_t m_commit_time;
  /// Current schedule phase
  unsigned int m_phase{0};
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_TRANSACTION_ORDER_SCHEDULE_H
