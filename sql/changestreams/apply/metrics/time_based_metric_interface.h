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

#ifndef CHANGESTREAMS_APPLY_METRICS_TIME_BASED_METRIC_INTERFACE_H
#define CHANGESTREAMS_APPLY_METRICS_TIME_BASED_METRIC_INTERFACE_H

#include <scope_guard.h>  // create_scope_guard
#include <cstdint>        // int64_t

/// @brief Abstract class for time based metrics implementations
class Time_based_metric_interface {
 public:
  virtual ~Time_based_metric_interface() = default;

  /// @brief Resets the counter and time to 0
  virtual void reset() = 0;

  /// @brief Returns the total time waited across all executions of the
  /// start/stop methods, minus the absolute start time of the last one in case
  /// it has not ended.
  /// @return The total time waited, in nanoseconds.
  virtual int64_t get_time() const = 0;

  /// @brief Increment the counter.
  ///
  /// @note The counter is normally incremented automatically each time
  /// time_scope is called. This function is needed only for objects where that
  /// functionality has been disabled.
  virtual void increment_counter() = 0;

  /// @brief Returns the number of times we waited on give spot
  /// @return the number of times waited
  virtual int64_t get_count() const = 0;

  /// Start the timer, and return an object that will stop the timer when it
  /// is deleted.
  [[nodiscard]] auto time_scope() {
    start_timer();
    return create_scope_guard([=, this] { this->stop_timer(); });
  }

 protected:
  /// @brief Starts the timer.
  ///
  /// Used internally; the public interface is @c time_scope().
  virtual void start_timer() = 0;

  /// @brief Stops the timer.
  ///
  /// Used internally; the public interface is @c time_scope().
  virtual void stop_timer() = 0;
};

#endif /* CHANGESTREAMS_APPLY_METRICS_TIME_BASED_METRIC_INTERFACE_H */
