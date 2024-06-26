/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "storage/perfschema/telemetry_pfs_metrics.h"

#include <mysql/psi/mysql_metric.h>  // mysql_meter_(un)register
#include "storage/perfschema/pfs_buffer_container.h"  // PFS metric counters
#include "storage/perfschema/pfs_instr_class.h"       // PFS metric counters

//
// Telemetry metric sources instrumented within the PS itself
// are being defined below.
//

template <typename T, typename U>
constexpr bool CanTypeFitValue(const U value) {
  return ((value > U(0)) == (T(value) > T(0))) && U(T(value)) == value;
}

template <typename T, typename U>
T Clamp(U x) {
  return CanTypeFitValue<T>(x) ? T(x)
         : x < 0               ? std::numeric_limits<T>::min()
                               : std::numeric_limits<T>::max();
}

// simple (no measurement attributes supported) metric callback
template <typename T>
static void get_metric_simple_integer(void *measurement_context,
                                      measurement_delivery_callback_t delivery,
                                      void *delivery_context) {
  assert(measurement_context != nullptr);
  assert(delivery != nullptr);
  // OTEL only supports int64_t integer counters, clamp wider types
  const T measurement = *(T *)measurement_context;
  const auto value = Clamp<int64_t>(measurement);
  delivery->value_int64(delivery_context, value);
}

static void get_metric_mutex_instances_lost(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // see show_func_mutex_instances_lost()
  assert(delivery != nullptr);
  const auto measurement = global_mutex_container.get_lost_counter();
  const auto value = Clamp<int64_t>(measurement);
  delivery->value_int64(delivery_context, value);
}

static PSI_metric_info_v1 ps_metrics[] = {
    {"accounts_lost", "",
     "The number of times a row could not be added to the accounts table "
     "because it was full (Performance_schema_accounts_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(global_account_container.m_lost)>,
     &global_account_container.m_lost},
    {"cond_classes_lost", "",
     "How many condition instruments could not be loaded "
     "(Performance_schema_cond_classes_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(cond_class_lost)>, &cond_class_lost},
    {"cond_instances_lost", "",
     "How many condition instrument instances could not be created "
     "(Performance_schema_cond_instances_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(global_cond_container.m_lost)>,
     &global_cond_container.m_lost},
    {"digest_lost", "",
     "The number of digest instances that could not be instrumented in the "
     "events_statements_summary_by_digest table "
     "(Performance_schema_digest_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(digest_lost)>, &digest_lost},
    {"file_classes_lost", "",
     "How many file instruments could not be loaded "
     "(Performance_schema_file_classes_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(file_class_lost)>, &file_class_lost},
    {"file_handles_lost", "",
     "How many file instrument instances could not be opened "
     "(Performance_schema_file_handles_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(file_handle_lost)>, &file_handle_lost},
    {"file_instances_lost", "",
     "How many file instrument instances could not be created "
     "(Performance_schema_file_instances_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(global_file_container.m_lost)>,
     &global_file_container.m_lost},
    {"hosts_lost", "",
     "The number of times a row could not be added to the hosts table because "
     "it was full (Performance_schema_hosts_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(global_host_container.m_lost)>,
     &global_host_container.m_lost},
    {"index_stat_lost", "",
     "The number of indexes for which statistics were lost "
     "(Performance_schema_index_stat_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<
         decltype(global_table_share_index_container.m_lost)>,
     &global_table_share_index_container.m_lost},
    {"locker_lost", "",
     "How many events are 'lost' or not recorded "
     "(Performance_schema_locker_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(locker_lost)>, &locker_lost},
    {"memory_classes_lost", "",
     "The number of times a memory instrument could not be loaded "
     "(Performance_schema_memory_classes_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(memory_class_lost)>,
     &memory_class_lost},
    {"metadata_lock_lost", "",
     "The number of metadata locks that could not be instrumented in the "
     "metadata_locks table (Performance_schema_metadata_lock_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(global_mdl_container.m_lost)>,
     &global_mdl_container.m_lost},
    {"meter_lost", "",
     "How many meter instruments could not be loaded "
     "(Performance_schema_meter_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(meter_class_lost)>, &meter_class_lost},
    {"metric_lost", "",
     "How many metric instruments could not be loaded "
     "(Performance_schema_metric_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(metric_class_lost)>,
     &metric_class_lost},
    {"logger_lost", "",
     "How many logger instruments could not be loaded "
     "(Performance_schema_logger_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(logger_class_lost)>,
     &logger_class_lost},
    {"mutex_classes_lost", "",
     "How many mutex instruments could not be loaded "
     "(Performance_schema_mutex_classes_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(mutex_class_lost)>, &mutex_class_lost},
    {"mutex_instances_lost", "",
     "How many mutex instrument instances could not be created "
     "(Performance_schema_mutex_instances_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_mutex_instances_lost, nullptr},
    {"nested_statement_lost", "",
     "The number of stored program statements for which statistics were lost "
     "(Performance_schema_nested_statement_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(nested_statement_lost)>,
     &nested_statement_lost},
    {"prepared_statements_lost", "",
     "The number of prepared statements that could not be instrumented in the "
     "prepared_statements_instances table "
     "(Performance_schema_prepared_statements_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(global_prepared_stmt_container.m_lost)>,
     &global_prepared_stmt_container.m_lost},
    {"program_lost", "",
     "The number of stored programs for which statistics were lost "
     "(Performance_schema_program_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(global_program_container.m_lost)>,
     &global_program_container.m_lost},
    {"rwlock_classes_lost", "",
     "How many rwlock instruments could not be loaded "
     "(Performance_schema_rwlock_classes_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(rwlock_class_lost)>,
     &rwlock_class_lost},
    {"rwlock_instances_lost", "",
     "How many rwlock instrument instances could not be created "
     "(Performance_schema_rwlock_instances_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(global_rwlock_container.m_lost)>,
     &global_rwlock_container.m_lost},
    {"session_connect_attrs_longest_seen", "",
     "Longest seen connection attribute received "
     "(Performance_schema_session_connect_attrs_longest_seen)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(session_connect_attrs_longest_seen)>,
     &session_connect_attrs_longest_seen},
    {"session_connect_attrs_lost", "",
     "The number of connections for which connection attribute truncation has "
     "occurred (Performance_schema_session_connect_attrs_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(session_connect_attrs_lost)>,
     &session_connect_attrs_lost},
    {"socket_classes_lost", "",
     "How many socket instruments could not be loaded "
     "(Performance_schema_socket_classes_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(socket_class_lost)>,
     &socket_class_lost},
    {"socket_instances_lost", "",
     "How many socket instrument instances could not be created "
     "(Performance_schema_socket_instances_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(global_socket_container.m_lost)>,
     &global_socket_container.m_lost},
    {"stage_classes_lost", "",
     "How many stage instruments could not be loaded "
     "(Performance_schema_stage_classes_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(stage_class_lost)>, &stage_class_lost},
    {"statement_classes_lost", "",
     "How many statement instruments could not be loaded "
     "(Performance_schema_statement_classes_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(statement_class_lost)>,
     &statement_class_lost},
    {"table_handles_lost", "",
     "How many table instrument instances could not be opened "
     "(Performance_schema_table_handles_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(global_table_container.m_lost)>,
     &global_table_container.m_lost},
    {"table_instances_lost", "",
     "How many table instrument instances could not be created "
     "(Performance_schema_table_instances_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(global_table_share_container.m_lost)>,
     &global_table_share_container.m_lost},
    {"table_lock_stat_lost", "",
     "The number of tables for which lock statistics were lost "
     "(Performance_schema_table_lock_stat_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<
         decltype(global_table_share_lock_container.m_lost)>,
     &global_table_share_lock_container.m_lost},
    {"thread_classes_lost", "",
     "How many thread instruments could not be loaded "
     "(Performance_schema_thread_classes_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(thread_class_lost)>,
     &thread_class_lost},
    {"thread_instances_lost", "",
     "The number of thread instances that could not be instrumented in the "
     "threads table (Performance_schema_thread_instances_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(global_thread_container.m_lost)>,
     &global_thread_container.m_lost},
    {"users_lost", "",
     "The number of times a row could not be added to the users table because "
     "it was full (Performance_schema_users_lost)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_integer<decltype(global_user_container.m_lost)>,
     &global_user_container.m_lost}};

static PSI_meter_info_v1 ps_meters[] = {
    {"mysql.perf_schema", "MySql performance_schema lost instruments", 10, 0, 0,
     ps_metrics, std::size(ps_metrics)}};

void register_pfs_metric_sources() {
  mysql_meter_register(ps_meters, std::size(ps_meters));
}

void unregister_pfs_metric_sources() {
  mysql_meter_unregister(ps_meters, std::size(ps_meters));
}
