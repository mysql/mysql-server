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

#include "applier_metrics.h"
#include <chrono>

namespace cs::apply::instruments {

void Applier_metrics::reset() {
  m_last_applier_start_micros = 0;
  m_sum_applier_execution_time.reset();
  m_transactions_committed = 0;
  m_transactions_received_count = 0;
  m_first_received_relay_log.assign("");
  m_metrics_breakpoint_state.store(Metrics_breakpoint_state::unset);
  m_transactions_committed_size_sum = 0;
  m_transactions_received_size_sum = 0;
  m_events_committed_count = 0;

  m_wait_for_work_from_source.reset();
  m_wait_for_worker_available.reset();
  m_wait_for_transaction_dependency.reset();
  m_wait_due_to_worker_queues_memory_exceeds_max.reset();
  m_wait_due_to_worker_queue_full.reset();
  m_time_to_read_from_relay_log.reset();
  m_order_commit_wait_count = 0;
  m_order_commit_waited_time = 0;
}

void Applier_metrics::start_applier_timer() {
  m_sum_applier_execution_time.start_timer();
  auto now = std::chrono::system_clock::now().time_since_epoch();
  auto now_micros = std::chrono::duration_cast<std::chrono::microseconds>(now);
  m_last_applier_start_micros = now_micros.count();
}

void Applier_metrics::stop_applier_timer() {
  m_sum_applier_execution_time.stop_timer();
}

int64_t Applier_metrics::get_last_applier_start_micros() const {
  return m_last_applier_start_micros;
}

int64_t Applier_metrics::get_total_execution_time() const {
  return m_sum_applier_execution_time.get_sum_time_elapsed();
}

void Applier_metrics::inc_transactions_committed_count(int64_t amount) {
  m_transactions_committed += amount;
}

int64_t Applier_metrics::get_transactions_committed_count() const {
  return m_transactions_committed;
}

void Applier_metrics::inc_transactions_received_count(int64_t amount) {
  m_transactions_received_count += amount;
}

int64_t Applier_metrics::get_transactions_received_count() const {
  return m_transactions_received_count;
}

void Applier_metrics::inc_transactions_committed_size_sum(int64_t amount) {
  m_transactions_committed_size_sum += amount;
}

int64_t Applier_metrics::get_transactions_committed_size_sum() const {
  return m_transactions_committed_size_sum;
}

void Applier_metrics::inc_transactions_received_size_sum(int64_t amount) {
  m_transactions_received_size_sum += amount;
}

int64_t Applier_metrics::get_transactions_received_size_sum() const {
  return m_transactions_received_size_sum;
}

void Applier_metrics::inc_events_committed_count(int64_t delta) {
  m_events_committed_count += delta;
}

int64_t Applier_metrics::get_events_committed_count() const {
  return m_events_committed_count;
}

void Applier_metrics::set_metrics_breakpoint(const char *relay_log_filename) {
  // Called by receiver
  if (m_metrics_breakpoint_state.load(std::memory_order_acquire) ==
      Metrics_breakpoint_state::unset) {
    m_first_received_relay_log.assign(relay_log_filename);
    m_metrics_breakpoint_state.store(Metrics_breakpoint_state::before,
                                     std::memory_order_release);
  }
}

bool Applier_metrics::is_after_metrics_breakpoint() const {
  return m_metrics_breakpoint_state.load(std::memory_order_acquire) ==
         Metrics_breakpoint_state::after;
}

void Applier_metrics::check_metrics_breakpoint(const char *relay_log_filename) {
  // Called by coordinator
  if (m_metrics_breakpoint_state.load(std::memory_order_acquire) ==
      Metrics_breakpoint_state::before) {
    if (m_first_received_relay_log == relay_log_filename) {
      m_metrics_breakpoint_state.store(Metrics_breakpoint_state::after,
                                       std::memory_order_release);
    }
  }
}

Time_based_metric_interface &
Applier_metrics::get_work_from_source_wait_metric() {
  return m_wait_for_work_from_source;
}

Time_based_metric_interface &
Applier_metrics::get_workers_available_wait_metric() {
  return m_wait_for_worker_available;
}

Time_based_metric_interface &
Applier_metrics::get_transaction_dependency_wait_metric() {
  return m_wait_for_transaction_dependency;
}

Time_based_metric_interface &
Applier_metrics::get_worker_queues_memory_exceeds_max_wait_metric() {
  return m_wait_due_to_worker_queues_memory_exceeds_max;
}

Time_based_metric_interface &
Applier_metrics::get_worker_queues_full_wait_metric() {
  return m_wait_due_to_worker_queue_full;
}

Time_based_metric_interface &
Applier_metrics::get_time_to_read_from_relay_log_metric() {
  return m_time_to_read_from_relay_log;
}

void Applier_metrics::inc_commit_order_wait_stored_metrics(int64_t count,
                                                           int64_t time) {
  m_order_commit_wait_count += count;
  m_order_commit_waited_time += time;
}

int64_t Applier_metrics::get_number_of_waits_on_commit_order() const {
  return m_order_commit_wait_count;
}

int64_t Applier_metrics::get_wait_time_on_commit_order() const {
  return m_order_commit_waited_time;
}

}  // namespace cs::apply::instruments
