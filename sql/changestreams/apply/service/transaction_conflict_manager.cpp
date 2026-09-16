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

#include "sql/changestreams/apply/service/transaction_conflict_manager.h"

using namespace mysql::binlog::event;

namespace mysql::csa {

void Transaction_conflict_manager::start() {
  m_is_stopped = false;
  m_scheduled_tasks = 0;
  m_thread = Thread_type(&Transaction_conflict_manager::run_thread, this);
}

void Transaction_conflict_manager::stop() {
  m_is_stopped = true;
  m_cache.notify_all();
  m_end.wait(false);
  m_thread.join();
}

bool Transaction_conflict_manager::is_stopped() const { return m_is_stopped; }

void Transaction_conflict_manager::run_thread() {
  auto stop_waiting = [this]() -> bool { return is_stopped(); };
  while (!stop_waiting() || m_scheduled_tasks > 0) {
    auto [task, validity] = m_cache.dequeue(stop_waiting);
    if (!validity) {
      assert(is_stopped());
      break;
    }
    task();
    --m_scheduled_tasks;
  }
  m_end = true;
  m_end.notify_one();
}

void Transaction_conflict_manager::enqueue(Rescue_task &&task) {
  ++m_scheduled_tasks;
  if (is_stopped()) {
    task.set_promise();
    --m_scheduled_tasks;
    return;
  }
  m_cache.enqueue(std::move(task));
}

Transaction_conflict_monitor::Instances_map
    Transaction_conflict_monitor::m_instances;

std::atomic<bool> Transaction_conflict_monitor::m_init{false};
std::atomic<bool> Transaction_conflict_monitor::m_ready{false};

Transaction_conflict_manager_sptr &Transaction_conflict_monitor::get(
    std::size_t instance_id) {
  if (!m_init.exchange(true)) {
    Transaction_conflict_monitor::init();
    m_ready.store(true);
    m_ready.notify_all();
  } else {
    m_ready.wait(false);
  }
  assert(instance_id < max_instances);
  return m_instances[instance_id];
}

void Transaction_conflict_monitor::init() {}

}  // namespace mysql::csa
