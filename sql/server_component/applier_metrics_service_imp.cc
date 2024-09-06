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

#include "applier_metrics_service_imp.h"
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/rpl_applier_metrics_service.h>
#include <cassert>
#include <cstring>
#include "mysql/abi_helpers/packet.h"  // Packet_builder
#include "sql/psi_memory_key.h"
#include "sql/rpl_mi.h"
#include "sql/rpl_msr.h"  // channel_map
#include "sql/rpl_rli_pdb.h"

// This holds now; in case we remove metrics in the future, we may have
// to adjust.
static constexpr int number_of_applier_metrics = applier_metric_type_end;
static constexpr int number_of_worker_metrics = worker_metric_type_end;

DEFINE_BOOL_METHOD(Applier_metrics_service_handler::get_applier_metrics,
                   (Applier_metrics_table * table_p)) {
  channel_map.rdlock();

  // Count channels/rows.
  size_t row_count = 0;
  for (auto *mi : channel_map.all_channels_view()) {
    if (Multisource_info::is_channel_configured(mi)) ++row_count;
  }

  // Allocate array of rows
  auto &table = *table_p;
  table.allocate(row_count, key_memory_applier_metric_service);

  // Iterate over channels/rows
  auto row_it = table.begin();
  for (auto *mi : channel_map.all_channels_view()) {
    if (!Multisource_info::is_channel_configured(mi)) continue;

    // Allocate this row
    auto &row = *row_it;
    row.allocate(number_of_applier_metrics, key_memory_applier_metric_service);

    // Initialize builder
    mysql::abi_helpers::Packet_builder<Enum_applier_metric_type> builder(row);

    // Get pointer to data source
    auto &coord_metrics = mi->rli->get_applier_metrics();

    // Store all the fields in the row
    builder.push_string_copy(applier_metrics_channel_name_t, mi->get_channel(),
                             key_memory_applier_metric_service);

    builder.push_int(applier_execution_time_t,
                     coord_metrics.get_total_execution_time());

    builder.push_int(last_applier_start_t,
                     coord_metrics.get_last_applier_start_micros());

    int64_t transactions_committed_count =
        coord_metrics.get_transactions_committed_count();
    int64_t transactions_committed_size_sum =
        coord_metrics.get_transactions_committed_size_sum();
    builder.push_int(transactions_committed_t, transactions_committed_count);

    unsigned long long transaction_ongoing_count{0};
    unsigned long long transactions_ongoing_full_size_sum{0};
    unsigned long long transactions_ongoing_progress_size_sum{0};
    unsigned long long waits_due_to_commit_order_count{0};
    unsigned long long waits_due_to_commit_order_sum_time{0};
    // Prevent concurrent threads from deleting workers
    mysql_mutex_lock(&mi->rli->data_lock);
    auto worker_count = mi->rli->get_worker_count();
    for (size_t i = 0; i < worker_count; i++) {
      auto &worker_metrics = mi->rli->get_worker(i)->get_worker_metrics();
      if (worker_metrics.get_transaction_ongoing_full_size() > 0) {
        transaction_ongoing_count++;
      }
      transactions_ongoing_full_size_sum +=
          worker_metrics.get_transaction_ongoing_full_size();
      transactions_ongoing_progress_size_sum +=
          worker_metrics.get_transaction_ongoing_progress_size();

      waits_due_to_commit_order_count +=
          worker_metrics.get_number_of_waits_on_commit_order();
      waits_due_to_commit_order_sum_time +=
          worker_metrics.get_wait_time_on_commit_order();
    }
    mysql_mutex_unlock(&mi->rli->data_lock);

    builder.push_int(transaction_ongoing_t, transaction_ongoing_count);

    builder.push_int(transactions_ongoing_full_size_sum_t,
                     transactions_ongoing_full_size_sum);

    builder.push_int(transactions_ongoing_progress_size_sum_t,
                     transactions_ongoing_progress_size_sum);

    builder.push_int(waits_due_to_commit_order_count_t,
                     waits_due_to_commit_order_count +
                         coord_metrics.get_number_of_waits_on_commit_order());

    builder.push_int(waits_due_to_commit_order_sum_time_t,
                     waits_due_to_commit_order_sum_time +
                         coord_metrics.get_wait_time_on_commit_order());

    int64_t transactions_received_count =
        coord_metrics.get_transactions_received_count();
    int64_t transactions_received_size_sum =
        coord_metrics.get_transactions_received_size_sum();
    int64_t transactions_pending_count = 0;
    int64_t transactions_pending_size_sum = 0;
    bool are_transaction_pending_counts_known = false;
    bool are_transaction_pending_sizes_known = false;

    if (coord_metrics.is_after_metrics_breakpoint()) {
      are_transaction_pending_counts_known = true;
      are_transaction_pending_sizes_known = true;
      transactions_pending_count =
          transactions_received_count - transactions_committed_count;
      transactions_pending_size_sum =
          transactions_received_size_sum - transactions_committed_size_sum;
      if (transactions_pending_size_sum < 0) {
        // Can happen in theory because we don't read
        // transactions_received_size_sum and transactions_committed_size_sum
        // atomically. So it is in theory possible that we read first
        // "received", then we receive and commit another transaction, then we
        // read "committed". Otherwise this should not happen.
        transactions_pending_size_sum = 0;
      }

    } else if (global_gtid_mode.get() == Gtid_mode::ON) {
      // If we did not yet count the number of pending transactions, we can
      // estimate it by looking at the GTID variables.
      Tsid_map tsid_map{nullptr};
      Gtid_set unapplied_gtids{&tsid_map, nullptr};
      mi->rli->get_tsid_lock()->wrlock();
      enum_return_status r_status =
          unapplied_gtids.add_gtid_set(mi->rli->get_gtid_set());
      mi->rli->get_tsid_lock()->unlock();

      if (RETURN_STATUS_OK != r_status) {
        are_transaction_pending_counts_known = false;
        transactions_pending_count = 0;
      } else {
        global_tsid_lock->wrlock();
        unapplied_gtids.remove_gtid_set(gtid_state->get_executed_gtids());
        global_tsid_lock->unlock();

        are_transaction_pending_counts_known = true;
        transactions_pending_count = unapplied_gtids.get_count();
      }
    }

    builder.push_bool(are_transaction_pending_counts_unknown_t,
                      !are_transaction_pending_counts_known);

    builder.push_int(transaction_pending_t, transactions_pending_count);

    builder.push_bool(are_transaction_pending_sizes_unknown_t,
                      !are_transaction_pending_sizes_known);

    builder.push_int(transactions_pending_size_sum_t,
                     transactions_pending_size_sum);

    builder.push_int(transactions_committed_size_sum_t,
                     transactions_committed_size_sum);

    builder.push_int(events_committed_count_t,
                     coord_metrics.get_events_committed_count());

    builder.push_int(
        waits_for_work_from_source_count_t,
        coord_metrics.get_work_from_source_wait_metric().get_count());

    builder.push_int(waits_for_work_from_source_sum_time_t,
                     coord_metrics.get_work_from_source_wait_metric()
                         .get_sum_time_elapsed());

    builder.push_int(
        waits_for_available_worker_count_t,
        coord_metrics.get_workers_available_wait_metric().get_count());

    builder.push_int(waits_for_available_worker_sum_time_t,
                     coord_metrics.get_workers_available_wait_metric()
                         .get_sum_time_elapsed());

    builder.push_int(
        waits_for_commit_dependency_count_t,
        coord_metrics.get_transaction_dependency_wait_metric().get_count());

    builder.push_int(waits_for_commit_dependency_sum_time_t,
                     coord_metrics.get_transaction_dependency_wait_metric()
                         .get_sum_time_elapsed());

    builder.push_int(
        waits_for_queues_memory_count_t,
        coord_metrics.get_worker_queues_memory_exceeds_max_wait_metric()
            .get_count());

    builder.push_int(
        waits_for_queues_memory_sum_time_t,
        coord_metrics.get_worker_queues_memory_exceeds_max_wait_metric()
            .get_sum_time_elapsed());

    builder.push_int(
        waits_for_queues_full_count_t,
        coord_metrics.get_worker_queues_full_wait_metric().get_count());

    builder.push_int(waits_for_queues_full_sum_time_t,
                     coord_metrics.get_worker_queues_full_wait_metric()
                         .get_sum_time_elapsed());

    builder.push_int(time_to_read_from_relay_log_t,
                     coord_metrics.get_time_to_read_from_relay_log_metric()
                         .get_sum_time_elapsed());

    assert(builder.get_position() == number_of_applier_metrics);

    ++row_it;
  }
  channel_map.unlock();
  return 0;
}

DEFINE_METHOD(void, Applier_metrics_service_handler::free_applier_metrics,
              (Applier_metrics_table * table)) {
  for (auto &row : *table) {
    my_free(row[0].m_data.m_string);
    row.free();
  }
  table->free();
}

DEFINE_BOOL_METHOD(Applier_metrics_service_handler::get_worker_metrics,
                   (Worker_metrics_table * table_p)) {
  channel_map.rdlock();

  // Temporary row storage. We can't compute the number of rows in advance,
  // because that would require holding the data locks for all channels at the
  // same time. Instead we use a growable vector, and copy it to the output once
  // we are done.
  std::vector<Worker_metrics_row> row_vector;

  // Iterate over all channels.
  for (auto *mi : channel_map.all_channels_view()) {
    if (!Multisource_info::is_channel_configured(mi)) {
      continue;
    }

    // Prevent concurrent threads from deleting workers
    mysql_mutex_lock(&mi->rli->data_lock);

    // Iterate over workers in this channel.
    auto worker_count = mi->rli->get_worker_count();
    for (std::size_t worker_index = 0; worker_index != worker_count;
         ++worker_index) {
      // Allocate this row
      auto &row = row_vector.emplace_back();
      row.allocate(number_of_worker_metrics, key_memory_applier_metric_service);

      // Initialize builder
      mysql::abi_helpers::Packet_builder<Enum_worker_metric_type> builder(row);

      // Get data source
      auto *worker = mi->rli->get_worker(worker_index);

      // Store all the fields in the row
      builder.push_string_copy(worker_metrics_channel_name_t, mi->get_channel(),
                               key_memory_applier_metric_service);

      builder.push_int(worker_id_t, worker->id);

      bool is_thread_id_available = false;
      ulonglong thread_id = 0;

#ifdef HAVE_PSI_THREAD_INTERFACE
      mysql_mutex_lock(&worker->jobs_lock);
      if (worker->running_status == Slave_worker::RUNNING) {
        PSI_thread *psi = thd_get_psi(worker->info_thd);
        if (psi != nullptr) {
          thread_id = PSI_THREAD_CALL(get_thread_internal_id)(psi);
          is_thread_id_available = true;
        }
      }
      mysql_mutex_unlock(&worker->jobs_lock);

#endif /* HAVE_PSI_THREAD_INTERFACE */

      builder.push_bool(is_thread_id_unknown_t, !is_thread_id_available);
      builder.push_int(thread_id_t, thread_id);

      builder.push_int(
          transaction_ongoing_full_size_t,
          worker->get_worker_metrics().get_transaction_ongoing_full_size());
      builder.push_int(
          transaction_ongoing_progress_size_t,
          worker->get_worker_metrics().get_transaction_ongoing_progress_size());

      {
        using cs::apply::instruments::Worker_metrics;
        Worker_transaction_type transaction_type = UNKNOWN_TRX_TYPE;
        switch (worker->get_worker_metrics().get_transaction_type()) {
          case Worker_metrics::Transaction_type_info::UNKNOWN:
            transaction_type = UNKNOWN_TRX_TYPE;
            break;
          case Worker_metrics::Transaction_type_info::DML:
            transaction_type = DML_TRX_TYPE;
            break;
          case Worker_metrics::Transaction_type_info::DDL:
            transaction_type = DDL_TRX_TYPE;
            break;
        }
        builder.push_int(transaction_type_t, transaction_type);
      }

      assert(builder.get_position() == number_of_worker_metrics);
    }

    mysql_mutex_unlock(&mi->rli->data_lock);
  }
  channel_map.unlock();

  // Copy rows from the temporary vector to the output.
  auto &table = *table_p;
  table.allocate(row_vector.size(), key_memory_applier_metric_service);
  if (!row_vector.empty()) {
    std::memcpy(table.data(), row_vector.data(),
                sizeof(Worker_metrics_row) * row_vector.size());
  }

  return 0;
}

DEFINE_METHOD(void, Applier_metrics_service_handler::free_worker_metrics,
              (Worker_metrics_table * table)) {
  for (auto &row : *table) {
    my_free(row[0].m_data.m_string);
    row.free();
  }
  table->free();
}

DEFINE_BOOL_METHOD(Applier_metrics_service_handler::enable_metric_collection,
                   ()) {
  enable_applier_metric_collection();
  return 0;
}

DEFINE_BOOL_METHOD(Applier_metrics_service_handler::disable_metric_collection,
                   ()) {
  disable_applier_metric_collection();
  return 0;
}
