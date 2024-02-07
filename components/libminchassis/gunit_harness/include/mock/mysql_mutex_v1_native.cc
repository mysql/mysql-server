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

#include <stdio.h>
#include <mutex>
#include "mysql/components/component_implementation.h"
#include "mysql/components/service_implementation.h"
#include "mysql/components/services/mysql_mutex.h"

namespace mysql_mutex_v1_native {

static void _register(const char * /*category*/, PSI_mutex_info * /*info*/,
                      int /*count*/) {}

static int _init(PSI_mutex_key /*key*/, mysql_mutex_t *that,
                 const native_mutexattr_t * /*attr*/, const char * /*src_file*/,
                 unsigned int /*src_line*/) {
  std::mutex *mtx = new std::mutex();
  that->m_psi = reinterpret_cast<struct PSI_mutex *>(mtx);
  return 0;
}

static int _destroy(mysql_mutex_t *that, const char * /*src_file*/,
                    unsigned int /*src_line*/) {
  std::mutex *mtx = reinterpret_cast<std::mutex *>(that->m_psi);
  delete mtx;
  that->m_psi = nullptr;
  return 0;
}

static int _lock(mysql_mutex_t *that, const char * /*src_file*/,
                 unsigned int /*src_line*/) {
  std::mutex *mtx = reinterpret_cast<std::mutex *>(that->m_psi);
  mtx->lock();
  return 0;
}

static int _trylock(mysql_mutex_t *that, const char * /*src_file*/,
                    unsigned int /*src_line*/) {
  std::mutex *mtx = reinterpret_cast<std::mutex *>(that->m_psi);
  return mtx->try_lock() ? 0 : -1;
}

static int _unlock(mysql_mutex_t *that, const char * /*src_file*/,
                   unsigned int /*src_line*/) {
  std::mutex *mtx = reinterpret_cast<std::mutex *>(that->m_psi);
  mtx->unlock();
  return 0;
}

}  // namespace mysql_mutex_v1_native

BEGIN_SERVICE_IMPLEMENTATION(HARNESS_COMPONENT_NAME, mysql_mutex_v1)
mysql_mutex_v1_native::_register, mysql_mutex_v1_native::_init,
    mysql_mutex_v1_native::_destroy, mysql_mutex_v1_native::_lock,
    mysql_mutex_v1_native::_trylock, mysql_mutex_v1_native::_unlock,
    END_SERVICE_IMPLEMENTATION();
