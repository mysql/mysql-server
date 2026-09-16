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

#ifndef MYSQL_CSA_JOB_APPLIER_H
#define MYSQL_CSA_JOB_APPLIER_H

#include "sql/changestreams/apply/context/relay_context.h"
#include "sql/changestreams/apply/context/session.h"
#include "sql/changestreams/apply/jobs/job_binlog.h"
#include "sql/changestreams/apply/resource/resource_monitor.h"

namespace mysql::csa {

/// @brief Concrete class representing job applied by the applier. Contains
/// logic to execute (prepare, commit, retry) transaction.
class Job_applier : public Job_binlog {
 public:
  using Resource_monitor_ref = Resource_instance_monitor_ref;
  /// @brief Constructor.
  /// @param channel Channel this job comes from.
  /// @param max_retries Maximum number of retries for this job.
  /// @param fetch_object Object to fetch job data.
  /// @param stat_monitor Object for monitoring statistics.
  /// @param res_monitor Object for monitoring resources.
  Job_applier(Channel *channel, unsigned int max_retries,
              std::shared_ptr<Fetchable_transaction> fetch_object,
              Stat_monitor_ref stat_monitor, Resource_monitor_ref res_monitor);
  /// @brief Destructor.
  virtual ~Job_applier() override;
  /// @brief Attaches to relay context.
  /// @param thread_id Thread pool worker identifier
  /// @return False on success, true otherwise.
  bool attach(Thread_id thread_id) override;
  /// @brief Detaches from relay context.
  /// @param thread_id Thread pool worker identifier
  /// @return False on success, true otherwise.
  bool detach(Thread_id thread_id) override;
  /// @brief Checks whether this job is attached to relay log context.
  /// @return True if attached. False otherwise.
  bool is_attached() const override;
  /// @brief Restarts internal state. Prepares for retry.
  /// @return False on success, true otherwise.
  bool restart() override;
  /// @brief Presents job identifier.
  /// @return String with job identifier.
  std::string to_string() override;
  /// @brief Sets job failure.
  void set_failure() override;
  /// @brief Prepares this job to be applied
  /// @param ss Pointer to session service.
  void prepare_for_apply(Session_service_ptr ss);
  /// @brief Internal function to wait for rollback and restart transaction in
  /// case deadlock has been found
  /// @return True if transaction cannot be rolled back or retried. False
  /// otherwise
  bool wait_for_rollback_and_restart();

 protected:
  /// @brief Type alias for log event pointer.
  using Log_event_ptr = Managed_event::Log_event_ptr;
  /// @brief Transaction prepare phase, control events execute fully in
  /// prepare phase
  /// @param thread_id Thread pool worker identifier
  bool prepare(Thread_id thread_id) override;
  /// @brief Transaction commit phase, noop for control events
  /// @param thread_id Thread pool worker identifier
  bool commit(Thread_id thread_id) override;
  /// @brief Transaction "register for commit" phase, noop for control events
  /// @param thread_id Thread pool worker identifier
  /// @return False on success, true on error
  bool commit_register(Thread_id thread_id) override;
  /// Applies a single event (internal helper)
  /// @param ev Event to apply
  /// @param thd THD session to apply an event
  /// @return False on success, true on error
  bool apply_event(const Log_event_ptr &ev, THD *thd);
  /// @brief Apply events from phase until phase ends or transaction ends
  /// @param thread_id Thread pool worker identifier
  /// @return False on success, true otherwise
  bool run_phase(Thread_id thread_id);
  /// Checks and handles RPCO conflict if designated.
  /// @param thread_id Thread pool worker identifier
  /// @retval True RPCO conflict detected
  /// @retval False No RPCO conflict
  bool check_rpco_conflict(Thread_id thread_id);
  /// Acquires session if not already obtained. Prepares parallel worker context
  /// @param thread_id Thread pool worker identifier
  void ensure_session(uint thread_id);
  /// Called to clean up when a transaction is skipped (skipping commit phase):
  /// - unregister from commit order manager
  void finish_before_commit();
  /// Check if transaction can be retried
  /// @return True if transaction can be retried, false otherwise
  bool can_be_retried() override;
  /// Internal function that starts the transaction telemetry tracking
  void start_telemetry();
  /// Internal function that finishes the transaction telemetry tracking
  void finish_telemetry();
  /// Keeps ROWS_QUERY event memory alive until Relay_log_info cleanup clears
  /// the query pointers that reference it.
  void sync_rows_query_event_retention();

  /// @brief Relay context used to apply transaction.
  mysql::csa::Relay_context_ptr m_relay_context;
  /// @brief The session service to secure a THD and RLI objects when
  /// applying the job.
  Session_service_ptr m_session_service;
  /// @brief Attach flag. Set to true when job is attached to the relay context.
  bool m_is_attached{false};
  /// Commit event, saved here for the commit phase in case of retry
  Log_event_ptr m_commit_event;
  /// The currently active ROWS_QUERY event for statement-level processlist
  /// visibility.
  Log_event_ptr m_rows_query_event;
  /// Resource monitoring object for the current channel instance
  Resource_monitor_ref m_resource_monitor;
  /// Internal flag for skipping transaction used to check if we need
  /// to wait for unregistering from COM queue in commit phase.
  bool m_skip{false};
  /// Internal flag to skip transaction rollback, used by the
  /// restart
  bool m_skip_rollback{false};
  /// Flag indicating that trx registered in the COM queue
  bool m_co_registered{false};
};

}  // namespace mysql::csa

#endif
