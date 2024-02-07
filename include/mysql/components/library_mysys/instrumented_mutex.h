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

#ifndef INSTRUMENTED_MUTEX_H
#define INSTRUMENTED_MUTEX_H
#include <mutex>
#include "mysql/components/services/mysql_mutex.h"

namespace mysql {
/**
  mutex is a C++ STL mutex (std::mutex) implementation using the
  instrumented MySQL mutex component API.

  This allows for P_S instrumentation of mutexes in components.

  Example usage:
  @code
  #include <mysql/components/library_mysys/instrumented_mutex.h>
  namespace x {
    ...
    using mysql::mutex;
    ...
    mutex m(KEY_psi_mutex);
    ...
    std::unique_lock(m);
    ...
  };
  @endcode
*/

class mutex {
 public:
  mutex(PSI_mutex_key key) noexcept : m_key(key) {
    mysql_mutex_init(m_key, &m_mtx, nullptr);
  }
  mutex(const mutex &) = delete;
  ~mutex() { mysql_mutex_destroy(&m_mtx); }
  void lock() { mysql_mutex_lock(&m_mtx); }
  bool try_lock() { return 0 == mysql_mutex_trylock(&m_mtx); }
  void unlock() { mysql_mutex_unlock(&m_mtx); }
  mysql_mutex_t *native_handle() { return &m_mtx; }

 protected:
  mysql_mutex_t m_mtx;
  PSI_mutex_key m_key;
};

}  // namespace mysql

#endif  // INSTRUMENTED_MUTEX_H
