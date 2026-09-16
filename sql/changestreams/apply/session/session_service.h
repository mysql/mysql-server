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

#ifndef MYSQL_CSA_SESSION_SERVICE_H
#define MYSQL_CSA_SESSION_SERVICE_H

#include <memory>
#include <optional>
#include "mysql/scheduler/task_id.h"
#include "mysql/scheduler/task_registry.h"
#include "mysql/utils/return_status.h"
#include "sql/changestreams/apply/context/relay_context.h"
#include "sql/changestreams/apply/service/session_legacy_stats.h"
#include "sql/log_event.h"  // Log_event

namespace mysql::csa {

class Session_service;
/// @brief Shared pointer to Session_service
using Session_service_ptr = std::shared_ptr<Session_service>;

/// @brief Service that provides a free session to executing transactions.
/// Base class for maintaining session pool available for CSA workers
class Session_service {
 public:
  /// @brief Alias for task identifier
  using Task_id = mysql::scheduler::Task_id;

  /// @brief Virtual destructor
  virtual ~Session_service() = default;
  /// @brief Initializes the session service
  /// @param session_number The number of sessions to add to the initial pool
  /// @param channel_rli Parent RLI - channel RLI to share common applier config
  /// @return False if initialization succeeds, true otherwise
  virtual bool init(std::size_t session_number,
                    Relay_log_info *channel_rli) = 0;
  /// @brief Deinitializes the session service
  /// @return False if deinitialization succeeds, true otherwise
  virtual bool deinit() = 0;
  /// @brief Obtains session statistics
  /// @return Upon success, returns corresponding legacy stats. If they are
  /// unavailable due to e.g. non-existing session id, returns an empty object
  std::optional<Session_legacy_stats> get_session_stats(std::size_t session_id);
  /// Obtains the number of all sessions
  /// @return Number of session in this session pool
  virtual std::size_t get_session_number();
  /// Acquires session
  /// @param id Attach id for the session (internal sequence session id)
  /// @return Acquired session shared pointer
  virtual Relay_context_ptr acquire_session(Task_id id) = 0;
  /// @brief Releases a session back to the pool
  /// @param session Session to return to the session pool
  /// @param id Session internal ID
  virtual void release_session(Relay_context_ptr session, Task_id id) = 0;
  /// Performs clean up of sessions, including rollback
  void clean_sessions();
  /// Enables stop error suppression for sessions that have no reported error.
  void enable_stop_error_suppression_for_clean_sessions();

  /// Checks whether session service contains a given THD thread id
  /// @param thd_id THD thread identifier to check
  /// @return When true, session service contains THD with a given thd_id.
  /// False otherwise.
  bool has_thd_id(unsigned int thd_id) const;

  /// Awakes sessions to faster end execution
  /// @param force_kill When true, kills THDs
  void awake_sessions(bool force_kill);

 protected:
  /// Aggregates all sessions in order to look at specific session data
  std::unordered_map<std::size_t, Relay_context_ptr> m_all_sessions;
  /// Captured THD identifiers for easy access
  std::unordered_set<unsigned int> m_thd_ids;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_SESSION_SERVICE_H
