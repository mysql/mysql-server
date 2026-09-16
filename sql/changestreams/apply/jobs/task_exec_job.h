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

#ifndef MYSQL_CSA_TASK_EXEC_JOB_H
#define MYSQL_CSA_TASK_EXEC_JOB_H

#include <iostream>
#include <string>
#include "mysql/scheduler/statistics_instance_monitor.h"
#include "mysql/scheduler/task_result.h"
#include "sql/changestreams/apply/jobs/job.h"
#include "sql/changestreams/apply/session/session_service.h"

namespace mysql::csa {

/// @brief A task that executes a job in the change streams apply system.
/// It handles job execution, potential retries on failures, and error
/// management.
class Task_exec_job {
 public:
  /// @brief Type alias for the task result from the mysql scheduler.
  using Task_result = mysql::scheduler::Task_result;
  /// @brief Type alias for the session service in change streams apply.
  using Session_service_ptr = mysql::csa::Session_service_ptr;
  /// @brief Type alias for the statistics monitor instance for the channel
  using Stat_monitor_ref = scheduler::Statistics_instance_monitor_ref;

  /// @brief Checks if task executed with an error - this function is called
  /// by Scheduler to get task status
  bool is_error() const;

  /// @brief Constructor
  /// @param job Job handle created by the job provider
  /// @param job_id Job id assigned by the job provider
  /// @param session_service  Handle to session service, providing sessions
  /// to which this job can attach.
  /// @param stat_monitor Statistic monitoring object
  /// @param stop_ref If retried, this flag will be checked to determine
  /// whether applier has been stopped. Otherwise, we skip this check for
  /// performance
  Task_exec_job(Job_ptr job, uint64_t job_id,
                Session_service_ptr session_service,
                Stat_monitor_ref stat_monitor, std::atomic<bool> &stop_ref);

  /// @brief This function executes a job. If job fails and can be retried,
  /// job is reexecuted. In case retry fails or is not possible, error is
  /// written into the job, function returns an error, causing scheduler
  /// to stop
  Task_result operator()(unsigned int thread_id);

 private:
  /// @brief Function executing a job - internal implementation
  /// @param job Job pointer
  /// @param job_id Internal job identifier
  /// @param thread_id Thread pool worker identifier
  /// @see operator()
  Task_result run(Job *job, uint64_t job_id, unsigned int thread_id);

  /// @brief Indicates that a task executed with a non-recoverable error (stop)
  void set_error();

  /// @brief Internal error flag
  bool m_is_error{false};

  /// @brief Shared job handle provided by the job provider.
  std::shared_ptr<Job> m_job;
  /// @brief Job id assigned by the job provider.
  uint64_t m_job_id{0};
  /// @brief Assigned session service handle.
  Session_service_ptr m_session_service;

  /// @brief Checks if task has been started
  bool m_started{false};
  /// Statistics monitoring object for the current instance
  scheduler::Statistics_instance_monitor_ref m_stat_monitor;
  /// If retried, this flag will be checked to determine
  /// whether applier has been stopped. Otherwise, we skip this check for
  /// performance
  std::atomic<bool> &m_applier_stop;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_TASK_EXEC_JOB_H
