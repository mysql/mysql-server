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

#ifndef MYSQL_CSA_APPLIER_MONITOR_H
#define MYSQL_CSA_APPLIER_MONITOR_H

#include <chrono>
#include <cstddef>

#include "sql/changestreams/apply/service/csa_channel.h"

namespace mysql::csa {

class Applier_channel_monitor {
 public:
  explicit Applier_channel_monitor(Csa_channel &channel);

  /// @brief Initializes monitoring with custom refresh interval (default 10s).
  /// Resets refresh time to now() and sets interval.
  /// @param refresh_time Custom interval in milliseconds.
  void init_monitoring(std::chrono::milliseconds refresh_time =
                           std::chrono::milliseconds{10000});

  /// @brief Detects if the applier is stalled by monitoring progress metrics
  /// (applied events, scheduler/commit clocks, active transactions) over a
  /// periodic interval (default 10s). If no progress detected and unblock
  /// attempts remain, calls scheduler unblock function to mitigate stalls.
  /// Void return; unblock counter limited per stall.
  /// Call periodically from applier oversight.
  void check_applier_progress();

  std::size_t get_allowed_unblocks() const;
  std::size_t get_current_unblock_counter() const;
  std::size_t get_total_unblock_counter() const;

 private:
  /// Helper refreshing internal statistics
  void refresh_values();

  /// Monitored CSA channel reference
  Csa_channel &m_channel;
  /// Previous recorded value of applied events
  std::size_t m_previous_applied_events{0};
  /// Current recorded value of applied events
  std::size_t m_current_applied_events{0};
  /// Previous recorded clock value
  std::size_t m_previous_clock_value{0};
  /// Current recorded clock value
  std::size_t m_current_clock_value{0};
  /// Active transactions
  std::size_t m_active_trx{0};
  /// Previous recorded commit order clock value
  std::size_t m_previous_commit_clock_value{0};
  /// Current recorded commit order clock value
  std::size_t m_current_commit_clock_value{0};
  /// Last time we refreshed values
  std::chrono::time_point<std::chrono::system_clock> m_time_refresh;
  /// Refresh current values (and previous) each 'm_refresh_interval' seconds
  std::chrono::milliseconds m_refresh_interval{10000};
  /// Current attempts to unblocking the applier
  std::size_t m_current_unblock_counter{0};
  /// Total attempts to unblocking the applier
  std::size_t m_total_unblock_counter{0};
  /// Allowed clock unblocks for the current stall
  std::size_t m_allowed_unblocks{0};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_APPLIER_MONITOR_H
