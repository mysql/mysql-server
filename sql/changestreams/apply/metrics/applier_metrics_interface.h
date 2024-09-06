/*
   Copyright (c) 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef CS_APPLIER_METRICS_AGGREGATOR_H
#define CS_APPLIER_METRICS_AGGREGATOR_H

#include <cstdint>  // int64_t
#include <string>

class Time_based_metric_interface;

namespace cs::apply::instruments {

/// @brief This abstract class is an interface for classes that
/// contain replication applier data as counters and wait times
class Applier_metrics_interface {
 public:
  virtual ~Applier_metrics_interface() = default;

  /// @brief Starts the timer when the applier metrics collection began.
  /// Sets the state to running.
  /// This can be queried later to know for how long time the stats have been
  /// collected, i.e., the duration.
  virtual void start_applier_timer() = 0;

  /// @brief Calculates the total time the applier ran.
  /// Sets the state to not running
  /// Sums to time since start to the total running time
  virtual void stop_applier_timer() = 0;

  /// @brief Gets the time point when the metric timer started.
  /// @return The time point since the collection of statistics started.
  virtual int64_t get_last_applier_start_micros() const = 0;

  /// @brief Returns the total time the applier was running
  /// @return Amount of time the applier threads were running for this channel
  virtual int64_t get_total_execution_time() const = 0;

  /// @brief increment the number of transactions committed.
  /// @param amount the amount of transactions to increment.
  virtual void inc_transactions_committed_count(int64_t amount) = 0;

  /// @brief Gets the number of transactions committed.
  /// @return the number of transactions committed.
  virtual int64_t get_transactions_committed_count() const = 0;

  /// @brief increment the number of transactions received.
  /// @param amount the amount of transactions to increment.
  virtual void inc_transactions_received_count(int64_t amount) = 0;

  /// @brief Gets the number of transactions received.
  /// @return the number of transactions received.
  virtual int64_t get_transactions_received_count() const = 0;

  /// @brief increment the size of transactions committed.
  /// @param amount the size amount to increment.
  virtual void inc_transactions_committed_size_sum(int64_t amount) = 0;

  /// @brief Gets the total sum of the size of committed transactions
  /// @return the total size of committed transactions
  virtual int64_t get_transactions_committed_size_sum() const = 0;

  /// @brief increment the pending size of queued transactions.
  /// @param amount the size amount to increment.
  virtual void inc_transactions_received_size_sum(int64_t amount) = 0;

  /// @brief Gets the pending size sum of queued transactions
  /// @return the exectuted size of pending transactions
  virtual int64_t get_transactions_received_size_sum() const = 0;

  /// @brief increment the number of events scheduled by a given amount.
  /// @param amount the amount of events to increment.
  virtual void inc_events_committed_count(int64_t amount) = 0;

  /// @brief Gets the number of events scheduled.
  /// @return the number of events scheduled.
  virtual int64_t get_events_committed_count() const = 0;

  /// @brief Resets the statistics to zero.
  virtual void reset() = 0;

  /// @brief Query whether the size/count of received transactions has been
  /// completely computed.
  ///
  /// Among other things, we track track the count and size of *pending*
  /// transactions, i.e., the transactions that are received but not yet
  /// committed. Internally, in this class, we track these metrics using two
  /// sets of transactions: the size/count of *committed* transactions and the
  /// size/count of *received* transactions. The size/count of pending
  /// transactions can be computed as the difference between the two.
  ///
  /// The correct initial value for the received transactions would be the size
  /// of all not yet applied transactions in the relay log. To get that metric
  /// correct at the time the server starts (or the time the user enables
  /// collecting metrics), we would have to scan the relay logs. But that can be
  /// too expensive. So instead we just take a note that the metric is not yet
  /// known. Until the metric is known, we display the value as NULL to the
  /// user. Internally, we compute the initial value progressively, while
  /// applying those transactions.
  ///
  /// We define the *metrics breakpoint* as the point in the relay log such that
  /// when the point is reached we know that the size/count of received
  /// transactions is completely computed. The metrics breakpoint is (the start
  /// of) the first relay log the receiver writes to.
  ///
  /// Sizes/counts of transactions which appear before the metrics breakpoint
  /// are incremented when those transactions commit. When the metrics
  /// breakpoint is reached, the coordinator waits for preceding transactions to
  /// commit, and then declares that the metrics have been computed.
  /// Sizes/counts of transactions which appear after the metrics breakpoint are
  /// incremented when those transactions are fully received and written to the
  /// relay log.
  ///
  /// When the receiver starts, it uses @c set_metrics_breakpoint to set the
  /// metric breakpoint to the relay log in which it writes the first event.
  ///
  /// It is guaranteed that the applier, when it reaches first relay log that
  /// was received after the receiver thread started, waits for preceding
  /// transactions to complete. It does this while applying the
  /// Format_description_log_event from the source. Therefore, after any such
  /// wait, it uses @c check_metrics_breakpoint to checks if the current relay
  /// log is the metrics breakpoint. If that is the case, the internal flag @c
  /// m_is_after_metrics_breakpoint is set to true, and this makes subsequent
  /// calls to @c is_after_metrics_breakpoint return true.
  ///
  /// When the coordinator schedules an event to a worker, it propagates @c
  /// is_after_metrics_breakpoint to the worker, through @c
  /// Slave_job_item::m_is_after_metrics_breakpoint. When the worker commits the
  /// transaction, it checks the flag. If the flag is false, it increments the
  /// count/size of received transactions.
  ///
  /// This function may be called from many different threads.
  ///
  /// @return true if the size/count of received transactions is initialized,
  /// false otherwise.
  virtual bool is_after_metrics_breakpoint() const = 0;

  /// @brief If the metrics breakpoint has not been set yet, set it to the given
  /// filename.
  ///
  /// This function must only be called by the receiver thread.
  ///
  /// @see is_after_metrics_breakpoint.
  virtual void set_metrics_breakpoint(const char *relay_log_filename) = 0;

  /// @brief If the metrics breakpoint has been set and is equal to the given
  /// filename, remember that we are now after the metrics breakpoint, so that
  /// subsequent calls to @c is_after_metrics_breakpoint return true.
  ///
  /// This function must only be called by the coordinator thread.
  ///
  /// @see is_after_metrics_breakpoint.
  virtual void check_metrics_breakpoint(const char *relay_log_filename) = 0;

  /// @brief Returns time metrics for waits on work from the source
  /// @return a Time_based_metric_interface object that contains metric
  /// information on a wait
  virtual Time_based_metric_interface &get_work_from_source_wait_metric() = 0;

  /// @brief Returns time metrics for waits on available workers
  /// @return a Time_based_metric_interface object that contains metric
  /// information on a wait
  virtual Time_based_metric_interface &get_workers_available_wait_metric() = 0;

  /// @brief Returns time metrics for waits on transaction dependecies on
  /// workers
  /// @return a Time_based_metric_interface object that contains metric
  /// information on a wait
  virtual Time_based_metric_interface &
  get_transaction_dependency_wait_metric() = 0;

  /// @brief Returns time metrics for waits when a worker queue exceeds max
  /// memory
  /// @return a Time_based_metric_interface object that contains metric
  /// information on a wait
  virtual Time_based_metric_interface &
  get_worker_queues_memory_exceeds_max_wait_metric() = 0;

  /// @brief Returns time metrics for waits when the worker queues are full
  /// @return a Time_based_metric_interface object that contains metric
  /// information on a wait
  virtual Time_based_metric_interface &get_worker_queues_full_wait_metric() = 0;

  /// @brief Returns time metrics for relay log read wait times
  /// @return a Time_based_metric_interface object that contains metric
  /// information on a wait
  virtual Time_based_metric_interface &
  get_time_to_read_from_relay_log_metric() = 0;

  /// @brief Increments the stored values for the commit order metrics.
  /// @param count The count for commit order waits
  /// @param time The time waited on commit order
  virtual void inc_commit_order_wait_stored_metrics(int64_t count,
                                                    int64_t time) = 0;

  /// @brief Gets the stored number of times we waited on committed order
  /// @return the stored number of commit order waits
  virtual int64_t get_number_of_waits_on_commit_order() const = 0;

  /// @brief Gets the stored summed time waited on commit order
  /// @return the stored sum of the time waited on commit
  virtual int64_t get_wait_time_on_commit_order() const = 0;
};
}  // namespace cs::apply::instruments

#endif /* CS_APPLIER_METRICS_AGGREGATOR_H */
