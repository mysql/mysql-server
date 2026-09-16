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

#ifndef MYSQL_CSA_SESSION_H
#define MYSQL_CSA_SESSION_H

#include "mysql/concurrency/spin_lock_mutex.h"
#include "sql/sql_class.h"

namespace mysql::csa {

/// Wraps up THD object and provides simple function to manage its execution
/// within CSA
class Session {
 public:
  /// Construct from configuration in parent_thd
  /// @param parent_thd Configuration THD
  /// @param seq_num Session sequence number
  Session(THD *parent_thd, std::size_t seq_num);
  /// Destructor
  virtual ~Session();
  /// Checks if created session is valid
  /// True when session is valid, false otherwise
  bool is_valid() const;
  /// Obtains handled THD identifier
  /// @return THD identifier
  unsigned int get_thd_id() const;
  /// Obtains THD handle
  THD *get_thd();
  /// Obtains thread identifier
  ulonglong get_thread_id() const;
  /// Awakes the handled session
  /// @param force_kill If true, kills session
  void awake(bool force_kill);
  /// Attach current thread to session. Must succeed.
  /// @param temporary_tables Temporary tables we attach
  void attach(TABLE *temporary_tables);
  /// Detach current thread from session
  void detach();
  /// Check if CSA killed this session
  /// @return True only if this session was killed by CSA; false otherwise
  bool self_killed() const { return m_is_killed; }
  /// @return True only if this session was killed; false otherwise
  bool is_killed() const { return m_thd->is_killed() || self_killed(); }

 protected:
  /// The base server session associated with this processor.
  THD *m_thd;
  /// PSI thread id
  ulonglong m_psi_id{0};
  /// Saved THD stack
  const char *m_saved_thread_stack{""};
  /// Saved stack pointer
  long m_stack_ptr{0};
  /// Validity flag
  bool m_is_valid{false};
  /// Attach flag
  bool m_is_attached{false};
  /// We save our own kill flag and protect modifications with LOCK_thd_data
  bool m_is_killed{false};
  /// Session lock for protecting attach/detach operations. While LOCK_thd_data
  /// could be used, in a large session pool we avoid its potential syscalls
  /// by using a spinlock instead.
  concurrency::Spin_lock_mutex m_session_lock;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_SESSION_H
