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

#include "mysql/scheduler/clock_lwm_registry.h"
#include <limits>

namespace mysql::scheduler {

Clock_lwm_registry::Time_point_t Clock_lwm_registry::now() const {
  return m_current_lwm.load();
}

Clock_lwm_registry::Time_point_t Clock_lwm_registry::start_time() const {
  return 0;
}

bool Clock_lwm_registry::add_time(Task_id task_id, Time_point_t) {
  return m_executed_registry.activate(task_id.get(), Task_state{true, false});
}

// When task detects that it updates LWM, it is responsible
// for iterating over the next tasks and trying to update the state.
// This "victim" task iterates as long as update of the LWM succeeds.
// We keep the accurate value of the LWM at all times. Once LWM passes
// a task ID value, we may deactivate the task.
bool Clock_lwm_registry::tick(Task_id task_id, Time_point_t) {
  auto bare_id = task_id.get();
  bool updated = false;
  bool time_changed = false;

  // Detect wrap-around: if task_id < current LWM and LWM near overflow
  uint64_t current_lwm = m_current_lwm.load();
  if (bare_id < current_lwm &&
      current_lwm > (std::numeric_limits<uint64_t>::max() / 2)) {
    // Reset LWM to 0 for new epoch post-wrap
    uint64_t old = current_lwm;
    m_current_lwm.compare_exchange_strong(old, 0ULL);
    // Note: Existing registry entries for old epoch tasks will be ignored
    // post-reset as LWM starts from 0; in production, consider generation
    // counter for cleanup
  }

  [[maybe_unused]] bool finished_task = m_executed_registry.apply(
      task_id, [bare_id, &updated, &time_changed, this](Task_state &state) {
        state.finished = true;
        uint64_t id = bare_id;
        uint64_t next_id = (bare_id == std::numeric_limits<uint64_t>::max())
                               ? 0ULL
                               : bare_id + 1;
        updated = m_current_lwm.compare_exchange_strong(id, next_id);
        time_changed = updated || time_changed;
      });
  assert(finished_task);
  if (updated) {
    [[maybe_unused]] auto removed = m_executed_registry.deactivate(bare_id);
    assert(removed);
  }
  bool applied = true;
  while (updated && applied) {
    bare_id =
        (bare_id == std::numeric_limits<uint64_t>::max()) ? 0ULL : bare_id + 1;
    updated = false;  // Reset for next iteration
    if (m_executed_registry.bucket_active(Task_id(bare_id))) {
      applied = m_executed_registry.apply(
          bare_id, [bare_id, &updated, &time_changed, this](Task_state &state) {
            uint64_t id = bare_id;
            uint64_t next_id = (bare_id == std::numeric_limits<uint64_t>::max())
                                   ? 0ULL
                                   : bare_id + 1;
            if (state.finished) {
              updated = m_current_lwm.compare_exchange_strong(id, next_id);
              time_changed = updated || time_changed;
            }
          });
    }
    if (updated) {
      [[maybe_unused]] auto removed = m_executed_registry.deactivate(bare_id);
      assert(removed);
    }
  }
  return time_changed;
}

void Clock_lwm_registry::test_set_current_lwm(Time_point_t lwm) {
  m_current_lwm.store(lwm);
}

}  // namespace mysql::scheduler
