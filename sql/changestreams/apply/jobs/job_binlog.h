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

#ifndef MYSQL_CSA_JOB_BINLOG_H
#define MYSQL_CSA_JOB_BINLOG_H

#include <mysql/psi/mysql_mutex.h>  // mysql_mutex_t
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "mysql/scheduler/statistics_instance_monitor.h"
#include "sql/changestreams/apply/context/channel.h"
#include "sql/changestreams/apply/core/managed_event.h"
#include "sql/changestreams/apply/jobs/fetchable_transaction.h"
#include "sql/changestreams/apply/jobs/job.h"
#include "sql/changestreams/apply/session/session_service.h"

namespace mysql::csa {

/// Lists all phases the Job_binlog can be in
enum class Transaction_phase {
  prepare = 0,          /// Transaction prepare
  commit_register = 1,  ///< Register for commit phase
  commit_binlog = 2,    ///< Committing phase
  retry_commit = 3,     ///< Retrying commit
  done = 4              ///< Done, successfully or with failure
};

/// @brief The Job_binlog class is a container that holds a buffer of fetchable
/// events and context for the transaction to be applied in change streams.
///
/// The context includes the originating channel and parallelization window
/// information.
class Job_binlog : public Job {
 public:
  /// @brief Alias to Statistics_instance_monitor_ref contained in the scheduler
  /// library
  using Stat_monitor_ref = scheduler::Statistics_instance_monitor_ref;
  /// @brief Deleted copy constructor.
  Job_binlog(const Job_binlog &) = delete;

  /// @brief Deleted assignment operator.
  Job_binlog &operator=(const Job_binlog &) = delete;

  /// @brief Constructor for Job_binlog.
  /// @param channel The channel this job is coming from.
  /// @param max_retries The maximum number of retries for this job.
  /// @param fetch_object The object to fetch job data from.
  /// @param stat_monitor Object for monitoring statistics
  Job_binlog(Channel *channel, unsigned int max_retries,
             std::shared_ptr<Fetchable_transaction> fetch_object,
             Stat_monitor_ref stat_monitor);
  /// @brief Destructor for Job_binlog.
  virtual ~Job_binlog() override;
  /// @brief Gets the channel this job comes from.
  /// @return Pointer to the channel this job comes from.
  Channel *get_channel() const;
  /// @brief Gets the transaction identifier as a string.
  /// @return The transaction identifier as a string.
  const std::string &get_channel_id() const;
  /// Get transaction GTID
  /// @return Transaction GTID
  std::string get_trx_id() const;
  /// @brief Gets the transaction GTID.
  /// @return The transaction GTID.
  const mysql::gtid::Gtid &get_trx_gtid() const { return m_trx_gtid; }
  /// @brief Gets the last committed value to determine when this transaction
  /// can run.
  /// @return The transaction's last committed value.
  unsigned long long get_last_committed() const;
  /// @brief Gets the sequence number of this transaction to determine when it
  /// can run and commit.
  /// @return The transaction's sequence number.
  unsigned long long get_sequence_number() const;
  /// @brief Gets the transaction length.
  /// @return The transaction length.
  unsigned long long get_trx_length() const;
  /// Run prepare phase of this transaction
  /// @param thread_id Thread pool worker identifier
  /// @return False on success. True on failure
  virtual bool prepare(Thread_id thread_id) = 0;
  /// Run commit phase of this transaction
  /// @param thread_id Thread pool worker identifier
  /// @return False on success. True on failure
  virtual bool commit(Thread_id thread_id) = 0;
  /// Register the transaction for commit phase
  /// @param thread_id Thread pool worker identifier
  /// @return False on success. True on failure
  virtual bool commit_register(Thread_id thread_id) = 0;
  /// Mark the transaction as done (done with failure or done successfully
  void set_done() override;
  /// @brief Obtains unique instance id to gather statistics separately for
  /// different "instances".
  /// @details Since we want to gather statistics separately for different.
  /// channels, this is set to channel id. The instance ID is used as a key to
  /// aggregate statistics for each channel.
  /// @return int Unique identifier for the instance (channel ID).
  unsigned int get_instance_id() const override;
  /// @brief Checks if this transaction has finished fetching.
  /// @return True if complete, false otherwise.
  virtual bool is_complete();
  /// @brief Restarts the job and prepares for retry.
  /// @return False on success, true on failure.
  bool restart() override;
  /// @brief Binlog job needs to be called twice to apply transaction:
  /// - 1st run - prepare
  /// - 2nd run - commit
  /// Applier job may run up to slave_trans_retries times. If we retry
  /// "prepare" phase, nothing changes. If we retry "commit", we run
  /// both phases, prepare + commit.
  /// @param thread_id Thread pool worker identifier
  bool run(Thread_id thread_id) override;
  /// @brief Checks whether handled job is a transaction (supports two phases)
  /// @return True if handled job is a transaction
  bool is_trx() const override;
  /// @brief success callback
  void set_success() override;
  /// Skip this job, considered complete and done without failure
  void skip() override;

 protected:
  /// Restarts job with fetch metadata
  /// @param all When true, this is a retry and all object states must be
  /// restarted
  bool restart_internal(bool all);
  /// @brief The GTID of the transaction. "<unknown>" if not available.
  mysql::gtid::Gtid m_trx_gtid;
  /// @brief The channel this transaction is coming from.
  Channel *m_channel;
  /// @brief The first event fetched to obtain GTID information.
  mysql::csa::Managed_event m_first_event;
  /// @brief The cursor for the events to process.
  uint32_t m_next_event{0};
  /// Statistics monitoring object for the current instance
  Stat_monitor_ref m_stat_monitor;
  /// Information on how to fetch transaction data
  std::shared_ptr<Fetchable_transaction> m_fetch_metadata;
  /// Transaction phase indicator:
  /// - prepare - this state means that transaction needs to be prepared
  /// - commit - this state means that transaction needs to be committed
  /// - done - transaction is committed
  /// - retry_commit - transaction prepare phase succeeded, but commit did not
  ///   and we need to retry full transaction in one run.
  Transaction_phase m_phase{Transaction_phase::prepare};
};

}  // namespace mysql::csa

#endif
