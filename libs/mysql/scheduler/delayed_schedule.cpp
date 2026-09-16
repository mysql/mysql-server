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

#include "mysql/scheduler/delayed_schedule.h"

#include <mutex>

namespace mysql::scheduler {

// microseconds rate, microseconds delay
Delayed_schedule::Delayed_schedule(Task_id task_id, Scheduler_clock_ptr clock,
                                   uint64_t delay)
    : m_task_id(task_id), m_clock(clock), m_task_delay(delay) {}

bool Delayed_schedule::next() { return false; }

const Time_delay_type &Delayed_schedule::get_task_delay() const {
  return m_task_delay;
}

const Scheduler_clock_ptr &Delayed_schedule::get_clock() const {
  return m_clock;
}

const Task_id &Delayed_schedule::get_id() const { return m_task_id; }

}  // namespace mysql::scheduler
