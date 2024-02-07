/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef INSTRUMENTED_CONDITION_VARIABLE_H
#define INSTRUMENTED_CONDITION_VARIABLE_H
#include <condition_variable>
#include <ctime>
#include <mutex>
#include "mysql/components/library_mysys/instrumented_mutex.h"
#include "mysql/components/services/mysql_cond.h"

namespace mysql {

using std::cv_status;
using std::unique_lock;
using std::chrono::time_point;

/**
  condition_variable is a C++ STL conditional variable
  (std::condition_variable) implementation using the instrumented MySQL
  conditional variable component API.

  This allows for P_S instrumentation of conditional variables in components.

  @note Some methods are missing. Implement as needed.

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
  void wait(unique_lock<mutex> &lock) {
    mysql_cond_wait(&m_cond, lock.mutex()->native_handle());
  }
  template <class Predicate>
  void wait(unique_lock<mutex> &lock, Predicate stop_waiting) {
    while (!stop_waiting()) wait(lock);
  }
  template <class Clock, class Duration>
  cv_status wait_until(unique_lock<mutex> &lock,
                       const time_point<Clock, Duration> &abs_time) {
    struct timespec tm {
      Clock::to_time_t(abs_time), 0
    };
    return (0 == mysql_cond_timedwait(&m_cond,
                                      reinterpret_cast<mysql_mutex_t *>(
                                          lock.mutex()->native_handle()),
                                      &tm)
                ? cv_status::no_timeout
                : cv_status::timeout);
  }

 protected:
  PSI_cond_key m_key;
  mysql_cond_t m_cond;
};

}  // namespace mysql

#endif  // INSTRUMENTED_CONDITION_VARIABLE_H
