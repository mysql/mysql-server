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

#ifndef MYSQL_CSA_SERVICE_CSA_CHANNEL_H
#define MYSQL_CSA_SERVICE_CSA_CHANNEL_H

#include <memory>
#include <ostream>
#include "mysql/scheduler/dependency_tracker_stub.h"
#include "mysql/scheduler/schedule_factory.h"
#include "mysql/scheduler/scheduler.h"
#include "mysql/scheduler/scheduler_clock.h"
#include "mysql/scheduler/thread_pool.h"
#include "sql/changestreams/apply/core/transaction_provider.h"
#include "sql/changestreams/apply/session/session_service.h"

namespace mysql::csa {

/// Aggregates applier structures for a single channel
struct Csa_channel {
  /// @brief Thread pool type for the scheduler
  using Thread_pool = scheduler::Thread_pool<scheduler::Task_result, 8192>;
  /// @brief Shared pointer to Thread_pool
  using Thread_pool_ptr = std::shared_ptr<Thread_pool>;
  /// @brief Shared pointer to Scheduler_clock
  using Clock_ptr = std::shared_ptr<scheduler::Scheduler_clock>;
  /// @brief Scheduler type
  using Scheduler_type = scheduler::Scheduler;
  /// @brief Shared pointer to Scheduler_type
  using Scheduler_ptr = std::shared_ptr<Scheduler_type>;
  /// @brief Schedule factory type
  using Schedule_factory = scheduler::Schedule_factory;
  /// @brief Shared pointer to Schedule_factory
  using Schedule_factory_ptr = std::shared_ptr<Schedule_factory>;
  /// @brief Shared pointer to Session_service
  using Session_service_ptr = std::shared_ptr<Session_service>;

  /// @brief Thread pool instance
  Thread_pool_ptr thread_pool;
  /// @brief Channel clock
  Clock_ptr scheduler_clock;
  /// @brief Channel commit order clock
  Clock_ptr commit_order_clock;
  /// @brief Session service - sessions contain channel information
  Session_service_ptr session_service;
  /// @brief Scheduler
  Scheduler_ptr scheduler;
  /// @brief Transaction provider for this channel
  Transaction_provider_sptr provider;
  /// Channel unique instance id
  std::size_t channel_instance_id;
  /// Channel THD
  THD *channel_thd;
  /// Stop channel flag, to avoid retries after applier stop
  std::atomic<bool> m_channel_stopped{false};
  /// Stop stop flag, when enabled, CSA won't kill ongoing sessions
  std::atomic<bool> m_soft_stop_enabled{false};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_SERVICE_CSA_CHANNEL_H
