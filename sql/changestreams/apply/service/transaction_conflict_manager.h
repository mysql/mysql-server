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

#ifndef MYSQL_CSA_TRANSACTION_CONFLICT_MANAGER_H
#define MYSQL_CSA_TRANSACTION_CONFLICT_MANAGER_H

#include <array>
#include <fstream>
#include <functional>
#include <memory>

#include "mysql/concurrency/locking_queue.h"
#include "mysql/concurrency/thread.h"
#include "mysql/scheduler/constants.h"
#include "mysql/scheduler/logger_stream.h"
#include "mysql/scheduler/statistics_monitor.h"
#include "mysql/utils/return_status.h"
#include "sql/changestreams/apply/service/rescue_task.h"

namespace mysql::csa {

class Transaction_conflict_manager;
using Transaction_conflict_manager_sptr =
    std::unique_ptr<Transaction_conflict_manager>;

/// Rescue thread for solving conflicts between transactions
/// Main methods are:
/// - start : Runs asynchronous thread
/// - stop : Stops execution and blocks until thread is joined
/// - rollback: Schedules transaction to rollback
/// - is_stopped : Checks whether stop has been requested
class Transaction_conflict_manager {
 public:
  using Elem_type = Rescue_task;
  /// Starts asynchronous thread that decodes jobs from the stream
  void start();
  /// Stops asynchronous thread, notifies it and and waits for notification that
  /// thread has been stopped and may be joined. Next, joins the thread.
  void stop();
  /// Checks if stop has been requested
  /// @return True if stop has been requested internally (error) or externally
  /// (log wait for update)
  bool is_stopped() const;
  /// @brief Check if provider has an error
  /// @return True in case an error occurred, false otherwise
  bool is_error() const;
  /// @brief Preempt this transaction
  void enqueue(Rescue_task &&task);

 private:
  /// Function executed by m_thread
  void run_thread();

  /// Thread type
  using Thread_type = mysql::concurrency::Thread;
  using Queue_type = mysql::concurrency::Locking_queue<Elem_type>;

  /// Queue into which decoder thread puts data jobs
  Queue_type m_cache;
  /// Decoder thread object
  Thread_type m_thread;
  /// Notification atomic for end of execution
  std::atomic<bool> m_end{false};
  /// Variable to gracefully stop the thread
  std::atomic<bool> m_is_stopped{false};
  /// The number of scheduled tasks
  std::atomic<std::size_t> m_scheduled_tasks{0};
};

/// @brief Singleton Transaction_conflict_manager for all of the registered
/// "instances". We typically track statistics separately for user-defined
/// channels
class Transaction_conflict_monitor {
 public:
  /// Obtain Transaction_conflict_manager for unique instance id
  /// @param instance_id Unique instance id for the replication channel
  static Transaction_conflict_manager_sptr &get(std::size_t instance_id);

 protected:
  /// @brief This initialization function is called by "get" if needed
  static void init();
  Transaction_conflict_monitor() = default;
  static inline constexpr int max_instances =
      scheduler::Constants::max_instances;
  using Instances_map =
      std::array<Transaction_conflict_manager_sptr, max_instances>;
  static Instances_map m_instances;
  static std::atomic<bool> m_init;
  static std::atomic<bool> m_ready;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_TRANSACTION_CONFLICT_MANAGER_H
