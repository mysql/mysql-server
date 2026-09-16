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

#ifndef MYSQL_SCHEDULER_SCHEDULE_FACTORY_H
#define MYSQL_SCHEDULER_SCHEDULE_FACTORY_H

#include "mysql/scheduler/scheduler_clock.h"
#include "mysql/scheduler/task_schedule.h"
#include "mysql/scheduler/time.h"

namespace mysql::scheduler {

/// @class Schedule_factory
/// @brief Schedule factory class - creates task schedule object pointer based
/// on input parameters
/// @details Creates tasks starting from arbitrary point of time (steady_clock
/// point of time)
class Schedule_factory {
 public:
  /// @brief Constructor
  /// @param clock_ptr Clock based on which schedules will be created
  Schedule_factory(Scheduler_clock_ptr clock_ptr);

  /// @param clock_ptr Clock based on which schedules will be created
  /// @param phase_clock Clock based on which schedule for the second task
  /// phase will be created (to support Transaction_order_schedule)
  Schedule_factory(Scheduler_clock_ptr clock_ptr,
                   Scheduler_clock_ptr phase_clock);

  /// @brief create
  /// @details Creates one-shot task schedule
  Task_schedule_ptr create(Task_id task_id, uint64_t delay) const;

  /// @brief clock disabled
  /// @details Creates one-shot task schedule
  Task_schedule_ptr create(Task_id task_id) const;

  /// @details Creates transaction task schedule
  Task_schedule_ptr create(Task_id task_id, uint64_t delay, bool is_trx);

  /// @brief Destructor
  virtual ~Schedule_factory() = default;

 private:
  Scheduler_clock_ptr m_clock;
  Scheduler_clock_ptr m_phase_clock;
  uint64_t m_phase_delay{0};
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_SCHEDULE_FACTORY_H
