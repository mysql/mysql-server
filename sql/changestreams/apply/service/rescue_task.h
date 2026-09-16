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

#ifndef MYSQL_CSA_SERVICE_RESCUE_TASK_H
#define MYSQL_CSA_SERVICE_RESCUE_TASK_H

#include <future>
#include "mysql/utils/return_status.h"
#include "sql/rpl_rli.h"

namespace mysql::csa {

/// Types of rescue operations
enum class Rescue_operation_type {
  rollback  // rollback conflicting transaction
};

/// Rescue task to rollback conflicting transaction
class Rescue_task {
 public:
  /// Constructor
  /// @param rli RLI attached to the transaction to rescue
  /// @param op_type Rescue operation type
  /// @param action_promise Promise used to synchronize thread waiting for
  /// rescue operation to finish
  /// @param job_id Job internal identifier
  Rescue_task(Relay_log_info *rli, Rescue_operation_type op_type,
              std::promise<bool> &action_promise, int job_id);
  /// Default constructor, required by synchronized queue
  Rescue_task() = default;
  /// Perform rescue operation
  void operator()();
  /// Sets promise value, e.g. in case when task cannot be executed (stop)
  void set_promise();

 private:
  /// Rolls back transaction attached to RLI
  void rollback_trx();
  /// RLI attached to the transaction to rescue
  Relay_log_info *m_rli{nullptr};
  /// Rescue operation type
  Rescue_operation_type m_rescue_operation_type{
      Rescue_operation_type::rollback};
  /// Promise to synchronize on after operation is done
  std::optional<std::reference_wrapper<std::promise<bool>>> m_action_promise;
  /// Job internal identifier
  int m_job_id{0};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_SERVICE_RESCUE_TASK_H
