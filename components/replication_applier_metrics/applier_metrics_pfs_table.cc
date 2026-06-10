/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include "applier_metrics_pfs_table.h"
#include "mysql/components/component_implementation.h"  // SERVICE_PLACEHOLDER
#include "mysql/components/services/rpl_applier_metrics_service.h"  // Enum_applier_metric_type
#include "packet_based_table_with_cursor.h"  // Packet_based_table_with_cursor
#include "row_proxy.h"                       // Row_proxy
#include "table_with_cursor.h"  // get_table_share_from_table_with_cursor

extern REQUIRES_SERVICE_PLACEHOLDER(replication_applier_metrics);

namespace applier_metrics {

/// Implementation of the replication_applier_metrics table.
class Applier_metrics_table_with_cursor {
 public:
  using Type_info_t = Row_proxy_type_info<Enum_applier_metric_type,
                                          applier_metric_type_end, 24>;

  /// Return the table name.
  static constexpr const char *get_table_name() {
    return "replication_applier_metrics";
  }

  /// Return the table definition.
  static constexpr const char *get_table_definition() {
    return "CHANNEL_NAME CHAR(64) NOT NULL COMMENT 'The channel name.',"
           "TOTAL_ACTIVE_TIME_DURATION BIGINT NOT NULL "
           "COMMENT 'Total channel applier uptime (ns).',"
           "LAST_APPLIER_START TIMESTAMP NOT NULL "
           "COMMENT 'The last time (since server start) when the channel "
           "applier was started.',"
           "TRANSACTIONS_COMMITTED_COUNT BIGINT NOT NULL "
           "COMMENT 'The number of transactions committed so far.',"
           "TRANSACTIONS_ONGOING_COUNT BIGINT NOT NULL "
           "COMMENT 'The number of ongoing transactions in the applier.',"
           "TRANSACTIONS_PENDING_COUNT BIGINT "
           "COMMENT 'The number of transactions that are in queue and not yet "
           "committed, including both unscheduled and ongoing transactions; or "
           "NULL when this has not yet been computed.',"
           "TRANSACTIONS_COMMITTED_SIZE_BYTES_SUM BIGINT NOT NULL "
           "COMMENT 'The size in bytes of transactions committed so far. For "
           "compressed transactions, this counts the decompressed size and not "
           "the payload event.',"
           "TRANSACTIONS_ONGOING_FULL_SIZE_BYTES_SUM BIGINT NOT NULL "
           "COMMENT 'The size in bytes of transactions currently being "
           "applied. "
           "For compressed transactions, this counts the decompressed size and "
           "not the payload event.',"
           "TRANSACTIONS_ONGOING_PROGRESS_SIZE_BYTES_SUM BIGINT NOT NULL "
           "COMMENT 'The size in bytes of already executed events for "
           "transactions currently being applied. "
           "For compressed transactions, this counts the decompressed size and "
           "not the payload event.',"
           "TRANSACTIONS_PENDING_SIZE_BYTES_SUM BIGINT "
           "COMMENT 'The size in bytes of transactions that are in queue and "
           "not yet committed, including both unscheduled and ongoing "
           "transactions; or NULL when this has not yet been computed. "
           "For compressed transactions, this counts the decompressed size and "
           "not the payload event.',"
           "EVENTS_COMMITTED_COUNT BIGINT NOT NULL "
           "COMMENT 'The total number of events in all committed transactions. "
           "For compressed transactions, this counts all the embedded events, "
           "not the payload/envelope event.',"
           "WAITS_FOR_WORK_FROM_SOURCE_COUNT BIGINT NOT NULL "
           "COMMENT 'Number of times when the replication applier is waiting "
           "for work from upstream, i.e., when the coordinator has scheduled "
           "all transactions that are received.',"
           "WAITS_FOR_WORK_FROM_SOURCE_SUM_TIME BIGINT NOT NULL "
           "COMMENT 'Sum of the time (ns) spent by the replication applier "
           "waiting for work from upstream, i.e., when the coordinator has "
           "scheduled all transactions that are received.',"
           "WAITS_FOR_AVAILABLE_WORKER_COUNT BIGINT NOT NULL "
           "COMMENT 'The number of times the replication applier waited with "
           "scheduling a transaction until some worker became available.',"
           "WAITS_FOR_AVAILABLE_WORKER_SUM_TIME BIGINT NOT NULL "
           "COMMENT 'The aggregated time (ns) the replication applier "
           "waited with scheduling a transaction until some worker became "
           "available.',"
           "WAITS_COMMIT_SCHEDULE_DEPENDENCY_COUNT BIGINT NOT NULL "
           "COMMENT 'The number of times the replication applier waited for "
           "the set of dependent transactions to commit before scheduling a "
           "transaction.',"
           "WAITS_COMMIT_SCHEDULE_DEPENDENCY_SUM_TIME BIGINT NOT NULL "
           "COMMENT 'The aggregated time (ns) the replication applier "
           "waited for the set of dependent transactions to commit before "
           "scheduling a transaction.',"
           "WAITS_FOR_WORKER_QUEUE_MEMORY_COUNT BIGINT NOT NULL "
           "COMMENT 'The number of times the replication applier waited with "
           "scheduling an event to a worker until the worker has reduced the "
           "queue size below replica_pending_jobs_size_max bytes.',"
           "WAITS_FOR_WORKER_QUEUE_MEMORY_SUM_TIME BIGINT NOT NULL "
           "COMMENT 'The aggregated time (ns) the replication applier "
           "waited with scheduling an event to a worker until the worker has "
           "reduced the queue size below replica_pending_jobs_size_max bytes.',"
           "WAITS_WORKER_QUEUES_FULL_COUNT BIGINT NOT NULL "
           "COMMENT 'The number of times the replication applier waited "
           "because the queue for the worker contained the maximum number of "
           "16384 events.',"
           "WAITS_WORKER_QUEUES_FULL_SUM_TIME BIGINT NOT NULL "
           "COMMENT 'The aggregated time (ns) the replication applier waited "
           "because the queue for the worker contained the maximum number of "
           "16384 events.',"
           "WAITS_DUE_TO_COMMIT_ORDER_COUNT BIGINT NOT NULL "
           "COMMENT 'The number of times a worker waited for a preceding "
           "transaction to commit before committing the current transaction.',"
           "WAITS_DUE_TO_COMMIT_ORDER_SUM_TIME BIGINT NOT NULL "
           "COMMENT 'The total time (ns) workers waited for a preceding "
           "transaction to commit before committing the current transaction.',"
           "TIME_TO_READ_FROM_RELAY_LOG_SUM_TIME BIGINT NOT NULL "
           "COMMENT 'Time spent (ns) by the replication applier on "
           "reading events from relay log.'";
  }

  /// Return a const reference to the Row_view_definition_t which defines how
  /// to retrieve row values from a Packet.
  static const auto &get_row_view_definition() {
    static Type_info_t::Row_view_definition_t definition{{
        {applier_metrics_channel_name_t, string_type},  // CHANNEL_NAME
        {applier_execution_time_t,
         longlong_type},                         // TOTAL_ACTIVE_TIME_DURATION
        {last_applier_start_t, timestamp_type},  // LAST_CHANNEL_START
        {transactions_committed_t,
         longlong_type},                         // TRANSACTIONS_COMMITTED_COUNT
        {transaction_ongoing_t, longlong_type},  // TRANSACTIONS_ONGOING_COUNT
        {transaction_pending_t, longlong_type,
         are_transaction_pending_counts_unknown_t},  // TRANSACTIONS_PENDING_COUNT
        {transactions_committed_size_sum_t,
         longlong_type},  // TRANSACTIONS_COMMITTED_SIZE_BYTES_SUM
        {transactions_ongoing_full_size_sum_t,
         longlong_type},  // TRANSACTIONS_ONGOING_FULL_SIZE_BYTES_SUM
        {transactions_ongoing_progress_size_sum_t,
         longlong_type},  // TRANSACTIONS_ONGOING_PROGRESS_SIZE_BYTES_SUM
        {transactions_pending_size_sum_t, longlong_type,
         are_transaction_pending_sizes_unknown_t},  // TRANSACTIONS_PENDING_SIZE_BYTES_SUM
        {events_committed_count_t, longlong_type},  // EVENTS_COMMITTED_COUNT
        {waits_for_work_from_source_count_t,
         longlong_type},  // WAITS_FOR_WORK_FROM_SOURCE_COUNT
        {waits_for_work_from_source_sum_time_t,
         longlong_type},  // WAITS_FOR_WORK_FROM_SOURCE_SUM_TIME
        {waits_for_available_worker_count_t,
         longlong_type},  // WAITS_FOR_AVAILABLE_WORKER_COUNT
        {waits_for_available_worker_sum_time_t,
         longlong_type},  // WAITS_FOR_AVAILABLE_WORKER_SUM_TIME
        {waits_for_commit_dependency_count_t,
         longlong_type},  // WAITS_COMMIT_SCHEDULE_DEPENDENCY_COUNT
        {waits_for_commit_dependency_sum_time_t,
         longlong_type},  // WAITS_COMMIT_SCHEDULE_DEPENDENCY_SUM_TIME
        {waits_for_queues_memory_count_t,
         longlong_type},  // WAITS_FOR_WORKER_QUEUE_MEMORY_COUNT
        {waits_for_queues_memory_sum_time_t,
         longlong_type},  // WAITS_FOR_WORKER_QUEUE_MEMORY_SUM_TIME
        {waits_for_queues_full_count_t,
         longlong_type},  // WAITS_WORKER_QUEUES_FULL_COUNT
        {waits_for_queues_full_sum_time_t,
         longlong_type},  // WAITS_WORKER_QUEUES_FULL_SUM_TIME
        {waits_due_to_commit_order_count_t,
         longlong_type},  // WAITS_DUE_TO_COMMIT_ORDER_COUNT
        {waits_due_to_commit_order_sum_time_t,
         longlong_type},  // WAITS_DUE_TO_COMMIT_ORDER_SUM_TIME
        {time_to_read_from_relay_log_t,
         longlong_type}  // TIME_TO_READ_FROM_RELAY_LOG_SUM_TIME
    }};
    return definition;
  }

  /// Return the table data.
  static auto get_table_data() {
    Type_info_t::Table_t table;
    SERVICE_PLACEHOLDER(replication_applier_metrics)
        ->get_applier_metrics(&table);
    return table;
  }

  /// Free the table data.
  static void free_table_data(Type_info_t::Table_t table) {
    SERVICE_PLACEHOLDER(replication_applier_metrics)
        ->free_applier_metrics(&table);
  }
};

/// Return the table share for the replication_applier_metrics table.
PFS_engine_table_share_proxy *get_applier_metrics_table_share() {
  return get_table_share_from_table_with_cursor<
      Packet_based_table_with_cursor<Applier_metrics_table_with_cursor>>();
}

}  // namespace applier_metrics
