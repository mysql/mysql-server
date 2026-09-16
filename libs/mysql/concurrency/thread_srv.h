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

#ifndef MYSQL_CONCURRENCY_THREAD_SRV_H
#define MYSQL_CONCURRENCY_THREAD_SRV_H

#ifdef MYSQL_CONCURRENCY_THREAD_STL_H
#error Inclusion of both thread_stl.h and thread_srv.h is prohibited.
#endif

#include <atomic>
#include <cstdint>
#include <functional>
#include "mysql/concurrency/condition_variable_wrapper.h"
#include "mysql/psi/mysql_cond.h"
#include "mysql/psi/mysql_memory.h"
#include "mysql/psi/mysql_thread.h"

#define MDEF_TH_KEY(key) key,

namespace mysql::concurrency {

using Thread_key = PSI_thread_key;

/// @brief Wrapper to mysql thread, which matches interface of std::thread
class Thread {
 public:
  Thread() = default;
  Thread(const Thread &) = delete;
  Thread &operator=(const Thread &src) = delete;
  Thread(Thread &&) = default;
  Thread &operator=(Thread &&) = default;

  /// @brief Creates a thread, runs thread function
  /// @param thread_key PSI thread key
  /// @param run_func Function called asynchronously in a separate thread
  /// @param args Function arguments
  template <class Callable, class... Args>
  Thread(Thread_key thread_key, Callable &&run_func, Args &&...args);

  /// @brief Creates a thread, runs thread function
  /// @param run_func Function called asynchronously in a separate thread
  /// @param args Function arguments
  template <class Callable, class... Args>
  Thread(Callable &&run_func, Args &&...args);

  /// @brief Joins thread
  void join() { my_thread_join(&m_thread_handle, nullptr); }

  /// @brief Checks whether a thread was created successfully.
  /// @return True if thread can be joined, false otherwise.
  bool joinable() const { return m_creation_error_code == 0; }

  /// @brief Returns the thread creation error code.
  /// @return 0 on success, error code on failure.
  int creation_error_code() const { return m_creation_error_code; }

 private:
  /// Mysql thread handle
  my_thread_handle m_thread_handle;
  /// Thread attributes
  my_thread_attr_t m_thread_attr;
  /// Thread key
  Thread_key m_thread_key{0};
  /// Thread creation error code.
  int m_creation_error_code{0};
};

/// @brief Fetches internal id, PSI id in case linked with mysqld, or
/// internal thread id
inline unsigned long long fetch_thread_mysql_id(
    [[maybe_unused]] std::size_t my_internal_id) {
#ifdef HAVE_PSI_THREAD_INTERFACE
  struct PSI_thread *psi = PSI_THREAD_CALL(get_thread)();
  return PSI_THREAD_CALL(get_thread_internal_id)(psi);
#else
  return my_internal_id;
#endif
}

}  // namespace mysql::concurrency

#define MDEF_CREATE_THREAD(thread_key, callable, ...) \
  mysql::concurrency::Thread(thread_key, callable, __VA_ARGS__)

#include "mysql/concurrency/thread_srv_impl.hpp"

#endif  // MYSQL_CONCURRENCY_THREAD_SRV_H
