// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
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

#ifndef CHANGESTREAMS_APPLY_METRICS_TIME_BASED_METRIC_STUB_H
#define CHANGESTREAMS_APPLY_METRICS_TIME_BASED_METRIC_STUB_H

#include "time_based_metric_interface.h"

/// @brief Class that intends to be a dummy end point for time metrics
class Time_based_metric_stub : public Time_based_metric_interface {
 public:
  /// @brief Resets the counter and summed time to 0
  void reset() override;

  /// @brief Returns the time waited across all executions of the start/stop
  /// methods
  /// @return The total time waited, in nanoseconds.
  int64_t get_time() const override;

  /// @brief Increments the waiting counter
  void increment_counter() override;

  /// @brief Returns the number of time we waited on give spot
  /// @return the number of times waited
  int64_t get_count() const override;

 protected:
  /// @brief Starts counting time we are waiting on something
  void start_timer() override;

  /// @brief Stops the timer for the wait.
  ///        Requires start_timer to be called first
  void stop_timer() override;
};

#endif /* CHANGESTREAMS_APPLY_METRICS_TIME_BASED_METRIC_STUB_H */
