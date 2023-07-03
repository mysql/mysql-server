/*
 * Copyright (c) 2018, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_SRC_HELPER_MULTITHREAD_RW_LOCK_H_
#define PLUGIN_X_SRC_HELPER_MULTITHREAD_RW_LOCK_H_

#include "mysql/psi/mysql_rwlock.h"

namespace xpl {

class RWLock {
 public:
  explicit RWLock(PSI_rwlock_key key);
  ~RWLock();

  operator mysql_rwlock_t *() { return &m_rwlock; }

  bool rlock() {
    const auto result = mysql_rwlock_rdlock(&m_rwlock);
    assert(EDEADLK != result);
    return 0 == result;
  }

  bool wlock() {
    auto result = mysql_rwlock_wrlock(&m_rwlock);
    assert(EDEADLK != result);
    return 0 == result;
  }

  bool try_wlock() { return mysql_rwlock_trywrlock(&m_rwlock) == 0; }

  void unlock() { mysql_rwlock_unlock(&m_rwlock); }

 private:
  RWLock(const RWLock &) = delete;
  RWLock &operator=(const RWLock &) = delete;

  mysql_rwlock_t m_rwlock;
};

class RWLock_readlock {
 public:
  explicit RWLock_readlock(RWLock *lock) : m_lock(lock) {
    if (!m_lock->rlock()) m_lock = nullptr;
  }

  RWLock_readlock(RWLock_readlock &&obj) : m_lock(obj.m_lock) {
    obj.m_lock = nullptr;
  }

  ~RWLock_readlock() {
    if (m_lock) m_lock->unlock();
  }

 private:
  RWLock *m_lock;
};

class RWLock_writelock {
 public:
  explicit RWLock_writelock(RWLock *lock,
                            const bool dont_wait_when_cant_lock = false)
      : m_lock(lock) {
    if (dont_wait_when_cant_lock) {
      // Don't hold reference to the lock, in case when
      // it was not locked.
      if (!m_lock->try_wlock()) m_lock = nullptr;
    } else {
      if (!m_lock->wlock()) m_lock = nullptr;
    }
  }

  RWLock_writelock(RWLock_writelock &&obj) : m_lock(obj.m_lock) {
    obj.m_lock = nullptr;
  }

  ~RWLock_writelock() {
    if (m_lock) m_lock->unlock();
  }

  bool locked() const { return m_lock; }

 private:
  RWLock *m_lock;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_HELPER_MULTITHREAD_RW_LOCK_H_
