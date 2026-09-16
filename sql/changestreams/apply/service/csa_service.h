// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
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

#ifndef MYSQL_CSA_SERVICE_H
#define MYSQL_CSA_SERVICE_H

#include <mysql/plugin.h>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include "mysql/scheduler/clock_lwm_registry.h"
#include "mysql/scheduler/commit_order_clock.h"
#include "mysql/scheduler/delayed_schedule.h"
#include "mysql/scheduler/dependency_tracker_stub.h"
#include "mysql/scheduler/schedule_factory.h"
#include "mysql/scheduler/scheduler.h"
#include "mysql/scheduler/scheduler_clock.h"
#include "mysql/scheduler/statistics_map.h"
#include "mysql/scheduler/task_result.h"
#include "mysql/scheduler/thread_pool.h"
#include "sql/changestreams/apply/context/tune.h"
#include "sql/changestreams/apply/jobs/task_exec_job.h"
#include "sql/changestreams/apply/psi/psi.h"
#include "sql/changestreams/apply/service/csa_channel.h"
#include "sql/changestreams/apply/service/session_legacy_stats.h"
#include "sql/changestreams/apply/session/session_service.h"
#include "sql/changestreams/apply/session/session_service_bounded_queue.h"
#include "sql/changestreams/apply/util/module.h"

namespace mysql::csa {

/// @brief Service class for the Change Streams Applier (CSA)
class Csa_service : public Module {
 public:
  /// @brief Result type for tasks
  using Task_result = mysql::scheduler::Task_result;
  /// @brief Thread pool type
  using Thread_pool =
      mysql::scheduler::Thread_pool<Task_result,
                                    mysql::csa::tune::scheduler_tp_queue_size>;
  /// @brief Shared pointer to Thread_pool
  using Thread_pool_ptr = std::shared_ptr<Thread_pool>;
  /// @brief Clock type used in the scheduler
  using Clock_type = mysql::scheduler::Clock_lwm_registry;
  /// @brief Shared pointer to Scheduler_clock
  using Clock_ptr = std::shared_ptr<mysql::scheduler::Scheduler_clock>;
  /// @brief Scheduler type
  using Scheduler_type = mysql::scheduler::Scheduler;
  /// @brief Shared pointer to Scheduler_type
  using Scheduler_ptr = std::shared_ptr<Scheduler_type>;
  /// @brief Schedule factory type
  using Schedule_factory = mysql::scheduler::Schedule_factory;
  /// @brief Shared pointer to Schedule_factory
  using Schedule_factory_ptr = std::shared_ptr<Schedule_factory>;
  /// @brief Dependency tracker type
  using Dependency_tracker = mysql::scheduler::Dependency_tracker_stub;
  /// @brief Shared pointer to Dependency_tracker
  using Dependency_tracker_ptr = mysql::scheduler::Dependency_tracker_ptr;
  /// @brief Shared pointer to Session_service
  using Session_service_ptr = mysql::csa::Session_service_ptr;
  using Sched_stat_map = mysql::scheduler::Statistics_map;
  using Commit_order_clock_type = mysql::scheduler::Commit_order_clock;

  /// @brief Constructor
  Csa_service();
  /// @brief Destructor
  virtual ~Csa_service() override;
  /// @brief Synchronous method that runs the channel applier
  /// @param rli Channel RLI object
  /// @return True if run succeeds, false otherwise
  bool run(Relay_log_info *rli);
  /// @brief Synchronous method that removes data and destroys
  /// objects related to the channel
  /// @param rli Channel RLI object
  void remove(Relay_log_info *rli);
  /// @brief Obtains the number of workers configured for the channel
  std::size_t get_workers_number(const char *channel);
  /// @brief Obtains session legacy statistics kept in the RLI object
  /// @param channel Channel id
  /// @param worker_id Id of the worker for which we want to extract the session
  /// @return Upon success, returns corresponding legacy stats. If they are
  /// unavailable due to e.g. channel reconfiguration, returns an empty object
  std::optional<Session_legacy_stats> get_session_legacy_stats(
      const char *channel, std::size_t worker_id);
  /// Checks whether applier for the channel has applied all of the work and
  /// is waiting for more tasks
  /// @param channel Applier channel name
  /// @return When true - channel is waiting for more work. When false - channel
  /// is currently applying. When no value - no channel or channel is inactive
  std::optional<bool> has_applied_all_work(const char *channel);

  /// Checks whether CSA contains session with a given THD thread_id
  /// @param channel Selected channel
  /// @param thread_id THD thread identifier
  bool is_csa_event_applier(const char *channel, unsigned int thread_id);

  /// This function is used only to initialize channel legacy statistics and
  /// to make empty statistics available in case channel was not activated
  /// @param channel Channel name
  /// @param channel_unique_id Channel unique integer identifier
  /// @param worker_num Workers number currently configured for this channel
  /// @return True on success, false on failure
  [[nodiscard]] bool initialize_channel_data(const char *channel,
                                             std::size_t channel_unique_id,
                                             std::size_t worker_num);

  /// This function is used only to remove cached channel statistics upon
  /// channel deletion
  /// @param channel Channel name
  /// @param remove When true, erases channel data. Otherwise, clears values.
  void clear_channel_data(const char *channel, bool remove);

  /// @brief Stops applier now
  /// @param channel Channel name
  /// @param force_kill When true, awakes all sessions
  void stop(const char *channel, bool force_kill);

 private:
  /// @brief Obtains session legacy statistics kept in the RLI object, no lock
  /// @param channel Channel id
  /// @param worker_id Id of the worker for which we want to extract the session
  /// @return Upon success, returns corresponding legacy stats. If they are
  /// unavailable due to e.g. channel reconfiguration, returns an empty object
  std::optional<Session_legacy_stats> get_session_legacy_stats_internal(
      const char *channel, std::size_t worker_id);
  /// @brief Initialization
  /// @retval 0 Success
  /// @retval 1 Failure
  bool do_init() override;
  /// @brief Deinitialization
  /// @retval 0 Success
  /// @retval 1 Failure
  bool do_deinit() override;
  /// @brief Helper function to clean up channel data upon destruction
  /// @param channel Channel identifier
  void clean_up_context(const std::string &channel);

  /// The number of references (channels), protected with internal lock
  unsigned int m_kernel_ref_count{0};
  /// Channel data
  std::unordered_map<std::string, Csa_channel> csa_channels;
  /// Cached statistics for inactive channels
  std::unordered_map<std::string, std::vector<Session_legacy_stats>>
      m_inactive_channel_stats;
  /// When true, channels will execute soft stop after stop of the
  /// channel was requested
  std::atomic<bool> m_soft_stop{false};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_SERVICE_H
