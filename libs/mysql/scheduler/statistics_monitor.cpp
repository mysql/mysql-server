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

#include "mysql/scheduler/statistics_monitor.h"
#include <cassert>
#include <iostream>
#include "mysql/scheduler/constants.h"
#include "mysql/scheduler/logger_stream.h"

namespace mysql::scheduler {

std::atomic<bool> Statistics_monitor::m_init{false};
std::atomic<bool> Statistics_monitor::m_ready{false};

Statistics_instance_monitor &Statistics_monitor::get(std::size_t instance_id) {
  if (!m_init.exchange(true)) {
    Statistics_monitor::init();
    m_ready.store(true);
    m_ready.notify_all();
  } else {
    m_ready.wait(false);
  }
  assert(instance_id < Constants::max_instances);
  return instances()[instance_id];
}

void Statistics_monitor::clear(std::size_t instance_id) {
  if (!m_init.load()) {
    return;
  }
  assert(instance_id < Constants::max_instances);
  instances()[instance_id].clear();
}

void Statistics_monitor::init() {}

Statistics_monitor::Instances_map &Statistics_monitor::instances() {
  // Deliberately never destroyed: scheduler worker activity can outlive static
  // teardown order, so global destruction of statistics storage is unsafe.
  alignas(Instances_map) static unsigned char storage[sizeof(Instances_map)];
  static auto *instances_map = new (storage) Instances_map();
  return *instances_map;
}

}  // namespace mysql::scheduler
