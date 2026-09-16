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

#include "mysql/scheduler/commit_order_clock.h"
#include <cassert>
#include <iostream>

#include "mysql/scheduler/logger_stream.h"

namespace mysql::scheduler {

Commit_order_clock::Commit_order_clock(Time_point_t start_point) {
  m_clock.store(start_point);
}

Commit_order_clock::Time_point_t Commit_order_clock::now() const {
  return m_clock.load();
}

Commit_order_clock::Time_point_t Commit_order_clock::start_time() const {
  return 0;
}

bool Commit_order_clock::add_time(Task_id, Time_point_t) { return true; }

bool Commit_order_clock::tick(Task_id, [[maybe_unused]] Time_point_t time) {
  bool already_ticked = false;
  if (m_twicked_count.load() != 0) {
    // we need to check if we need to move m_clock or not
    auto cached = m_twicked_count.load();
    while (!already_ticked && cached > 0) {
      already_ticked = m_twicked_count.compare_exchange_strong(
          cached, cached - 1, std::memory_order_acq_rel,
          std::memory_order_acquire);
      cached = m_twicked_count.load();
    }
  }
  if (!already_ticked) {
    ++m_clock;
  }
  return true;
}

}  // namespace mysql::scheduler
