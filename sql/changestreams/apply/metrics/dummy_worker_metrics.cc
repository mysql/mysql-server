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

#include "dummy_worker_metrics.h"

namespace cs::apply::instruments {

void Dummy_worker_metrics::reset() { return; }

Worker_metrics::Transaction_type_info
Dummy_worker_metrics::get_transaction_type() const {
  return Worker_metrics::Transaction_type_info::UNKNOWN;
}

void Dummy_worker_metrics::set_transaction_type(
    Dummy_worker_metrics::Transaction_type_info) {
  return;
}

void Dummy_worker_metrics::set_transaction_ongoing_full_size(int64_t) {}

int64_t Dummy_worker_metrics::get_transaction_ongoing_full_size() const {
  return 0;
}

void Dummy_worker_metrics::inc_transaction_ongoing_progress_size(int64_t) {
  return;
}

void Dummy_worker_metrics::reset_transaction_ongoing_progress_size() { return; }

int64_t Dummy_worker_metrics::get_transaction_ongoing_progress_size() const {
  return 0;
}

int64_t Dummy_worker_metrics::get_wait_time_on_commit_order() const {
  return 0;
}

void Dummy_worker_metrics::inc_waited_time_on_commit_order(unsigned long) {
  return;
}

int64_t Dummy_worker_metrics::get_number_of_waits_on_commit_order() const {
  return 0;
}

void Dummy_worker_metrics::inc_number_of_waits_on_commit_order() { return; }

}  // namespace cs::apply::instruments
