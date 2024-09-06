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

#include "applier_metrics_stub.h"

namespace cs::apply::instruments {

void Applier_metrics_stub::reset() {}

void Applier_metrics_stub::start_applier_timer() {}

void Applier_metrics_stub::stop_applier_timer() {}

int64_t Applier_metrics_stub::get_last_applier_start_micros() const {
  return 0;
}

int64_t Applier_metrics_stub::get_total_execution_time() const { return 0; }

void Applier_metrics_stub::inc_transactions_committed_count(int64_t) {}

int64_t Applier_metrics_stub::get_transactions_committed_count() const {
  return 0;
}

void Applier_metrics_stub::inc_transactions_received_count(int64_t) {}

int64_t Applier_metrics_stub::get_transactions_received_count() const {
  return 0;
}

void Applier_metrics_stub::inc_transactions_committed_size_sum(int64_t) {}

int64_t Applier_metrics_stub::get_transactions_committed_size_sum() const {
  return 0;
}

void Applier_metrics_stub::inc_transactions_received_size_sum(int64_t) {}

int64_t Applier_metrics_stub::get_transactions_received_size_sum() const {
  return 0;
}

void Applier_metrics_stub::inc_events_committed_count(int64_t) {}

int64_t Applier_metrics_stub::get_events_committed_count() const { return 0; }

bool Applier_metrics_stub::is_after_metrics_breakpoint() const { return false; }

void Applier_metrics_stub::set_metrics_breakpoint(const char *) {}

void Applier_metrics_stub::check_metrics_breakpoint(const char *) {}

Time_based_metric_interface &
Applier_metrics_stub::get_work_from_source_wait_metric() {
  return m_wait_for_work_from_source;
}

Time_based_metric_interface &
Applier_metrics_stub::get_workers_available_wait_metric() {
  return m_wait_for_worker_available;
}

Time_based_metric_interface &
Applier_metrics_stub::get_transaction_dependency_wait_metric() {
  return m_wait_for_transaction_dependency;
}

Time_based_metric_interface &
Applier_metrics_stub::get_worker_queues_memory_exceeds_max_wait_metric() {
  return m_wait_due_to_worker_queues_memory_exceeds_max;
}

Time_based_metric_interface &
Applier_metrics_stub::get_worker_queues_full_wait_metric() {
  return m_wait_due_to_worker_queue_full;
}

Time_based_metric_interface &
Applier_metrics_stub::get_time_to_read_from_relay_log_metric() {
  return m_time_to_read_from_relay_log;
}

void Applier_metrics_stub::inc_commit_order_wait_stored_metrics(int64_t,
                                                                int64_t) {}

int64_t Applier_metrics_stub::get_number_of_waits_on_commit_order() const {
  return 0;
}

int64_t Applier_metrics_stub::get_wait_time_on_commit_order() const {
  return 0;
}

}  // namespace cs::apply::instruments
