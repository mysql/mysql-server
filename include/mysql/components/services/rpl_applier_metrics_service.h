/* Copyright (c) 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef RPL_APPLIER_METRICS_SERVICE_H
#define RPL_APPLIER_METRICS_SERVICE_H

#include <mysql/components/service.h>
#include "mysql/abi_helpers/abi_helpers.h"  // Array_view

/// @brief Gives a type to each piece of data extracted
enum Enum_applier_metric_type {
  /// @brief CHANNEL_NAME
  applier_metrics_channel_name_t = 0,
  /// @brief Total execution time
  applier_execution_time_t = 1,
  /// @brief Last applier start
  last_applier_start_t = 2,
  /// @brief Transactions committed
  transactions_committed_t = 3,
  /// @brief Transactions ongoing
  transaction_ongoing_t = 4,
  /// @brief transaction pending values unknown
  are_transaction_pending_counts_unknown_t = 5,
  /// @brief Transactions pending
  transaction_pending_t = 6,
  /// @brief Transactions committed total size
  transactions_committed_size_sum_t = 7,
  /// @brief Transactions ongoing total full size
  transactions_ongoing_full_size_sum_t = 8,
  /// @brief Transactions ongoing total executed size
  transactions_ongoing_progress_size_sum_t = 9,
  /// @brief Are transaction pending values unknown
  are_transaction_pending_sizes_unknown_t = 10,
  /// @brief Transactions pending total size
  transactions_pending_size_sum_t = 11,
  /// @brief Events scheduled
  events_committed_count_t = 12,
  /// @brief the number of waits for work from source
  waits_for_work_from_source_count_t = 13,
  /// @brief the time waited for work from source
  waits_for_work_from_source_sum_time_t = 14,
  /// @brief the number of waits for a worker to be available
  waits_for_available_worker_count_t = 15,
  /// @brief the time waited for a worker to be available
  waits_for_available_worker_sum_time_t = 16,
  /// @brief the number of waits on transaction dependencies
  waits_for_commit_dependency_count_t = 17,
  /// @brief the time waited on transaction dependencies
  waits_for_commit_dependency_sum_time_t = 18,
  /// @brief the number of waits due to the lack of memory for queueing
  waits_for_queues_memory_count_t = 19,
  /// @brief the time waited due to the lack of memory for queueing
  waits_for_queues_memory_sum_time_t = 20,
  /// @brief the number of waits when worker queues were full
  waits_for_queues_full_count_t = 21,
  /// @brief the time waited when the worker queues were full
  waits_for_queues_full_sum_time_t = 22,
  /// @brief The number of times workers waited for the commit order
  waits_due_to_commit_order_count_t = 23,
  /// @brief The total time workers waited for the commit order
  waits_due_to_commit_order_sum_time_t = 24,
  /// @brief the time sum spent reading from the relay log
  time_to_read_from_relay_log_t = 25,

  /// @brief last element of the enum, not used for any field. This is the only
  /// enum entry that is allowed to change in new versions. Insert new fields
  /// above this one.
  applier_metric_type_end
};

/// @brief The information for each replication worker
enum Enum_worker_metric_type {
  /// @brief CHANNEL_NAME
  worker_metrics_channel_name_t = 0,
  /// @brief The worker id
  worker_id_t = 1,
  /// @brief Is the thread id unknown
  is_thread_id_unknown_t = 2,
  /// @brief The worker thread id
  thread_id_t = 3,
  /// @brief The worker transaction being worked
  transaction_type_t = 4,
  /// @brief The transactions ongoing total full size
  transaction_ongoing_full_size_t = 5,
  /// @brief The transaction ongoing total executed size
  transaction_ongoing_progress_size_t = 6,

  /// @brief last element of the enum, not used for any field. This is the only
  /// enum entry that is allowed to change in new versions. Insert new fields
  /// above this one.
  worker_metric_type_end
};

using Applier_metric_field =
    mysql::abi_helpers::Field<Enum_applier_metric_type>;

using Applier_metrics_row =
    mysql::abi_helpers::Array_view<Applier_metric_field>;

using Applier_metrics_table =
    mysql::abi_helpers::Array_view<Applier_metrics_row>;

/// @brief Transaction type for workers. Encoded as a 1-based enum, matching
/// the table definition.
enum Worker_transaction_type {
  UNKNOWN_TRX_TYPE = 1,
  DML_TRX_TYPE = 2,
  DDL_TRX_TYPE = 3
};

using Worker_metric_field = mysql::abi_helpers::Field<Enum_worker_metric_type>;

using Worker_metrics_row = mysql::abi_helpers::Array_view<Worker_metric_field>;

using Worker_metrics_table = mysql::abi_helpers::Array_view<Worker_metrics_row>;

/**
  @ingroup group_components_services_inventory

  A service that allows you to extract stats from the replica applier.
  You can extract general metrics for every running coordindator.
  You can extract metrics for all replication workers running
*/
BEGIN_SERVICE_DEFINITION(replication_applier_metrics)

/// @brief Get metrics for the replication applier.
/// @param[out] table pointer, whose value will be set to an array of arrays of
/// fields in which the metric values are stored.
///  @return
///    @retval FALSE Succeeded.
///    @retval TRUE  Failed.
DECLARE_BOOL_METHOD(get_applier_metrics, (Applier_metrics_table * table));

/// @brief Free memory for object holding metrics for the replication applier.
/// @param[out] table Pointer to object that was previously retrieved from
/// @c get_applier_metrics.
DECLARE_METHOD(void, free_applier_metrics, (Applier_metrics_table * table));

/// @brief Get metrics for replication workers.
/// @param[out] table pointer, whose value will be set to an array of arrays of
/// fields in which the metric values are stored.
///  @return
///    @retval FALSE Succeeded.
///    @retval TRUE  Failed.
DECLARE_BOOL_METHOD(get_worker_metrics, (Worker_metrics_table * table));

/// @brief Free memory for object holding metrics for the replication workers.
/// @param[out] table Pointer to object that was previously retrieved from
/// @c get_worker_metrics.
DECLARE_METHOD(void, free_worker_metrics, (Worker_metrics_table * table));

/// @brief Enables metric collection in the server for replication applier
/// components
///  @return
///    @retval FALSE Succeeded.
///    @retval TRUE  Failed.
DECLARE_BOOL_METHOD(enable_metric_collection, ());

/// @brief Enables metric collection in the server for replication applier
/// components
///  @return
///    @retval FALSE Succeeded.
///    @retval TRUE  Failed.
DECLARE_BOOL_METHOD(disable_metric_collection, ());

END_SERVICE_DEFINITION(replication_applier_metrics)

#endif /* RPL_APPLIER_METRICS_SERVICE_H */
