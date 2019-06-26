#ifndef INCLUDE_GUARD_H_
#define INCLUDE_GUARD_H_
/* Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file guard.h RAII classes for locking/unlocking, inspired by the guard
  class of Boost fame.
*/

#include <mysql/psi/mysql_mutex.h>
#include <mysql/psi/mysql_rwlock.h>
#include <mysql/psi/mysql_thread.h>

/// Guards a mutex.
class Mutex_guard {
 public:
  Mutex_guard(const Mutex_guard &lock) = delete;

  /**
    Object construction that locks specified mutex.

    @param mutex Mutex pointer to be locked.
  */
  explicit Mutex_guard(mysql_mutex_t *mutex) : m_mutex(mutex) {
    mysql_mutex_lock(m_mutex);
  }

  /// Destroys object and unlocks the mutex.
  ~Mutex_guard() { mysql_mutex_unlock(m_mutex); }

 private:
  mysql_mutex_t *m_mutex;
};

#endif  // INCLUDE_GUARD_H_
