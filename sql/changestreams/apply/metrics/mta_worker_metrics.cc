// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
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

#include "mta_worker_metrics.h"

namespace cs::apply::instruments {

Mta_worker_metrics &Mta_worker_metrics::operator=(
    const Mta_worker_metrics &other) {
  m_transaction_type = other.get_transaction_type();
  m_transaction_ongoing_full_size = other.get_transaction_ongoing_full_size();
  m_transaction_ongoing_progress_size =
      other.get_transaction_ongoing_progress_size();
  m_waits_due_to_commit_order = other.m_waits_due_to_commit_order;
  return *this;
}

void Mta_worker_metrics::reset() { *this = Mta_worker_metrics(); }

Mta_worker_metrics::Transaction_type_info
Mta_worker_metrics::get_transaction_type() const {
  return m_transaction_type;
}

void Mta_worker_metrics::set_transaction_type(
    Mta_worker_metrics::Transaction_type_info type_info) {
  m_transaction_type = type_info;
}

void Mta_worker_metrics::set_transaction_ongoing_full_size(int64_t amount) {
  m_transaction_ongoing_full_size = amount;
}

int64_t Mta_worker_metrics::get_transaction_ongoing_full_size() const {
  return m_transaction_ongoing_full_size;
}

void Mta_worker_metrics::inc_transaction_ongoing_progress_size(int64_t amount) {
  m_transaction_ongoing_progress_size += amount;
}

void Mta_worker_metrics::reset_transaction_ongoing_progress_size() {
  m_transaction_ongoing_progress_size = 0;
}

int64_t Mta_worker_metrics::get_transaction_ongoing_progress_size() const {
  return m_transaction_ongoing_progress_size;
}

Time_based_metric_interface &
Mta_worker_metrics::get_waits_due_to_commit_order() {
  return m_waits_due_to_commit_order;
}

}  // namespace cs::apply::instruments
