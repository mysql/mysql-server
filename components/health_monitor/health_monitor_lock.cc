/* Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#include "health_monitor_lock.h"

namespace hm {

mutex_guard::mutex_guard(mysql_mutex_t *lock)
    : m_lock{lock != nullptr && mysql_mutex_lock(lock) == 0 ? lock : nullptr} {}

mutex_guard::mutex_guard(mutex_guard &&lock) noexcept : m_lock{lock.m_lock} {
  lock.m_lock = nullptr;
}

mutex_guard::~mutex_guard() {
  if (m_lock != nullptr) {
    mysql_mutex_unlock(m_lock);
  }
}

rwlock_guard::rwlock_guard(mysql_rwlock_t *lock, bool write,
                           const char *file [[maybe_unused]],
                           int line [[maybe_unused]]) {
  if (lock == nullptr) {
    m_lock = nullptr;
    return;
  }
  if (write) {
    m_lock =
        mysql_rwlock_wrlock_with_src(lock, file, line) == 0 ? lock : nullptr;
  } else {
    m_lock =
        mysql_rwlock_rdlock_with_src(lock, file, line) == 0 ? lock : nullptr;
  }
}

rwlock_guard::rwlock_guard(rwlock_guard &&lock) noexcept : m_lock{lock.m_lock} {
  lock.m_lock = nullptr;
}

rwlock_guard::~rwlock_guard() {
  // If null, then lock was set to remain locked or moved to another object
  if (m_lock != nullptr) {
    mysql_rwlock_unlock(m_lock);
  }
}

int rwlock_wrlock(mysql_rwlock_t *lock, const char *file [[maybe_unused]],
                  int line [[maybe_unused]]) {
  return mysql_rwlock_wrlock_with_src(lock, file, line) ? 1 : 0;
}

int rwlock_rdlock(mysql_rwlock_t *lock, const char *file [[maybe_unused]],
                  int line [[maybe_unused]]) {
  return mysql_rwlock_rdlock_with_src(lock, file, line) ? 1 : 0;
}

void rwlock_unlock(mysql_rwlock_t *lock) {
  if (lock != nullptr) {
    mysql_rwlock_unlock(lock);
  }
}

}  // namespace hm
