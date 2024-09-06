/*
   Copyright (c) 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/changestreams/apply/metrics/time_based_metric.h"
#include <cassert>
#include <chrono>

Time_based_metric::Time_based_metric(bool manual_counting)
    : m_manual_counting(manual_counting) {}

Time_based_metric &Time_based_metric::operator=(
    const Time_based_metric &other) {
  m_count.store(other.m_count.load());
  m_time.store(other.m_count.load());
  return *this;
}

void Time_based_metric::reset() {
  m_time.store(0);
  m_count.store(0);
}

void Time_based_metric::start_timer() {
  auto now_x = now();
  [[maybe_unused]] auto previous_value = m_time.fetch_sub(now_x);
  assert(previous_value < now_x);
  assert(previous_value >= 0);
  if (!m_manual_counting) m_count.fetch_add(1);
}

void Time_based_metric::stop_timer() {
  [[maybe_unused]] auto previous_value = m_time.fetch_add(now());
  assert(previous_value < 0);
}

int64_t Time_based_metric::get_sum_time_elapsed() const {
  auto ret = m_time.load();
  if (ret < 0) ret += now();
  assert(ret >= 0);
  return ret;
}

void Time_based_metric::increment_counter() {
  assert(m_manual_counting);
  m_count.fetch_add(1);
}

int64_t Time_based_metric::get_count() const { return m_count.load(); }

int64_t Time_based_metric::now() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
