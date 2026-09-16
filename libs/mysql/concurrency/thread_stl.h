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

#ifndef MYSQL_CONCURRENCY_THREAD_STL_H
#define MYSQL_CONCURRENCY_THREAD_STL_H

#ifdef MYSQL_CONCURRENCY_THREAD_SRV_H
#error Inclusion of both thread_stl.h and thread_srv.h is prohibited.
#endif

#include <cstdint>
#include <thread>
#include "mysql/concurrency/key_stub.h"

#define MDEF_TH_KEY(key)

/// @addtogroup GroupLibsMysqlConcurrency
/// @{

namespace mysql::concurrency {

using Thread = std::thread;
using Thread_key = Key_stub;

/// @brief Fetches internal id, PSI id in case linked with mysqld, or
/// internal thread id
/// @return mysql internal id
inline unsigned long long fetch_thread_mysql_id(std::size_t my_internal_id) {
  return my_internal_id;
}

}  // namespace mysql::concurrency

#define MDEF_CREATE_THREAD(thread_key, callable, ...) \
  mysql::concurrency::Thread(callable, __VA_ARGS__)

/// @}

#endif  // MYSQL_CONCURRENCY_THREAD_STL_H
