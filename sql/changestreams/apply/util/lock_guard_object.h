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

#ifndef MYSQL_CSA_LOCK_OBJECT_GUARD_H
#define MYSQL_CSA_LOCK_OBJECT_GUARD_H

#include <mysql/psi/mysql_mutex.h>
#include <mysql/psi/mysql_rwlock.h>
#include "my_sys.h"
#include "mysql/service_mysql_alloc.h"

#include "mysql/scheduler/logger_stream.h"
#include "sql/changestreams/apply/psi/psi.h"
#include "sql/changestreams/apply/util/initable.h"

namespace mysql::csa {

/// @brief Scope guard to unlock an object
///
/// Assumes the object is locked upon construction and calls unlock() on
/// destruction unless reset() is called.
/// @tparam Type of the object to unlock
template <typename Type>
class Lock_guard_object {
 public:
  /// @brief Unlocks the object if the locked flag is set and then resets the
  /// flag.
  void reset() {
    if (locked) m_obj->unlock();
    locked = false;
  }
  /// @brief Destructor that unlocks if not unlocked
  ~Lock_guard_object() { reset(); }
  /// @brief Constructor
  /// @param module Pointer to the object to unlock
  Lock_guard_object(Type *module) : m_obj(module) {}

 protected:
  /// @brief Locked object pointer
  Type *m_obj;
  /// @brief Lock flag
  bool locked{true};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_LOCK_OBJECT_GUARD_H
