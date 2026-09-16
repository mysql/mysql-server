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

#ifndef MYSQL_CS_APPLY_CSA_WORKER_CONTEXT_H
#define MYSQL_CS_APPLY_CSA_WORKER_CONTEXT_H

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include "sql/changestreams/apply/metrics/dummy_worker_metrics.h"
#include "sql/changestreams/apply/parallel_worker_context.h"
#include "sql/changestreams/apply/service/transaction_conflict_manager.h"

class Relay_log_info;
class THD;
class MDL_context;

namespace cs::apply {

/// @brief Class representing the interface for parallel worker context. It
/// accesses basic information like worker id, transaction id, channel id and
/// other functionatlities needed by the commit order manager.
/// @note This context is needed to disconnect Slave_worker class from
/// execution path, Slave_worker implements Parallel_worker_context interface
class Csa_worker_context : public Parallel_worker_context {
 public:
  Csa_worker_context(Trx_id trx_seq_num, Worker_id worker_id,
                     const std::string &channel_id, THD *trx_ctx,
                     int current_retry, int retries_num);

  using Trx_id = Parallel_worker_context::Trx_id;
  using Worker_id = Parallel_worker_context::Worker_id;

  /// @brief Indicates that commit order deadlock has been found
  /// @param for_self When true, a thread reports for its own
  void report_commit_order_deadlock(bool for_self = false) override;
  /// @brief Checks if commit order deadlock has been found
  /// @return True if commit order deadlock has been found, false otherwise
  bool found_commit_order_deadlock() const override;
  /// @brief Resets commit order deadlock
  void reset_commit_order_deadlock() override;
  /// @brief Checks if arg parallel worker executes transaction coming from
  /// the same channel
  /// @param arg Context to compare against
  bool is_same_channel(const Parallel_worker_context *arg) const override;
  /// @brief Returns transaction id (we use sequence number)
  /// @return transaction id
  THD *get_transaction_ctx() override;
  /// @brief Returns worker id
  /// @return Worker identifier
  Worker_id get_worker_id() const override;
  /// @brief Obtains worker metrics
  /// @return Reference to worker metrics object
  instruments::Worker_metrics &get_worker_metrics() override;
  /// @brief MDL context accessor
  /// @return MDL context obj pointer
  MDL_context *get_mdl_context() override;
  /// @brief Channel identifier accessor
  /// @return channel identifier
  const std::string &get_channel_id() const;
  /// @brief Obtain transaction id (sequence number)
  /// @return Transaction sequence number
  Trx_id get_trx_id() override;
  /// @brief Get "for channel" id. Builds string in flight when needed
  /// @param upper_case Pass true if upper case is needed
  /// @return "for channel" string
  const char *get_for_channel_id(bool upper_case) const override;
  /// Updates internal data for a new transaction
  /// @param trx_seq_num Transaction sequence number
  /// @param worker_id Worker identifier
  /// @param trx_ctx Transaction THD
  /// @param current_retry Current retry number
  void update(Trx_id trx_seq_num, Worker_id worker_id, THD *trx_ctx,
              int current_retry);
  /// Updates internal data for ongoing transaction
  /// @param worker_id Worker identifier
  /// @param current_retry Current retry number
  void update(Worker_id worker_id, int current_retry);
  /// Updates all internal data for new transaction and channel
  /// @param trx_seq_num Transaction sequence number
  /// @param worker_id Worker identifier
  /// @param channel_id Channel identifier
  /// @param trx_ctx Transaction THD
  /// @param current_retry Current retry number
  /// @param retries_num Number of possible retries for this transaction
  void update(Trx_id trx_seq_num, Worker_id worker_id,
              const std::string &channel_id, THD *trx_ctx, int current_retry,
              int retries_num);
  /// @brief Returns information on whether THD transaction can be retried
  /// @return true if transaction can be retried
  bool can_be_retried(THD *thd) override;
  /// @brief Checks whether this transaction error is temporary
  /// @param thd THD handle
  /// @param error Additional error information
  bool has_temporary_error(THD *thd, int error);
  /// Mark transaction as prepared and externally rollbackable.
  void set_applied();
  /// Move transaction from prepared into committing state.
  void set_committing();
  /// Check if external rollback is on-going
  /// @return True when external rollback was already requested.
  bool is_rollback_requested() const { return m_rollback.load(); }
  /// Handle commit order deadlock - one of the callers will request
  /// transaction rollback due to rpco deadlock
  void handle_commit_order_deadlock();
  /// Waits for external rollback to finish
  bool wait_for_rollback();
  /// Worker metrics
  static instruments::Dummy_worker_metrics m_disabled_worker_metrics;
  /// Checks whether this worker is CSA worker
  /// @return true for CSA parallel worker context. False otherwise
  bool is_csa() const override;

 private:
  /// RPCO lifecycle visible to deadlock reporting:
  /// - preparing: transaction is still in apply/prepare work
  /// - prepared: transaction left apply and may be rescued externally
  /// - commit: transaction entered commit ownership and must resolve there
  enum class Rpco_state : std::uint8_t { preparing, prepared, commit };

  /// Sticky indicator that this transaction was reported as an RPCO victim.
  std::atomic<bool> m_is_commit_order_deadlock{false};
  /// Set once an external rollback request has been enqueued.
  std::atomic<bool> m_rollback{false};
  /// Current RPCO lifecycle state used to decide how to handle reports.
  std::atomic<Rpco_state> m_rpco_state{Rpco_state::preparing};
  /// Completes when the external rollback worker finishes rescue handling.
  std::promise<bool> m_rollback_promise;
  /// Transaction identifier reused by commit-order logic.
  Trx_id m_trx_id{0};
  /// Current worker slot owning this transaction context.
  Worker_id m_worker_id{0};
  /// Replication channel this transaction belongs to.
  std::string m_channel_id{""};
  /// THD currently bound to this transaction context.
  THD *m_trx_ctx{nullptr};
  /// Retry number of the current attempt.
  int m_current_retry{0};
  /// Maximum number of retries allowed for this transaction.
  int m_retries_num{0};
  /// Cached "for channel" suffix, built lazily for diagnostics.
  mutable std::string m_for_channel_id{""};
};

}  // namespace cs::apply

#endif  // MYSQL_CS_APPLY_CSA_WORKER_CONTEXT_H
