/* Copyright (c) 2023, Oracle and/or its affiliates.

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

#ifndef INSTRUMENTED_CONDITION_VARIABLE_H
#define INSTRUMENTED_CONDITION_VARIABLE_H
#include <condition_variable>
#include "mysql/components/library_mysys/instrumented_mutex.h"
#include "mysql/components/services/mysql_cond.h"

namespace mysql {

/**
  condition_variable is a C++ STL conditional variable
  (std::condition_variable) implementation using the instrumented MySQL
  conditional variable component API.

  This allows for P_S instrumentation of conditional variables in components.

  Example usage:
  @code
  #include <mysql/components/library_mysys/instrumented_condition_variable.h>
  #include <mysql/components/library_mysys/instrumented_mutex.h>
  namespace x {
    ...
    using mysql::condition_varible;
    ...
    condition_variable m(KEY_psi_cond);
    ...
    m.notifyAll(..);
    ...
  };
  @endcode

 */
class condition_variable {
 public:
  condition_variable(PSI_cond_key key) : m_key(key) {
    mysql_cond_init(m_key, &m_cond);
  }
  condition_variable(const condition_variable &) = delete;
  ~condition_variable() { mysql_cond_destroy(&m_cond); }
  void notify_one() noexcept { mysql_cond_signal(&m_cond); }
  void notify_all() noexcept { mysql_cond_broadcast(&m_cond); }
  void wait(std::unique_lock<mutex> &lock) {
    mysql_cond_wait(&m_cond, lock.mutex()->native_handle());
  }
  template <class Predicate>
  void wait(std::unique_lock<mutex> &lock, Predicate stop_waiting) {
    while (!stop_waiting()) wait(lock);
  }
  template <class Rep, class Period>
  std::cv_status wait_for(std::unique_lock<mutex> &lock,
                          const std::chrono::duration<Rep, Period> &rel_time) {
    struct timespec tm {
      0, 0
    };
    tm.tv_sec =
        std::chrono::duration_cast<std::chrono::seconds>(rel_time).count();
    tm.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(
                     rel_time - std::chrono::seconds(tm.tv_sec))
                     .count();
    return 0 == mysql_cond_timedwait(&m_cond,
                                     reinterpret_cast<mysql_mutex_t *>(
                                         lock.mutex()->native_handle()),
                                     &tm)
               ? std::cv_status::no_timeout
               : std::cv_status::timeout;
  }

  template <class Rep, class Period, class Predicate>
  bool wait_for(std::unique_lock<mutex> &lock,
                const std::chrono::duration<Rep, Period> &rel_time,
                Predicate stop_waiting) {
    return wait_until(lock, std::chrono::system_clock::now() + rel_time,
                      std::move(stop_waiting));
  }

  template <class Clock, class Duration>
  std::cv_status wait_until(
      std::unique_lock<mutex> &lock,
      const std::chrono::time_point<Clock, Duration> &timeout_time) {
    auto now = std::chrono::system_clock::now();
    if (timeout_time < now) return std::cv_status::timeout;
    return wait_for(lock, timeout_time - now);
  }

  template <class Clock, class Duration, class Predicate>
  bool wait_until(std::unique_lock<mutex> &lock,
                  const std::chrono::time_point<Clock, Duration> &timeout_time,
                  Predicate stop_waiting) {
    while (!stop_waiting()) {
      if (wait_until(lock, timeout_time) == std::cv_status::timeout) {
        return stop_waiting();
      }
    }
    return true;
  }

 protected:
  mysql_cond_t m_cond;
  PSI_cond_key m_key;
};

}  // namespace mysql

#endif  // INSTRUMENTED_CONDITION_VARIABLE_H
