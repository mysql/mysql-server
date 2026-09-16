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

#include "mysql/scheduler/transaction_order_schedule.h"

#include <mutex>

namespace mysql::scheduler {

// microseconds rate, microseconds delay
Transaction_order_schedule::Transaction_order_schedule(
    Task_id task_id, Scheduler_clock_ptr trx_clock, uint64_t trx_time,
    Scheduler_clock_ptr commit_clock, uint64_t commit_time)
    : m_task_id(task_id),
      m_trx_clock(trx_clock),
      m_trx_time(trx_time),
      m_commit_clock(commit_clock),
      m_commit_time(commit_time) {}

bool Transaction_order_schedule::is_finished() const {
  if (m_phase == 0) {
    return false;
  }
  return true;
}

bool Transaction_order_schedule::next() {
  if (m_phase == 0) {
    m_phase = 1;
    return true;
  }
  return false;
}

const Time_delay_type &Transaction_order_schedule::get_task_delay() const {
  return get_phase_delay(m_phase);
}

const Scheduler_clock_ptr &Transaction_order_schedule::get_clock() const {
  return get_phase_clock(m_phase);
}

const Task_id &Transaction_order_schedule::get_id() const { return m_task_id; }

const Time_delay_type &Transaction_order_schedule::get_phase_delay(
    unsigned int phase_id) const {
  if (phase_id == 0) {
    return m_trx_time;
  }
  return m_commit_time;
}

const Scheduler_clock_ptr &Transaction_order_schedule::get_phase_clock(
    unsigned int phase_id) const {
  if (phase_id == 0) {
    return m_trx_clock;
  }
  return m_commit_clock;
}

unsigned int Transaction_order_schedule::get_phase_id() const {
  return m_phase;
}

}  // namespace mysql::scheduler
