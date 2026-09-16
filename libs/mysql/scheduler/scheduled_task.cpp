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

#include "mysql/scheduler/scheduled_task.h"

#include <mutex>

namespace mysql::scheduler {

Repeatable_task_state::Repeatable_task_state(
    std::atomic<std::size_t> &scheduled_tasks_cnt,
    Repeatable_task_type &&repeatable_task)
    : m_scheduled_tasks_cnt(scheduled_tasks_cnt),
      m_repeatable_task(std::move(repeatable_task)) {}

Repeatable_task_state::~Repeatable_task_state() { --m_scheduled_tasks_cnt; }

Scheduled_task::Scheduled_task(const Task_id &id,
                               Scheduled_task::Func_type &&task,
                               const Repeatable_task_state_ptr &repeatable_task,
                               const Task_schedule_ptr &schedule,
                               unsigned int phase)
    : m_task_id(id),
      m_task(std::move(task)),
      m_repeatable_task(repeatable_task),
      m_schedule(schedule),
      m_phase(phase) {}

bool Scheduled_task::operator<(const Scheduled_task &arg) const {
  return !(m_schedule->has_higher_priority(*(arg.m_schedule), m_phase));
}

}  // namespace mysql::scheduler
