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

#ifndef MYSQL_CSA_JOB_H
#define MYSQL_CSA_JOB_H

#include <my_systime.h>
#include <atomic>
#include <deque>
#include <functional>
#include <iostream>
#include "mysql/scheduler/logger_stream.h"
#include "mysql/scheduler/task_id.h"

namespace mysql::csa {

/// Forward declaration of the Job class.
class Job;

/// Type alias for a pointer to Job.
using Job_ptr = Job *;

/// A job represents a single unit of work applied by worker pool threads.
/// Job defines execution path, the number of times the job runs, the number
/// of retries, how job attaches to applier context and detaches from it.
/// This is an abstract class for a job.
class Job {
 public:
  /// Deleted copy constructor.
  Job(const Job &) = delete;
  /// Deleted assignment operator.
  Job &operator=(const Job &) = delete;
  /// Type alias for thread identifier.
  using Thread_id = unsigned int;

  /// @brief Constructor.
  /// @param max_retries The number of times a job can be retried.
  Job(unsigned int max_retries);
  /// @brief Destructor.
  virtual ~Job();
  /// @brief Obtains unique instance id to gather statistics separately for
  /// different "instances"
  /// @return Instance unique identifier
  virtual unsigned int get_instance_id() const;
  /// @brief Checks whether handled job is a transaction (supports two phases)
  /// @return True if handled job is a transaction
  virtual bool is_trx() const;
  /// @brief Resets the job for retrying.
  /// This must be called before retrying, so that the internal
  /// pointers are reset to the proper place.
  /// @return True on error. False on success.
  virtual bool restart();
  /// @brief Implements execution of the full job or a single job phase.
  /// @param thread_id Thread pool worker identifier
  /// @return False on success. True on error.
  virtual bool run(Thread_id thread_id) = 0;
  /// @brief Attaches the job.
  /// @param thread_id Thread pool worker identifier
  /// @return True on error. False on success.
  virtual bool attach(Thread_id thread_id) = 0;
  /// @brief Returns an id based on which the job attaches to applier context
  /// @return Attach id
  Thread_id get_attach_id() const;
  /// @brief Detaches the job.
  /// @param thread_id Thread pool worker identifier
  /// @return True on error. False on success.
  virtual bool detach(Thread_id thread_id) = 0;
  /// @brief Gets the job identifier.
  /// @return Job identifier - job sequence number.
  virtual std::size_t get_id() const;
  /// @brief Increments the number of retries for the job.
  virtual void inc_retries();
  /// @brief Checks whether this job can be retried.
  /// @return True if the job can be retried. False otherwise.
  virtual bool can_be_retried();
  /// @brief Gets the number of retries for this job.
  /// @return The number of times this job was retried.
  virtual unsigned int get_retries() const;
  /// @brief Checks whether this job is in error state.
  /// @return True if the job errored out. False otherwise.
  virtual bool is_error();
  /// @brief Checks whether applier stop has been requested for this job.
  /// @return True if stop was requested. False otherwise.
  virtual bool is_stopped() const;
  /// @brief Checks whether job has a fatal, non-recoverable error.
  /// @return True when fatal error detected. False otherwise.
  virtual bool is_fatal_error();
  /// @brief Injects the applier stop flag for this job.
  /// @param applier_stop Stop flag reference.
  virtual void set_applier_stop(std::atomic<bool> &applier_stop);
  /// @brief Global job success callback.
  virtual void set_success();
  /// @brief Global job failure callback.
  virtual void set_failure();
  /// @brief Sets fatal job error.
  virtual void set_fatal_error();
  /// @brief Sets job error.
  virtual void set_error();
  /// @brief Sets this job as done.
  virtual void set_done();
  /// @brief Checks whether the job is done.
  /// @return True if the job is done, false otherwise.
  virtual bool is_done();
  /// @brief Checks whether the job is currently attached.
  /// @return True if attached, false if detached.
  virtual bool is_attached() const = 0;
  /// @brief Converts the job to a string representation.
  /// @return String representation of the job.
  virtual std::string to_string();
  /// Skip this job, considered complete and done without failure
  virtual void skip() {}

 protected:
  /// Error flag for job
  bool m_is_error{false};
  /// Specifies if this job has a fatal error - we cannot retry it
  bool m_is_fatal_error{false};
  /// When set to true, job is done (successfully or not)
  bool m_is_done{false};
  /// Maximum number of retries for this job
  unsigned int m_max_retries;
  /// Next job id. Used to generate ids for jobs
  static std::atomic<std::size_t> next_id;
  /// This job sequence id
  std::size_t m_id{0};
  /// Worker pool thread id
  Thread_id m_thread_id{0};
  /// The number of times this job was retried
  std::size_t m_trx_retries{0};
  /// Channel stop flag used to interrupt wait loops.
  std::reference_wrapper<std::atomic<bool>> m_applier_stop;
};

}  // namespace mysql::csa

#endif
