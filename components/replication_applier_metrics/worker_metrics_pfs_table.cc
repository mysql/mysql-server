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

#include "worker_metrics_pfs_table.h"
#include "mysql/components/component_implementation.h"  // SERVICE_PLACEHOLDER
#include "mysql/components/services/rpl_applier_metrics_service.h"  // Enum_applier_metric_type
#include "packet_based_table_with_cursor.h"  // Packet_based_table_with_cursor
#include "row_proxy.h"                       // Row_proxy
#include "table_with_cursor.h"  // get_table_share_from_table_with_cursor

extern REQUIRES_SERVICE_PLACEHOLDER(replication_applier_metrics);

namespace applier_metrics {

/// Implementation of the replication_applier_progress_by_worker table.
class Worker_metrics_table_with_cursor {
 public:
  using Type_info_t =
      Row_proxy_type_info<Enum_worker_metric_type, worker_metric_type_end, 6>;

  /// Return the table name.
  static constexpr const char *get_table_name() {
    return "replication_applier_progress_by_worker";
  }

  /// Return the table definition.
  static constexpr const char *get_table_definition() {
    return "CHANNEL_NAME CHAR(64) NOT NULL COMMENT 'The channel name.',"
           "WORKER_ID BIGINT NOT NULL COMMENT 'The worker id.',"
           "THREAD_ID BIGINT COMMENT 'The thread id.',"
           "ONGOING_TRANSACTION_TYPE ENUM('UNASSIGNED', 'DML', 'DDL') NOT NULL "
           "COMMENT 'The type of the ongoing transaction.',"
           "ONGOING_TRANSACTION_FULL_SIZE_BYTES BIGINT NOT NULL "
           "COMMENT 'The total size in bytes of the ongoing transaction. "
           "For compressed transactions, this counts the decompressed size and "
           "not the payload event.',"
           "ONGOING_TRANSACTION_APPLIED_SIZE_BYTES BIGINT NOT NULL "
           "COMMENT 'The size in bytes of all the events executed for the "
           "ongoing transaction. "
           "For compressed transactions, this counts the decompressed size and "
           "not the payload event.'";
  }

  /// Return a const reference to the Row_proxy_definition_t which defines how
  /// to retrieve row values from a Packet.
  static const auto &get_row_view_definition() {
    static Type_info_t::Row_view_definition_t definition{{
        {worker_metrics_channel_name_t, string_type},          // CHANNEL_NAME
        {worker_id_t, longlong_type},                          // WORKER_ID
        {thread_id_t, longlong_type, is_thread_id_unknown_t},  // THREAD_ID
        {transaction_type_t, enum_type},  // ONGOING_TRANSACTION_TYPE
        {transaction_ongoing_full_size_t,
         longlong_type},  // ONGOING_TRANSACTION_FULL_SIZE_BYTES
        {transaction_ongoing_progress_size_t,
         longlong_type}  // ONGOING_TRANSACTION_APPLIED_SIZE_BYTES
    }};
    return definition;
  }

  /// Return the table data.
  static auto get_table_data() {
    Type_info_t::Table_t table;
    SERVICE_PLACEHOLDER(replication_applier_metrics)
        ->get_worker_metrics(&table);
    return table;
  }

  /// Free the table data.
  static void free_table_data(Type_info_t::Table_t table) {
    SERVICE_PLACEHOLDER(replication_applier_metrics)
        ->free_worker_metrics(&table);
  }
};

/// Return the table share for the replication_applier_progress_by_worker table.
PFS_engine_table_share_proxy *get_worker_metrics_table_share() {
  return get_table_share_from_table_with_cursor<
      Packet_based_table_with_cursor<Worker_metrics_table_with_cursor>>();
}

}  // namespace applier_metrics
