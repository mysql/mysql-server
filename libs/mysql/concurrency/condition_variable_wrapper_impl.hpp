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

#include "mysql/concurrency/condition_variable_wrapper.h"

namespace mysql::concurrency {

template <class Predicate>
void Condition_variable_wrapper::wait(std::unique_lock<Mutex_wrapper> &lock,
                                      Predicate pred) {
  while (!pred()) {
    mysql_cond_wait(&m_cv, lock.mutex()->native_handle());
  }
}

template <class Rep, class Period, class Predicate>
bool Condition_variable_wrapper::wait_for(
    std::unique_lock<Mutex_wrapper> &lock,
    const std::chrono::duration<Rep, Period> &rel_time, Predicate pred) {
  struct timespec tm;
  auto end_time = std::chrono::system_clock::now() + rel_time;
  tm.tv_sec = std::chrono::system_clock::to_time_t(end_time);
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                end_time.time_since_epoch())
                .count() %
            1000000000;
  tm.tv_nsec = ns;
  bool last_pred_value{false};
  bool timeout = false;
  while (!(last_pred_value = pred()) && !timeout) {
    timeout = mysql_cond_timedwait(&m_cv, lock.mutex()->native_handle(), &tm);
  }
  return last_pred_value;
}

template <class Rep, class Period>
std::cv_status Condition_variable_wrapper::wait_for(
    std::unique_lock<Mutex_wrapper> &lock,
    const std::chrono::duration<Rep, Period> &rel_time) {
  struct timespec tm;
  auto end_time = std::chrono::system_clock::now() + rel_time;
  tm.tv_sec = std::chrono::system_clock::to_time_t(end_time);
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                end_time.time_since_epoch())
                .count() %
            1000000000;
  tm.tv_nsec = ns;
  return mysql_cond_timedwait(&m_cv, lock.mutex()->native_handle(), &tm)
             ? std::cv_status::timeout
             : std::cv_status::no_timeout;
}

template <class Clock, class Duration, class Predicate>
bool Condition_variable_wrapper::wait_until(
    std::unique_lock<Mutex_wrapper> &lock,
    const std::chrono::time_point<Clock, Duration> &abs_time, Predicate pred) {
  struct timespec tm;
  tm.tv_sec = Clock::to_time_t(abs_time);
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                abs_time.time_since_epoch())
                .count() %
            1000000000;
  tm.tv_nsec = ns;
  bool timeout = false;
  bool last_pred_value = false;
  while (!(last_pred_value = pred()) && !timeout) {
    timeout = mysql_cond_timedwait(&m_cv, lock.mutex()->native_handle(), &tm);
  }
  return last_pred_value;
}

template <class Clock, class Duration>
std::cv_status Condition_variable_wrapper::wait_until(
    std::unique_lock<Mutex_wrapper> &lock,
    const std::chrono::time_point<Clock, Duration> &abs_time) {
  struct timespec tm;
  tm.tv_sec = Clock::to_time_t(abs_time);
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                abs_time.time_since_epoch())
                .count() %
            1000000000;
  tm.tv_nsec = ns;
  return mysql_cond_timedwait(&m_cv, lock.mutex()->native_handle(), &tm)
             ? std::cv_status::timeout
             : std::cv_status::no_timeout;
}

}  // namespace mysql::concurrency
