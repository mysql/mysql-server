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

#pragma once

#include <mysql/components/services/mysql_mutex.h>
#include <mysql/components/services/mysql_rwlock.h>

namespace hm {

constexpr bool Lock_Write = true;
constexpr bool Lock_Read = false;

/**
  Scoped mutex.
*/
class mutex_guard {
 public:
  explicit mutex_guard(mysql_mutex_t *lock);
  mutex_guard(mutex_guard &&lock) noexcept;
  ~mutex_guard();

 private:
  mysql_mutex_t *m_lock;
};

/**
  Scoped r/w lock.
*/
class rwlock_guard {
 public:
  /**
    Acquire a scoped lock on a r/w lock.
    @param lock   Lock object
    @param write  True if write lock
    @param file   Source file name
    @param line   Source line number
  */
  rwlock_guard(mysql_rwlock_t *lock, bool write,
               const char *file [[maybe_unused]], int line [[maybe_unused]]);
  rwlock_guard(rwlock_guard &&lock) noexcept;
  ~rwlock_guard();

 private:
  mysql_rwlock_t *m_lock;
};

/**
  Acquire a write lock.
  @param lock  Lock object
  @param file  Source file name
  @param line  Source line number
  @return 0 success, 1 failure
*/
int rwlock_wrlock(mysql_rwlock_t *lock, const char *file [[maybe_unused]],
                  int line [[maybe_unused]]);

/**
  Acquire a read lock.
  @param lock  Lock object
  @param file  Source file name
  @param line  Source line number
  @return 0 success, 1 failure
*/
int rwlock_rdlock(mysql_rwlock_t *lock, const char *file [[maybe_unused]],
                  int line [[maybe_unused]]);

/**
  Unlock a read or write lock.
  @param lock  Lock object
*/
void rwlock_unlock(mysql_rwlock_t *lock);

}  // namespace hm
