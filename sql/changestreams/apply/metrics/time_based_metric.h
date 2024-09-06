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

#ifndef CS_MTA_TIME_BASED_METRIC
#define CS_MTA_TIME_BASED_METRIC

#include <atomic>
#include "time_based_metric_interface.h"

/// @brief Class that encodes how much time we waited for something
class Time_based_metric : public Time_based_metric_interface {
 public:
  /// @brief Constructor that allows you to define counting as being manual
  /// @param manual_counting shall count be automatic on start_timer or not
  /// (default false)
  explicit Time_based_metric(bool manual_counting = false);

  /// @brief Assignment operator
  /// @param other the object that is copied
  /// @return this object
  Time_based_metric &operator=(const Time_based_metric &other);

  /// @brief Deleted copy constructor, move constructor, move assignment
  /// operator.
  Time_based_metric(const Time_based_metric &) = delete;
  Time_based_metric(Time_based_metric &&) = delete;
  Time_based_metric &operator=(Time_based_metric &&) = delete;

  /// @brief Default destuctor.
  ~Time_based_metric() override = default;

  /// @brief Resets the counter and summed time to 0
  void reset() override;

  /// @brief Starts counting time we are waiting on something
  void start_timer() override;

  /// @brief Stops the timer for the wait.
  ///        Requires start_timer to be called first
  void stop_timer() override;

  /// @brief Returns the time waited across all executions of the start/stop
  /// methods
  /// @return The total time waited
  int64_t get_sum_time_elapsed() const override;

  /// @brief Increments the waiting counter
  void increment_counter() override;

  /// @brief Returns the number of time we waited on give spot
  /// @return the number of times waited
  int64_t get_count() const override;

 private:
  /// @brief Helper to get current time.
  /// @return Current time since the epoch for steady_clock, in nanoseconds.
  static int64_t now();

  /// The total nanoseconds of all completed waits, minus the absolute start
  /// time of an ongoing wait, if any.
  ///
  /// If there is no ongoing wait, this is nonnegative and is the correct
  /// metric. If there is an ongoing wait, this is negative, and the correct
  /// value is given by adding the current time to it:
  /// result = sum_of_completed_waits + current_time - start_of_current_wait
  std::atomic<int64_t> m_time{0};
  /// @brief The number of times we waited.
  std::atomic<int64_t> m_count{0};

  /// If false, the counter is incremented automatically by start_time, and the
  /// caller must not invoke increment_counter. If true, the counter is not
  /// incremented by start_time, so the caller has to invoke increment_counter.
  bool m_manual_counting{false};
};

#endif /* CS_MTA_TIME_BASED_METRIC */
