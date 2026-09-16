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

#include "mysql/scheduler/sharded_counter.h"
#include <chrono>
#include <iostream>

namespace mysql::scheduler {

long long Sharded_counter::get(std::size_t thread_id) const {
  if (!m_enabled || thread_id >= m_value.size()) {
    return 0;
  }
  return m_value.at(thread_id).load();
}

long long Sharded_counter::get() const {
  if (!m_enabled) {
    return 0;
  }
  long long result{0};
  for (const auto &entry : m_value) {
    result += entry.second.load();
  }
  return result;
}

void Sharded_counter::store(long long arg, std::size_t thread_id) {
  if (!m_enabled || thread_id >= m_value.size()) {
    return;
  }
  m_value.at(thread_id).store(arg);
}

void Sharded_counter::add(long long arg, std::size_t thread_id) {
  if (!m_enabled || thread_id >= m_value.size()) {
    return;
  }
  m_value.at(thread_id) += arg;
}

void Sharded_counter::init(std::size_t num_threads, bool enabled) {
  m_enabled = enabled;
  if (!m_enabled) {
    return;
  }
  if (m_value.size() > 0) {
    m_value.clear();
  }
  for (std::size_t tid = 0; tid < num_threads; ++tid) {
    m_value.emplace(std::piecewise_construct, std::forward_as_tuple(tid),
                    std::forward_as_tuple(0));
  }
}

void Sharded_counter::reset() {
  if (!m_enabled) {
    return;
  }
  for (auto &entry : m_value) {
    entry.second = 0;
  }
}

namespace {
inline long long get_current_time_us() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
}  // namespace

void Sharded_counter::start_time(std::size_t thread_id) {
  if (!m_enabled || thread_id >= m_value.size()) {
    return;
  }
  auto time_us = get_current_time_us();
  m_value.at(thread_id) -= time_us;
}

void Sharded_counter::stop_time(std::size_t thread_id) {
  if (!m_enabled || thread_id >= m_value.size()) {
    return;
  }
  auto time_us = get_current_time_us();
  m_value.at(thread_id) += time_us;
}

long long Sharded_counter::get_timer(std::size_t thread_id) const {
  if (!m_enabled || thread_id >= m_value.size()) {
    return 0;
  }
  auto result = m_value.at(thread_id).load();
  if (result < 0) {
    result += get_current_time_us();
  }
  return result;
}

long long Sharded_counter::get_timer() const {
  if (!m_enabled) {
    return 0;
  }
  long long result{0};
  for (const auto &entry : m_value) {
    result += get_timer(entry.first);
  }
  return result;
}

}  // namespace mysql::scheduler
