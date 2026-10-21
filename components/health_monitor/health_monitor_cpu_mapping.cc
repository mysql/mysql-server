/* Copyright (c) 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "health_monitor_cpu_mapping.h"

#include <chrono>
#include <cstdint>
#include <iterator>
#include <utility>
#include <vector>

namespace hm {
namespace {

bool convert_milliseconds(std::chrono::milliseconds value, uint64_t &output) {
  if (value.count() < 0) return false;
  output = static_cast<uint64_t>(value.count());
  return true;
}

bool set_portable_times(const mysql::system_info::Cpu_times &source,
                        System_CPU_Stats &destination) {
  destination.m_cpu = source.cpu;
  return convert_milliseconds(source.user, destination.m_user_ms) &&
         convert_milliseconds(source.nice, destination.m_nice_ms) &&
         convert_milliseconds(source.system, destination.m_system_ms) &&
         convert_milliseconds(source.idle, destination.m_idle_ms);
}

bool set_extended_times(const mysql::system_info::Cpu_extended_times &source,
                        System_CPU_Stats &destination) {
  if (!convert_milliseconds(source.io_wait, destination.m_iowait_ms) ||
      !convert_milliseconds(source.irq, destination.m_irq_ms) ||
      !convert_milliseconds(source.soft_irq, destination.m_softirq_ms) ||
      !convert_milliseconds(source.steal, destination.m_steal_ms) ||
      !convert_milliseconds(source.guest, destination.m_guest_ms) ||
      !convert_milliseconds(source.guest_nice, destination.m_guest_nice_ms)) {
    return false;
  }
  destination.m_extended_times_available = true;
  return true;
}

}  // namespace

bool convert_host_cpu_snapshot(
    const mysql::system_info::Host_cpu_snapshot &snapshot,
    const time_t timestamp, std::vector<System_CPU_Stats> &output) {
  if (!snapshot.times.has_value() || snapshot.times.value().cpus.empty()) {
    return false;
  }

  const auto &times = snapshot.times.value().cpus;
  const bool extended_available = snapshot.extended_times.has_value();
  if (extended_available &&
      snapshot.extended_times.value().cpus.size() != times.size()) {
    return false;
  }

  std::vector<System_CPU_Stats> converted;
  converted.reserve(times.size());
  for (size_t index = 0; index < times.size(); ++index) {
    System_CPU_Stats record;
    record.m_timestamp = timestamp;
    if (!set_portable_times(times[index], record)) return false;

    if (extended_available) {
      const auto &extended = snapshot.extended_times.value().cpus[index];
      if (extended.cpu != record.m_cpu ||
          !set_extended_times(extended, record)) {
        return false;
      }
    }
    converted.push_back(std::move(record));
  }

  output.insert(output.end(), std::make_move_iterator(converted.begin()),
                std::make_move_iterator(converted.end()));
  return true;
}

}  // namespace hm
