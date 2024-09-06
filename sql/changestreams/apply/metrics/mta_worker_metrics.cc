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

#include "mta_worker_metrics.h"

namespace cs::apply::instruments {

void Mta_worker_metrics::reset() {
  m_transaction_type = Transaction_type_info::UNKNOWN;
  m_transaction_ongoing_full_size = 0;
  m_transaction_ongoing_progress_size = 0;
  m_order_commit_wait_count = 0;
  m_order_commit_waited_time = 0;
}

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

int64_t Mta_worker_metrics::get_wait_time_on_commit_order() const {
  return m_order_commit_waited_time;
}

void Mta_worker_metrics::inc_waited_time_on_commit_order(unsigned long amount) {
  m_order_commit_waited_time += amount;
}

int64_t Mta_worker_metrics::get_number_of_waits_on_commit_order() const {
  return m_order_commit_wait_count;
}

void Mta_worker_metrics::inc_number_of_waits_on_commit_order() {
  m_order_commit_wait_count++;
}

void Mta_worker_metrics::copy_stats_from(const Mta_worker_metrics &other) {
  m_transaction_type = other.get_transaction_type();
  m_transaction_ongoing_full_size = other.get_transaction_ongoing_full_size();
  m_transaction_ongoing_progress_size =
      other.get_transaction_ongoing_progress_size();
  m_order_commit_waited_time = other.get_wait_time_on_commit_order();
  m_order_commit_wait_count = other.get_number_of_waits_on_commit_order();
}

}  // namespace cs::apply::instruments
