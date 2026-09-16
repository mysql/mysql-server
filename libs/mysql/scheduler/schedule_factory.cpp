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

#include "mysql/scheduler/schedule_factory.h"
#include "mysql/scheduler/delayed_schedule.h"
#include "mysql/scheduler/transaction_order_schedule.h"

#include <cassert>
#include <mutex>

namespace mysql::scheduler {

Schedule_factory::Schedule_factory(Scheduler_clock_ptr clock)
    : m_clock(clock) {}

Schedule_factory::Schedule_factory(Scheduler_clock_ptr clock_ptr,
                                   Scheduler_clock_ptr phase_clock)
    : m_clock(clock_ptr), m_phase_clock(phase_clock) {}

Task_schedule_ptr Schedule_factory::create(Task_id task_id,
                                           uint64_t delay) const {
  return Task_schedule_ptr(
      new Delayed_schedule(task_id, m_clock, m_clock->start_time() + delay));
}

Task_schedule_ptr Schedule_factory::create(Task_id task_id) const {
  return Task_schedule_ptr(new Delayed_schedule(task_id, m_clock, 0));
}

Task_schedule_ptr Schedule_factory::create(Task_id task_id, uint64_t delay,
                                           bool is_trx) {
  if (is_trx) {
    assert(m_phase_clock);
    return Task_schedule_ptr(new Transaction_order_schedule(
        task_id, m_clock, m_clock->start_time() + delay, m_phase_clock,
        m_phase_delay++));
  }
  return Task_schedule_ptr(
      new Delayed_schedule(task_id, m_clock, m_clock->start_time() + delay));
}

}  // namespace mysql::scheduler
