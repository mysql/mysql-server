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

#ifndef PLUGIN_X_SRC_HELPER_MULTITHREAD_MUTEX_H_
#define PLUGIN_X_SRC_HELPER_MULTITHREAD_MUTEX_H_

#include "mutex_lock.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/psi/psi_base.h"
#include "thr_mutex.h"

namespace xpl {

class Mutex {
 public:
  friend class Cond;

 public:
  Mutex(PSI_mutex_key key = PSI_NOT_INSTRUMENTED);
  ~Mutex();

  operator mysql_mutex_t *();

  bool try_lock();
  void lock();
  void unlock();

 private:
  Mutex(const Mutex &) = delete;
  Mutex &operator=(const Mutex &) = delete;

  mysql_mutex_t m_mutex;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_HELPER_MULTITHREAD_MUTEX_H_
