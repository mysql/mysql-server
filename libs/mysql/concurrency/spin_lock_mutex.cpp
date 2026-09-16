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

#include "mysql/concurrency/spin_lock_mutex.h"

namespace mysql::concurrency {

void Spin_lock_mutex::lock() noexcept {
  for (;;) {
    // try to acquire the lock
    if (!m_lock.test_and_set(std::memory_order_acquire)) {
      return;
    }
    // wait until locked, reduce contention by uisng NOP / YIELD...
    while (m_lock.test(std::memory_order_relaxed)) {
      spin_yield();
    }
  }
}

bool Spin_lock_mutex::try_lock() noexcept {
  return !m_lock.test(std::memory_order_relaxed) &&
         !m_lock.test_and_set(std::memory_order_acquire);
}

void Spin_lock_mutex::unlock() { m_lock.clear(std::memory_order_release); }

}  // namespace mysql::concurrency
