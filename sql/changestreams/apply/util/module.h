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

#ifndef MYSQL_CSA_MODULE_H
#define MYSQL_CSA_MODULE_H

#include <mysql/psi/mysql_mutex.h>
#include <mysql/psi/mysql_rwlock.h>
#include "my_sys.h"
#include "mysql/service_mysql_alloc.h"

#include "mysql/scheduler/logger_stream.h"
#include "sql/changestreams/apply/psi/psi.h"
#include "sql/changestreams/apply/util/initable.h"
#include "sql/changestreams/apply/util/lock_guard_object.h"

namespace mysql::csa {

/// @brief Represents a lockable module
class Module : public Initable {
 public:
  /// @brief Enum defining possible states of the module
  enum class State { off = 0, on, start, stop };

  friend class Lock_guard_object<Module>;
  using Lock_guard = Lock_guard_object<Module>;

  /// @brief Constructs the module
  /// @param mutex_key PSI mutex key
  /// @param rwlock_key PSI rwlock key
  Module(PSI_mutex_key mutex_key, PSI_rwlock_key rwlock_key);

  /// @brief Destructor
  virtual ~Module() override;

  /// @brief Initializes internal locking
  void init_locking();

  /// @brief Initializes the module
  /// @return True on error, false on success
  bool init() override;

  /// @brief Acquires a read lock on this object
  /// @return Scope guard that unlocks at the end of the scope
  [[nodiscard]] Lock_guard rdlock();

  /// @brief Acquires a write lock on this object
  /// @return Scope guard that unlocks at the end of the scope
  [[nodiscard]] Lock_guard wrlock();

  /// @brief Deinitializes the module
  /// @return True on error, false on success
  bool deinit() override;

  /// @brief Checks if the module is initialized
  /// @return True if initialized, false otherwise
  bool is_inited();

 protected:
  /// @brief Unlocks this module
  void unlock();

  /// @brief Initialization hook for derived classes
  /// @return True on error, false on success
  virtual bool do_init();
  /// @brief Deinitialization hook for derived classes
  /// @return True on error, false on success
  virtual bool do_deinit();

 private:
  /// @brief PSI key for the read-write lock
  PSI_rwlock_key m_rwlock_key;
  /// @brief Read-write lock for the module
  mysql_rwlock_t m_rwlock;
  /// @brief State of the module
  std::atomic<State> m_inited{State::off};
};

}  // namespace mysql::csa

#endif
