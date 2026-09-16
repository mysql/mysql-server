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

#include "mysql/scheduler/task_schedule.h"

#include <cassert>
#include <mutex>

namespace mysql::scheduler {

Task_schedule::Task_schedule() {}

bool Task_schedule::has_higher_priority(const Task_schedule &arg,
                                        int phase) const {
  // assert that two schedules work according to the same clock
  assert(get_phase_clock(phase) == arg.get_phase_clock(phase));
  if (this->get_phase_delay(phase) == arg.get_phase_delay(phase)) {
    return get_id() < arg.get_id();
  }
  return (this->get_phase_delay(phase) < arg.get_phase_delay(phase));
}

}  // namespace mysql::scheduler
