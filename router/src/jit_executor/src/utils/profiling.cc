/*
 * Copyright (c) 2018, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

// #include "mysqlshdk/libs/utils/profiling.h"
#include "utils/profiling.h"
#include <cassert>
#include <iostream>

#include "utils/utils_string.h"

namespace shcore {

namespace utils {
void Global_profiler::print_stats() {
  std::map<std::string, double> global_time;
  std::map<std::string, double> global_hits;

  std::lock_guard<std::mutex> lock(m_mutex);
  std::cout << "THREAD PROFILE DATA" << std::endl;
  std::cout << "===================" << std::endl;
  for (const auto &thread_profile : m_time_profilers) {
    std::cout << "THREAD " << thread_profile.first << std::endl;
    std::cout << "-------------------" << std::endl;

    for (const auto &profile : thread_profile.second) {
      auto time = profile.second.total_milliseconds_elapsed();
      auto hits = profile.second.trace_points().size();
      global_time[profile.first] += time;
      global_hits[profile.first] += hits;

      auto msec = static_cast<unsigned long long int>(time);
      std::string duration =
          str_format("%02llu:%02llu:%02llu.%03llus", msec / 3600000ull,
                     (msec % 3600000ull) / 60000ull, (msec % 60000ull) / 1000,
                     msec % 1000ull);

      std::cout << profile.first << " Hits : " << hits
                << " Total Time: " << duration << std::endl;
    }
  }

  std::cout << "GLOBAL PROFILE DATA" << std::endl;
  std::cout << "===================" << std::endl;
  for (const auto &global : global_hits) {
    auto msec = static_cast<unsigned long long int>(global_time[global.first]);
    std::string duration =
        str_format("%02llu:%02llu:%02llu.%03llus", msec / 3600000ull,
                   (msec % 3600000ull) / 60000ull, (msec % 60000ull) / 1000,
                   msec % 1000ull);

    std::cout << global.first << " Hits: " << global.second
              << " Total Time: " << duration << std::endl;
  }
  std::cout << "===================" << std::endl;
}

void Global_profiler::reset() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_time_profilers.clear();
}

}  // namespace utils
namespace profiling {

shcore::utils::Global_profiler *g_active_profiler = nullptr;

void activate(bool trace_total) {
  assert(g_active_profiler == nullptr);

  g_active_profiler = new shcore::utils::Global_profiler();

  if (trace_total) {
    stage_begin("Total");
  }
}

void deactivate(bool trace_total) {
  if (trace_total) {
    stage_end("Total");
  }
  delete g_active_profiler;
  g_active_profiler = nullptr;
}

}  // namespace profiling
}  // namespace shcore
