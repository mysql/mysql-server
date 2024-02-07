/*
 * Copyright (c) 2018, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#include "plugin/x/src/helper/multithread/cond.h"

#include "my_systime.h"

namespace xpl {

Cond::Cond(PSI_cond_key key) { mysql_cond_init(key, &m_cond); }

Cond::~Cond() { mysql_cond_destroy(&m_cond); }

void Cond::wait(Mutex &mutex) { mysql_cond_wait(&m_cond, &mutex.m_mutex); }

int Cond::timed_wait(Mutex &mutex, unsigned long long nanoseconds) {
  timespec ts;

  set_timespec_nsec(&ts, nanoseconds);

  return mysql_cond_timedwait(&m_cond, &mutex.m_mutex, &ts);
}

void Cond::signal() { mysql_cond_signal(&m_cond); }

void Cond::signal(Mutex &mutex) {
  MUTEX_LOCK(lock, mutex);

  signal();
}

void Cond::broadcast() { mysql_cond_broadcast(&m_cond); }

void Cond::broadcast(Mutex &mutex) {
  MUTEX_LOCK(lock, mutex);

  broadcast();
}

}  // namespace xpl
