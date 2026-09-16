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

#ifndef MYSQL_CSA_RELAY_CONTEXT_H
#define MYSQL_CSA_RELAY_CONTEXT_H

#include "sql/changestreams/apply/context/session.h"
#include "sql/changestreams/apply/csa_worker_context.h"
#include "sql/rpl_replica_commit_order_manager.h"
#include "sql/rpl_rli.h"

namespace cs::apply {
class Csa_worker_context;
}

namespace mysql::csa {

class Relay_context;
using Relay_context_ptr = std::shared_ptr<Relay_context>;

/// Handles and operates the handled session
/// @see Session
class Relay_context {
 public:
  using Parallel_worker_context = cs::apply::Parallel_worker_context;
  using Parallel_worker_context_ptr = cs::apply::Parallel_worker_context_ptr;
  using Csa_worker_context = cs::apply::Csa_worker_context;
  using Trx_id = Parallel_worker_context::Trx_id;
  using Worker_id = Parallel_worker_context::Worker_id;
  /// Constructor
  /// @param id This relay context id
  /// @param channel_config_rli Channel configuration RLI
  Relay_context(std::size_t id, Relay_log_info *channel_config_rli);
  /// Destructor
  virtual ~Relay_context();
  /// Obtains associated RLI pointer
  /// @return RLI pointer from handled session
  Relay_log_info *get_relay_log_info();
  /// Obtains THD identifier
  /// @return Handled THD identifier
  unsigned int get_thd_id() const;
  /// Checks validity
  /// @return True if valid, false otherwise.
  bool is_valid();
  /// Attaches to session. Must succeed.
  void attach_session();
  /// Detaches from session
  void detach_session();
  /// Obtains session handle
  /// @return Session reference
  Session &get_session() { return m_session; }
  /// Obtains this relay context id
  std::size_t get_id() const;
  /// @brief Sets FDE for this relay context
  /// @param fde FDE that will be used to apply transactions
  void set_fde(Format_description_log_event *fde);
  /// Sets parallel worker context for the next transaction
  /// @param com Current commit order manager pointer
  /// @param trx_seq_num Transaction sequence number
  /// @param worker_id Worker identifier - sequence number
  /// @param channel_id Channel identifier - string
  /// @param current_retry Current number of retries
  /// @param retries_num Allowed number of retries
  void set_parallel_worker_context(Commit_order_manager *com,
                                   Trx_id trx_seq_num, Worker_id worker_id,
                                   const std::string &channel_id,
                                   int current_retry, int retries_num);
  /// @brief if attached to commit order manager, registers parallel worker
  /// to commit order (using parallel worker context)
  void register_to_commit_order();
  /// @brief Report error to parent thread
  /// @param trx_id Applier transaction identifier
  void report_error(const std::string &trx_id);
  /// Enables stop error suppression if this context had no reported error.
  void enable_stop_error_suppression_if_clean();
  /// Obtain parallel worker context
  /// @return pointer to object containing worker context
  cs::apply::Csa_worker_context *get_parallel_worker_context();
  /// Clean-up session
  void clean();
  /// Awakes sessions to faster end execution
  /// @param force_kill When true, kills THDs
  void awake(bool force_kill);
  /// Attaches to RLI
  void attach_rli();
  /// Detaches from RLI
  void detach_rli();
  /// Prepares context for retry
  /// @param current_count Current retries number for this transaction
  /// @param skip_rollback True if we need to skip rollback (already rolled
  /// back)
  void retry_transaction(int current_count, bool skip_rollback);
  /// Waits for external parallel context rollback
  /// @return True on failure, false on success
  bool wait_for_rollback();
  /// Check if handled context can be retried
  /// @return True if transaction can be retried, false otherwise
  bool can_be_retried();

 private:
  /// A pointer to the relay context. It shall be a dummy context,
  /// as there is no need to store positions in tables or files,
  /// since this applier only supports GTIDs
  std::unique_ptr<Relay_log_info> m_rli;
  /// Session
  Session m_session;
  /// Flag indicating whether this session is currently attached to CSA worker
  bool m_attached{false};
  /// Commit order manager
  Commit_order_manager *m_commit_order_manager{nullptr};
  /// Parent channel configuration RLI
  Relay_log_info *m_channel_config_rli{nullptr};
  /// This relay context identifier
  std::size_t m_id{0};
  /// Pointer to CSA worker context
  std::unique_ptr<Csa_worker_context> m_csa_worker_context;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_RELAY_CONTEXT_H
