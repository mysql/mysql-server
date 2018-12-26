/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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
  RWLock(PSI_rwlock_key key = PSI_NOT_INSTRUMENTED);
  ~RWLock();

  operator mysql_rwlock_t *() { return &m_rwlock; }

  void rlock() { mysql_rwlock_rdlock(&m_rwlock); }

  void wlock() { mysql_rwlock_wrlock(&m_rwlock); }

  bool try_wlock() { return mysql_rwlock_trywrlock(&m_rwlock) == 0; }

  void unlock() { mysql_rwlock_unlock(&m_rwlock); }

 private:
  RWLock(const RWLock &) = delete;
  RWLock &operator=(const RWLock &) = delete;

  mysql_rwlock_t m_rwlock;
};

class RWLock_readlock {
 public:
  RWLock_readlock(RWLock &lock) : m_lock(lock) { m_lock.rlock(); }

  ~RWLock_readlock() { m_lock.unlock(); }

 private:
  RWLock_readlock(const RWLock_readlock &) = delete;
  RWLock_readlock &operator=(RWLock_readlock &) = delete;

  RWLock &m_lock;
};

class RWLock_writelock {
 public:
  RWLock_writelock(RWLock &lock, const bool dont_wait_when_cant_lock = false)
      : m_lock(lock) {
    if (dont_wait_when_cant_lock)
      m_locked = m_lock.try_wlock() == 0;
    else {
      m_lock.wlock();
      m_locked = true;
    }
  }

  ~RWLock_writelock() { m_lock.unlock(); }

  bool locked() const { return m_locked; }

 private:
  RWLock_writelock(const RWLock_writelock &) = delete;
  RWLock_writelock &operator=(RWLock_writelock &) = delete;

  RWLock &m_lock;
  bool m_locked;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_HELPER_MULTITHREAD_RW_LOCK_H_
