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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_statistics_manager.h"

// SUM VARS
uint64_t Gcs_xcom_statistics_manager_interface_impl::get_sum_var_value(
    Gcs_cumulative_statistics_enum to_get) const {
  return m_sum_statistics.at(to_get);
}
void Gcs_xcom_statistics_manager_interface_impl::set_sum_var_value(
    Gcs_cumulative_statistics_enum to_set, uint64_t to_add) {
  m_sum_statistics.at(to_set) += to_add;
}

// COUNT VARS
uint64_t Gcs_xcom_statistics_manager_interface_impl::get_count_var_value(
    Gcs_counter_statistics_enum to_get) const {
  return m_count_statistics.at(to_get);
}

void Gcs_xcom_statistics_manager_interface_impl::set_count_var_value(
    Gcs_counter_statistics_enum to_set) {
  m_count_statistics.at(to_set)++;
}

// TIMESTAMP VALUES
unsigned long long
Gcs_xcom_statistics_manager_interface_impl::get_timestamp_var_value(
    Gcs_time_statistics_enum to_get) const {
  return m_time_statistics.at(to_get);
}

void Gcs_xcom_statistics_manager_interface_impl::set_timestamp_var_value(
    Gcs_time_statistics_enum to_set, unsigned long long new_value) {
  m_time_statistics.at(to_set) = new_value;
}

void Gcs_xcom_statistics_manager_interface_impl::set_sum_timestamp_var_value(
    Gcs_time_statistics_enum to_set, unsigned long long to_add) {
  m_time_statistics.at(to_set) += to_add;
}

// ALL OTHER VARS
std::vector<Gcs_node_suspicious>
Gcs_xcom_statistics_manager_interface_impl::get_all_suspicious() const {
  std::vector<Gcs_node_suspicious> retval;

  for (auto const &[node, number_of_fails] : m_suspicious_statistics)
    retval.push_back({node, number_of_fails});

  return retval;
}

void Gcs_xcom_statistics_manager_interface_impl::add_suspicious_for_a_node(
    std::string node_id) {
  if (const auto &[it, inserted] =
          m_suspicious_statistics.try_emplace(node_id, 1);
      !inserted) {
    it->second++;
  }
}