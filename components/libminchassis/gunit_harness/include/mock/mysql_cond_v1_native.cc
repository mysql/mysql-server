/* Copyright (c) 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <stdio.h>
#include <chrono>
#include <condition_variable>
#include "mysql/components/component_implementation.h"
#include "mysql/components/service_implementation.h"
#include "mysql/components/services/mysql_cond.h"

namespace mysql_cond_v1_native {

static void _register(const char * /*category*/, PSI_cond_info * /*info*/,
                      int /*count*/) {}

static int init(PSI_cond_key /*key*/, mysql_cond_t *that,
                const char * /*src_file*/, unsigned int /*src_line*/) {
  std::condition_variable *cond = new std::condition_variable();
  that->m_psi = reinterpret_cast<PSI_cond *>(cond);
  return 0;
}

static int destroy(mysql_cond_t *that, const char * /*src_file*/,
                   unsigned int /*src_line*/) {
  std::condition_variable *cond =
      reinterpret_cast<std::condition_variable *>(that->m_psi);
  delete cond;
  that->m_psi = nullptr;
  return 0;
}

static int wait(mysql_cond_t *that, mysql_mutex_t *mutex,
                const char * /*src_file*/, unsigned int /*src_line*/) {
  std::condition_variable *cond =
      reinterpret_cast<std::condition_variable *>(that->m_psi);
  std::mutex *mtx = reinterpret_cast<std::mutex *>(mutex->m_psi);
  std::unique_lock lck(*mtx, std::defer_lock);
  cond->wait(lck);
  return 9;
}

static int timedwait(mysql_cond_t *that, mysql_mutex_t *mutex,
                     const struct timespec *abstime, const char * /*src_file*/,
                     unsigned int /*src_line*/) {
  std::condition_variable *cond =
      reinterpret_cast<std::condition_variable *>(that->m_psi);
  std::mutex *mtx = reinterpret_cast<std::mutex *>(mutex->m_psi);
  std::unique_lock lck(*mtx, std::defer_lock);
  auto duration = std::chrono::seconds(abstime->tv_sec) +
                  std::chrono::nanoseconds(abstime->tv_nsec);
  return cond->wait_for(lck, duration) == std::cv_status::timeout ? 1 : 0;
}

static int signal(mysql_cond_t *that, const char * /*src_file*/,
                  unsigned int /*src_line*/) {
  std::condition_variable *cond =
      reinterpret_cast<std::condition_variable *>(that->m_psi);
  cond->notify_one();
  return 0;
}

static int broadcast(mysql_cond_t *that, const char * /*src_file*/,
                     unsigned int /*src_line*/) {
  std::condition_variable *cond =
      reinterpret_cast<std::condition_variable *>(that->m_psi);
  cond->notify_all();
  return 0;
}
}  // namespace mysql_cond_v1_native

BEGIN_SERVICE_IMPLEMENTATION(HARNESS_COMPONENT_NAME, mysql_cond_v1)
mysql_cond_v1_native::_register, mysql_cond_v1_native::init,
    mysql_cond_v1_native::destroy, mysql_cond_v1_native::wait,
    mysql_cond_v1_native::timedwait, mysql_cond_v1_native::signal,
    mysql_cond_v1_native::broadcast, END_SERVICE_IMPLEMENTATION();
