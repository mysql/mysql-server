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

#ifndef MYSQL_CS_APPLY_PARALLEL_WORKER_CONTEXT_H
#define MYSQL_CS_APPLY_PARALLEL_WORKER_CONTEXT_H

#include <cstdint>
#include <memory>
#include <string>
#include "sql/changestreams/apply/metrics/worker_metrics.h"

class Relay_log_info;
class THD;
class MDL_context;
class Commit_order_manager;

namespace cs::apply {

class Parallel_worker_context;
using Parallel_worker_context_ptr = std::unique_ptr<Parallel_worker_context>;

/// @brief Class representing the interface for parallel worker context. It
/// accesses basic information like worker id, transaction id, channel id and
/// other functionatlities needed by the commit order manager.
/// @note This context is needed to disconnect Slave_worker class from
/// execution path, Slave_worker implements Parallel_worker_context interface
class Parallel_worker_context {
 public:
  Parallel_worker_context() = default;
  virtual ~Parallel_worker_context() = default;
  using Trx_id = int64_t;
  using Worker_id = uint64_t;
  using Channel_id = std::string;

  /// @brief Indicates that commit order deadlock has been found
  /// @param for_self When true, a thread reports for its own
  virtual void report_commit_order_deadlock(bool for_self = false) = 0;
  /// @brief Checks if commit order deadlock has been found
  /// @return True if commit order deadlock has been found, false otherwise
  virtual bool found_commit_order_deadlock() const = 0;
  /// @brief Resets commit order deadlock
  virtual void reset_commit_order_deadlock() = 0;
  /// @brief Checks if arg parallel worker executes transaction coming from
  /// the same channel
  virtual bool is_same_channel(const Parallel_worker_context *other) const = 0;
  /// @brief Returns transaction id (we use sequence number)
  /// @return transaction id
  virtual THD *get_transaction_ctx() = 0;
  /// @brief Obtains worker metrics
  /// @return Reference to worker metrics object
  virtual instruments::Worker_metrics &get_worker_metrics() = 0;
  /// @brief Returns worker id
  /// @return Worker identifier
  virtual Worker_id get_worker_id() const = 0;
  /// @brief Obtains MDL context needed for commit
  /// @return MDL context
  virtual MDL_context *get_mdl_context() = 0;
  /// @brief Returns information on whether THD transaction can be retried
  /// @return true if transaction can be retried
  virtual bool can_be_retried(THD *thd) = 0;
  /// @brief Obtain transaction id (sequence number)
  /// @return Transaction sequence number
  virtual Trx_id get_trx_id() = 0;
  /// @brief Get "for channel" id. Builds string in flight when needed
  /// Pass true as boolean argument if upper case is needed
  /// @return "for channel" string
  virtual const char *get_for_channel_id(bool) const { return ""; }
  /// Checks whether this worker is CSA worker
  /// @return true for CSA parallel worker context. False otherwise
  virtual bool is_csa() const { return false; }
};

}  // namespace cs::apply

#endif  // MYSQL_CS_APPLY_PARALLEL_WORKER_CONTEXT_H
