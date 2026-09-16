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

#ifndef MYSQL_CONCURRENCY_CONDITION_VARIABLE_WRAPPER_H
#define MYSQL_CONCURRENCY_CONDITION_VARIABLE_WRAPPER_H

#include <condition_variable>
#include <mutex>
#include "mysql/concurrency/mutex_wrapper.h"
#include "mysql/psi/mysql_cond.h"

namespace mysql::concurrency {

/// @brief MySQL wrapper for a condition variable, using mysql_cond_t as
/// implementation of a condition variable having interface of
/// condition_variable provided by the STL
class Condition_variable_wrapper {
 public:
  Condition_variable_wrapper(PSI_cond_key key);
  ~Condition_variable_wrapper();

  /// @brief Wait until notified or predicate is false
  /// @tparam Predicate Type of the predicate function
  /// @param[in,out] lock Unique lock on the mutex to wait on
  /// @param[in] pred Predicate function to check
  template <class Predicate>
  void wait(std::unique_lock<Mutex_wrapper> &lock, Predicate pred);

  /// @brief Wait for a relative timeout or until notified
  /// @tparam Rep Representation type of the duration
  /// @tparam Period Period type of the duration
  /// @tparam Predicate Type of the predicate function
  /// @param[in,out] lock Unique lock on the mutex to wait on
  /// @param[in] rel_time Relative time to wait
  /// @param[in] pred Predicate function to check
  /// @return True if predicate is true, false if timeout occurred
  template <class Rep, class Period, class Predicate>
  bool wait_for(std::unique_lock<Mutex_wrapper> &lock,
                const std::chrono::duration<Rep, Period> &rel_time,
                Predicate pred);

  /// @brief Wait for a relative timeout
  /// @tparam Rep Representation type of the duration
  /// @tparam Period Period type of the duration
  /// @param[in,out] lock Unique lock on the mutex to wait on
  /// @param[in] rel_time Relative time to wait
  /// @return cv_status indicating whether timeout occurred or not
  template <class Rep, class Period>
  std::cv_status wait_for(std::unique_lock<Mutex_wrapper> &lock,
                          const std::chrono::duration<Rep, Period> &rel_time);

  /// @brief Wait until an absolute timeout or until notified
  /// @tparam Clock Clock type for the time point
  /// @tparam Duration Duration type for the time point
  /// @tparam Predicate Type of the predicate function
  /// @param[in,out] lock Unique lock on the mutex to wait on
  /// @param[in] abs_time Absolute time to wait until
  /// @param[in] pred Predicate function to check
  /// @return True if predicate is true, false if timeout occurred
  template <class Clock, class Duration, class Predicate>
  bool wait_until(std::unique_lock<Mutex_wrapper> &lock,
                  const std::chrono::time_point<Clock, Duration> &abs_time,
                  Predicate pred);

  /// @brief Wait until an absolute timeout
  /// @tparam Clock Clock type for the time point
  /// @tparam Duration Duration type for the time point
  /// @param[in,out] lock Unique lock on the mutex to wait on
  /// @param[in] abs_time Absolute time to wait until
  /// @return cv_status indicating whether timeout occurred or not
  template <class Clock, class Duration>
  std::cv_status wait_until(
      std::unique_lock<Mutex_wrapper> &lock,
      const std::chrono::time_point<Clock, Duration> &abs_time);

  /// @brief Notify one waiting thread
  void notify_one();

  /// @brief Notify all waiting threads
  void notify_all();

  /// @brief Wait until notified
  /// @param[in,out] lock Unique lock on the mutex to wait on
  void wait(std::unique_lock<Mutex_wrapper> &lock);

  // Disable copy-move semantics
  Condition_variable_wrapper(const Condition_variable_wrapper &) = delete;
  Condition_variable_wrapper(Condition_variable_wrapper &&) = delete;
  Condition_variable_wrapper &operator=(const Condition_variable_wrapper &src) =
      delete;
  Condition_variable_wrapper &operator=(Condition_variable_wrapper &&src) =
      delete;

 protected:
  mysql_cond_t m_cv;
};

}  // namespace mysql::concurrency

#include "mysql/concurrency/condition_variable_wrapper_impl.hpp"

#endif  // MYSQL_CONCURRENCY_CONDITION_VARIABLE_WRAPPER_H
