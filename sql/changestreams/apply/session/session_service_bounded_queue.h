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

#ifndef MYSQL_CSA_SESSION_SERVICE_BOUNDED_QUEUE_H
#define MYSQL_CSA_SESSION_SERVICE_BOUNDED_QUEUE_H

#include <memory>
#include "mysql/concurrency/sync_bounded_queue.h"
#include "mysql/scheduler/task_id.h"
#include "mysql/scheduler/task_registry.h"
#include "mysql/utils/return_status.h"
#include "sql/changestreams/apply/context/relay_context.h"
#include "sql/changestreams/apply/context/tune.h"
#include "sql/changestreams/apply/session/session_service.h"
#include "sql/changestreams/apply/session/session_service_psi.h"
#include "sql/log_event.h"  // Log_event

namespace mysql::csa {

/// @brief Service that provides a free session to executing transactions
class Session_service_bounded_queue : public Session_service {
 public:
  /// @brief Alias for task identifier from base class
  using Task_id = Session_service::Task_id;
  /// @brief Constructor
  /// @param psi_params Instrumentation parameters
  Session_service_bounded_queue(Session_service_psi psi_params = {});
  /// @brief Virtual destructor
  virtual ~Session_service_bounded_queue() override;

  /// @brief Initializes the session service with a bounded queue
  /// @param session_number The number of sessions to add to initial pool
  /// @param channel_rli Parent RLI - channel RLI to share common applier config
  /// @return True if initialization succeeds, false otherwise
  bool init(std::size_t session_number, Relay_log_info *channel_rli) override;
  /// @brief Deinitializes the session service
  /// @return False on success, true on failure
  bool deinit() override;

  /// @brief Acquires a session for the given task ID
  /// @param id The task ID
  /// @return A shared pointer to the Relay_context
  Relay_context_ptr acquire_session(Task_id id) override;
  /// @brief Releases a session back to the pool
  /// @param session Session to return to the session pool
  /// @param id Session internal ID
  void release_session(Relay_context_ptr session, Task_id id) override;

 private:
  /// @brief Type alias for a shared pointer to Relay_context
  using Relay_context_entry = std::shared_ptr<Relay_context>;
  /// @brief Type alias for the synchronized bounded queue of Relay_context_ptr
  using Session_registry = mysql::concurrency::Sync_bounded_queue<
      Relay_context_ptr, tune::csa_session_default_cache_size>;
  /// @brief The session registry queue
  Session_registry m_sessions;

  /// @brief The number of maintained sessions
  std::size_t m_session_number{tune::csa_session_default_cache_size};
  /// Pointer to the channel RLI (to share data, such as applier privileges)
  Relay_log_info *m_channel_rli{nullptr};
  /// PSI parameters
  Session_service_psi m_psi;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_SESSION_SERVICE_BOUNDED_QUEUE_H
