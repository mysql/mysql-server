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

#ifndef MYSQL_CONCURRENCY_MUTEX_WRAPPER_H
#define MYSQL_CONCURRENCY_MUTEX_WRAPPER_H

#include "mysql/psi/mysql_mutex.h"

namespace mysql::concurrency {

/// @brief MySQL wrapper for a mutex, template which may be specialized with
/// a specific implementation of a mutex, e.g. MySQL mutex, and satisfying
/// the following requirements:
///  - Lockable
///  - Destructible
///  - not copyable
///  - not movable
class Mutex_wrapper {
 public:
  Mutex_wrapper(PSI_mutex_key key);
  ~Mutex_wrapper();

  // Disable copy-move semantics
  Mutex_wrapper(const Mutex_wrapper &) = delete;
  Mutex_wrapper(Mutex_wrapper &&) = delete;
  Mutex_wrapper &operator=(const Mutex_wrapper &src) = delete;
  Mutex_wrapper &operator=(Mutex_wrapper &&src) = delete;

  /// @brief Acquire a lock, blocks access to a critical section until "unlock"
  /// is called
  void lock() noexcept;

  /// Non-blocking try-lock operation
  /// @retval true Lock has been acquired
  /// @retval false Lock has NOT been acquired
  bool try_lock() noexcept;

  /// @brief Unblocks access to a critical section for other threads
  void unlock();

  /// @brief Returns the native handle of the mutex
  /// @return Reference to the underlying mysql_mutex_t object
  decltype(auto) native_handle() { return &m_mutex; }

 protected:
  mysql_mutex_t m_mutex;
};

}  // namespace mysql::concurrency

#endif  // MYSQL_CONCURRENCY_MUTEX_WRAPPER_H
